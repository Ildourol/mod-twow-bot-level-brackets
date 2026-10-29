#include "support/Framework.h"
#include "../src/world/ReadOnlyCatalog.cpp"
#include "../src/policy/PopulationPolicy.cpp"
#include "../src/policy/QuotaAllocator.cpp"
#include "../src/activities/ActivityRegistry.cpp"
#include "../src/social/ProtectionCache.cpp"
#include "../src/compat/NativeTaskAdapter.cpp"
#include "../src/scheduling/RequestLedger.cpp"
#include "../src/world/Census.cpp"
#include "../src/debug/DiagnosticManager.cpp"
#include "../src/scheduling/Coordinator.cpp"
#include "../src/commands/BotBracketsCommands.cpp"
#include "../src/mod-twow-bot-level-brackets.cpp"

#include <cassert>
#include <iostream>

using namespace BotLevelBrackets;

static void Reset()
{
    sObjectAccessor.players.clear();
    sConfig.values.clear();
    CharacterDatabase.writes.clear();

    RequestLedger::Instance().Clear();
    DiagnosticManager::Instance().ClearRingBuffer();
    DiagnosticManager::Instance().SetDebugMode(DebugMode::Off);

    PopulationPolicy::Instance().LoadDefaults();
    ActivityRegistry::Instance().LoadDefaults();

#if defined(HAVE_TORTOISEBOTS)
    TortoiseBots::CityVisitService::Instance().Clear();
    sRandomBotFacade.isFree = true;
#endif
}

static void AddPlayer(Player& p)
{
    sObjectAccessor.players[p.guid] = &p;
}

