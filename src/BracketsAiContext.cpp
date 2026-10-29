#include "BracketsAiContext.h"

#if defined(HAVE_TORTOISEBOTS)

#include "BotEngineAdapter.h"
#include "BracketsConfig.h"
#include "DestinationCatalog.h"
#include "OpportunityLedger.h"
#include "BracketsDebug.h"
#include "Log.h"

namespace Brackets
{
    void OpportunityStrategy::InitNonCombatTriggers(std::list<ai::TriggerNode*>& triggers)
    {
        triggers.push_back(new ai::TriggerNode(
            "blb city opportunity trigger",
            ai::NextAction::array(0, new ai::NextAction("blb request city opportunity", 5.0f), nullptr)));

        triggers.push_back(new ai::TriggerNode(
            "blb world opportunity trigger",
            ai::NextAction::array(0, new ai::NextAction("blb request world opportunity", 5.0f), nullptr)));
    }

    bool CityOpportunityAction::isUseful()
    {
        if (!CanChooseTravel())
            return false;

        Player* bot = BotEngineAdapter::GetAiPlayer(ai);
        if (!bot)
            return false;

        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(bot->GetObjectGuid());
        return (intent.intentId > 0 && intent.domain == DOMAIN_CITY &&
                (intent.state == INTENT_STATE_PROPOSED || intent.state == INTENT_STATE_PLANNING));
    }

    bool CityOpportunityAction::Execute(ai::Event& /*event*/)
    {
        if (!CanChooseTravel())
            return false;

        Player* bot = BotEngineAdapter::GetAiPlayer(ai);
        if (!bot)
            return false;

        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(bot->GetObjectGuid());
        if (intent.intentId == 0 || intent.domain != DOMAIN_CITY)
            return false;

        const CityInfo* city = DestinationCatalog::Instance().GetCity(static_cast<CityId>(intent.destinationId));
        if (!city)
            return false;

        ai::TravelTarget* target = context->GetValue<ai::TravelTarget*>("travel target")->Get();
        if (!target)
            return false;

        ai::PlayerTravelInfo travelInfo(bot);
        // Generic RPG, Vendor, Repair purposes
        uint32 purposeFlag = (1 << 6) | (1 << 9) | (1 << 8);
        ai::DestinationList rawList = sTravelMgr.GetDestinations(travelInfo, purposeFlag, {}, true, 25000.0f);

        ai::PartitionedTravelList partitioned;
        for (ai::TravelDestination* dest : rawList)
        {
            if (!dest)
                continue;

            for (ai::WorldPosition* pos : dest->GetPoints())
            {
                if (pos && DestinationCatalog::Instance().IsInCity(city->id, pos->GetMapId(), pos->GetAreaId(), pos->getX(), pos->getY(), pos->getZ()))
                {
                    uint32 dist = static_cast<uint32>(travelInfo.getPosition().distance(*pos));
                    partitioned[dist].push_back({dest, pos, static_cast<float>(dist)});
                }
            }
        }

        bool ok = SetBestTarget(nullptr, target, partitioned, true);
        if (ok)
        {
            OpportunityLedger::Instance().NotifyTravelAdopted(bot->GetObjectGuid(), target);
            BracketsDebug::Instance().LogEvent(CAT_CITY, DOMAIN_CITY, intent.intentId, bot->GetObjectGuid().GetCounter(),
                                               city->name, intent.activityName, "TargetAdopted", "Success",
                                               "Adopted city travel target");
            return true;
        }
        else
        {
            BracketsDebug::Instance().LogEvent(CAT_CITY, DOMAIN_CITY, intent.intentId, bot->GetObjectGuid().GetCounter(),
                                               city->name, intent.activityName, "SearchEmpty", "NoValidDestination",
                                               "SetBestTarget found no valid city destination");
            OpportunityLedger::Instance().TransitionState(intent.intentId, INTENT_STATE_DEFERRED, REASON_SEARCH_EMPTY);
            return false;
        }
    }

    bool WorldOpportunityAction::isUseful()
    {
        if (!CanChooseTravel())
            return false;

        Player* bot = BotEngineAdapter::GetAiPlayer(ai);
        if (!bot)
            return false;

        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(bot->GetObjectGuid());
        return (intent.intentId > 0 && intent.domain == DOMAIN_WORLD &&
                (intent.state == INTENT_STATE_PROPOSED || intent.state == INTENT_STATE_PLANNING));
    }

