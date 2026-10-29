#include "NativeTaskAdapter.h"

#if defined(HAVE_TORTOISEBOTS)
#include "Common.h"
#include "Objects/Player.h"
#include "Group/Group.h"
#include "ObjectAccessor.h"
#include "runtime/BotActivityLease.h"
#include "runtime/BotManager.h"
#include "runtime/PlayerbotAIStorage.h"
#include "ai/playerbot/RandomBotFacade.h"
#include "ai/playerbot/PlayerbotAI.h"
#include "ai/playerbot/TravelMgr.h"
#include "ai/playerbot/strategy/AiObjectContext.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Player.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "playerbot/PlayerbotAI.h"
#include "playerbot/RandomPlayerbotMgr.h"
#else
// Generic Headless / Test doubles
#include "Objects/Player.h"
#include "Group/Group.h"
#include "ObjectAccessor.h"
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
        Player* member = ref->GetSource();
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
    uint32_t dwellDurationMs = 0;
    uint32_t configGeneration = 0;
    enum State : uint8_t { Traveling, Visiting } state = Traveling;
};

static std::unordered_map<ObjectGuid, CityVisitRecord> s_cityVisits;
static std::mutex s_cityVisitsMutex;
static uint64_t s_nextCityRequestId = 1;

void NativeTaskAdapter::Clear()
{
    std::lock_guard<std::mutex> lock(s_cityVisitsMutex);
    s_cityVisits.clear();
}

TaskAdmissionResult NativeTaskAdapter::RequestCityVisit(Player* bot, CityId cityId,
                                                        uint32_t dwellDurationMs,
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
        rec.dwellDurationMs = dwellDurationMs;
        rec.configGeneration = configGeneration;

        if (IsBotInCity(bot, cityId))
        {
            rec.state = CityVisitRecord::Visiting;
            rec.arrivedAt = rec.requestedAt;
            s_cityVisits[bot->GetObjectGuid()] = rec;
        }
        else
        {
            rec.state = CityVisitRecord::Traveling;
            s_cityVisits[bot->GetObjectGuid()] = rec;
        }
    }

    if (IsBotInCity(bot, cityId))
    {
#if defined(HAVE_TORTOISEBOTS)
        PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
        if (ai)
            ai->ChangeStrategy("+rpg", BotState::BOT_STATE_NON_COMBAT);
#elif defined(HAVE_LEGACY_PLAYERBOTS)
        PlayerbotAI* ai = bot->GetPlayerbotAI();
        if (ai)
            ai->ChangeStrategy("+rpg", BotState::BOT_STATE_NON_COMBAT);
#endif
    }
    else
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
            if (IsBotInCity(bot, it->second.cityId))
            {
                it->second.state = CityVisitRecord::Visiting;
                it->second.arrivedAt = now;
#if defined(HAVE_TORTOISEBOTS)
                PlayerbotAI* ai = PlayerbotAIStorage::Instance().GetAI(bot);
                if (ai)
                    ai->ChangeStrategy("+rpg", BotState::BOT_STATE_NON_COMBAT);
#elif defined(HAVE_LEGACY_PLAYERBOTS)
                PlayerbotAI* ai = bot->GetPlayerbotAI();
                if (ai)
                    ai->ChangeStrategy("+rpg", BotState::BOT_STATE_NON_COMBAT);
#endif
            }
            else if (now - it->second.requestedAt > 900)
            {
                it = s_cityVisits.erase(it);
                continue;
            }
        }
        else if (it->second.state == CityVisitRecord::Visiting)
        {
            uint32_t dwellSec = it->second.dwellDurationMs / 1000;
            if (now - it->second.arrivedAt >= dwellSec)
            {
                it = s_cityVisits.erase(it);
                continue;
            }
        }

        ++it;
    }
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

} // namespace BotLevelBrackets
