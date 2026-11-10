#include "NativeTaskAdapter.h"
#include "../policy/PopulationPolicy.h"

#if defined(HAVE_TORTOISEBOTS)
#include "Common.h"
#include "Objects/Player.h"
#include "Group/Group.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Maps/Map.h"
#include "Maps/MapManager.h"
#include "Movement/MovementPacketSender.h"
#include "Movement/MotionMaster.h"
#include "runtime/BotActivityLease.h"
#include "runtime/BotManager.h"
#include "runtime/PlayerbotAIStorage.h"
#include "ai/playerbot/RandomBotFacade.h"
#include "ai/playerbot/PlayerbotAI.h"
#include "ai/playerbot/PlayerbotAIConfig.h"
#include "ai/playerbot/PlayerbotFactory.h"
#include "ai/playerbot/TravelMgr.h"
#include "ai/playerbot/strategy/AiObjectContext.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Player.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "playerbot/PlayerbotAI.h"
#include "playerbot/PlayerbotFactory.h"
#include "playerbot/RandomPlayerbotMgr.h"
#else
// Generic Headless / Test doubles
#include "Objects/Player.h"
#include "Group/Group.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#endif

#include <unordered_map>
#include <mutex>
#include <ctime>

namespace BotLevelBrackets
{

const char* GetTaskAdmissionResultName(TaskAdmissionResult res)
{
    switch (res)
    {
        case TaskAdmissionResult::Accepted:        return "Accepted";
        case TaskAdmissionResult::AlreadyActive:   return "AlreadyActive";
        case TaskAdmissionResult::Busy:            return "Busy";
        case TaskAdmissionResult::Protected:       return "Protected";
        case TaskAdmissionResult::UnsupportedCity: return "UnsupportedCity";
        case TaskAdmissionResult::UnsafeRoute:     return "UnsafeRoute";
        case TaskAdmissionResult::Unavailable:     return "Unavailable";
        case TaskAdmissionResult::BudgetExhausted: return "BudgetExhausted";
        default:                                   return "Failed";
    }
}

EngineKind NativeTaskAdapter::GetEngineKind()
{
#if defined(HAVE_TORTOISEBOTS)
    return EngineKind::TortoiseBots;
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    return EngineKind::LegacyPlayerBots;
#else
    return EngineKind::GenericHeadless;
#endif
}

const char* NativeTaskAdapter::GetEngineName()
{
#if defined(HAVE_TORTOISEBOTS)
    return "TortoiseBots (Native Modular)";
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    return "Legacy PlayerBots";
#else
    return "Generic Headless Session Mode";
#endif
}

bool NativeTaskAdapter::IsBot(Player* player)
{
    if (!player)
        return false;

#if defined(HAVE_TORTOISEBOTS)
    return TortoiseBots::BotManager::Instance().IsBot(player->GetObjectGuid());
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    return sRandomPlayerbotMgr.IsRandomBot(player);
#else
    return player->GetSession() && player->GetSession()->IsHeadless();
#endif
}

bool NativeTaskAdapter::IsRandomBot(Player* player)
{
    if (!player)
        return false;

#if defined(HAVE_TORTOISEBOTS)
    return TortoiseBots::BotManager::Instance().IsRandomBot(player->GetObjectGuid());
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    return sRandomPlayerbotMgr.IsRandomBot(player);
#else
    return IsBot(player);
#endif
}

bool NativeTaskAdapter::IsInRandomAccountList(uint32_t accountId)
{
#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
    return sPlayerbotAIConfig.IsInRandomAccountList(accountId);
#else
    (void)accountId;
    return false;
#endif
}

bool NativeTaskAdapter::HasRealPlayerMaster(Player* player)
{
    if (!player)
        return false;

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(player);
    if (ai && ai->HasRealPlayerMaster())
        return true;
    TortoiseBots::BotRecord* rec = TortoiseBots::BotManager::Instance().FindBot(player->GetObjectGuid());
    if (rec && !rec->masterGuid.IsEmpty())
        return true;
    return false;
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    PlayerbotAI* ai = player->GetPlayerbotAI();
    return ai && ai->HasRealPlayerMaster();
#else
    return false;
#endif
}

bool NativeTaskAdapter::HasRealPlayerInParty(Player* player)
{
    if (!player)
        return false;

    Group* group = player->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->getSource();
        if (member && member->IsInWorld() && member->GetSession() && !member->GetSession()->IsHeadless())
            return true;
    }
    return false;
}

bool NativeTaskAdapter::IsFreeBot(Player* player)
{
    if (!player || !IsBot(player))
        return false;

    if (HasRealPlayerMaster(player))
        return false;

    if (HasRealPlayerInParty(player))
        return false;

#if defined(HAVE_TORTOISEBOTS)
    return sRandomBotFacade.IsFreeBot(player);
#else
    return true;
#endif
}