    bool WorldOpportunityAction::Execute(ai::Event& /*event*/)
    {
        if (!CanChooseTravel())
            return false;

        Player* bot = BotEngineAdapter::GetAiPlayer(ai);
        if (!bot)
            return false;

        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(bot->GetObjectGuid());
        if (intent.intentId == 0 || intent.domain != DOMAIN_WORLD)
            return false;

        const OutdoorRegion* reg = DestinationCatalog::Instance().GetRegion(intent.destinationId);
        if (!reg)
            return false;

        ai::TravelTarget* target = context->GetValue<ai::TravelTarget*>("travel target")->Get();
        if (!target)
            return false;

        const auto* actDef = ActivityRegistry::Instance().GetActivity(intent.activityId);
        uint32 purposeFlag = actDef ? actDef->nativePurposeFlag : (1 << 12); // Default Grind

        ai::PlayerTravelInfo travelInfo(bot);
        ai::DestinationList rawList = sTravelMgr.GetDestinations(travelInfo, purposeFlag, {}, true, 25000.0f);

        ai::PartitionedTravelList partitioned;
        for (ai::TravelDestination* dest : rawList)
        {
            if (!dest)
                continue;

            for (ai::WorldPosition* pos : dest->GetPoints())
            {
                if (pos && pos->GetMapId() == reg->mapId &&
                    (pos->GetAreaId() == reg->zoneId || reg->areaIds.find(pos->GetAreaId()) != reg->areaIds.end()))
                {
                    uint32 dist = static_cast<uint32>(travelInfo.getPosition().distance(*pos));
                    partitioned[dist].push_back({dest, pos, static_cast<float>(dist)});
                }
            }
        }

        bool ok = SetBestTarget(nullptr, target, partitioned, true);
        if (ok)
        {
            OpportunityLedger::Instance().NotifyTravelAdopted(bot->GetObjectGuid(), target);
            BracketsDebug::Instance().LogEvent(CAT_WORLD, DOMAIN_WORLD, intent.intentId, bot->GetObjectGuid().GetCounter(),
                                               reg->name, intent.activityName, "TargetAdopted", "Success",
                                               "Adopted world travel target");
            return true;
        }
        else
        {
            BracketsDebug::Instance().LogEvent(CAT_WORLD, DOMAIN_WORLD, intent.intentId, bot->GetObjectGuid().GetCounter(),
                                               reg->name, intent.activityName, "SearchEmpty", "NoValidDestination",
                                               "SetBestTarget found no valid region destination");
            OpportunityLedger::Instance().TransitionState(intent.intentId, INTENT_STATE_DEFERRED, REASON_SEARCH_EMPTY);
            return false;
        }
    }

    bool CityOpportunityTrigger::IsActive()
    {
        if (!ai)
            return false;

        Player* bot = BotEngineAdapter::GetAiPlayer(ai);
        if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat())
            return false;

        if (ai->HasRealPlayerMaster() || bot->IsTaxiFlying() || bot->IsBeingTeleported())
            return false;

        if (AI_VALUE(bool, "travel target active"))
            return false;

        if (!AI_VALUE(bool, "can move around"))
            return false;

        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(bot->GetObjectGuid());
        return (intent.intentId > 0 && intent.domain == DOMAIN_CITY && intent.state == INTENT_STATE_PROPOSED);
    }

    bool WorldOpportunityTrigger::IsActive()
    {
        if (!ai)
            return false;

        Player* bot = BotEngineAdapter::GetAiPlayer(ai);
        if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat())
            return false;

        if (ai->HasRealPlayerMaster() || bot->IsTaxiFlying() || bot->IsBeingTeleported())
            return false;

        if (AI_VALUE(bool, "travel target active"))
            return false;

        if (!AI_VALUE(bool, "can move around"))
            return false;

        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(bot->GetObjectGuid());
        return (intent.intentId > 0 && intent.domain == DOMAIN_WORLD && intent.state == INTENT_STATE_PROPOSED);
    }

    std::string OpportunityIntentValidValue::Format()
    {
        return Calculate() ? "true" : "false";
    }

    bool OpportunityIntentValidValue::Calculate()
    {
        if (!ai)
            return false;
        Player* bot = BotEngineAdapter::GetAiPlayer(ai);
        if (!bot)
            return false;
        return OpportunityLedger::Instance().IsIntentValid(bot->GetObjectGuid(), GetQualifier());
    }

    BracketsStrategyContext::BracketsStrategyContext()
        : ai::NamedObjectContext<ai::Strategy>()
    {
        creators["blb opportunities"] = [](PlayerbotAI* ai) { return new OpportunityStrategy(ai); };
    }

    BracketsActionContext::BracketsActionContext()
        : ai::NamedObjectContext<ai::Action>()
    {
        creators["blb request city opportunity"] = [](PlayerbotAI* ai) { return new CityOpportunityAction(ai); };
        creators["blb request world opportunity"] = [](PlayerbotAI* ai) { return new WorldOpportunityAction(ai); };
    }

    BracketsTriggerContext::BracketsTriggerContext()
        : ai::NamedObjectContext<ai::Trigger>()
    {
        creators["blb city opportunity trigger"] = [](PlayerbotAI* ai) { return new CityOpportunityTrigger(ai); };
        creators["blb world opportunity trigger"] = [](PlayerbotAI* ai) { return new WorldOpportunityTrigger(ai); };
    }

    BracketsValueContext::BracketsValueContext()
        : ai::NamedObjectContext<ai::UntypedValue>()
    {
        creators["blb intent valid"] = [](PlayerbotAI* ai) { return new OpportunityIntentValidValue(ai); };
    }

    void BracketsAiContextAugmenter(PlayerbotAI* /*ai*/, ai::AiObjectContext* context)
    {
        if (!context)
            return;

        // Fresh non-shared contexts per bot: context list destructor will delete them when bot is torn down
        context->AddShared(new BracketsStrategyContext());
        context->AddShared(new BracketsActionContext());
        context->AddShared(new BracketsTriggerContext());
        context->AddShared(new BracketsValueContext());
    }

    void RegisterBracketsAiExtension()
    {
        RegisterAiContextAugmenter(BracketsAiContextAugmenter);
        sLog.outString("BotLevelBrackets: Registered external AI Context Augmenter with TortoiseBots engine.");
    }

    void EnsureBotStrategy(PlayerbotAI* ai, bool enabled)
    {
        if (!ai)
            return;

        bool has = ai->HasStrategy("blb opportunities", BotState::BOT_STATE_NON_COMBAT);
        if (enabled && !has)
        {
            ai->ChangeStrategy("+blb opportunities", BotState::BOT_STATE_NON_COMBAT);
        }
        else if (!enabled && has)
        {
            ai->ChangeStrategy("-blb opportunities", BotState::BOT_STATE_NON_COMBAT);
        }
    }
}

#endif // HAVE_TORTOISEBOTS
