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
    DebugEnvironment::Instance().SetMode(DEBUG_MODE_OFF);

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
    assert(ActivityRegistry::Instance().GetActivityCount() == 43);
    const ActivityDefinition* act1 = ActivityRegistry::Instance().GetActivity(1);
    assert(act1 != nullptr && act1->key == "AuctionBrowse");
    assert((act1->domainMask & DOMAIN_CITY) != 0);

    const ActivityDefinition* act18 = ActivityRegistry::Instance().GetActivity(18);
    assert(act18 != nullptr && act18->key == "GrindMob");
    assert((act18->domainMask & DOMAIN_WORLD) != 0);

    const ActivityDefinition* byKey = ActivityRegistry::Instance().GetActivity("TavernRest");
    assert(byKey != nullptr && byKey->activityId == 5);

    // 3. DestinationCatalog Verification
    assert(DestinationCatalog::Instance().GetCityCount() == 8);
    assert(DestinationCatalog::Instance().GetRegionCount() == 38);
    assert(DestinationCatalog::Instance().GetCity(CITY_STORMWIND) != nullptr);
    assert(DestinationCatalog::Instance().GetCity(CITY_ALAH_THALAS) != nullptr);
    assert(DestinationCatalog::Instance().GetCity(CITY_SPARKWATER_PORT) != nullptr);
    LandingPoint lp;
    assert(DestinationCatalog::Instance().GetCityLandingPoint(CITY_STORMWIND, lp));
    assert(lp.mapId == 0);

    // 4. OpportunityLedger & Lifecycle State Machine
    ObjectGuid bGuid = bot.GetObjectGuid();
    OpportunityIntent intent = OpportunityLedger::Instance().ProposeIntent(bGuid, DOMAIN_CITY, CITY_STORMWIND, 1, 180);
    assert(intent.intentId > 0);
    assert(intent.state == INTENT_STATE_PROPOSED);
    assert(OpportunityLedger::Instance().GetActiveIntent(bGuid).intentId == intent.intentId);

    assert(OpportunityLedger::Instance().TransitionIntent(bGuid, INTENT_STATE_PLANNING));
    assert(OpportunityLedger::Instance().TransitionIntent(bGuid, INTENT_STATE_TRAVELING));
    assert(OpportunityLedger::Instance().TransitionIntent(bGuid, INTENT_STATE_ARRIVED));
    assert(OpportunityLedger::Instance().ReleaseIntent(bGuid, REASON_EXPIRED));
    assert(OpportunityLedger::Instance().GetActiveIntent(bGuid).intentId == 0);
    assert(OpportunityLedger::Instance().IsInCooldown(bGuid, DOMAIN_CITY, CITY_STORMWIND, 1800));

    // 5. PopulationCensus & Largest-Remainder Quota Math
    std::vector<float> shares = {1.5f, 2.5f, 0.0f};
    std::vector<uint32> intQuotas = PopulationCensus::CalculateLargestRemainderQuotas(shares, 4);
    assert(intQuotas.size() == 3);
    uint32 sum = 0;
    for (uint32 q : intQuotas) sum += q;
    assert(sum == 4);

    int32 deficit = PopulationCensus::CalculateActionableDeficit(10, 6, 2);
    assert(deficit == 2);
    int32 noDeficit = PopulationCensus::CalculateActionableDeficit(10, 8, 3);
    assert(noDeficit == 0);

    // 6. SocialProtection Verification
    SocialProtection::Instance().AddSocialFriend(human.GetObjectGuid(), bot.GetObjectGuid());
    assert(SocialProtection::Instance().IsSocialProtected(bot.GetObjectGuid()));

    SocialProtection::Instance().AddPersistentGuild(42);
    assert(SocialProtection::Instance().IsSocialProtectedGuild(42));

    // 7. CityRelocationFallback Safeguards
    assert(!CityRelocationFallback::Instance().IsAllowed(bot.GetObjectGuid(), CITY_STORMWIND));
    assert(CityRelocationFallback::Instance().GetRelocationsInLastHour() == 0);

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