bool NativeTaskAdapter::IsInWorldAndReady(Player* player)
{
    if (!player || !player->IsInWorld() || !player->IsAlive())
        return false;

    if (player->IsInCombat() || player->IsBeingTeleported())
        return false;

    if (player->GetMap() && (player->GetMap()->IsDungeon() || player->GetMap()->IsRaid() || player->GetMap()->IsBattleGround()))
        return false;

    return true;
}

struct CityVisitRecord
{
    uint64_t requestId = 0;
    ObjectGuid botGuid;
    CityId cityId = CityId::None;
    uint32_t requestedAt = 0;
    uint32_t arrivedAt = 0;
    uint32_t lastPulseAt = 0;
    uint32_t minDwellDurationMs = 300000;
    uint32_t maxDwellDurationMs = 600000;
    TransitMode transitMode = TransitMode::Travel;
    uint32_t configGeneration = 0;
    enum State : uint8_t { Traveling, Visiting } state = Traveling;
};

static std::unordered_map<ObjectGuid, CityVisitRecord> s_cityVisits;
static std::mutex s_cityVisitsMutex;
static uint64_t s_nextCityRequestId = 1;

// -----------------------------------------------------------------------------
// ARRIVAL FLOOR & ANTI-FALL ENGINE (mod-twow-arrival-floor)
// -----------------------------------------------------------------------------
struct ArrivalFloorEntry
{
    ObjectGuid botGuid;
    uint32_t mapId = 0;
    float arrivalX = 0.0f;
    float arrivalY = 0.0f;
    float arrivalZ = 0.0f;
    uint32_t arrivedAt = 0;
    uint32_t holdDurationMs = 5000;
    uint32_t lifts = 0;
};

static std::unordered_map<ObjectGuid, ArrivalFloorEntry> s_arrivalFloors;
static std::mutex s_arrivalFloorMutex;
static std::atomic<uint32_t> s_statsArrivalLifts{0};

float NativeTaskAdapter::SampleFloorHeight(uint32_t mapId, float x, float y, float initialZ, float searchRange)
{
    Map* map = sMapMgr.FindMap(mapId);
    if (!map)
        return initialZ;

    float sampledZ = map->GetHeight(x, y, initialZ + 2.0f, true, searchRange);
    if (sampledZ > -2000.0f && std::abs(sampledZ - initialZ) <= searchRange)
        return sampledZ;

    return initialZ;
}

void NativeTaskAdapter::RegisterArrivalFloor(Player* bot, uint32_t mapId, float x, float y, float z, uint32_t holdDurationMs)
{
    if (!bot)
        return;

    if (!PopulationPolicy::Instance().IsArrivalFloorHoldEnabled())
        return;

    ArrivalFloorEntry entry;
    entry.botGuid = bot->GetObjectGuid();
    entry.mapId = mapId;
    entry.arrivalX = x;
    entry.arrivalY = y;
    entry.arrivalZ = z;
    entry.arrivedAt = static_cast<uint32_t>(time(nullptr));
    entry.holdDurationMs = holdDurationMs;
    entry.lifts = 0;

    std::lock_guard<std::mutex> lock(s_arrivalFloorMutex);
    s_arrivalFloors[bot->GetObjectGuid()] = entry;
}

void NativeTaskAdapter::RemoveArrivalFloor(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_arrivalFloorMutex);
    s_arrivalFloors.erase(botGuid);
}

uint32_t NativeTaskAdapter::GetActiveArrivalFloorHolds()
{
    std::lock_guard<std::mutex> lock(s_arrivalFloorMutex);
    return static_cast<uint32_t>(s_arrivalFloors.size());
}

uint32_t NativeTaskAdapter::GetArrivalLiftsCount()
{
    return s_statsArrivalLifts.load(std::memory_order_relaxed);
}

void NativeTaskAdapter::Clear()
{
    {
        std::lock_guard<std::mutex> lock(s_cityVisitsMutex);
        s_cityVisits.clear();
    }
    {
        std::lock_guard<std::mutex> lock(s_arrivalFloorMutex);
        s_arrivalFloors.clear();
    }
}

void NativeTaskAdapter::ApplyCityArrivalProtocol(Player* bot, CityId cityId)
{
    (void)cityId;
    if (!bot || !bot->IsInWorld() || !bot->IsAlive())
        return;

    bot->StopMoving();
    if (bot->GetMotionMaster())
        bot->GetMotionMaster()->Clear(false);

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
    if (ai)
    {
        // 1. Full AI reset to clean up stale open-world travel targets, expired combat state, and queues
        ai->Reset(true);

        // 2. Configure non-combat strategies to enforce pure native TortoiseBots city protocol
        // Drop open-world grinding and travel, and activate native RPG strategy family
        ai->ChangeStrategy("-travel,-grind,+rpg,+collision", BotState::BOT_STATE_NON_COMBAT);
    }
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    PlayerbotAI* ai = bot->GetPlayerbotAI();
    if (ai)
    {
        ai->ChangeStrategy("-travel,-grind,+rpg", BotState::BOT_STATE_NON_COMBAT);
    }
#endif
}

