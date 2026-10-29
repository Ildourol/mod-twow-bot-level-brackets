#include "support/Framework.h"

#include "../src/ActivityRegistry.cpp"
#include "../src/DestinationCatalog.cpp"
#include "../src/OpportunityLedger.cpp"
#include "../src/PopulationCensus.cpp"
#include "../src/SocialProtection.cpp"
#include "../src/CityRelocationFallback.cpp"
#include "../src/BracketsConfig.cpp"
#include "../src/BracketsDebug.cpp"
#include "../src/OpportunityCoordinator.cpp"
#include "../src/mod-twow-bot-level-brackets.cpp"

using namespace Brackets;

static void ResetTestEnvironment()
{
    sObjectAccessor.players.clear();
    sConfig.values.clear();
    CharacterDatabase.writes.clear();

    ActivityRegistry::Instance().Initialize();
    DestinationCatalog::Instance().Initialize();
    OpportunityLedger::Instance().Clear();
    PopulationCensus::Instance().Reset();
    SocialProtection::Instance().Clear();
    CityRelocationFallback::Instance().Clear();
    BracketsDebug::Instance().ResetCounters();

    sConfig.values["BotLevelBrackets.Enabled"] = "1";
    sConfig.values["BotLevelBrackets.CheckFrequency"] = "300";
    ConfigManager::Instance().Load();
}

