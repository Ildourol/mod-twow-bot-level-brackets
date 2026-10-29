#ifndef BRACKETS_AI_CONTEXT_H
#define BRACKETS_AI_CONTEXT_H

#include "BracketsCommon.h"

#if defined(HAVE_TORTOISEBOTS) && !defined(BEHAVIOR_TEST_HARNESS)

#include "ai/playerbot/AiContextAugment.h"
#include "ai/playerbot/strategy/AiObjectContext.h"
#include "ai/playerbot/strategy/NamedObjectContext.h"
#include "ai/playerbot/strategy/Strategy.h"
#include "ai/playerbot/strategy/Action.h"
#include "ai/playerbot/strategy/Trigger.h"
#include "ai/playerbot/strategy/Value.h"
#include "ai/playerbot/strategy/actions/ChooseTravelTargetAction.h"
#include "ai/playerbot/TravelMgr.h"
#include "ai/playerbot/PlayerbotAI.h"

namespace Brackets
{
    // Custom non-combat strategy registered via external context seam
    class OpportunityStrategy : public ai::Strategy
    {
    public:
        OpportunityStrategy(PlayerbotAI* ai) : ai::Strategy(ai) {}
        virtual ~OpportunityStrategy() = default;

        virtual std::string getName() override { return "blb opportunities"; }
        virtual int GetType() override { return ai::STRATEGY_TYPE_NONCOMBAT; }
        virtual void InitNonCombatTriggers(std::list<ai::TriggerNode*>& triggers) override;
    };

    // City destination request action reusing ChooseTravelTargetAction helpers
    class CityOpportunityAction : public ai::ChooseTravelTargetAction
    {
    public:
        CityOpportunityAction(PlayerbotAI* ai, std::string name = "blb request city opportunity")
            : ai::ChooseTravelTargetAction(ai, name) {}
        virtual ~CityOpportunityAction() = default;

        virtual bool Execute(ai::Event& event) override;
        virtual bool isUseful() override;
    };

    // World destination request action reusing ChooseTravelTargetAction helpers
    class WorldOpportunityAction : public ai::ChooseTravelTargetAction
    {
    public:
        WorldOpportunityAction(PlayerbotAI* ai, std::string name = "blb request world opportunity")
            : ai::ChooseTravelTargetAction(ai, name) {}
        virtual ~WorldOpportunityAction() = default;

        virtual bool Execute(ai::Event& event) override;
        virtual bool isUseful() override;
    };

    // Trigger for city opportunities
    class CityOpportunityTrigger : public ai::Trigger
    {
    public:
        CityOpportunityTrigger(PlayerbotAI* ai, std::string name = "blb city opportunity trigger")
            : ai::Trigger(ai, name, 1) {}
        virtual ~CityOpportunityTrigger() = default;

        virtual bool IsActive() override;
    };

    // Trigger for world opportunities
    class WorldOpportunityTrigger : public ai::Trigger
    {
    public:
        WorldOpportunityTrigger(PlayerbotAI* ai, std::string name = "blb world opportunity trigger")
            : ai::Trigger(ai, name, 1) {}
        virtual ~WorldOpportunityTrigger() = default;

        virtual bool IsActive() override;
    };

    // Value for intent validity verification
    class OpportunityIntentValidValue : public ai::UntypedValue, public ai::Qualified
    {
    public:
        OpportunityIntentValidValue(PlayerbotAI* ai, std::string name = "blb intent valid")
            : ai::UntypedValue(ai, name) {}
        virtual ~OpportunityIntentValidValue() = default;

        virtual std::string Format() override;
        bool Calculate();
    };

    // Fresh NamedObjectContext subclasses appended per bot
    class BracketsStrategyContext : public ai::NamedObjectContext<ai::Strategy>
    {
    public:
        BracketsStrategyContext();
    };

    class BracketsActionContext : public ai::NamedObjectContext<ai::Action>
    {
    public:
        BracketsActionContext();
    };

    class BracketsTriggerContext : public ai::NamedObjectContext<ai::Trigger>
    {
    public:
        BracketsTriggerContext();
    };

    class BracketsValueContext : public ai::NamedObjectContext<ai::UntypedValue>
    {
    public:
        BracketsValueContext();
    };

    void BracketsAiContextAugmenter(PlayerbotAI* ai, ai::AiObjectContext* context);
    void RegisterBracketsAiExtension();
    void EnsureBotStrategy(PlayerbotAI* ai, bool enabled);
}

#else

namespace Brackets
{
    inline void RegisterBracketsAiExtension() {}
    inline void EnsureBotStrategy(void* /*ai*/, bool /*enabled*/) {}
}

#endif // HAVE_TORTOISEBOTS

#endif // BRACKETS_AI_CONTEXT_H
