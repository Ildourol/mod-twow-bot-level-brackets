#include "mod-twow-bot-level-brackets.h"
#include "BracketsCommon.h"
#include "BracketsConfig.h"
#include "DestinationCatalog.h"
#include "ActivityRegistry.h"
#include "OpportunityLedger.h"
#include "SocialProtection.h"
#include "PopulationCensus.h"
#include "CityRelocationFallback.h"
#include "OpportunityCoordinator.h"
#include "BracketsDebug.h"
#include "BracketsAiContext.h"
#include "BotEngineAdapter.h"

#include "ScriptObjects.h"
#include "ScriptMgr.h"
#include "Objects/Player.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Chat/Chat.h"
#include "Log.h"
#include "Config/Config.h"
#include "World.h"
#include "Group/Group.h"
#include "Guild/Guild.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Util.h"

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace Brackets;

// -----------------------------------------------------------------------------
// COMMAND HANDLERS
// -----------------------------------------------------------------------------

static void HandleStatusCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Bot Level Brackets Extended: System Status ===|r\n";
    ss << "Engine: |cffffff00" << BotEngineAdapter::GetEngineName() << "|r\n";

    const auto& config = ConfigManager::Instance().GetConfig();
    ss << "Master Enabled: " << (config.enabled ? "|cff00ff00YES|r" : "|cffff0000NO|r")
       << " (Gen " << config.generation << ")\n";
    ss << "City Path (Option 2): " << (config.cityEnabled ? "|cff00ff00ENABLED|r" : "|cffff0000DISABLED|r")
       << " [Mode: " << config.cityActivationMode << "]\n";
    ss << "World Path (Option 2): " << (config.worldEnabled ? "|cff00ff00ENABLED|r" : "|cffff0000DISABLED|r") << "\n";
    ss << "City Fallback (Option 3): " << (config.fallbackEnabled ? "|cff00ff00ENABLED|r" : "|cffff0000DISABLED|r")
       << " [Allowlist: " << (config.fallbackCities.empty() ? "None" : config.fallbackCities) << "]\n";
    ss << "Level Policy: |cffffff00" << (config.levelPolicyMode == LEVEL_POLICY_OBSERVE_NATIVE ? "ObserveNative (Advisory)" : "Advisory") << "|r\n";

    time_t now = time(nullptr);
    time_t nextCycle = OpportunityCoordinator::Instance().GetNextCycleTime();
    long secondsUntil = static_cast<long>(nextCycle - now);
    ss << "Scheduler Cadence: " << config.checkSeconds << "s (Next cycle in "
       << (secondsUntil > 0 ? secondsUntil : 0) << "s)\n";

    uint32 activeAlliance = OpportunityLedger::Instance().GetActiveIntentsCount(DOMAIN_CITY, 0) +
                            OpportunityLedger::Instance().GetActiveIntentsCount(DOMAIN_WORLD, 0);
    uint32 activeHorde = OpportunityLedger::Instance().GetActiveIntentsCount(DOMAIN_CITY, 1) +
                         OpportunityLedger::Instance().GetActiveIntentsCount(DOMAIN_WORLD, 1);
    ss << "Active Opportunities: Alliance=" << activeAlliance << ", Horde=" << activeHorde
       << " (Cap=" << config.maxOutstandingPerFaction << "/faction)\n";

    handler->SendSysMessage(ss.str().c_str());
}