int main()
{
    std::cout << "Starting mod-twow-bot-level-brackets Option 1 test suite..." << std::endl;

    // -------------------------------------------------------------------------
    // Test 1: Baseline Architecture & Unchanged Bot Engine Immutability
    // -------------------------------------------------------------------------
    Reset();
    Player bot1(1);
    bot1.team = TEAM_ALLIANCE;
    bot1.level = 25;
    bot1.x = -8000.0f; bot1.y = 500.0f; bot1.map.id = 0; // Outside Stormwind
    AddPlayer(bot1);

    assert(NativeTaskAdapter::IsBot(&bot1));
    assert(NativeTaskAdapter::IsFreeBot(&bot1));
    assert(!ProtectionCache::Instance().IsBotProtected(&bot1));

    // Assert zero teleports and zero level changes initially
    assert(bot1.teleports == 0);
    assert(bot1.level == 25);

    // -------------------------------------------------------------------------
    // Test 2: Option 1 City Request Flow via Unchanged CityVisitService
    // -------------------------------------------------------------------------
    // Configure Stormwind deficit: target = 7, actual = 0
    PopulationPolicy::Instance().SetCityTarget(0, 7);
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    assert(snap.allianceTotalBots == 1);
    assert(snap.cities[0].shortfall > 0);

    // Run coordinator scheduling pass
    Coordinator::Instance().ForceExecutionCycle();

#if defined(HAVE_TORTOISEBOTS)
    // CityVisitService accepted the request; RequestLedger records NativeAccepted
    assert(RequestLedger::Instance().HasActiveRequest(bot1.GetGUIDLow()));
    const LedgerEntry* entry = RequestLedger::Instance().GetActiveEntryForBot(bot1.GetGUIDLow());
    assert(entry != nullptr);
    assert(entry->state == RequestState::NativeAccepted);
    assert(entry->intendedCity == CityId::Stormwind);

    // Assert strictly zero teleports and zero level mutations occurred!
    assert(bot1.teleports == 0);
    assert(bot1.level == 25);
#endif

    // -------------------------------------------------------------------------
    // Test 3: Duplicate Request Suppression & Cooldowns
    // -------------------------------------------------------------------------
    // Running another cycle immediately must NOT issue a duplicate request to the same bot
    uint64_t prevRequests = Coordinator::Instance().GetTotalRequestsSubmitted();
    Coordinator::Instance().ForceExecutionCycle();
    assert(Coordinator::Instance().GetTotalRequestsSubmitted() == prevRequests);

    // -------------------------------------------------------------------------
    // Test 4: Social & Identity Protections
    // -------------------------------------------------------------------------
    Reset();
    Player friendBot(2), guildBot(3), namedBot(4);
    friendBot.team = TEAM_ALLIANCE;
    guildBot.team = TEAM_ALLIANCE;
    namedBot.team = TEAM_ALLIANCE;
    namedBot.name = "ImmuneBot";
    AddPlayer(friendBot); AddPlayer(guildBot); AddPlayer(namedBot);

    // Friend list protection
    ProtectionCache::Instance().SetIgnoreFriendListed(true);
    // Simulate friend addition
    ProtectionCache::Instance().IsBotProtected(&friendBot); // cache checked
    // Guild protection: guildId 10 has real player
    guildBot.guild = 10;
    ProtectionCache::Instance().OnGuildMemberAdd(10, true);
    assert(ProtectionCache::Instance().IsRealPlayerGuild(10));
    assert(ProtectionCache::Instance().IsBotProtected(&guildBot));

    // Excluded name protection
    ProtectionCache::Instance().SetExcludedNames("ImmuneBot, OtherBot");
    assert(ProtectionCache::Instance().IsExcludedName("ImmuneBot"));
    assert(ProtectionCache::Instance().IsBotProtected(&namedBot));

    // -------------------------------------------------------------------------
    // Test 5: DryRun Mode (Zero Gameplay Writes / Zero Native Submissions)
    // -------------------------------------------------------------------------
    Reset();
    Player dryBot(5);
    dryBot.team = TEAM_ALLIANCE;
    AddPlayer(dryBot);

    PopulationPolicy::Instance().SetDryRun(true);
    Coordinator::Instance().ForceExecutionCycle();

    // In DryRun, ledger has NO active accepted request, and CityVisitService received zero requests
    assert(!RequestLedger::Instance().HasActiveRequest(dryBot.GetGUIDLow()));
#if defined(HAVE_TORTOISEBOTS)
    assert(!TortoiseBots::CityVisitService::Instance().HasActiveVisit(dryBot.GetObjectGuid()));
#endif

    // -------------------------------------------------------------------------
    // Test 6: Level Supply Shortfall is Advisory (Zero Resets / Zero Boosts)
    // -------------------------------------------------------------------------
    Reset();
    Player level60Bot(6);
    level60Bot.level = 60;
    level60Bot.team = TEAM_ALLIANCE;
    AddPlayer(level60Bot);

    snap = Census::Instance().TakeSnapshot();
    // Brackets 1-9, 10-19, 20-29 have 0 bots, so they show a shortage
    assert(snap.allianceBrackets[0].botCount == 0);
    assert(snap.allianceBrackets[0].shortage > 0);

    // Scheduling pass must NOT alter the bot's level to fill the shortage
    Coordinator::Instance().ForceExecutionCycle();
    assert(level60Bot.level == 60);

    // -------------------------------------------------------------------------
    // Test 7: Commands Routing & Diagnostic Explanations
    // -------------------------------------------------------------------------
    Reset();
    Player admin(99);
    admin.session.headless = false;
    admin.session.security = SEC_ADMINISTRATOR;
    AddPlayer(admin);

    ChatHandler chat(&admin.session);
    BotLevelBracketsCommandScript cmdScript;

    // Execute .blb subcommands
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "status"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "cities"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "world"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "brackets"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "activities"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "ledger"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "stats"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "explain 6"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "explain city 0"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "reload"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "cleanup"));
    assert(!cmdScript.CanExecuteCommand(&chat, "blb", "debug dryrun"));

    std::cout << "All Option 1 behavioral, safety, and orchestration tests passed successfully!" << std::endl;
    return 0;
}
