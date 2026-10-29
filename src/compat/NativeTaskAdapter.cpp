#include "NativeTaskAdapter.h"

#if defined(HAVE_TORTOISEBOTS)
#include "Common.h"
#include "Objects/Player.h"
#include "Group/Group.h"
#include "runtime/CityVisitService.h"
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
#include "playerbot/PlayerbotAI.h"
#include "playerbot/RandomPlayerbotMgr.h"
#else
// Generic Headless / Test doubles
#include "Objects/Player.h"
#include "Group/Group.h"
#endif

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

#if defined(HAVE_TORTOISEBOTS)
    TortoiseBots::CityId tbCityId = static_cast<TortoiseBots::CityId>(cityId);
    TortoiseBots::CityVisitAdmission adm = TortoiseBots::CityVisitService::Instance().RequestVisit(
        bot, tbCityId, dwellDurationMs, "mod-twow-bot-level-brackets", configGeneration, outRequestId);

    switch (adm)
    {
        case TortoiseBots::CityVisitAdmission::Accepted:
            return TaskAdmissionResult::Accepted;
        case TortoiseBots::CityVisitAdmission::AlreadyActive:
            return TaskAdmissionResult::AlreadyActive;
        case TortoiseBots::CityVisitAdmission::Busy:
            return TaskAdmissionResult::Busy;
        case TortoiseBots::CityVisitAdmission::Protected:
            return TaskAdmissionResult::Protected;
        case TortoiseBots::CityVisitAdmission::UnsupportedCity:
            return TaskAdmissionResult::UnsupportedCity;
        case TortoiseBots::CityVisitAdmission::UnsafeRoute:
            return TaskAdmissionResult::UnsafeRoute;
        case TortoiseBots::CityVisitAdmission::BudgetExhausted:
            return TaskAdmissionResult::BudgetExhausted;
        default:
            return TaskAdmissionResult::Unavailable;
    }
#else
    // Legacy / Generic mode: no native CityVisitService available
    return TaskAdmissionResult::Unavailable;
#endif
}

bool NativeTaskAdapter::CancelCityVisit(ObjectGuid botGuid, const std::string& ownerId)
{
#if defined(HAVE_TORTOISEBOTS)
    return TortoiseBots::CityVisitService::Instance().CancelVisit(
        botGuid, TortoiseBots::CityVisitCancelReason::OwnerRequested, ownerId);
#else
    return false;
#endif
}

bool NativeTaskAdapter::HasActiveCityVisit(ObjectGuid botGuid)
{
#if defined(HAVE_TORTOISEBOTS)
    return TortoiseBots::CityVisitService::Instance().HasActiveVisit(botGuid);
#else
    return false;
#endif
}

CityId NativeTaskAdapter::GetTargetCity(ObjectGuid botGuid)
{
#if defined(HAVE_TORTOISEBOTS)
    TortoiseBots::CityId tbId = TortoiseBots::CityVisitService::Instance().GetTargetCity(botGuid);
    return static_cast<CityId>(tbId);
#else
    return CityId::None;
#endif
}

uint32_t NativeTaskAdapter::GetCityInboundCount(CityId cityId)
{
#if defined(HAVE_TORTOISEBOTS)
    return TortoiseBots::CityVisitService::Instance().GetInboundCount(
        static_cast<TortoiseBots::CityId>(cityId));
#else
    return 0;
#endif
}

uint32_t NativeTaskAdapter::GetCityResidentCount(CityId cityId)
{
#if defined(HAVE_TORTOISEBOTS)
    return TortoiseBots::CityVisitService::Instance().GetResidentCount(
        static_cast<TortoiseBots::CityId>(cityId));
#else
    return 0;
#endif
}

bool NativeTaskAdapter::IsBotInCity(Player* bot, CityId cityId)
{
    if (!bot)
        return false;

#if defined(HAVE_TORTOISEBOTS)
    return TortoiseBots::CityVisitService::Instance().IsBotInCity(
        bot, static_cast<TortoiseBots::CityId>(cityId));
#else
    const CityDefinition* def = ReadOnlyCatalog::Instance().GetCity(cityId);
    if (!def || bot->GetMapId() != def->mapId)
        return false;
    float dx = bot->GetPositionX() - def->centerX;
    float dy = bot->GetPositionY() - def->centerY;
    float dz = bot->GetPositionZ() - def->centerZ;
    return (dx * dx + dy * dy + dz * dz) <= (def->radius * def->radius);
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