TaskAdmissionResult NativeTaskAdapter::RequestCityVisit(Player* bot, CityId cityId,
                                                        uint32_t minDwellDurationMs,
                                                        uint32_t maxDwellDurationMs,
                                                        TransitMode transitMode,
                                                        uint32_t configGeneration,
                                                        uint64_t& outRequestId)
{
    outRequestId = 0;
    if (!bot || !IsInWorldAndReady(bot))
        return TaskAdmissionResult::Busy;

    if (!IsFreeBot(bot))
        return TaskAdmissionResult::Protected;

    const CityDefinition* def = ReadOnlyCatalog::Instance().GetCity(cityId);
    if (!def)
        return TaskAdmissionResult::UnsupportedCity;

    bool isVisiting = false;
    {
        std::lock_guard<std::mutex> lock(s_cityVisitsMutex);
        auto it = s_cityVisits.find(bot->GetObjectGuid());
        if (it != s_cityVisits.end())
            return TaskAdmissionResult::AlreadyActive;

        outRequestId = ++s_nextCityRequestId;
        CityVisitRecord rec;
        rec.requestId = outRequestId;
        rec.botGuid = bot->GetObjectGuid();
        rec.cityId = cityId;
        rec.requestedAt = static_cast<uint32_t>(time(nullptr));
        rec.arrivedAt = 0;
        rec.lastPulseAt = 0;
        rec.minDwellDurationMs = minDwellDurationMs;
        rec.maxDwellDurationMs = maxDwellDurationMs;
        rec.transitMode = transitMode;
        rec.configGeneration = configGeneration;

        if (transitMode == TransitMode::Teleport && !IsBotInCity(bot, cityId))
        {
            if (bot->IsMounted())
                bot->Unmount();

            // Disperse arrival coordinates across a 3.0 to 13.0 yard radius with random angle
            // to avoid stacking multiple teleported bots on top of each other.
            float angle = static_cast<float>(rand() % 628) / 100.0f;
            float radius = 3.0f + static_cast<float>(rand() % 100) / 10.0f;
            float landingX = def->centerX + std::cos(angle) * radius;
            float landingY = def->centerY + std::sin(angle) * radius;
            float landingZ = def->centerZ;
            float landingO = angle;

            // Cross-map dynamic floor height sampling (mod-twow-arrival-floor)
            landingZ = SampleFloorHeight(def->mapId, landingX, landingY, def->centerZ, 25.0f);

            bot->TeleportTo(def->mapId, landingX, landingY, landingZ, landingO);
            RegisterArrivalFloor(bot, def->mapId, landingX, landingY, landingZ,
                                 PopulationPolicy::Instance().GetArrivalHoldDurationMs());
            rec.state = CityVisitRecord::Traveling;
            rec.arrivedAt = 0;
            rec.lastPulseAt = 0;
        }
        else if (IsBotInCity(bot, cityId))
        {
            rec.state = CityVisitRecord::Visiting;
            rec.arrivedAt = rec.requestedAt;
            rec.lastPulseAt = rec.requestedAt;
        }
        else
        {
            rec.state = CityVisitRecord::Traveling;
            rec.arrivedAt = 0;
            rec.lastPulseAt = 0;
        }

        s_cityVisits[bot->GetObjectGuid()] = rec;
        isVisiting = (rec.state == CityVisitRecord::Visiting);
    }

    if (isVisiting)
    {
        ApplyCityArrivalProtocol(bot, cityId);
    }
    else if (transitMode != TransitMode::Teleport)
    {
#if defined(HAVE_TORTOISEBOTS)
        PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
        if (ai)
        {
            ai->ChangeStrategy("+travel", BotState::BOT_STATE_NON_COMBAT);
            ai->DoSpecificAction("choose travel target", Event(), true);
        }
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        PlayerbotAI* ai = bot->GetPlayerbotAI();
        if (ai)
        {
            ai->ChangeStrategy("+travel", BotState::BOT_STATE_NON_COMBAT);
            ai->DoSpecificAction("choose travel target", Event(), true);
        }
#endif
    }

    return TaskAdmissionResult::Accepted;
}

TaskAdmissionResult NativeTaskAdapter::RequestCityVisit(Player* bot, CityId cityId,
                                                        uint32_t dwellDurationMs,
                                                        uint32_t configGeneration,
                                                        uint64_t& outRequestId)
{
    return RequestCityVisit(bot, cityId, dwellDurationMs, dwellDurationMs, TransitMode::Travel, configGeneration, outRequestId);
}

