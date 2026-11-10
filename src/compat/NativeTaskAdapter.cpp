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
#include <sstream>
#include <algorithm>

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

            TeleportDestinationConfig customDest;
            uint32_t landingMap = def->mapId;
            float landingX = 0.0f;
            float landingY = 0.0f;
            float landingZ = 0.0f;
            float landingO = 0.0f;

            if (PopulationPolicy::Instance().GetCityLandingDestination(cityId, customDest))
            {
                landingMap = customDest.mapId;
                landingX = customDest.x;
                landingY = customDest.y;
                landingZ = customDest.z;
                landingO = customDest.o;
            }
            else
            {
#if defined(TEST_DOUBLES_MODE)
                landingX = def->centerX;
                landingY = def->centerY;
                landingZ = def->centerZ;
                landingO = 0.0f;
#else
                landingX = def->bankX;
                landingY = def->bankY;
                landingZ = def->bankZ;
                landingO = def->bankO;
#endif
            }

#if !defined(TEST_DOUBLES_MODE)
            // Disperse arrival coordinates slightly across a 1.0 to 2.5 yard radius
            // so multiple bots arriving inside the bank do not stack on the exact same tile.
            float angle = static_cast<float>(rand() % 628) / 100.0f;
            float radius = 1.0f + static_cast<float>(rand() % 15) / 10.0f;
            landingX += std::cos(angle) * radius;
            landingY += std::sin(angle) * radius;
            landingO = angle;

            // Cross-map dynamic floor height sampling (mod-twow-arrival-floor)
            landingZ = SampleFloorHeight(landingMap, landingX, landingY, landingZ, 15.0f);