static void HandleHubsCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Capital City Hub Population Census ===|r\n";

    CensusSnapshot snap = PopulationCensus::Instance().TakeSnapshot();
    for (uint8 i = 0; i < CITY_COUNT; ++i)
    {
        const auto* city = DestinationCatalog::Instance().GetCity(static_cast<CityId>(i));
        const auto& rec = snap.cities[i];
        if (!city)
            continue;

        ss << "|cffffff00" << std::left << std::setw(13) << city->name << "|r "
           << (city->teamId == 0 ? "[Alli]" : "[Hord]")
           << " Res:" << std::setw(3) << rec.residents
           << " Inb:" << std::setw(2) << rec.confirmedInbound
           << " Pnd:" << std::setw(2) << rec.pending
           << " Eff:" << std::setw(3) << rec.effectivePopulation
           << " Tgt:" << std::setw(3) << rec.feasibleTarget
           << " Def:" << (rec.actionableDeficit > 0 ? "|cff00ff00" : "|cffaaaaaa")
           << std::setw(2) << rec.actionableDeficit << "|r\n";
    }

    handler->SendSysMessage(ss.str().c_str());
}

static void HandleWorldCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Outdoor Region Distribution Status ===|r\n";

    CensusSnapshot snap = PopulationCensus::Instance().TakeSnapshot();
    size_t shown = 0;
    for (const auto& rRec : snap.regions)
    {
        const auto* r = DestinationCatalog::Instance().GetRegion(rRec.regionId);
        if (!r)
            continue;

        if (rRec.residents > 0 || rRec.confirmedInbound > 0 || rRec.actionableDeficit > 0)
        {
            ss << "|cffffff00" << std::left << std::setw(20) << r->name << "|r "
               << "Lvl:" << std::setw(2) << static_cast<uint32>(r->minLevel) << "-" << std::setw(2) << static_cast<uint32>(r->maxLevel)
               << " Res:" << std::setw(2) << rRec.residents
               << " Inb:" << std::setw(2) << rRec.confirmedInbound
               << " Dsr:" << std::setw(2) << rRec.desired
               << " Def:" << (rRec.actionableDeficit > 0 ? "|cff00ff00" : "|cffaaaaaa")
               << std::setw(2) << rRec.actionableDeficit << "|r\n";
            shown++;
            if (shown >= 15)
                break;
        }
    }

    if (shown == 0)
        ss << "All outdoor regions currently balanced or idle.\n";

    handler->SendSysMessage(ss.str().c_str());
}

static void HandleBracketsCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Level Brackets Online Census ===|r\n";

    CensusSnapshot snap = PopulationCensus::Instance().TakeSnapshot();

    ss << "|cffffff00Alliance Level Distribution:|r (Total: " << snap.allianceBotsOnline << ")\n";
    for (size_t i = 0; i < snap.allianceBrackets.size(); ++i)
    {
        const auto& b = snap.allianceBrackets[i];
        ss << "  [" << std::setw(2) << static_cast<uint32>(b.lower) << "-"
           << std::setw(2) << static_cast<uint32>(b.upper) << "] Actual: "
           << std::setw(3) << b.actualCount << " | Desired: "
           << std::setw(3) << b.desiredCount << " ("
           << std::fixed << std::setprecision(1) << b.desiredPct << "%)\n";
    }

    ss << "|cffffff00Horde Level Distribution:|r (Total: " << snap.hordeBotsOnline << ")\n";
    for (size_t i = 0; i < snap.hordeBrackets.size(); ++i)
    {
        const auto& b = snap.hordeBrackets[i];
        ss << "  [" << std::setw(2) << static_cast<uint32>(b.lower) << "-"
           << std::setw(2) << static_cast<uint32>(b.upper) << "] Actual: "
           << std::setw(3) << b.actualCount << " | Desired: "
           << std::setw(3) << b.desiredCount << " ("
           << std::fixed << std::setprecision(1) << b.desiredPct << "%)\n";
    }

    handler->SendSysMessage(ss.str().c_str());
}