bool NativeTaskAdapter::CancelCityVisit(ObjectGuid botGuid, const std::string& ownerId)
{
    (void)ownerId;
    std::lock_guard<std::mutex> lock(s_cityVisitsMutex);
    auto it = s_cityVisits.find(botGuid);
    if (it == s_cityVisits.end())
        return false;

    s_cityVisits.erase(it);
    return true;
}

bool NativeTaskAdapter::HasActiveCityVisit(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_cityVisitsMutex);
    return s_cityVisits.find(botGuid) != s_cityVisits.end();
}

CityId NativeTaskAdapter::GetTargetCity(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_cityVisitsMutex);
    auto it = s_cityVisits.find(botGuid);
    if (it != s_cityVisits.end())
        return it->second.cityId;
    return CityId::None;
}

uint32_t NativeTaskAdapter::GetCityInboundCount(CityId cityId)
{
    std::lock_guard<std::mutex> lock(s_cityVisitsMutex);
    uint32_t count = 0;
    for (const auto& pair : s_cityVisits)
    {
        if (pair.second.cityId == cityId && pair.second.state == CityVisitRecord::Traveling)
            ++count;
    }
    return count;
}

uint32_t NativeTaskAdapter::GetCityResidentCount(CityId cityId)
{
    const CityDefinition* def = ReadOnlyCatalog::Instance().GetCity(cityId);
    if (!def)
        return 0;

    uint32_t count = 0;
#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
    HashMapHolder<Player>::MapType& players = sObjectAccessor.GetPlayers();
    for (auto& pair : players)
    {
        Player* p = pair.second;
        if (p && p->IsInWorld() && IsBot(p) && IsBotInCity(p, cityId))
            ++count;
    }
#endif
    return count;
}

bool NativeTaskAdapter::IsBotInCity(Player* bot, CityId cityId)
{
    if (!bot)
        return false;

    const CityDefinition* def = ReadOnlyCatalog::Instance().GetCity(cityId);
    if (!def || bot->GetMapId() != def->mapId)
        return false;

    float dx = bot->GetPositionX() - def->centerX;
    float dy = bot->GetPositionY() - def->centerY;
    float dz = bot->GetPositionZ() - def->centerZ;
    return (dx * dx + dy * dy + dz * dz) <= (def->radius * def->radius);
}