#endif

            bot->TeleportTo(landingMap, landingX, landingY, landingZ, landingO);
            RegisterArrivalFloor(bot, landingMap, landingX, landingY, landingZ,
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
                    ai->Reset(true);
                    ai->ResetStrategies();
                    ai->ChangeStrategy("-rpg,-rpg quest,-rpg vendor,-rpg explore,-rpg maintenance,-rpg craft,-rpg guild", BotState::BOT_STATE_NON_COMBAT);
                    ai->ChangeStrategy("+travel,+grind", BotState::BOT_STATE_NON_COMBAT);
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
                if (it->second.transitMode == TransitMode::Teleport)
                {
                    // City to World Teleport: Teleport back to appropriate outdoor leveling zone with full strategy reset
                    TeleportBotToLevelZone(bot, bot->GetLevel(), bot->GetTeamId());
                }

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

// Turtle WoW Zone Level Chart - Canonical Hardcoded Leveling Destinations per Bracket (No Cities)
static const std::vector<TeleportZoneDestination> s_AllianceBracketZones[7] =
{
    // Bracket 0: 1-9 (Elwynn Forest, Dun Morogh, Teldrassil, Thalassian Highlands)
    {
        { 0, -8921.09f,  -119.14f,  82.20f, 5.83f }, // Northshire Valley
        { 0, -9448.55f,    68.24f,  56.32f, 2.11f }, // Goldshire
        { 0, -6231.77f,   332.99f, 383.17f, 0.48f }, // Coldridge Valley
        { 0, -5597.31f,  -483.40f, 396.98f, 3.18f }, // Kharanos
        { 0, -5385.04f,   310.28f, 394.15f, 5.20f }, // Brewnall Village
        { 1, 10334.00f,   833.90f, 1326.11f, 3.62f }, // Shadowglen
        { 1,  9848.37f,   966.95f, 1306.38f, 3.77f }, // Dolanaar
        { 0,  3321.97f, -2447.81f,  107.18f, 3.47f }, // High Elf Starter / Thalassian Highlands
        { 0,  3625.95f, -2434.33f,   67.09f, 0.07f }  // Thalassian Highlands
    },
    // Bracket 1: 10-19 (Westfall, Loch Modan, Darkshore)
    {
        { 0, -10235.20f,  1222.47f,  43.63f, 6.24f }, // Westfall
        { 0, -10624.50f,  1096.66f,  33.76f, 1.31f }, // Sentinel Hill
        { 0,  -5202.94f, -2855.18f, 336.82f, 0.38f }, // Loch Modan
        { 0,  -5352.54f, -2948.53f, 323.78f, 5.34f }, // Thelsamar
        { 1,   5756.25f,   298.51f,  20.60f, 5.97f }, // Darkshore
        { 1,   6376.26f,   558.12f,  15.11f, 2.59f }  // Auberdine
    },
    // Bracket 2: 20-29 (Redridge Mountains, Duskwood, Wetlands, Ashenvale, Stonetalon, Southshore, Balor)
    {
        { 0,  -9551.81f, -2204.73f,  93.47f, 5.47f }, // Redridge Mountains
        { 0,  -9266.59f, -2188.77f,  64.09f, 2.10f }, // Lakeshire
        { 0, -10898.30f,  -364.78f,  39.27f, 3.05f }, // Duskwood
        { 0, -10573.00f, -1182.51f,  28.01f, 0.31f }, // Darkshire
        { 0,  -3242.81f, -2469.04f,  15.92f, 6.04f }, // Wetlands
        { 0,  -3769.32f,  -744.26f,   8.01f, 1.96f }, // Menethil Harbor
        { 1,   2676.19f,  -422.91f, 107.12f, 0.65f }, // Astranaar (Ashenvale)
        { 1,   2678.38f,  1497.46f, 233.87f, 6.26f }, // Stonetalon Peak
        { 0,   -853.22f,  -533.53f,   9.99f, 0.24f }, // Southshore (Hillsbrad)
        { 0,  -9180.10f,  2573.64f,   9.47f, 0.40f }  // Balor (Turtle WoW)
    },
    // Bracket 3: 30-39 (Arathi Highlands, Alterac, STV, Desolace, Dustwallow, 1000 Needles, Badlands, Swamp of Sorrows, Gilneas)
    {
        { 0,  -1508.51f, -2732.06f,  32.50f, 3.36f }, // Arathi Highlands
        { 0,  -1246.61f, -2529.32f,  20.61f, 0.74f }, // Refuge Pointe
        { 0,    370.76f,  -491.36f, 175.36f, 5.38f }, // Alterac Mountains
        { 0, -11322.40f,  -202.49f,  75.64f, 0.43f }, // Rebel Camp (Northern STV)
        { 0, -14297.20f,   530.99f,   8.78f, 3.99f }, // Booty Bay
        { 1,   -606.40f,  2211.75f,  92.98f, 0.81f }, // Desolace
        { 1,    176.43f,  1309.76f, 190.18f, 0.56f }, // Nijel's Point
        { 1,  -3641.30f, -4358.93f,   8.35f, 3.82f }, // Theramore (Dustwallow Marsh)
        { 1,  -4969.02f, -1726.89f, -62.13f, 3.79f }, // Thousand Needles
        { 0,  -6779.20f, -3423.64f, 241.67f, 0.65f }, // Badlands
        { 0, -10345.40f, -2773.42f,  21.99f, 5.08f }, // Swamp of Sorrows
        { 0,   -696.85f,  1498.40f,   8.70f, 2.72f }, // Gilneas (Turtle WoW)
        { 815,-1095.44f,  2234.75f, 182.86f, 2.60f }  // Gilneas City (Turtle WoW)
    },
    // Bracket 4: 40-49 (Feralas, Tanaris, The Hinterlands, Searing Gorge, Blasted Lands, Azshara, Icepoint Rock, Lapidis)
    {
        { 1,  -4841.19f,  1309.44f,  81.39f, 1.49f }, // Feralas
        { 1,  -4317.47f,  3287.35f,  18.29f, 3.13f }, // Feathermoon Stronghold
        { 1,  -7931.20f, -3414.28f,  80.74f, 0.67f }, // Tanaris
        { 1,  -7177.15f, -3785.34f,   8.37f, 6.10f }, // Gadgetzan
        { 0,    119.39f, -3190.37f, 117.33f, 2.34f }, // The Hinterlands
        { 0,    260.37f, -2125.21f, 119.57f, 3.18f }, // Aerie Peak
        { 0,  -7012.47f, -1065.13f, 241.79f, 5.63f }, // Searing Gorge
        { 0,  -6506.47f, -1149.95f, 307.71f, 4.18f }, // Thorium Point
        { 0, -11182.50f, -3016.67f,   7.42f, 4.00f }, // Blasted Lands
        { 1,   3341.36f, -4603.79f,  92.50f, 5.28f }, // Azshara
        { 1,  13675.00f, -6593.94f,   3.04f, 0.88f }, // Icepoint Rock (Turtle WoW)
        { 0, -12533.20f,  3631.64f,  14.56f, 0.19f }  // Lapidis Isle (Turtle WoW)
    },
    // Bracket 5: 50-59 (Western Plaguelands, Eastern Plaguelands, Winterspring, Felwood, Burning Steppes, Un'Goro, Silithus, Tel'Abim, Hyjal)
    {
        { 0,   1728.65f, -1602.25f,  63.43f, 1.66f }, // Western Plaguelands
        { 0,   2300.97f, -4613.36f,  73.62f, 0.37f }, // Eastern Plaguelands
        { 0,   2289.89f, -5281.52f,  82.13f, 4.55f }, // Light's Hope Chapel
        { 1,   6759.18f, -4419.63f, 763.21f, 4.43f }, // Winterspring
        { 1,   6725.69f, -4619.44f, 720.91f, 4.67f }, // Everlook
        { 1,   4102.25f, -1006.79f, 272.72f, 0.79f }, // Felwood
        { 0,  -8118.54f, -1633.83f, 132.99f, 0.09f }, // Burning Steppes
        { 0,  -8372.77f, -2754.46f, 186.62f, 3.43f }, // Morgan's Vigil
        { 0,  -6506.47f, -1149.95f, 307.71f, 4.18f }, // Thorium Point
        { 1,  -7943.22f, -2119.09f,-218.34f, 6.07f }, // Un'Goro Crater
        { 1,  -6152.25f, -1087.60f,-201.44f, 0.71f }, // Marshal's Refuge
        { 1,  -7426.87f,  1005.31f,   1.13f, 2.96f }, // Silithus
        { 1,  -6818.09f,   733.81f,  41.57f, 2.31f }, // Cenarion Hold
        { 1,  -8878.96f, -6536.87f,  13.48f, 1.53f }, // Tel'Abim (Turtle WoW)
        { 1,   4602.55f, -3881.32f, 944.18f, 0.95f }  // Hyjal Entrance (Turtle WoW)
    },
    // Bracket 6: 60 (Endgame Level 60 Zones)
    {
        { 0,   1728.65f, -1602.25f,  63.43f, 1.66f }, // Western Plaguelands
        { 0,   2289.89f, -5281.52f,  82.13f, 4.55f }, // Light's Hope Chapel (EPL)
        { 1,  -6818.09f,   733.81f,  41.57f, 2.31f }, // Cenarion Hold (Silithus)
        { 1,   6725.69f, -4619.44f, 720.91f, 4.67f }, // Everlook (Winterspring)
        { 0,  -8372.77f, -2754.46f, 186.62f, 3.43f }, // Morgan's Vigil (Burning Steppes)
        { 0,  -6506.47f, -1149.95f, 307.71f, 4.18f }, // Thorium Point
        { 1,  -6152.25f, -1087.60f,-201.44f, 0.71f }, // Marshal's Refuge (Un'Goro)
        { 1,  -8878.96f, -6536.87f,  13.48f, 1.53f }, // Tel'Abim (Turtle WoW)
        { 1,   4602.55f, -3881.32f, 944.18f, 0.95f }  // Hyjal Entrance (Turtle WoW)
    }
};

static const std::vector<TeleportZoneDestination> s_HordeBracketZones[7] =
{
    // Bracket 0: 1-9 (Durotar, Tirisfal Glades, Mulgore, Blackstone Island)
    {
        { 1,  -601.29f, -4296.76f,  37.81f, 1.65f }, // Valley of Trials
        { 1,   326.81f, -4706.65f,  15.37f, 4.16f }, // Razor Hill
        { 1,  -828.00f, -4927.10f,  20.16f, 1.58f }, // Sen'jin Village
        { 0,  1843.50f,  1590.00f,  93.30f, 3.09f }, // Deathknell
        { 0,  2259.25f,   290.43f,  34.11f, 0.99f }, // Brill
        { 1, -2919.35f,  -264.54f,  53.62f, 0.41f }, // Camp Narache
        { 1, -2240.91f,  -399.17f,  -9.42f, 2.53f }, // Bloodhoof Village
        { 1,  -275.60f, -7474.59f,  73.19f, 2.51f }  // Blackstone Island (Goblin)
    },
    // Bracket 1: 10-19 (Northern Barrens, Silverpine Forest, Durotar)
    {
        { 1,  -452.84f, -2650.76f,  95.52f, 0.24f }, // The Crossroads
        { 1,    48.00f, -2715.55f,  91.67f, 0.16f }, // Barrens
        { 0,   878.74f,  1359.33f,  50.36f, 5.90f }, // Silverpine Forest
        { 0,   504.53f,  1539.08f, 129.50f, 1.36f }, // The Sepulcher
        { 1,   326.81f, -4706.65f,  15.37f, 4.16f }  // Razor Hill
    },
    // Bracket 2: 20-29 (Southern Barrens, Ashenvale, Stonetalon, Hillsbrad, Thousand Needles, Balor)
    {
        { 1, -2372.00f, -1960.00f,  96.00f, 0.00f }, // Camp Taurajo
        { 1,  2270.94f, -2538.19f,  93.92f, 0.06f }, // Splintertree Post (Ashenvale)
        { 1,  3376.86f,  1013.05f,   3.34f, 3.82f }, // Zoram'gar Outpost
        { 1,  1570.92f,  1031.52f, 137.96f, 3.33f }, // Stonetalon Mountains
        { 1,   966.15f,   926.50f, 104.65f, 1.27f }, // Sun Rock Retreat
        { 0,   -34.15f,  -923.37f,  54.56f, 0.15f }, // Tarren Mill (Hillsbrad)
        { 0,  -436.66f,  -581.25f,  53.59f, 1.26f }, // Hillsbrad Foothills
        { 1, -5431.78f, -2449.38f,  89.28f, 2.33f }, // Freewind Post (Thousand Needles)
        { 0, -9180.10f,  2573.64f,   9.47f, 0.40f }  // Balor (Turtle WoW)
    },
    // Bracket 3: 30-39 (Arathi Highlands, Alterac, STV, Desolace, Dustwallow, 1000 Needles, Badlands, Swamp of Sorrows, Gilneas)
    {
        { 0,  -941.01f, -3526.66f,  70.94f, 3.49f }, // Hammerfall (Arathi)
        { 0,  -1508.51f, -2732.06f,  32.50f, 3.36f }, // Arathi Highlands
        { 0,    370.76f,  -491.36f, 175.36f, 5.38f }, // Alterac Mountains
        { 0, -12388.90f,   172.58f,   2.83f, 1.92f }, // Grom'gol Base Camp (STV)
        { 0, -14297.20f,   530.99f,   8.78f, 3.99f }, // Booty Bay
        { 1,  -606.40f,  2211.75f,  92.98f, 0.81f }, // Desolace
        { 1, -1690.00f,  3280.00f,  65.00f, 0.00f }, // Ghost Walker Post
        { 1, -3130.67f, -2908.43f,  34.10f, 1.43f }, // Brackenwall Village (Dustwallow)
        { 1, -5431.78f, -2449.38f,  89.28f, 2.33f }, // Freewind Post (Thousand Needles)
        { 0, -6692.48f, -2175.31f, 244.15f, 0.43f }, // Kargath (Badlands)
        { 0, -10446.90f, -3261.91f,  20.18f, 5.02f }, // Stonard (Swamp of Sorrows)
        { 0,   -696.85f,  1498.40f,   8.70f, 2.72f }, // Gilneas (Turtle WoW)
        { 815,-1095.44f,  2234.75f, 182.86f, 2.60f }  // Gilneas City (Turtle WoW)
    },
    // Bracket 4: 40-49 (Feralas, Tanaris, The Hinterlands, Searing Gorge, Badlands, Swamp of Sorrows, Blasted Lands, Azshara, Icepoint, Lapidis)
    {
        { 1, -4396.70f,   224.84f,  25.41f, 4.94f }, // Camp Mojache (Feralas)
        { 1, -7931.20f, -3414.28f,  80.74f, 0.67f }, // Tanaris
        { 1, -7177.15f, -3785.34f,   8.37f, 6.10f }, // Gadgetzan
        { 0,  -557.23f, -4581.27f,   9.59f, 1.02f }, // Revantusk Village (Hinterlands)
        { 0, -7012.47f, -1065.13f, 241.79f, 5.63f }, // Searing Gorge
        { 0, -6506.47f, -1149.95f, 307.71f, 4.18f }, // Thorium Point
        { 0, -6692.48f, -2175.31f, 244.15f, 0.43f }, // Kargath
        { 0, -10446.90f, -3261.91f,  20.18f, 5.02f }, // Stonard
        { 0, -11182.50f, -3016.67f,   7.42f, 4.00f }, // Blasted Lands
        { 1,  3341.36f, -4603.79f,  92.50f, 5.28f }, // Azshara
        { 1,  1980.00f, -4250.00f,  45.00f, 0.00f }, // Valormok
        { 1, 13675.00f, -6593.94f,   3.04f, 0.88f }, // Icepoint Rock (Turtle WoW)
        { 0, -12533.20f,  3631.64f,  14.56f, 0.19f }  // Lapidis Isle (Turtle WoW)
    },
    // Bracket 5: 50-59 (Western Plaguelands, Eastern Plaguelands, Winterspring, Felwood, Burning Steppes, Un'Goro, Silithus, Tel'Abim, Hyjal)
    {
        { 0,  1728.65f, -1602.25f,  63.43f, 1.66f }, // Western Plaguelands
        { 0,  2300.97f, -4613.36f,  73.62f, 0.37f }, // Eastern Plaguelands
        { 0,  2289.89f, -5281.52f,  82.13f, 4.55f }, // Light's Hope Chapel
        { 1,  6759.18f, -4419.63f, 763.21f, 4.43f }, // Winterspring
        { 1,  6725.69f, -4619.44f, 720.91f, 4.67f }, // Everlook
        { 1,  2380.00f, -1170.00f, 318.00f, 0.00f }, // Bloodvenom Post (Felwood)
        { 0, -7501.51f, -2183.08f, 165.93f, 6.07f }, // Flame Crest (Burning Steppes)
        { 0, -6506.47f, -1149.95f, 307.71f, 4.18f }, // Thorium Point
        { 1, -7943.22f, -2119.09f,-218.34f, 6.07f }, // Un'Goro Crater
        { 1, -6152.25f, -1087.60f,-201.44f, 0.71f }, // Marshal's Refuge
        { 1, -7426.87f,  1005.31f,   1.13f, 2.96f }, // Silithus
        { 1, -6818.09f,   733.81f,  41.57f, 2.31f }, // Cenarion Hold
        { 1, -8878.96f, -6536.87f,  13.48f, 1.53f }, // Tel'Abim (Turtle WoW)
        { 1,  4602.55f, -3881.32f, 944.18f, 0.95f }  // Hyjal Entrance (Turtle WoW)
    },
    // Bracket 6: 60 (Endgame Level 60 Zones)
    {
        { 0,  1728.65f, -1602.25f,  63.43f, 1.66f }, // Western Plaguelands
        { 0,  2289.89f, -5281.52f,  82.13f, 4.55f }, // Light's Hope Chapel (EPL)
        { 1, -6818.09f,   733.81f,  41.57f, 2.31f }, // Cenarion Hold (Silithus)
        { 1,  6725.69f, -4619.44f, 720.91f, 4.67f }, // Everlook (Winterspring)
        { 0, -7501.51f, -2183.08f, 165.93f, 6.07f }, // Flame Crest (Burning Steppes)
        { 0, -6506.47f, -1149.95f, 307.71f, 4.18f }, // Thorium Point
        { 1, -6152.25f, -1087.60f,-201.44f, 0.71f }, // Marshal's Refuge (Un'Goro)
        { 1, -8878.96f, -6536.87f,  13.48f, 1.53f }, // Tel'Abim (Turtle WoW)
        { 1,  4602.55f, -3881.32f, 944.18f, 0.95f }  // Hyjal Entrance (Turtle WoW)
    }
};

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

    // 5. Autogear Equipment Pass (based on server configuration caps, not Best/epic gear)
    factory.UsePoolGearCap();
    factory.AutogearOwned(sPlayerbotAIConfig.ownedBotAutogearQualityCap, sPlayerbotAIConfig.ownedBotAutogearIlvlCap);
    factory.EnchantEquipment();

    // 6. Starter Kit, Projectiles & Consumables
    factory.EnsureStarterKit();
    factory.AddFood();
    factory.AddConsumes();
    factory.AddBandages();

    // 7. Full Item Durability Maintenance & Vital Stat Recovery
    bot->DurabilityRepairAll(false, 0.0f);
    bot->SetHealth(bot->GetMaxHealth());
    if (bot->GetMaxPower(POWER_MANA) > 0)
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
    else if (bot->GetMaxPower(POWER_ENERGY) > 0)
        bot->SetPower(POWER_ENERGY, bot->GetMaxPower(POWER_ENERGY));

    // 8. AI Travel Target Expiration & State Reset
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

bool NativeTaskAdapter::ResolveTeleportLocation(const std::string& input, TeleportDestinationConfig& outDest)
{
    size_t first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return false;
    size_t last = input.find_last_not_of(" \t\r\n");
    std::string token = input.substr(first, (last - first + 1));
    if (token.empty())
        return false;

    // 1. First, try parsing as raw numbers: "mapId x y z [o]"
    {
        std::istringstream iss(token);
        uint32_t mapId = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (iss >> mapId >> x >> y >> z)
        {
            float o = 0.0f;
            iss >> o;
            outDest.mapId = mapId;
            outDest.x = x;
            outDest.y = y;
            outDest.z = z;
            outDest.o = o;
            outDest.name = token;
            return true;
        }
    }

    // 2. Try looking up in core's game_tele store via sObjectMgr.GetGameTele(name)
#if !defined(TEST_DOUBLES_MODE)
    const GameTele* tele = sObjectMgr.GetGameTele(token);
    if (tele)
    {
        outDest.mapId = tele->mapId;
        outDest.x = tele->x;
        outDest.y = tele->y;
        outDest.z = tele->z;
        outDest.o = tele->o;
        outDest.name = tele->name;
        return true;
    }

    // Normalized match: strip apostrophes, hyphens, and spaces (e.g. "gromgol" -> "Grom'golBaseCamp")
    std::string cleanQuery = token;
    cleanQuery.erase(std::remove_if(cleanQuery.begin(), cleanQuery.end(), [](char c) {
        return c == '\'' || c == '-' || c == ' ' || c == '_';
    }), cleanQuery.end());
    std::transform(cleanQuery.begin(), cleanQuery.end(), cleanQuery.begin(), ::tolower);

    const auto& teleMap = sObjectMgr.GetGameTeleMap();
    for (const auto& pair : teleMap)
    {
        std::string candClean = pair.second.name;
        candClean.erase(std::remove_if(candClean.begin(), candClean.end(), [](char c) {
            return c == '\'' || c == '-' || c == ' ' || c == '_';
        }), candClean.end());
        std::transform(candClean.begin(), candClean.end(), candClean.begin(), ::tolower);

        if (candClean == cleanQuery || candClean.find(cleanQuery) != std::string::npos || cleanQuery.find(candClean) != std::string::npos)
        {
            outDest.mapId = pair.second.mapId;
            outDest.x = pair.second.x;
            outDest.y = pair.second.y;
            outDest.z = pair.second.z;
            outDest.o = pair.second.o;
            outDest.name = pair.second.name;
            return true;
        }
    }
#else
    outDest.mapId = 0;
    outDest.x = 0.0f;
    outDest.y = 0.0f;
    outDest.z = 0.0f;
    outDest.o = 0.0f;
    outDest.name = token;
    return true;
#endif

    return false;
}

std::vector<TeleportDestinationConfig> NativeTaskAdapter::ParseTeleportLocationPool(const std::string& commaSeparated)
{
    std::vector<TeleportDestinationConfig> pool;
    std::istringstream ss(commaSeparated);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        TeleportDestinationConfig dest;
        if (ResolveTeleportLocation(token, dest))
            pool.push_back(dest);
    }
    return pool;
}