static void HandlePendingCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Active Opportunity Ledger Entries ===|r\n";

    auto list = OpportunityLedger::Instance().GetActiveIntentsSnapshot();
    if (list.empty())
    {
        ss << "No active opportunities pending or in transit.\n";
    }
    else
    {
        time_t now = time(nullptr);
        for (const auto& it : list)
        {
            ss << "#" << it.intentId << " Bot=" << it.botGuid.GetCounter()
               << " [" << DomainToString(it.domain) << "] Dest='" << it.destinationName << "'"
               << " Act='" << it.activityName << "'"
               << " State=|cffffff00" << IntentStateToString(it.state) << "|r"
               << " (Age: " << (now - it.creationTime) << "s)\n";
        }
    }

    ss << "|cff888888Historical level reset queue: RETIRED (Option 2 operates via native travel).|r\n";
    handler->SendSysMessage(ss.str().c_str());
}

static void HandleVisitsCommand(ChatHandler* handler, const char* args)
{
    std::ostringstream ss;
    uint32 guidLow = 0;
    if (args && *args)
        guidLow = static_cast<uint32>(atoi(args));

    if (guidLow > 0)
    {
        ss << "|cff00ccff=== Opportunity Visit History for Bot #" << guidLow << " ===|r\n";
        OpportunityIntent active = OpportunityLedger::Instance().GetActiveIntent(ObjectGuid(guidLow));
        if (active.intentId > 0)
        {
            ss << "Active: ID=" << active.intentId << " Domain=" << DomainToString(active.domain)
               << " Dest='" << active.destinationName << "' State=" << IntentStateToString(active.state) << "\n";
        }
        else
        {
            ss << "Active: None.\n";
        }

        auto history = OpportunityLedger::Instance().GetRecentHistorySnapshot(50);
        size_t count = 0;
        for (const auto& h : history)
        {
            if (h.botGuid.GetCounter() == guidLow)
            {
                ss << "  [Intent #" << h.intentId << "] " << DomainToString(h.domain)
                   << " -> '" << h.destinationName << "' (" << h.activityName << ")"
                   << " Terminal: " << IntentStateToString(h.state)
                   << " (" << TerminalReasonToString(h.terminalReason) << ")\n";
                count++;
            }
        }
        if (count == 0)
            ss << "No historical records found for this bot.\n";
    }
    else
    {
        ss << "|cff00ccff=== Recent Opportunity History ===|r\n";
        auto history = OpportunityLedger::Instance().GetRecentHistorySnapshot(10);
        for (const auto& h : history)
        {
            ss << "Bot #" << h.botGuid.GetCounter() << " -> " << h.destinationName
               << " (" << h.activityName << ") Result: " << TerminalReasonToString(h.terminalReason) << "\n";
        }
    }

    handler->SendSysMessage(ss.str().c_str());
}

static void HandleFallbackCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Option 3 City Relocation Fallback Status ===|r\n";

    const auto& config = ConfigManager::Instance().GetConfig();
    ss << "Fallback Enabled: " << (config.fallbackEnabled ? "|cff00ff00YES|r" : "|cffff0000NO (Default)|r") << "\n";
    ss << "Allowlisted Reason: |cffffff00" << config.fallbackAllowedReason << "|r\n";
    ss << "Allowlisted Cities: " << (config.fallbackCities.empty() ? "|cffff0000None|r" : config.fallbackCities) << "\n";
    ss << "Hourly Relocation Limit: " << config.fallbackMaxPerHour << "/hour\n";
    ss << "Relocations In Past Hour: " << CityRelocationFallback::Instance().GetRelocationsInPastHour() << "\n";
    ss << "Cumulative Fallbacks: " << CityRelocationFallback::Instance().GetTotalRelocations()
       << " (Failed: " << CityRelocationFallback::Instance().GetFailedRelocations() << ")\n";
    ss << "|cffaaaaaaNotice: Relocation is never triggered by travel delays, retries, or timeouts.|r\n";

    handler->SendSysMessage(ss.str().c_str());
}