void NativeTaskAdapter::Update(uint32_t diff)
{
    (void)diff;
    uint32_t now = static_cast<uint32_t>(time(nullptr));
    std::lock_guard<std::mutex> lock(s_cityVisitsMutex);

    for (auto it = s_cityVisits.begin(); it != s_cityVisits.end(); )
    {
        Player* bot = ObjectAccessor::FindPlayer(it->first);
        if (!bot || !bot->IsInWorld() || !bot->IsAlive() || !IsFreeBot(bot))
        {
            it = s_cityVisits.erase(it);
            continue;
        }

        if (it->second.state == CityVisitRecord::Traveling)
        {
            // Wait until teleport/travel has completed and player is in world and physically in city
            if (!bot->IsBeingTeleported() && IsBotInCity(bot, it->second.cityId))
            {
                it->second.state = CityVisitRecord::Visiting;
                it->second.arrivedAt = now;
                it->second.lastPulseAt = now;
                ApplyCityArrivalProtocol(bot, it->second.cityId);
            }
            else if (now - it->second.requestedAt > 900)
            {
                it = s_cityVisits.erase(it);
                continue;
            }
        }
        else if (it->second.state == CityVisitRecord::Visiting)
        {
            if (!IsBotInCity(bot, it->second.cityId))
            {
                it = s_cityVisits.erase(it);
                continue;
            }

            // State maintenance: ensure visiting bot maintains native TortoiseBots city protocol
            if (now - it->second.lastPulseAt >= 5)
            {
                it->second.lastPulseAt = now;
#if defined(HAVE_TORTOISEBOTS)
                PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
                if (ai)
                {
                    if (ai->HasStrategy("travel", BotState::BOT_STATE_NON_COMBAT))
                        ai->ChangeStrategy("-travel", BotState::BOT_STATE_NON_COMBAT);
                    if (ai->HasStrategy("grind", BotState::BOT_STATE_NON_COMBAT))
                        ai->ChangeStrategy("-grind", BotState::BOT_STATE_NON_COMBAT);
                    if (!ai->HasStrategy("rpg", BotState::BOT_STATE_NON_COMBAT))
                        ai->ChangeStrategy("+rpg", BotState::BOT_STATE_NON_COMBAT);
                }
#endif
            }

            uint32_t elapsedSec = now - it->second.arrivedAt;
            uint32_t minDwellSec = it->second.minDwellDurationMs / 1000;
            uint32_t maxDwellSec = it->second.maxDwellDurationMs / 1000;

            bool release = false;
            if (elapsedSec >= minDwellSec)
            {
                if (AreCityErrandsSatisfied(bot, it->second.cityId))
                    release = true;
                else if (elapsedSec >= maxDwellSec)
                    release = true;
            }

            if (release)
            {
#if defined(HAVE_TORTOISEBOTS)
                PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
                if (ai)
                {
                    ai->ChangeStrategy("-rpg,-rpg quest,-rpg vendor,-rpg explore,-rpg maintenance,-rpg craft,-rpg guild", BotState::BOT_STATE_NON_COMBAT);
                    ai->ChangeStrategy("+travel,+grind", BotState::BOT_STATE_NON_COMBAT);
                    ai->Reset(true);
                    ai->DoSpecificAction("choose travel target", Event(), true);
                }
#elif defined(HAVE_LEGACY_PLAYERBOTS)
                PlayerbotAI* ai = bot->GetPlayerbotAI();
                if (ai)
                {
                    ai->ChangeStrategy("-rpg", BotState::BOT_STATE_NON_COMBAT);
                    ai->ChangeStrategy("+travel,+grind", BotState::BOT_STATE_NON_COMBAT);
                    ai->DoSpecificAction("choose travel target", Event(), true);
                }
#endif
                it = s_cityVisits.erase(it);
                continue;
            }
        }

        ++it;
    }

    // -------------------------------------------------------------------------
    // ArrivalFloor Anti-Fall & WMO Floor Hold Engine (mod-twow-arrival-floor)
    // -------------------------------------------------------------------------
    if (PopulationPolicy::Instance().IsArrivalFloorHoldEnabled())
    {
        std::vector<ObjectGuid> expiredOrMoved;
        {
            std::lock_guard<std::mutex> lock(s_arrivalFloorMutex);
            for (auto& pair : s_arrivalFloors)
            {
                ArrivalFloorEntry& entry = pair.second;
                Player* bot = sObjectAccessor.FindPlayer(entry.botGuid);
                if (!bot || !bot->IsInWorld() || !bot->IsAlive() || (bot->GetSession() && bot->GetSession()->isLogingOut()))
                {
                    expiredOrMoved.push_back(pair.first);
                    continue;
                }

                if (bot->IsBeingTeleported())
                    continue;

                if (bot->GetMapId() != entry.mapId)
                {
                    if (now - entry.arrivedAt > 30)
                        expiredOrMoved.push_back(pair.first);
                    continue;
                }

                uint32_t holdSec = entry.holdDurationMs / 1000;
                if (holdSec == 0)
                    holdSec = 5;
                if (now - entry.arrivedAt >= holdSec)
                {
                    expiredOrMoved.push_back(pair.first);
                    continue;
                }

                float dx = bot->GetPositionX() - entry.arrivalX;
                float dy = bot->GetPositionY() - entry.arrivalY;
                if ((dx * dx + dy * dy) > 0.5f * 0.5f)
                {
                    // Bot moved intentionally away from arrival spot: release hold
                    expiredOrMoved.push_back(pair.first);
                    continue;
                }

                if (bot->m_movementInfo.HasMovementFlag(MOVEFLAG_JUMPING))
                {
                    expiredOrMoved.push_back(pair.first);
                    continue;
                }

                // Anti-fall check: bot dropped below arrival floor Z by more than 0.3 yards
                float currentZ = bot->GetPositionZ();
                if (currentZ < entry.arrivalZ - 0.3f && entry.lifts < PopulationPolicy::Instance().GetMaxFloorLifts())
                {
                    bot->NearTeleportTo(entry.arrivalX, entry.arrivalY, entry.arrivalZ, bot->GetOrientation());
                    MovementPacketSender::SendTeleportToObservers(bot, entry.arrivalX, entry.arrivalY, entry.arrivalZ, bot->GetOrientation());
                    entry.lifts++;
                    s_statsArrivalLifts.fetch_add(1, std::memory_order_relaxed);
                }
            }

            for (const auto& guid : expiredOrMoved)
            {
                s_arrivalFloors.erase(guid);
            }
        }
    }
}

bool NativeTaskAdapter::AreCityErrandsSatisfied(Player* bot, CityId cityId)
{
    if (!bot || !bot->IsInWorld() || !bot->IsAlive())
        return true;

    if (!IsBotInCity(bot, cityId))
        return true;

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
    if (!ai || !ai->GetAiObjectContext())
        return true;

    // If bot currently has an active RPG target, it is actively running an errand
    GuidPosition rpgTarget = ai->GetAiObjectContext()->GetValue<GuidPosition>("rpg target")->Get();
    if (rpgTarget && rpgTarget.GetWorldObject(bot->GetInstanceId()))
        return false;

    // Check if there are unvisited potential targets
    std::list<ObjectGuid> possibleTargets = ai->GetAiObjectContext()->GetValue<std::list<ObjectGuid>>("possible rpg targets")->Get();
    if (possibleTargets.empty())
        return true;

    return false;
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    PlayerbotAI* ai = bot->GetPlayerbotAI();
    if (!ai || !ai->GetAiObjectContext())
        return true;
    return !ai->GetAiObjectContext()->GetValue<GuidPosition>("rpg target")->Get();
#else
    return true;
#endif
}