bool NativeTaskAdapter::TeleportBotToLevelZone(Player* bot, uint8_t newLevel, uint8_t teamId)
{
    if (!bot || !bot->IsInWorld())
        return false;

    // Clear active movement generators to stop bot running on old path
    if (bot->GetMotionMaster())
        bot->GetMotionMaster()->Clear(false);

    if (bot->IsMounted())
        bot->Unmount();

    // Item durability maintenance and vital stat replenishment upon teleportation
    bot->DurabilityRepairAll(false, 0.0f);
    bot->SetHealth(bot->GetMaxHealth());
    if (bot->GetMaxPower(POWER_MANA) > 0)
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
    else if (bot->GetMaxPower(POWER_ENERGY) > 0)
        bot->SetPower(POWER_ENERGY, bot->GetMaxPower(POWER_ENERGY));

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
    if (ai)
    {
        // Expire any stale travel target pointing to old zone/continent
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
        // AI reset and strategy renewal for open-world outdoor activities (World-to-World or City-to-World)
        ai->Reset(true);
        ai->ResetStrategies();
        ai->ChangeStrategy("-rpg,-rpg quest,-rpg vendor,-rpg explore,-rpg maintenance,-rpg craft,-rpg guild", BotState::BOT_STATE_NON_COMBAT);
        ai->ChangeStrategy("+travel,+grind", BotState::BOT_STATE_NON_COMBAT);
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

    if (teamId > 1)
        return false;

    uint32_t targetMapId = 0;
    float landingX = 0.0f;
    float landingY = 0.0f;
    float landingZ = 0.0f;
    float landingO = 0.0f;
    bool destFound = false;

    // Racial starter zone awareness for level 1-9 (bracket 0)
    if (newLevel < 10)
    {
        uint8_t race = bot->GetRace();
        const auto& starterPool = PopulationPolicy::Instance().GetStarterLocationPool(race);
        if (!starterPool.empty())
        {
            const auto& picked = starterPool[rand() % starterPool.size()];
            targetMapId = picked.mapId;
            landingX = picked.x;
            landingY = picked.y;
            landingZ = picked.z;
            landingO = picked.o;
            destFound = true;
        }
        else
        {
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
                targetMapId = racialDest.mapId;
                landingX = racialDest.x;
                landingY = racialDest.y;
                landingZ = racialDest.z;
                landingO = racialDest.o;
                destFound = true;
            }
        }
    }

    if (!destFound)
    {
        size_t zoneIndex = newLevel < 10 ? 0 : std::min<size_t>(newLevel / 10, 6);
        const auto& rangePool = PopulationPolicy::Instance().GetRangeLocationPool(teamId, zoneIndex);
        if (!rangePool.empty())
        {
            const auto& picked = rangePool[rand() % rangePool.size()];
            targetMapId = picked.mapId;
            landingX = picked.x;
            landingY = picked.y;
            landingZ = picked.z;
            landingO = picked.o;
            destFound = true;
        }
        else
        {
            const auto& hardcodedPool = (teamId == 0) ? s_AllianceBracketZones[zoneIndex] : s_HordeBracketZones[zoneIndex];
            if (!hardcodedPool.empty())
            {
                const auto& dest = hardcodedPool[rand() % hardcodedPool.size()];
                targetMapId = dest.mapId;
                landingX = dest.x;
                landingY = dest.y;
                landingZ = dest.z;
                landingO = dest.o;
                destFound = true;
            }
        }
    }

    if (destFound)
    {
        float floorZ = SampleFloorHeight(targetMapId, landingX, landingY, landingZ, 25.0f);
        bot->TeleportTo(targetMapId, landingX, landingY, floorZ, landingO);
        RegisterArrivalFloor(bot, targetMapId, landingX, landingY, floorZ,
                             PopulationPolicy::Instance().GetArrivalHoldDurationMs());
        return true;
    }

    return false;
}

bool NativeTaskAdapter::TeleportBotToCity(Player* bot, CityId cityId)
{
    if (!bot || !bot->IsInWorld())
        return false;

    uint64_t reqId = 0;
    uint32_t minDwell = PopulationPolicy::Instance().GetCityMinDwellDurationMs();
    uint32_t maxDwell = PopulationPolicy::Instance().GetCityMaxDwellDurationMs();
    TaskAdmissionResult res = RequestCityVisit(bot, cityId, minDwell, maxDwell, TransitMode::Teleport, 0, reqId);
    return (res == TaskAdmissionResult::Accepted);
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