static void HandleStatsCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Bot Level Brackets Performance Telemetry ===|r\n";

    const auto& metrics = BracketsDebug::Instance().GetMetrics();
    ss << "Last Census Calculation: " << metrics.lastCensusTimeMs << " ms\n";
    ss << "Last Scheduler Evaluation: " << metrics.lastSchedulerTimeMs << " ms\n";
    ss << "Admissions (This Cycle): " << metrics.admissionsThisCycle << "\n";
    ss << "Cumulative Admissions: " << metrics.cumulativeAdmissions << "\n";
    ss << "Cumulative Fallbacks: " << CityRelocationFallback::Instance().GetTotalRelocations() << "\n";
    ss << "Dropped Debug Events: " << metrics.droppedEvents << "\n";

    handler->SendSysMessage(ss.str().c_str());
}

static void HandleActivitiesCommand(ChatHandler* handler)
{
    std::ostringstream ss;
    ss << "|cff00ccff=== Registered Native Activities (43 Catalog) ===|r\n";

    const auto& all = ActivityRegistry::Instance().GetAllDefinitions();
    for (const auto& def : all)
    {
        const auto& cfg = ActivityRegistry::Instance().GetConfig(def.id);
        ss << std::left << std::setw(20) << def.name
           << " [" << SupportLevelToString(def.support) << "]"
           << " En:" << (cfg.enabled ? "1" : "0")
           << " C:" << (cfg.cityEnabled ? "1" : "0")
           << " W:" << (cfg.worldEnabled ? "1" : "0")
           << " Wt:" << std::fixed << std::setprecision(1) << cfg.weight
           << " Intended:" << ActivityRegistry::Instance().GetIntendedCount(def.id) << "\n";
    }

    handler->SendSysMessage(ss.str().c_str());
}

static void HandleExplainCommand(ChatHandler* handler, const char* args)
{
    if (!args || !*args)
    {
        handler->SendSysMessage("Usage: .blb explain <guid> | city <id> | region <id>");
        return;
    }

    std::stringstream ss(args);
    std::string sub;
    ss >> sub;

    if (sub == "city")
    {
        uint32 id = 0;
        ss >> id;
        handler->SendSysMessage(BracketsDebug::Instance().ExplainCity(id).c_str());
    }
    else if (sub == "region")
    {
        uint32 id = 0;
        ss >> id;
        handler->SendSysMessage(BracketsDebug::Instance().ExplainRegion(id).c_str());
    }
    else
    {
        uint32 guid = static_cast<uint32>(atoi(sub.c_str()));
        handler->SendSysMessage(BracketsDebug::Instance().ExplainBot(guid).c_str());
    }
}

static void HandleDebugCommand(ChatHandler* handler, const char* args)
{
    if (!args || !*args)
    {
        handler->SendSysMessage("Usage: .blb debug status | trace <guid> <seconds> | stop | export | counters reset");
        return;
    }

    std::stringstream ss(args);
    std::string sub;
    ss >> sub;

    if (sub == "status")
    {
        const auto& config = ConfigManager::Instance().GetConfig();
        std::ostringstream out;
        out << "Debug Mode: " << (config.debugMode == DEBUG_MODE_OFF ? "Off" :
                                 config.debugMode == DEBUG_MODE_SUMMARY ? "Summary" :
                                 config.debugMode == DEBUG_MODE_TRACE ? "Trace" : "DryRun") << "\n";
        out << "Output: " << config.debugOutput << "\n";
        out << "Ring Buffer Capacity: " << config.debugRingBufferEvents << " events\n";
        out << "Recent Events Snapshot Size: " << BracketsDebug::Instance().GetRecentEventsSnapshot(100).size() << "\n";
        handler->SendSysMessage(out.str().c_str());
    }
    else if (sub == "trace")
    {
        uint32 guid = 0;
        uint32 sec = 300;
        ss >> guid >> sec;
        if (guid == 0)
        {
            handler->SendSysMessage("Usage: .blb debug trace <botGuidLow> [seconds]");
            return;
        }
        BracketsDebug::Instance().StartTrace(guid, sec);
        handler->SendSysMessage("Started detailed debug trace for bot.");
    }
    else if (sub == "stop")
    {
        BracketsDebug::Instance().StopTrace();
        handler->SendSysMessage("Stopped active debug trace.");
    }
    else if (sub == "counters" && ss.str().find("reset") != std::string::npos)
    {
        BracketsDebug::Instance().ResetCounters();
        CityRelocationFallback::Instance().ResetCounters();
        ActivityRegistry::Instance().ResetCounters();
        handler->SendSysMessage("Debug and telemetry counters reset.");
    }
    else if (sub == "export")
    {
        std::string err;
        std::string path = "logs/bot_level_brackets/debug_events.jsonl";
        if (BracketsDebug::Instance().ExportJsonl(path, &err))
        {
            handler->SendSysMessage("Successfully exported debug events to logs/bot_level_brackets/debug_events.jsonl");
        }
        else
        {
            handler->SendSysMessage(err.c_str());
        }
    }
    else
    {
        handler->SendSysMessage("Unknown debug sub-command.");
    }
}