bool NativeTaskAdapter::DispatchNativeAction(Player* bot, const std::string& actionName)
{
    if (!bot || !IsInWorldAndReady(bot))
        return false;

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
    if (!ai)
        return false;
    // Native DoSpecificAction invokes Engine::ExecuteAction which executes isUseful() and isPossible()
    return ai->DoSpecificAction(actionName, Event(), true);
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    PlayerbotAI* ai = bot->GetPlayerbotAI();
    if (!ai)
        return false;
    return ai->DoSpecificAction(actionName, Event(), true);
#else
    (void)actionName;
    return false;
#endif
}

bool NativeTaskAdapter::IsBotTraveling(Player* bot)
{
    if (!bot)
        return false;

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
    if (!ai || !ai->GetAiObjectContext())
        return false;
    TravelTarget* tt = ai->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
    if (tt && tt->GetStatus() == TravelStatus::TRAVEL_STATUS_TRAVEL)
        return true;
#endif
    return false;
}

std::string NativeTaskAdapter::GetBotTravelStatusString(Player* bot)
{
    if (!bot)
        return "Unknown";

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
    if (!ai || !ai->GetAiObjectContext())
        return "NoAI";
    TravelTarget* tt = ai->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
    if (!tt)
        return "NoTarget";

    switch (tt->GetStatus())
    {
        case TravelStatus::TRAVEL_STATUS_NONE:     return "None";
        case TravelStatus::TRAVEL_STATUS_PREPARE:  return "Prepare";
        case TravelStatus::TRAVEL_STATUS_READY:    return "Ready";
        case TravelStatus::TRAVEL_STATUS_TRAVEL:   return "Traveling";
        case TravelStatus::TRAVEL_STATUS_WORK:     return "Working";
        case TravelStatus::TRAVEL_STATUS_COOLDOWN: return "Cooldown";
        case TravelStatus::TRAVEL_STATUS_EXPIRED:  return "Expired";
        default:                                   return "Active";
    }
#else
    return "Idle";
#endif
}

// -----------------------------------------------------------------------------
// LEVEL ADJUSTMENT & PROVISIONING DATA
// -----------------------------------------------------------------------------

struct TeleportZoneDestination
{
    uint32_t mapId;
    float x;
    float y;
    float z;
    float o;
};

struct StarterValleyArea
{
    uint32_t mapId;
    float cx;
    float cy;
    float cz;
    float radius;
    uint8_t teamId;
};

static const StarterValleyArea s_StartingAreas[] =
{
    // Alliance starter valleys
    { 0, -8949.9f,  -132.5f,  83.5f, 250.0f, 0 }, // Northshire Valley (Human)
    { 0, -6240.3f,   331.0f, 382.8f, 250.0f, 0 }, // Coldridge Valley (Dwarf / Gnome)
    { 1, 10311.3f,   832.5f, 1326.4f, 250.0f, 0 }, // Shadowglen (Night Elf)
    { 0,  3322.0f, -2448.0f, 107.2f, 250.0f, 0 }, // Thalassian Highlands (High Elf - Turtle WoW)

    // Horde starter valleys
    { 1,  -618.0f, -4251.0f,  38.0f, 250.0f, 1 }, // Valley of Trials (Orc / Troll)
    { 1, -2917.6f,  -258.0f,  52.9f, 250.0f, 1 }, // Red Cloud Mesa (Tauren)
    { 0,  1676.7f,  1678.3f, 121.7f, 250.0f, 1 }, // Deathknell (Undead)
    { 1,  -275.6f, -7474.6f,  73.2f, 250.0f, 1 }  // Blackstone Island (Goblin - Turtle WoW)
};
static constexpr size_t s_NumStartingAreas = sizeof(s_StartingAreas) / sizeof(s_StartingAreas[0]);

static const TeleportZoneDestination s_AllianceZones[] =
{
    { 0,  -9464.0f,    62.0f,  56.0f, 0.0f },     // 1-9:   Elwynn Forest (Goldshire)
    { 0, -10515.0f,  1070.0f,  55.0f, 0.0f },     // 10-19: Westfall (Sentinel Hill)
    { 0,  -9250.0f, -2160.0f,  64.0f, 0.0f },     // 20-29: Redridge Mountains (Lakeshire)
    { 0, -11340.0f,  -200.0f,  75.0f, 0.0f },     // 30-39: Northern STV (Rebel Camp)
    { 1,  -4380.0f,  2060.0f,  49.0f, 0.0f },     // 40-49: Feralas (Feathermoon Stronghold)
    { 0,    940.0f, -1450.0f,  63.0f, 0.0f },     // 50-59: Western Plaguelands (Chillwind Camp)
    { 0,    940.0f, -1450.0f,  63.0f, 0.0f }      // 60:    Western Plaguelands (outdoors)
};
static constexpr size_t s_NumAllianceZones = sizeof(s_AllianceZones) / sizeof(s_AllianceZones[0]);

