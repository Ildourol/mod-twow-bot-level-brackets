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

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "Chat/Chat.h"
#include "Config/Config.h"
#include "Log.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Chat.h"
#include "Config/Config.h"
#include "Log.h"
#else
// Test doubles
#include "support/Framework.h"
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

    if (strcmp(subCommand, "world") == 0 || strcmp(subCommand, "regions") == 0)
        return HandleWorld(handler);

    if (strcmp(subCommand, "brackets") == 0)
        return HandleBrackets(handler);

    if (strcmp(subCommand, "activities") == 0)
        return HandleActivities(handler);

    if (strcmp(subCommand, "ledger") == 0 || strcmp(subCommand, "pending") == 0)
        return HandleLedger(handler);

    if (strcmp(subCommand, "stats") == 0)
        return HandleStats(handler);

    if (strcmp(subCommand, "explain") == 0)
        return HandleExplain(handler, args);

    if (strcmp(subCommand, "reload") == 0)
        return HandleReload(handler);

    if (strcmp(subCommand, "force") == 0)
        return HandleForce(handler);

    if (strcmp(subCommand, "cleanup") == 0)
        return HandleCleanup(handler);

    if (strcmp(subCommand, "debug") == 0)
        return HandleDebug(handler, args);

    handler->SendSysMessage("Unknown BLB subcommand. Valid: status, cities, world, brackets, activities, ledger, stats, explain, reload, force, cleanup, debug.");
    return true;
}

bool BotBracketsCommands::HandleStatus(ChatHandler* handler)
{
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    const auto& policy = PopulationPolicy::Instance();

    handler->PSendSysMessage("=== Turtle WoW Bot Level Brackets (Option 1 Orchestrator) ===");
    handler->PSendSysMessage("Master Status: %s | Mode: ExistingNativeTasks | DryRun: %s",
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
    return true;
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

bool BotBracketsCommands::HandleWorld(ChatHandler* handler)
{
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    const auto& policy = PopulationPolicy::Instance();
    const auto& catalog = ReadOnlyCatalog::Instance();

    handler->SendSysMessage("=== Outdoor World Distribution (Option 1) ===");
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
    handler->SendSysMessage("=== Level Bracket Distribution (ADVISORY - Native Owned) ===");
    handler->SendSysMessage("Notice: Option 1 strictly observes native level progression without character resets.");
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
    handler->SendSysMessage("=== Coordinator Performance & Request Telemetry ===");
    handler->PSendSysMessage("Total Scheduling Cycles Executed: %llu",
        static_cast<unsigned long long>(coord.GetTotalCyclesRun()));
    handler->PSendSysMessage("Total Native Task Requests Submitted: %llu",
        static_cast<unsigned long long>(coord.GetTotalRequestsSubmitted()));
    handler->PSendSysMessage("Total Native Requests Accepted: %llu",
        static_cast<unsigned long long>(coord.GetTotalRequestsAccepted()));
    handler->PSendSysMessage("Total Native Requests Refused/Rejected: %llu",
        static_cast<unsigned long long>(coord.GetTotalRequestsRejected()));
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

bool BotBracketsCommands::HandleForce(ChatHandler* handler)
{
    handler->SendSysMessage("Triggering immediate bounded coordinator scheduling pass...");
    Coordinator::Instance().ForceExecutionCycle();
    handler->SendSysMessage("Scheduling pass complete.");
    return true;
}

bool BotBracketsCommands::HandleCleanup(ChatHandler* handler)
{
    handler->SendSysMessage("Pruning stale ledger requests and expired cooldowns...");
    Coordinator::Instance().CleanupStaleEntries();
    handler->SendSysMessage("Cleanup complete.");
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