// -----------------------------------------------------------------------------
// SCRIPT OBJECTS IMPLEMENTATION
// -----------------------------------------------------------------------------

class BotLevelBracketsWorldScript : public WorldScript
{
public:
    BotLevelBracketsWorldScript() : WorldScript("BotLevelBracketsWorldScript") {}

    virtual void OnStartup() override
    {
        ConfigManager::Instance().LoadConfiguration(false);
        DestinationCatalog::Instance().Initialize();
        ActivityRegistry::Instance().Initialize();
        SocialProtection::Instance().Initialize();
        OpportunityCoordinator::Instance().Initialize();

#if defined(HAVE_TORTOISEBOTS)
        RegisterBracketsAiExtension();
#endif
    }

    virtual void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
        {
            uint32 oldGen = ConfigManager::Instance().GetGeneration();
            ConfigManager::Instance().LoadConfiguration(true);
            OpportunityLedger::Instance().InvalidateAllForReload(oldGen);
        }
    }

    virtual void OnUpdate(uint32 diff) override
    {
        OpportunityCoordinator::Instance().Update(diff);
    }
};

class BotLevelBracketsPlayerScript : public PlayerScript
{
public:
    BotLevelBracketsPlayerScript() : PlayerScript("BotLevelBracketsPlayerScript") {}

    virtual void OnLogin(Player* player) override
    {
        OpportunityCoordinator::Instance().HandlePlayerLogin(player);
    }

    virtual void OnLogout(Player* player) override
    {
        OpportunityCoordinator::Instance().HandlePlayerLogout(player);
    }
};

class BotLevelBracketsGuildScript : public GuildScript
{
public:
    BotLevelBracketsGuildScript() : GuildScript("BotLevelBracketsGuildScript") {}

    virtual void OnAddMember(Guild* guild, Player* player, uint8&) override
    {
        if (guild && player)
            SocialProtection::Instance().OnGuildMemberAdd(guild->GetId(), player);
    }

    virtual void OnRemoveMember(Guild* guild, Player* player, bool isDisband, bool) override
    {
        if (guild && player)
            SocialProtection::Instance().OnGuildMemberRemove(guild->GetId(), player, isDisband);
    }
};

class BotLevelBracketsCommandScript : public AllCommandScript
{
public:
    BotLevelBracketsCommandScript() : AllCommandScript("BotLevelBracketsCommandScript") {}

