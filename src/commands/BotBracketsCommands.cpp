#include "BotBracketsCommands.h"
#include "../policy/PopulationPolicy.h"
#include "../compat/NativeTaskAdapter.h"
#include "../world/Census.h"
#include "../world/ReadOnlyCatalog.h"
#include "../scheduling/RequestLedger.h"
#include "../scheduling/Coordinator.h"
#include "../activities/ActivityRegistry.h"
#include "../debug/DiagnosticManager.h"
#include "../social/ProtectionCache.h"
#include "../distribution/LevelDistributionManager.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "Chat/Chat.h"
#include "Config/Config.h"
#include "Log.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Chat.h"
#include "Config/Config.h"
#include "Log.h"
#include "Player.h"
#include "ObjectAccessor.h"
#else
#include "Common.h"
#include "Chat/Chat.h"
#include "Config/Config.h"
#include "Log.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#endif

#include <cstring>
#include <sstream>
#include <vector>
#include <iomanip>

namespace BotLevelBrackets
{

bool BotBracketsCommands::HandleCommand(ChatHandler* handler, const char* subCommand, const char* args)
{
    if (!handler)
        return false;

    if (!subCommand || strlen(subCommand) == 0 || strcmp(subCommand, "status") == 0)
        return HandleStatus(handler);

    if (strcmp(subCommand, "cities") == 0 || strcmp(subCommand, "hubs") == 0)
        return HandleCities(handler);

    if (strcmp(subCommand, "city") == 0)
        return HandleCity(handler, args);

    if (strcmp(subCommand, "world") == 0 || strcmp(subCommand, "regions") == 0)
        return HandleWorld(handler);

    if (strcmp(subCommand, "brackets") == 0)
        return HandleBrackets(handler);

    if (strcmp(subCommand, "activities") == 0)
        return HandleActivities(handler);

    if (strcmp(subCommand, "ledger") == 0)
        return HandleLedger(handler);

    if (strcmp(subCommand, "pending") == 0)
        return HandlePending(handler, args);

    if (strcmp(subCommand, "boost") == 0)
        return HandleBoost(handler, args);

    if (strcmp(subCommand, "stats") == 0)
        return HandleStats(handler);

    if (strcmp(subCommand, "explain") == 0)
        return HandleExplain(handler, args);

    if (strcmp(subCommand, "reload") == 0)
        return HandleReload(handler);

    if (strcmp(subCommand, "force") == 0)
        return HandleForce(handler, args);

    if (strcmp(subCommand, "cleanup") == 0 || strcmp(subCommand, "guildcleanup") == 0)
        return HandleCleanup(handler);

    if (strcmp(subCommand, "debug") == 0)
        return HandleDebug(handler, args);

    handler->SendSysMessage("Unknown BLB subcommand. Valid: status, cities, city, world, brackets, boost, pending, activities, ledger, stats, explain, reload, force, cleanup, guildcleanup, debug.");
    return true;
}

bool BotBracketsCommands::HandleStatus(ChatHandler* handler)
{
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    const auto& policy = PopulationPolicy::Instance();

    handler->PSendSysMessage("=== Turtle WoW Bot Level Brackets ===");
    handler->PSendSysMessage("Master Status: %s | DryRun: %s",
        policy.IsEnabled() ? "ENABLED" : "DISABLED", policy.IsDryRun() ? "YES" : "NO");
    handler->PSendSysMessage("Active Engine: %s", NativeTaskAdapter::GetEngineName());
    handler->PSendSysMessage("Population: Alliance Bots=%u (Field=%u), Horde Bots=%u (Field=%u)",
        snap.allianceTotalBots, snap.allianceFieldBots, snap.hordeTotalBots, snap.hordeFieldBots);
    handler->PSendSysMessage("Humans: Alliance=%u, Horde=%u | Dynamic Humans: %s",
        snap.allianceTotalHumans, snap.hordeTotalHumans, policy.UseDynamicDistribution() ? "ON" : "OFF");
    handler->PSendSysMessage("City Orchestration: %s (Policy: %s)",
        policy.IsCityEnabled() ? "ON" : "OFF",
        policy.GetCityDestinationPolicy() == DestinationPolicy::RequireExact ? "RequireExact" : "AllowNativeChoice");
    handler->PSendSysMessage("World Orchestration: %s (Policy: %s)",
        policy.IsWorldEnabled() ? "ON" : "OFF",
        policy.GetWorldDestinationPolicy() == DestinationPolicy::RequireExact ? "RequireExact" : "AllowNativeChoice");
    handler->PSendSysMessage("Ledger: Alliance Outstanding=%u, Horde Outstanding=%u",
        RequestLedger::Instance().GetOutstandingCount(0), RequestLedger::Instance().GetOutstandingCount(1));
    return HandleBrackets(handler);
}

bool BotBracketsCommands::HandleCities(ChatHandler* handler)
{
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    handler->SendSysMessage("=== Racial Capital City Census & Orchestration Status ===");
    handler->SendSysMessage("ID | Name            | Team     | Res | Inb | Pnd | Eff | Tgt | Shortfall | Status");
    handler->SendSysMessage("---+-----------------+----------+-----+-----+-----+-----+-----+-----------+----------");

    for (size_t i = 0; i < 8; ++i)
    {
        const auto& c = snap.cities[i];
        const char* status = (c.imbalance == PopulationImbalance::Underpopulated) ? "Underpop" :
                             ((c.imbalance == PopulationImbalance::Overpopulated) ? "Overpop" : "Balanced");
        handler->PSendSysMessage("%2u | %-15s | %-8s | %3u | %3u | %3u | %3u | %3u | %9u | %s",
            static_cast<unsigned>(c.id), c.name, (c.teamId == 0 ? "Alliance" : "Horde"),
            c.residents, c.confirmedInbound, c.pendingReservations, c.effective,
            c.feasibleTarget, c.shortfall, status);
    }
    return true;
}

bool BotBracketsCommands::HandleCity(ChatHandler* handler, const char* args)
{
    if (!args || strlen(args) == 0)
    {
        handler->SendSysMessage("Usage: .blb city <botName> <cityName> OR .blb city <cityName> (with bot selected)");
        handler->SendSysMessage("Available Cities: stormwind, ironforge, darnassus, alahthalas, orgrimmar, undercity, thunderbluff, sparkwater");
        return true;
    }

    std::stringstream ss(args);
    std::string token1, token2;
    ss >> token1;
    ss >> token2;

    Player* targetBot = nullptr;
    std::string cityName;

    if (token2.empty())
    {
        // 1 token passed: token1 is city name, target is selected player
        cityName = token1;
        if (handler->GetPlayer() && handler->GetPlayer()->GetSelectionGuid())
            targetBot = sObjectMgr.GetPlayer(handler->GetPlayer()->GetSelectionGuid());
        else if (handler->GetPlayer())
            targetBot = handler->GetPlayer();
    }
    else
    {
        // 2 tokens: token1 is bot name, token2 is city name
        targetBot = sObjectAccessor.FindPlayerByName(token1.c_str());
        cityName = token2;
    }

    if (!targetBot)
    {
        handler->SendSysMessage("Target bot not found or no bot currently selected.");
        return true;
    }

    if (!NativeTaskAdapter::IsBot(targetBot))
    {
        handler->PSendSysMessage("Player '%s' is a real player, not an autonomous bot.", targetBot->GetName());
        return true;
    }

    // Resolve CityId from cityName
    CityId resolvedCity = CityId::None;
    std::string cleanCity = cityName;
    cleanCity.erase(std::remove_if(cleanCity.begin(), cleanCity.end(), [](char c) {
        return c == '\'' || c == '-' || c == ' ' || c == '_';
    }), cleanCity.end());
    std::transform(cleanCity.begin(), cleanCity.end(), cleanCity.begin(), ::tolower);

    if (cleanCity == "stormwind" || cleanCity == "sw")
        resolvedCity = CityId::Stormwind;
    else if (cleanCity == "ironforge" || cleanCity == "if")
        resolvedCity = CityId::Ironforge;
    else if (cleanCity == "darnassus" || cleanCity == "darn")
        resolvedCity = CityId::Darnassus;
    else if (cleanCity == "alahthalas" || cleanCity == "silvermoon" || cleanCity == "highelf")
        resolvedCity = CityId::AlahThalas;
    else if (cleanCity == "orgrimmar" || cleanCity == "org")
        resolvedCity = CityId::Orgrimmar;
    else if (cleanCity == "undercity" || cleanCity == "uc")
        resolvedCity = CityId::Undercity;
    else if (cleanCity == "thunderbluff" || cleanCity == "tb")
        resolvedCity = CityId::ThunderBluff;
    else if (cleanCity == "sparkwater" || cleanCity == "goblin")
        resolvedCity = CityId::Sparkwater;

    if (resolvedCity == CityId::None)
    {
        handler->PSendSysMessage("Unknown city '%s'. Valid: stormwind, ironforge, darnassus, alahthalas, orgrimmar, undercity, thunderbluff, sparkwater.", cityName.c_str());
        return true;
    }

    handler->PSendSysMessage("Teleporting bot '%s' (Level %u) to %s (Separate City Ladder Bank Landing)...",
        targetBot->GetName(), targetBot->GetLevel(), GetCityName(resolvedCity));

    if (NativeTaskAdapter::TeleportBotToCity(targetBot, resolvedCity))
    {
        handler->PSendSysMessage("Success: Bot '%s' (Level %u) arrived at %s bank!",
            targetBot->GetName(), targetBot->GetLevel(), GetCityName(resolvedCity));
    }
    else
    {
        handler->PSendSysMessage("Failed to teleport bot '%s' to %s. Bot may already be visiting or protected.",
            targetBot->GetName(), GetCityName(resolvedCity));
    }

    return true;
}

bool BotBracketsCommands::HandleWorld(ChatHandler* handler)
{
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    const auto& policy = PopulationPolicy::Instance();
    const auto& catalog = ReadOnlyCatalog::Instance();

    handler->SendSysMessage("=== Outdoor World Distribution ===");
    handler->PSendSysMessage("Field Reserve Target: %u%% | Alliance Field: %u, Horde Field: %u",
        policy.GetFieldReservePct(), snap.allianceFieldBots, snap.hordeFieldBots);
    handler->PSendSysMessage("Outdoor Destination Policy: %s",
        policy.GetWorldDestinationPolicy() == DestinationPolicy::RequireExact ? "RequireExact (Honest Missing Control)" : "AllowNativeChoice");

    handler->SendSysMessage("Outdoor Regional Catalog (Advisory Sample):");
    for (const auto& r : catalog.GetRegions())
    {
        handler->PSendSysMessage("  [%2u] %-20s | Map: %u, Zone: %u | Levels %2u-%2u",
            r.regionId, r.name, r.mapId, r.zoneId, r.minLevel, r.maxLevel);
    }
    return true;
}

bool BotBracketsCommands::HandleBrackets(ChatHandler* handler)
{
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    const auto& policy = PopulationPolicy::Instance();
    const auto& distMgr = LevelDistributionManager::Instance();

    handler->SendSysMessage("=== Level Bracket Distribution & Active Booster ===");
    handler->PSendSysMessage("Active Redistribution: %s | Teleport On Ding/Boost: %s | Pending Queue: %u",
        policy.IsLevelDistributionEnabled() ? "ENABLED" : "DISABLED",
        policy.IsTeleportOnLevelChange() ? "ENABLED" : "DISABLED",
        static_cast<uint32_t>(distMgr.GetPendingQueueSize()));
    handler->SendSysMessage("Faction  | Bracket | Bots | Actual % | Target % | Shortage | Humans");
    handler->SendSysMessage("---------+---------+------+----------+----------+----------+-------");

    auto printBrackets = [handler](const char* teamName, const std::vector<BracketCensusItem>& brackets)
    {
        for (const auto& b : brackets)
        {
            handler->PSendSysMessage("%-8s | %2u-%2u   | %4u | %7.1f%% | %7.1f%% | %+8d | %5u",
                teamName, b.lower, b.upper, b.botCount, b.botPercent, b.desiredPercent, b.shortage, b.humanCount);
        }
    };

    printBrackets("Alliance", snap.allianceBrackets);
    printBrackets("Horde", snap.hordeBrackets);
    return true;
}

bool BotBracketsCommands::HandleActivities(ChatHandler* handler)
{
    const auto& reg = ActivityRegistry::Instance();
    handler->PSendSysMessage("=== Audited Native Activities Inventory (Total: %u) ===",
        static_cast<unsigned>(reg.GetAllActivities().size()));
    handler->PSendSysMessage("Master Activities Enabled: %s | RequireSupported: %s",
        reg.IsActivitiesEnabled() ? "YES" : "NO", reg.RequireSupported() ? "YES" : "NO");
    handler->SendSysMessage("Activity ID          | Class                   | En | City | Wrld | Weight | CD (s) | MaxOut");
    handler->SendSysMessage("---------------------+-------------------------+----+------+------+--------+--------+-------");

    for (const auto& act : reg.GetAllActivities())
    {
        handler->PSendSysMessage("%-20s | %-23s | %-2s | %-4s | %-4s | %6.1f | %6u | %6u",
            act.id.c_str(), GetSupportClassName(act.supportClass),
            act.enabled ? "Y" : "N", act.cityEnabled ? "Y" : "N", act.worldEnabled ? "Y" : "N",
            act.weight, act.cooldownSeconds, act.maxOutstandingPerFaction);
    }
    return true;
}

bool BotBracketsCommands::HandleLedger(ChatHandler* handler)
{
    auto entries = RequestLedger::Instance().GetAllActiveEntries();
    handler->PSendSysMessage("=== Request Ledger (Active Entries: %u) ===",
        static_cast<unsigned>(entries.size()));

    if (entries.empty())
    {
        handler->SendSysMessage("Ledger is currently empty (all bots idle or on native business).");
        return true;
    }

    handler->SendSysMessage("Req ID | Bot GUID | Team | Domain   | Destination / Activity   | State          | Support");
    handler->SendSysMessage("-------+----------+------+----------+--------------------------+----------------+--------------------");
    for (const auto& e : entries)
    {
        std::string dest = (e.domain == RequestDomain::City) ? GetCityName(e.intendedCity) : e.activityId;
        handler->PSendSysMessage("%6llu | %8u | %-4s | %-8s | %-24s | %-14s | %s",
            static_cast<unsigned long long>(e.requestId), e.guidLow,
            (e.teamId == 0 ? "Ally" : "Hord"), GetRequestDomainName(e.domain),
            dest.c_str(), GetRequestStateName(e.state), GetSupportClassName(e.supportClass));
    }
    return true;
}

bool BotBracketsCommands::HandleStats(ChatHandler* handler)
{
    const auto& coord = Coordinator::Instance();
    const auto& distMgr = LevelDistributionManager::Instance();

    handler->SendSysMessage("=== Coordinator Performance & Telemetry ===");
    handler->PSendSysMessage("Total City Scheduling Cycles Executed: %llu",
        static_cast<unsigned long long>(coord.GetTotalCyclesRun()));
    handler->PSendSysMessage("Total Native Task Requests Submitted: %llu",
        static_cast<unsigned long long>(coord.GetTotalRequestsSubmitted()));
    handler->PSendSysMessage("Total Native Requests Accepted: %llu",
        static_cast<unsigned long long>(coord.GetTotalRequestsAccepted()));
    handler->PSendSysMessage("Total Native Requests Refused/Rejected: %llu",
        static_cast<unsigned long long>(coord.GetTotalRequestsRejected()));

    handler->SendSysMessage("=== Level Distribution & Dispersal Telemetry ===");
    handler->PSendSysMessage("Total Distribution Cycles Run: %llu",
        static_cast<unsigned long long>(distMgr.GetTotalCyclesRun()));
    handler->PSendSysMessage("Total Bots Level-Adjusted / Boosted: %llu",
        static_cast<unsigned long long>(distMgr.GetTotalBotsAdjusted()));
    handler->PSendSysMessage("Total Bots Deferred (In Combat/Busy): %llu",
        static_cast<unsigned long long>(distMgr.GetTotalBotsSkipped()));
    handler->PSendSysMessage("Total Bots Teleported to Leveling Zones: %llu",
        static_cast<unsigned long long>(distMgr.GetTotalBotsTeleported()));
    handler->PSendSysMessage("Total Over-Leveled Bots Dispersed from Starters: %llu",
        static_cast<unsigned long long>(distMgr.GetTotalDispersedStarter()));
    handler->PSendSysMessage("Total Stranded Bots Dispersed from Wrong Maps: %llu",
        static_cast<unsigned long long>(distMgr.GetTotalDispersedWrongMap()));
    handler->PSendSysMessage("Pending Reset Queue Size: %u",
        static_cast<uint32_t>(distMgr.GetPendingQueueSize()));

    handler->SendSysMessage("=== Arrival Floor & Anti-Fall Telemetry ===");
    handler->PSendSysMessage("Active Arrival Floor Holds: %u",
        static_cast<uint32_t>(NativeTaskAdapter::GetActiveArrivalFloorHolds()));
    handler->PSendSysMessage("Total Anti-Fall Floor Lifts Performed: %u",
        static_cast<uint32_t>(NativeTaskAdapter::GetArrivalLiftsCount()));
    return true;
}

bool BotBracketsCommands::HandleExplain(ChatHandler* handler, const char* args)
{
    if (!args || strlen(args) == 0)
    {
        handler->SendSysMessage("Usage: .blb explain <botGuid> OR .blb explain city <cityIndex> OR .blb explain region <regionId>");
        return true;
    }

    std::stringstream ss(args);
    std::string token;
    ss >> token;

    if (token == "city")
    {
        size_t cIdx = 0;
        if (ss >> cIdx)
            handler->SendSysMessage(DiagnosticManager::Instance().ExplainCity(cIdx).c_str());
        else
            handler->SendSysMessage("Usage: .blb explain city <0-7>");
        return true;
    }

    if (token == "region")
    {
        uint32_t rId = 0;
        if (ss >> rId)
            handler->SendSysMessage(DiagnosticManager::Instance().ExplainRegion(rId).c_str());
        else
            handler->SendSysMessage("Usage: .blb explain region <regionId>");
        return true;
    }

    uint32_t guidLow = 0;
    try {
        guidLow = std::stoul(token);
    } catch (...) {
        handler->SendSysMessage("Invalid bot GUID specified.");
        return true;
    }

    handler->SendSysMessage(DiagnosticManager::Instance().ExplainBot(guidLow).c_str());
    return true;
}

bool BotBracketsCommands::HandleReload(ChatHandler* handler)
{
    handler->SendSysMessage("Reloading mod-twow-bot-level-brackets configuration...");
    PopulationPolicy::Instance().IncrementConfigGeneration();
    RequestLedger::Instance().WithdrawStaleProposals(PopulationPolicy::Instance().GetConfigGeneration());
    ProtectionCache::Instance().RefreshFromDatabase();
    handler->PSendSysMessage("Configuration reloaded cleanly (Generation: %u). Active native jobs untouched.",
        PopulationPolicy::Instance().GetConfigGeneration());
    return true;
}

bool BotBracketsCommands::HandleForce(ChatHandler* handler, const char* args)
{
    std::string argStr = args ? args : "";
    if (argStr == "level" || argStr == "dist" || argStr == "distribution")
    {
        handler->SendSysMessage("Triggering immediate level bracket redistribution pass...");
        LevelDistributionManager::Instance().ForceDistributionCycle(handler);
        return true;
    }
    if (argStr == "disperse" || argStr == "evacuate")
    {
        handler->SendSysMessage("Triggering starting zone and wrong map dispersal passes...");
        LevelDistributionManager::Instance().ForceDispersalCycle(handler);
        return true;
    }

    handler->SendSysMessage("Triggering coordinator city pass and level distribution pass...");
    Coordinator::Instance().ForceExecutionCycle();
    LevelDistributionManager::Instance().ForceDistributionCycle(handler);
    handler->SendSysMessage("Force execution cycle complete.");
    return true;
}

bool BotBracketsCommands::HandleCleanup(ChatHandler* handler)
{
    handler->SendSysMessage("Pruning stale ledger requests and expired cooldowns...");
    Coordinator::Instance().CleanupStaleEntries();
    uint32_t removedGuilds = ProtectionCache::Instance().CleanupGuildTracker();
    uint32_t remainingGuilds = ProtectionCache::Instance().GetProtectedPersistentGuildsCount();
    handler->PSendSysMessage("[BotBrackets] Guild cleanup complete. Removed %u guild(s). %u guild(s) remain tracked.",
        removedGuilds, remainingGuilds);
    handler->SendSysMessage("Cleanup complete.");
    return true;
}

bool BotBracketsCommands::HandleBoost(ChatHandler* handler, const char* args)
{
    if (!args || strlen(args) == 0)
    {
        handler->SendSysMessage("Usage: .blb boost <botName|target> <targetLevel>");
        return true;
    }

    std::stringstream ss(args);
    std::string firstToken;
    ss >> firstToken;

    Player* targetBot = nullptr;
    uint32_t targetLevel = 0;

    // Check if firstToken is level (when target player is selected in-game)
    char* endPtr = nullptr;
    long lvlVal = strtol(firstToken.c_str(), &endPtr, 10);
    if (*endPtr == '\0')
    {
        targetLevel = static_cast<uint32_t>(lvlVal);
        if (handler->GetPlayer() && handler->GetPlayer()->GetSelectionGuid())
            targetBot = sObjectMgr.GetPlayer(handler->GetPlayer()->GetSelectionGuid());
        else if (handler->GetPlayer())
            targetBot = handler->GetPlayer();
    }
    else
    {
        targetBot = sObjectAccessor.FindPlayerByName(firstToken.c_str());
        if (!(ss >> targetLevel))
        {
            handler->SendSysMessage("Usage: .blb boost <botName> <targetLevel>");
            return true;
        }
    }

    if (!targetBot)
    {
        handler->SendSysMessage("Target bot not found or not currently online.");
        return true;
    }

    if (!NativeTaskAdapter::IsBot(targetBot))
    {
        handler->PSendSysMessage("Player '%s' is a real player, not an autonomous bot.", targetBot->GetName());
        return true;
    }

    if (targetLevel < 1 || targetLevel > 60)
    {
        handler->SendSysMessage("Target level must be between 1 and 60.");
        return true;
    }

    handler->PSendSysMessage("Executing full TortoiseBots level boost for bot '%s' (%u -> %u)...",
        targetBot->GetName(), targetBot->GetLevel(), targetLevel);

    if (LevelDistributionManager::Instance().BoostBotDirect(targetBot, static_cast<uint8_t>(targetLevel)))
    {
        handler->PSendSysMessage("Success: Bot '%s' boosted to level %u with full skills, mounts, professions, and autogear.",
            targetBot->GetName(), targetLevel);
    }
    else
    {
        handler->PSendSysMessage("Failed to boost bot '%s'. Check server logs for details.", targetBot->GetName());
    }
    return true;
}

bool BotBracketsCommands::HandlePending(ChatHandler* handler, const char* args)
{
    if (args && (strcmp(args, "clear") == 0 || strcmp(args, "reset") == 0))
    {
        WorldSession* session = handler ? handler->GetSession() : nullptr;
        if (session && session->GetSecurity() < SEC_ADMINISTRATOR)
        {
            handler->SendSysMessage("You do not have permission to execute this administrative command.");
            return false;
        }

        size_t count = LevelDistributionManager::Instance().GetPendingQueueSize();
        LevelDistributionManager::Instance().ClearPendingQueue();
        handler->PSendSysMessage("[BotBrackets] Cleared %u entries from in-memory pending reset queue.",
            static_cast<uint32_t>(count));
        return true;
    }

    auto snapshot = LevelDistributionManager::Instance().GetPendingQueueSnapshot();
    handler->PSendSysMessage("=== Pending Level Resets Queue (Total: %u) ===",
        static_cast<uint32_t>(snapshot.size()));

    if (snapshot.empty())
    {
        handler->SendSysMessage("Pending reset queue is empty.");
        return true;
    }

    handler->SendSysMessage("GUID Low | Bot Name        | Team | Target Range | Target Level | Age (s)");
    handler->SendSysMessage("---------+-----------------+------+--------------+--------------+--------");
    uint32_t now = static_cast<uint32_t>(time(nullptr));

    for (const auto& entry : snapshot)
    {
        Player* p = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_PLAYER, entry.guidLow));
        std::string name = p ? p->GetName() : "Unknown/Offline";
        uint32_t age = (now >= entry.enqueuedAt) ? (now - entry.enqueuedAt) : 0;