int main()
{
    ResetTestEnvironment();

    // 1. BotEngineAdapter Verification across engines
    Player bot(1);
    bot.session.headless = true;
    sObjectAccessor.players[bot.GetObjectGuid()] = &bot;

    Player human(2);
    human.session.headless = false;
    sObjectAccessor.players[human.GetObjectGuid()] = &human;

    assert(BotEngineAdapter::IsBot(&bot));
    assert(!BotEngineAdapter::IsBot(&human));
    assert(BotEngineAdapter::IsRandomBot(&bot));
    assert(BotEngineAdapter::GetEngineName() != nullptr);
    assert(!BotEngineAdapter::BotHasRealPlayerMaster(&bot));

#if defined(HAVE_TORTOISEBOTS)
    assert(BotEngineAdapter::GetEngineKind() == BotEngineAdapter::ENGINE_TORTOISEBOTS);
    PlayerbotAI* botAi = BotEngineAdapter::GetBotAI(&bot);
    assert(botAi != nullptr);
    assert(BotEngineAdapter::GetAiPlayer(botAi) == &bot);
#elif defined(HAVE_LEGACY_PLAYERBOTS)
    assert(BotEngineAdapter::GetEngineKind() == BotEngineAdapter::ENGINE_LEGACY_PLAYERBOTS);
    PlayerbotAI* botAi = BotEngineAdapter::GetBotAI(&bot);
    assert(botAi != nullptr);
    assert(BotEngineAdapter::GetAiPlayer(botAi) == &bot);
#else
    assert(BotEngineAdapter::GetEngineKind() == BotEngineAdapter::ENGINE_GENERIC_HEADLESS);
#endif

    // 2. ActivityRegistry Verification
    assert(ACTIVITY_COUNT == 42);
    const ActivityDefinition* act1 = ActivityRegistry::Instance().GetActivity(ACTIVITY_AUCTION_SELL);
    assert(act1 != nullptr && std::string(act1->name) == "AuctionSell");
    assert((act1->domainMask & DOMAIN_CITY) != 0);

    const ActivityDefinition* act18 = ActivityRegistry::Instance().GetActivity(ACTIVITY_GRINDING);
    assert(act18 != nullptr && std::string(act18->name) == "Grinding");
    assert((act18->domainMask & DOMAIN_WORLD) != 0);

    const ActivityDefinition* byName = ActivityRegistry::Instance().FindByName("RestSit");
    assert(byName != nullptr && byName->id == ACTIVITY_REST_SIT);

    // 3. DestinationCatalog Verification
    assert(DestinationCatalog::Instance().GetCity(CITY_STORMWIND) != nullptr);
    assert(DestinationCatalog::Instance().GetCity(CITY_ALAH_THALAS) != nullptr);
    assert(DestinationCatalog::Instance().GetCity(CITY_SPARKWATER) != nullptr);
    assert(DestinationCatalog::Instance().GetOutdoorRegions().size() == 34);
    const CityInfo* swInfo = DestinationCatalog::Instance().GetCity(CITY_STORMWIND);
    assert(swInfo != nullptr && swInfo->landing.mapId == 0);

    // 4. OpportunityLedger & Lifecycle State Machine
    ObjectGuid bGuid = bot.GetObjectGuid();
    uint64 intentId = OpportunityLedger::Instance().CreateIntent(
        bGuid, DOMAIN_CITY, CITY_STORMWIND, "Stormwind", ACTIVITY_GENERIC_RPG,
        1, bot.GetLevel(), bot.GetTeamId(), 180);
    assert(intentId > 0);
    assert(OpportunityLedger::Instance().HasActiveIntent(bGuid));
    OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(bGuid);
    assert(intent.intentId == intentId);
    assert(intent.state == INTENT_STATE_PROPOSED);

    assert(OpportunityLedger::Instance().TransitionState(intentId, INTENT_STATE_PLANNING));
    assert(OpportunityLedger::Instance().TransitionState(intentId, INTENT_STATE_TRAVELING));
    assert(OpportunityLedger::Instance().TransitionState(intentId, INTENT_STATE_ARRIVED));
    assert(OpportunityLedger::Instance().CancelIntent(bGuid, REASON_DEADLINE_EXPIRED));
    assert(!OpportunityLedger::Instance().HasActiveIntent(bGuid));

    // 5. PopulationCensus & Largest-Remainder Quota Math
    std::vector<float> shares = {1.5f, 2.5f, 0.0f};
    std::vector<uint32> intQuotas = PopulationCensus::CalculateLargestRemainderQuotas(shares, 4);
    assert(intQuotas.size() == 3);
    uint32 sum = 0;
    for (uint32 q : intQuotas) sum += q;
    assert(sum == 4);

    int32 deficit = PopulationCensus::CalculateActionableDeficit(10, 6, 2);
    assert(deficit == 4);
    int32 noDeficit = PopulationCensus::CalculateActionableDeficit(10, 8, 3);
    assert(noDeficit == 0);

    // 6. SocialProtection Verification
    SocialProtection::Instance().AddSocialFriend(human.GetObjectGuid(), bot.GetObjectGuid());
    assert(SocialProtection::Instance().IsSocialProtected(bot.GetObjectGuid()));

    SocialProtection::Instance().AddPersistentGuild(42);
    assert(SocialProtection::Instance().IsSocialProtectedGuild(42));

    // 7. CityRelocationFallback Safeguards
    assert(!CityRelocationFallback::Instance().CanRelocate(&bot, CITY_STORMWIND, REASON_ROUTE_UNSUPPORTED_BEFORE_START));
    assert(CityRelocationFallback::Instance().GetRelocationsInPastHour() == 0);

    // 8. Script Hooks & Commands Verification
    BotLevelBracketsWorldScript worldScript;
    worldScript.OnUpdate(100);

    BotLevelBracketsPlayerScript playerScript;
    playerScript.OnLogin(&bot);
    playerScript.OnLogout(&bot);

    BotLevelBracketsGuildScript guildScript;
    Guild g;
    uint8 rank = 0;
    guildScript.OnAddMember(&g, &bot, rank);
    guildScript.OnRemoveMember(&g, &bot, false, false);

    BotLevelBracketsCommandScript cmdScript;
    ChatHandler chat(&human.session);
    human.session.security = SEC_ADMINISTRATOR;
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "status"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "activities"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "reload"));

    return 0;
}