    virtual bool CanExecuteCommand(ChatHandler* handler, const char* cmdStr, const char* cmdArg) override
    {
        if (!handler || !cmdStr)
            return true;

        std::string command = cmdStr;
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        if (command != "blb" && command != "botbrackets")
            return true;

        if (handler->GetSession() && handler->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
        {
            handler->SendSysMessage("You do not have permission to execute Bot Level Brackets commands.");
            return false;
        }

        std::string sub = "";
        std::string subArgs = "";
        if (cmdArg && *cmdArg)
        {
            std::stringstream ss(cmdArg);
            ss >> sub;
            std::string remaining;
            std::getline(ss, remaining);
            size_t start = remaining.find_first_not_of(" \t");
            if (start != std::string::npos)
                subArgs = remaining.substr(start);
        }

        std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

        if (sub.empty() || sub == "help")
        {
            handler->SendSysMessage("|cff00ccffBot Level Brackets Extended Commands:|r");
            handler->SendSysMessage("  .blb status          - Overall system and engine status");
            handler->SendSysMessage("  .blb hubs            - Capital city population and quotas");
            handler->SendSysMessage("  .blb world / regions - Outdoor region distribution");
            handler->SendSysMessage("  .blb brackets        - Level distribution and advisory ladder");
            handler->SendSysMessage("  .blb pending         - Active opportunity reservations");
            handler->SendSysMessage("  .blb visits [guid]   - Opportunity history and metrics");
            handler->SendSysMessage("  .blb fallback        - Option 3 restricted fallback status");
            handler->SendSysMessage("  .blb stats           - Performance and scheduler telemetry");
            handler->SendSysMessage("  .blb activities      - 43 native activities catalog");
            handler->SendSysMessage("  .blb explain <guid>  - Bot diagnostic snapshot");
            handler->SendSysMessage("  .blb debug <subcmd>  - Debug status, trace, export");
            handler->SendSysMessage("  .blb reload          - Atomic configuration reload");
            handler->SendSysMessage("  .blb force           - Trigger one scheduling evaluation");
            handler->SendSysMessage("  .blb cleanup         - Idempotent cleanup of stale caches");
            return false;
        }
        else if (sub == "status")
        {
            HandleStatusCommand(handler);
        }
        else if (sub == "hubs")
        {
            HandleHubsCommand(handler);
        }
        else if (sub == "world" || sub == "regions")
        {
            HandleWorldCommand(handler);
        }
        else if (sub == "brackets")
        {
            HandleBracketsCommand(handler);
        }
        else if (sub == "pending")
        {
            HandlePendingCommand(handler);
        }
        else if (sub == "visits")
        {
            HandleVisitsCommand(handler, subArgs.c_str());
        }
        else if (sub == "fallback")
        {
            HandleFallbackCommand(handler);
        }
        else if (sub == "stats")
        {
            HandleStatsCommand(handler);
        }
        else if (sub == "activities")
        {
            HandleActivitiesCommand(handler);
        }
        else if (sub == "explain")
        {
            HandleExplainCommand(handler, subArgs.c_str());
        }
        else if (sub == "debug")
        {
            HandleDebugCommand(handler, subArgs.c_str());
        }
        else if (sub == "reload")
        {
            ConfigManager::Instance().LoadConfiguration(true);
            handler->SendSysMessage("Bot Level Brackets: Configuration reloaded successfully.");
        }
        else if (sub == "force")
        {
            OpportunityCoordinator::Instance().ForceCycle();
            handler->SendSysMessage("Bot Level Brackets: Forced one evaluation cycle.");
        }
        else if (sub == "cleanup")
        {
            SocialProtection::Instance().RefreshCaches();
            handler->SendSysMessage("Bot Level Brackets: Stale caches refreshed.");
        }
        else
        {
            handler->SendSysMessage("Unknown sub-command. Type '.blb help' for options.");
        }

        return false;
    }
};

// -----------------------------------------------------------------------------
// SCRIPT REGISTRATION ENTRYPOINTS
// -----------------------------------------------------------------------------

void Addmod_twow_bot_level_bracketsScripts()
{
    new BotLevelBracketsWorldScript();
    new BotLevelBracketsPlayerScript();
    new BotLevelBracketsGuildScript();
    new BotLevelBracketsCommandScript();
}

void Addmod_twow_bot_level_brackets_extendedScripts()
{
    Addmod_twow_bot_level_bracketsScripts();
}