        handler->PSendSysMessage("%8u | %-15s | %-4s | %12u | %12u | %7u",
            entry.guidLow, name.c_str(), (entry.teamId == 0 ? "Ally" : "Hord"),
            entry.targetRange, entry.targetLevel, age);
    }
    return true;
}

bool BotBracketsCommands::HandleDebug(ChatHandler* handler, const char* args)
{
    if (!args || strlen(args) == 0)
    {
        handler->PSendSysMessage("Debug Status: %s", GetDebugModeName(DiagnosticManager::Instance().GetDebugMode()));
        handler->SendSysMessage("Usage: .blb debug [off|summary|trace|dryrun|simulate|clear]");
        return true;
    }

    std::string sub = args;
    if (sub == "off")
    {
        DiagnosticManager::Instance().SetDebugMode(DebugMode::Off);
        handler->SendSysMessage("Debug mode set to Off.");
    }
    else if (sub == "summary")
    {
        DiagnosticManager::Instance().SetDebugMode(DebugMode::Summary);
        handler->SendSysMessage("Debug mode set to Summary.");
    }
    else if (sub == "trace")
    {
        DiagnosticManager::Instance().SetDebugMode(DebugMode::Trace);
        handler->SendSysMessage("Debug mode set to Trace (verbose core log logging).");
    }
    else if (sub == "dryrun")
    {
        DiagnosticManager::Instance().SetDebugMode(DebugMode::DryRun);
        handler->SendSysMessage("Debug mode set to DryRun (simulates requests without calling native APIs).");
    }
    else if (sub == "clear")
    {
        DiagnosticManager::Instance().ClearRingBuffer();
        handler->SendSysMessage("Diagnostic ring buffer cleared.");
    }
    else if (sub == "simulate")
    {
        handler->SendSysMessage("Running offline simulation of scheduling decisions...");
        CensusSnapshot snap = Census::Instance().TakeSnapshot();
        handler->PSendSysMessage("Simulation complete: Alliance Total=%u, Horde Total=%u",
            snap.allianceTotalBots, snap.hordeTotalBots);
    }
    else
    {
        handler->SendSysMessage("Unknown debug action. Valid: off, summary, trace, dryrun, simulate, clear.");
    }
    return true;
}

} // namespace BotLevelBrackets