static const TeleportZoneDestination s_HordeZones[] =
{
    { 1,   -618.0f, -4251.0f,  38.0f, 0.0f },     // 1-9:   Durotar (Valley of Trials)
    { 1,    310.0f, -4680.0f,  16.0f, 0.0f },     // 10-19: Durotar (Razor Hill)
    { 1,   -440.0f, -2650.0f,  96.0f, 0.0f },     // 20-29: The Barrens (Crossroads)
    { 0,    -24.0f,  -870.0f,  55.0f, 0.0f },     // 30-39: Hillsbrad Foothills (Tarren Mill)
    { 1,  -4400.0f,   190.0f,  25.0f, 0.0f },     // 40-49: Feralas (Camp Mojache)
    { 1,  -7180.0f, -1070.0f, -188.0f, 0.0f },    // 50-59: Un'Goro Crater (Entrance)
    { 1,  -7180.0f, -1070.0f, -188.0f, 0.0f }     // 60:    Un'Goro Crater (outdoors)
};
static constexpr size_t s_NumHordeZones = sizeof(s_HordeZones) / sizeof(s_HordeZones[0]);

bool NativeTaskAdapter::AdjustBotLevel(Player* bot, uint8_t newLevel)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut())
        return false;

    uint8_t oldLevel = bot->GetLevel();
    if (oldLevel == newLevel)
        return true;

    // Core level update
    if (newLevel > oldLevel)
    {
        bot->GiveLevel(newLevel);
    }
    else
    {
        bot->SetLevel(newLevel);
        bot->InitStatsForLevel(false);
        bot->SetUInt32Value(PLAYER_XP, 0);
        bot->SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr.GetXPForLevel(newLevel));
    }

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
    if (!ai)
        return false;

    PlayerbotFactory factory(bot, newLevel);

    // 1. Spec, Talents, Spells
    ai->DoSpecificAction("auto talents");
    ai->DoSpecificAction("auto learn spell");

    if (newLevel < oldLevel)
        factory.PruneOverLevelSpellRanks();

    PlayerbotFactory::EnsureSkillRewardedSpells(bot);
    PlayerbotFactory::EnsureTalentSpellsSaved(bot);

    // 2. Defense, Armor proficiencies, Weapon skills
    factory.InitSkills();

    // 3. Professions, Specializations, Recipes & Tools
    factory.InitAllSkills();
    factory.EnsurePrimaryProfessions();
    factory.EnsureSpecializations();
    factory.LearnSpecializationRecipes();
    factory.AddTools();

    // 4. Riding Skills & Mounts
    factory.InitMounts();

    // 5. Autogear Equipment Pass
    factory.UsePoolGearCap();
    factory.UpgradeGearBest();
    factory.EnchantEquipment();

    // 6. Starter Kit, Projectiles & Consumables
    factory.EnsureStarterKit();
    factory.AddFood();
    factory.AddConsumes();
    factory.AddBandages();

    // 7. AI Travel Target Expiration & State Reset
    if (ai->GetAiObjectContext())
    {
        TravelTarget* travelTarget = ai->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
        if (travelTarget)
        {
            sTravelMgr.SetNullTravelTarget(travelTarget);
            travelTarget->SetStatus(TravelStatus::TRAVEL_STATUS_EXPIRED);
            travelTarget->SetExpireIn(1000);
        }
    }

    ai->UpdateTalentSpec();
    ai->ResetStrategies();
    bot->SaveToDB();
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    PlayerbotFactory factory(bot, newLevel);
    factory.Randomize(false, false);
#endif

    return true;
}

bool NativeTaskAdapter::TeleportBotToLevelZone(Player* bot, uint8_t newLevel, uint8_t teamId)
{
    if (!bot || !bot->IsInWorld())
        return false;

    const TeleportZoneDestination* zones = nullptr;
    size_t zoneCount = 0;
    if (teamId == 0) // Alliance
    {
        zones = s_AllianceZones;
        zoneCount = s_NumAllianceZones;
    }
    else if (teamId == 1) // Horde
    {
        zones = s_HordeZones;
        zoneCount = s_NumHordeZones;
    }
    else
        return false;

    // Racial starter zone awareness for level 1-9 (bracket 0)
    if (newLevel < 10)
    {
        uint8_t race = bot->GetRace();
        TeleportZoneDestination racialDest{0, 0.0f, 0.0f, 0.0f, 0.0f};
        bool hasRacialDest = true;

        switch (race)
        {
            case RACE_HIGH_ELF: // High Elf (Turtle WoW) - Thalassian Highlands
                racialDest = { 0, 3322.0f, -2448.0f, 107.2f, 3.47f };
                break;
            case RACE_GOBLIN: // Goblin (Turtle WoW) - Blackstone Island
                racialDest = { 1, -275.6f, -7474.6f, 73.2f, 2.51f };
                break;
            case RACE_HUMAN: // Human - Northshire Valley
                racialDest = { 0, -8949.9f, -132.5f, 83.5f, 0.0f };
                break;
            case RACE_DWARF: // Dwarf / Gnome - Coldridge Valley
            case RACE_GNOME:
                racialDest = { 0, -6240.3f, 331.0f, 382.8f, 6.17f };
                break;
            case RACE_NIGHTELF: // Night Elf - Shadowglen
                racialDest = { 1, 10311.3f, 832.5f, 1326.4f, 5.69f };
                break;
            case RACE_ORC: // Orc / Troll - Valley of Trials
            case RACE_TROLL:
                racialDest = { 1, -618.0f, -4251.0f, 38.0f, 0.0f };
                break;
            case RACE_TAUREN: // Tauren - Red Cloud Mesa
                racialDest = { 1, -2917.6f, -258.0f, 52.9f, 0.0f };
                break;
            case RACE_UNDEAD: // Undead - Deathknell
                racialDest = { 0, 1676.7f, 1678.3f, 121.7f, 2.71f };
                break;
            default:
                hasRacialDest = false;
                break;
        }

        if (hasRacialDest)
        {
            if (bot->IsMounted())
                bot->Unmount();
            float landingZ = SampleFloorHeight(racialDest.mapId, racialDest.x, racialDest.y, racialDest.z, 25.0f);
            bot->TeleportTo(racialDest.mapId, racialDest.x, racialDest.y, landingZ, racialDest.o);
            RegisterArrivalFloor(bot, racialDest.mapId, racialDest.x, racialDest.y, landingZ,
                                 PopulationPolicy::Instance().GetArrivalHoldDurationMs());
            return true;
        }
    }

    size_t zoneIndex = newLevel < 10 ? 0 : std::min<size_t>(newLevel / 10, zoneCount - 1);
    const TeleportZoneDestination& dest = zones[zoneIndex];

    if (bot->IsMounted())
        bot->Unmount();

    float landingZ = SampleFloorHeight(dest.mapId, dest.x, dest.y, dest.z, 25.0f);
    bot->TeleportTo(dest.mapId, dest.x, dest.y, landingZ, dest.o);
    RegisterArrivalFloor(bot, dest.mapId, dest.x, dest.y, landingZ,
                         PopulationPolicy::Instance().GetArrivalHoldDurationMs());
    return true;
}

bool NativeTaskAdapter::IsInStarterValley(Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return false;

    uint32_t mapId = bot->GetMapId();
    float bx = bot->GetPositionX();
    float by = bot->GetPositionY();
    float bz = bot->GetPositionZ();

    for (size_t i = 0; i < s_NumStartingAreas; ++i)
    {
        const auto& area = s_StartingAreas[i];
        if (mapId != area.mapId)
            continue;

        float dx = bx - area.cx;
        float dy = by - area.cy;
        float dz = bz - area.cz;
        if (dx * dx + dy * dy + dz * dz <= area.radius * area.radius)
            return true;
    }
    return false;
}

bool NativeTaskAdapter::IsOnWrongMap(Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return false;

    if (bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsBattleGround() || bot->GetMap()->IsRaid()))
        return false;

    uint32_t mapId = bot->GetMapId();
    // Valid open-world / recognized maps:
    // 0 = Eastern Kingdoms, 1 = Kalimdor, 369 = Deeprun Tram,
    // 26 = Blood Ring Arena, 27 = Sunnyglade Valley,
    // 813/815/816 = Custom Turtle content instances/islands
    if (mapId == 0 || mapId == 1 || mapId == 369 || mapId == 26 || mapId == 27 ||
        mapId == 813 || mapId == 815 || mapId == 816)
        return false;

    return true;
}

bool NativeTaskAdapter::IsBotSafeForDispersal(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->IsAlive())
        return false;

    if (bot->IsInCombat() || bot->IsBeingTeleported() || bot->IsTaxiFlying() || bot->m_duel != nullptr)
        return false;

    if (bot->InBattleGround() || bot->InBattleGroundQueue())
        return false;

    if (bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsRaid() || bot->GetMap()->IsBattleGround()))
        return false;

    if (!IsFreeBot(bot))
        return false;

    if (bot->GetSession() && bot->GetSession()->isLogingOut())
        return false;

    return true;
}

} // namespace BotLevelBrackets
