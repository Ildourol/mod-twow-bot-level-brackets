#include "mod-twow-bot-level-brackets.h"
#include "compat/NativeTaskAdapter.h"
#include "policy/PopulationPolicy.h"
#include "policy/QuotaAllocator.h"
#include "world/ReadOnlyCatalog.h"
#include "world/Census.h"
#include "scheduling/Coordinator.h"
#include "scheduling/RequestLedger.h"
#include "activities/ActivityRegistry.h"
#include "social/ProtectionCache.h"
#include "commands/BotBracketsCommands.h"
#include "debug/DiagnosticManager.h"
#include "distribution/LevelDistributionManager.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
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
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "ScriptObjects.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Chat.h"
#include "Log.h"
#include "Config/Config.h"
#include "World.h"
#include "Group.h"
#include "Guild.h"
#else
#include "Common.h"
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
#endif

#include <string>
#include <algorithm>

using namespace BotLevelBrackets;

namespace
{

inline int GetInternalInt(const char* key, int def) { return sConfig.GetIntDefault(key, def); }
inline bool GetInternalBool(const char* key, bool def) { return sConfig.GetBoolDefault(key, def); }
inline float GetInternalFloat(const char* key, float def) { return sConfig.GetFloatDefault(key, def); }
inline std::string GetInternalString(const char* key, const char* def) { return sConfig.GetStringDefault(key, def); }

void LoadModuleConfiguration()
{
    auto& policy = PopulationPolicy::Instance();
    auto& reg = ActivityRegistry::Instance();
    auto& diag = DiagnosticManager::Instance();
    auto& prot = ProtectionCache::Instance();

    // 1. Core Orchestration Settings
    policy.SetEnabled(sConfig.GetBoolDefault("BotLevelBrackets.Enabled", true));
    policy.SetDryRun(GetInternalBool("BotLevelBrackets.Orchestration.DryRun", false));
    policy.SetCheckCadenceSeconds(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.CheckSeconds", 90)));
    policy.SetMaxRequestsPerCycle(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.MaxRequestsPerCycle", 5)));
    policy.SetMaxOutstandingPerFaction(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.MaxOutstandingPerFaction", 10)));
    policy.SetMaxLooseRequestsPerCycle(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.MaxLooseRequestsPerCycle", 1)));
    policy.SetRequestCooldownSeconds(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.RequestCooldownSeconds", 1800)));
    policy.SetRejectionBackoffSeconds(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.RejectionBackoffSeconds", 600)));
    policy.SetStabilitySeconds(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.StabilitySeconds", 60)));
    policy.SetDispatchTimeoutSeconds(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.DispatchTimeoutSeconds", 30)));
    policy.SetEarlyDepartureBackoffSeconds(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Orchestration.EarlyDepartureBackoffSeconds", 3600)));

    // 2. City Orchestration Settings
    policy.SetCityEnabled(sConfig.GetBoolDefault("BotLevelBrackets.City.Enabled", true));

    std::string cityDestStr = GetInternalString("BotLevelBrackets.City.DestinationPolicy", "RequireExact");
    policy.SetCityDestinationPolicy((cityDestStr == "AllowNativeChoice") ? DestinationPolicy::AllowNativeChoice : DestinationPolicy::RequireExact);

    policy.SetCityQuotaMode(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.City.QuotaMode", 2)));
    policy.SetCityMinLimitEnabled(GetInternalBool("BotLevelBrackets.City.MinLimitEnabled", false));
    policy.SetCityMaxLimitEnabled(GetInternalBool("BotLevelBrackets.City.MaxLimitEnabled", true));
    policy.SetCityLimitsMode(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.City.LimitsMode", 1)));
    policy.SetCityMinBots(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.City.MinBotsPerCity", 0)));
    policy.SetCityMaxBots(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MaxBotsPerCity", 60)));
    policy.SetCityHysteresis(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.City.HysteresisBots", 2)));
    policy.SetCityTransitMode(static_cast<TransitMode>(sConfig.GetIntDefault("BotLevelBrackets.City.TransitMode", 2)));
    policy.SetCityPickAllBrackets(sConfig.GetBoolDefault("BotLevelBrackets.City.PickAllBrackets", true));
    policy.SetCityMinLevel(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MinLevel", 1)));
    policy.SetCityDwellDurationMs(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.DwellDurationMs", 300000)));
    policy.SetCityMinDwellDurationMs(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.City.MinDwellDurationMs", 300000)));
    policy.SetCityMaxDwellDurationMs(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.City.MaxDwellDurationMs", 600000)));

    policy.SetCityTarget(0, sConfig.GetIntDefault("BotLevelBrackets.City.StormwindTarget", 10));
    policy.SetCityTarget(1, sConfig.GetIntDefault("BotLevelBrackets.City.IronforgeTarget", 8));
    policy.SetCityTarget(2, sConfig.GetIntDefault("BotLevelBrackets.City.DarnassusTarget", 6));
    policy.SetCityTarget(3, sConfig.GetIntDefault("BotLevelBrackets.City.AlahThalasTarget", 6));
    policy.SetCityTarget(4, sConfig.GetIntDefault("BotLevelBrackets.City.OrgrimmarTarget", 10));
    policy.SetCityTarget(5, sConfig.GetIntDefault("BotLevelBrackets.City.UndercityTarget", 8));
    policy.SetCityTarget(6, sConfig.GetIntDefault("BotLevelBrackets.City.ThunderBluffTarget", 6));
    policy.SetCityTarget(7, sConfig.GetIntDefault("BotLevelBrackets.City.SparkwaterTarget", 6));

    std::string landingTargetStr = GetInternalString("BotLevelBrackets.City.LandingTarget", "Teleport");
    if (landingTargetStr == "Bank" || landingTargetStr == "bank")
        policy.SetCityLandingTarget(CityLandingTarget::Bank);
    else
        policy.SetCityLandingTarget(CityLandingTarget::Teleport);

    // Optional city landing location overrides (defaults to .teleport command or bank landing coordinates)
    static const char* s_cityNames[] = {
        "Stormwind", "Ironforge", "Darnassus", "AlahThalas",
        "Orgrimmar", "Undercity", "ThunderBluff", "Sparkwater"
    };
    for (size_t i = 0; i < 8; ++i)
    {
        std::string key = std::string("BotLevelBrackets.City.") + s_cityNames[i] + ".Location";
        std::string val = sConfig.GetStringDefault(key.c_str(), "");
        if (!val.empty())
        {
            if (val == "bank" || val == "Bank")
            {
                const CityDefinition* def = ReadOnlyCatalog::Instance().GetCity(static_cast<CityId>(i + 1));
                if (def)
                {
                    TeleportDestinationConfig bankDest;
                    bankDest.mapId = def->mapId;
                    bankDest.x = def->bankX;
                    bankDest.y = def->bankY;
                    bankDest.z = def->bankZ;
                    bankDest.o = def->bankO;
                    bankDest.name = "Bank";
                    policy.SetCityLandingDestination(static_cast<CityId>(i + 1), bankDest);
                }
            }
            else
            {
                TeleportDestinationConfig dest;
                if (NativeTaskAdapter::ResolveTeleportLocation(val, dest))
                    policy.SetCityLandingDestination(static_cast<CityId>(i + 1), dest);
            }
        }
    }

    // 3. Outdoor World Distribution Settings
    policy.SetWorldEnabled(sConfig.GetBoolDefault("BotLevelBrackets.World.Enabled", true));
    std::string worldDestStr = GetInternalString("BotLevelBrackets.World.DestinationPolicy", "RequireExact");
    policy.SetWorldDestinationPolicy((worldDestStr == "AllowNativeChoice") ? DestinationPolicy::AllowNativeChoice : DestinationPolicy::RequireExact);
    policy.SetFieldReservePct(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.World.FieldReservePct", 70)));

    // 4. Level & Population Policy Settings & Active Redistribution
    policy.SetLevelDistributionEnabled(sConfig.GetBoolDefault("BotLevelBrackets.LevelDistribution.Enabled", true));
    policy.SetDistributionCheckFrequency(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.LevelDistribution.CheckFrequency", 300)));
    policy.SetCheckFlaggedFrequency(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.LevelDistribution.CheckFlaggedFrequency", 15)));
    policy.SetFlaggedProcessLimit(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.LevelDistribution.FlaggedProcessLimit", 5)));
    policy.SetMinBracketChangeThreshold(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.LevelDistribution.MinBracketChangeThreshold", 2)));
    policy.SetMaxPendingQueueSize(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.LevelDistribution.MaxPendingQueueSize", 0)));
    policy.SetPendingQueueTTL(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.LevelDistribution.PendingQueueTTL", 0)));
    policy.SetTeleportOnLevelChange(sConfig.GetBoolDefault("BotLevelBrackets.TeleportOnLevelChange", true));
    policy.SetArrivalFloorHoldEnabled(GetInternalBool("BotLevelBrackets.Teleport.ArrivalFloorHold", true));
    policy.SetArrivalHoldDurationMs(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Teleport.ArrivalHoldDurationMs", 5000)));
    policy.SetMaxFloorLifts(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Teleport.MaxFloorLifts", 6)));

    // Starting-Zone and Wrong-Map Dispersal Settings
    policy.SetStartingZoneDisperseEnabled(sConfig.GetBoolDefault("BotLevelBrackets.StartingZoneDisperse.Enabled", true));
    policy.SetStartingZoneDisperseFrequency(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.StartingZoneDisperse.Frequency", 15)));
    policy.SetWrongMapDisperseEnabled(sConfig.GetBoolDefault("BotLevelBrackets.WrongMapDisperse.Enabled", true));
    policy.SetWrongMapDisperseFrequency(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.WrongMapDisperse.Frequency", 60)));

    policy.SetUseDynamicDistribution(GetInternalBool("BotLevelBrackets.LevelPolicy.UseDynamic", false));
    policy.SetRealPlayerWeight(GetInternalFloat("BotLevelBrackets.LevelPolicy.HumanWeight", 1.0f));
    policy.SetSyncFactions(GetInternalBool("BotLevelBrackets.LevelPolicy.SyncFactions", false));

    // 5. Explicit Level Bracket Definitions (Alliance and Horde)
    struct BracketDef { uint8_t lower; uint8_t upper; };
    static const BracketDef s_defaultRanges[7] = {
        { 1,  9},
        {10, 19},
        {20, 29},
        {30, 39},
        {40, 49},
        {50, 59},
        {60, 60}
    };

    std::vector<LevelBracketConfig> allianceBrackets;
    allianceBrackets.push_back({ s_defaultRanges[0].lower, s_defaultRanges[0].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range1.Pct", 16)) });
    allianceBrackets.push_back({ s_defaultRanges[1].lower, s_defaultRanges[1].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range2.Pct", 14)) });
    allianceBrackets.push_back({ s_defaultRanges[2].lower, s_defaultRanges[2].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range3.Pct", 14)) });
    allianceBrackets.push_back({ s_defaultRanges[3].lower, s_defaultRanges[3].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range4.Pct", 14)) });
    allianceBrackets.push_back({ s_defaultRanges[4].lower, s_defaultRanges[4].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range5.Pct", 14)) });
    allianceBrackets.push_back({ s_defaultRanges[5].lower, s_defaultRanges[5].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range6.Pct", 14)) });
    allianceBrackets.push_back({ s_defaultRanges[6].lower, s_defaultRanges[6].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range7.Pct", 14)) });
    policy.SetLevelBrackets(0, allianceBrackets);

    std::vector<LevelBracketConfig> hordeBrackets;
    hordeBrackets.push_back({ s_defaultRanges[0].lower, s_defaultRanges[0].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range1.Pct", 16)) });
    hordeBrackets.push_back({ s_defaultRanges[1].lower, s_defaultRanges[1].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range2.Pct", 14)) });
    hordeBrackets.push_back({ s_defaultRanges[2].lower, s_defaultRanges[2].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range3.Pct", 14)) });
    hordeBrackets.push_back({ s_defaultRanges[3].lower, s_defaultRanges[3].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range4.Pct", 14)) });
    hordeBrackets.push_back({ s_defaultRanges[4].lower, s_defaultRanges[4].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range5.Pct", 14)) });
    hordeBrackets.push_back({ s_defaultRanges[5].lower, s_defaultRanges[5].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range6.Pct", 14)) });
    hordeBrackets.push_back({ s_defaultRanges[6].lower, s_defaultRanges[6].upper, static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range7.Pct", 14)) });
    policy.SetLevelBrackets(1, hordeBrackets);
    policy.ClampAndBalanceBrackets(true);

    // Optional starter race destination overrides (defaults to hardcoded starter valley coordinates)
    static const char* s_starterRaces[] = {
        "Human", "Dwarf", "Gnome", "NightElf", "HighElf",
        "Orc", "Troll", "Tauren", "Undead", "Goblin"
    };
    static const uint8_t s_raceIds[] = {
        RACE_HUMAN, RACE_DWARF, RACE_GNOME, RACE_NIGHTELF, RACE_HIGH_ELF,
        RACE_ORC, RACE_TROLL, RACE_TAUREN, RACE_UNDEAD, RACE_GOBLIN
    };
    for (size_t i = 0; i < 10; ++i)
    {
        std::string key = std::string("BotLevelBrackets.Starter.") + s_starterRaces[i] + ".Locations";
        std::string val = sConfig.GetStringDefault(key.c_str(), "");
        if (!val.empty())
            policy.SetStarterLocationPool(s_raceIds[i], NativeTaskAdapter::ParseTeleportLocationPool(val));
    }

    // Optional leveling zone destination overrides (defaults to hardcoded Turtle WoW Zone Level Chart tables)
    for (size_t b = 1; b <= 7; ++b)
    {
        std::string aKey = "BotLevelBrackets.Alliance.Range" + std::to_string(b) + ".Locations";
        std::string aVal = sConfig.GetStringDefault(aKey.c_str(), "");
        if (!aVal.empty())
            policy.SetRangeLocationPool(0, b - 1, NativeTaskAdapter::ParseTeleportLocationPool(aVal));

        std::string hKey = "BotLevelBrackets.Horde.Range" + std::to_string(b) + ".Locations";
        std::string hVal = sConfig.GetStringDefault(hKey.c_str(), "");
        if (!hVal.empty())
            policy.SetRangeLocationPool(1, b - 1, NativeTaskAdapter::ParseTeleportLocationPool(hVal));
    }

    // 6. Social & Identity Protections
    prot.SetIgnoreFriendListed(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreFriendListed", true));
    prot.SetIgnoreRealPlayerGuilds(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreRealPlayerGuilds", true));
    prot.SetIgnoreArenaTeams(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreArenaTeams", true));
    prot.SetSocialRefreshCadence(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Protection.SocialRefreshSeconds", 300)));
    prot.SetGuildTrackerCadence(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Protection.GuildTrackerUpdateSeconds", 600)));

    std::string exclNames = sConfig.GetStringDefault("BotLevelBrackets.Protection.ExcludedBotNames", "");
    if (exclNames.empty())
        exclNames = GetInternalString("BotLevelBrackets.ExcludeNames", "");
    prot.SetExcludedNames(exclNames);

    // 7. Activity Registry Controls
    reg.SetActivitiesEnabled(sConfig.GetBoolDefault("BotLevelBrackets.Activities.Enabled", true));
    reg.SetRequireSupported(GetInternalBool("BotLevelBrackets.Activities.RequireSupported", false));

    // Optional dynamic per-activity overrides
    for (auto const& act : reg.GetAllActivities())
    {
        std::string prefix = "BotLevelBrackets.Activities." + act.id + ".";
        ActivityDefinition* a = reg.GetActivity(act.id);
        if (a)
        {
            a->enabled = sConfig.GetBoolDefault((prefix + "Enabled").c_str(), a->enabled);
            a->cityEnabled = sConfig.GetBoolDefault((prefix + "CityEnabled").c_str(), a->cityEnabled);
            a->worldEnabled = sConfig.GetBoolDefault((prefix + "WorldEnabled").c_str(), a->worldEnabled);
            a->weight = sConfig.GetFloatDefault((prefix + "Weight").c_str(), a->weight);
            a->cooldownSeconds = static_cast<uint32_t>(sConfig.GetIntDefault((prefix + "CooldownSeconds").c_str(), a->cooldownSeconds));
            a->maxOutstandingPerFaction = static_cast<uint32_t>(sConfig.GetIntDefault((prefix + "MaxOutstandingPerFaction").c_str(), a->maxOutstandingPerFaction));
        }
    }

    // 8. Debug Settings
    std::string debugModeStr = sConfig.GetStringDefault("BotLevelBrackets.Debug.Mode", "Off");
    if (debugModeStr == "Summary")
        diag.SetDebugMode(DebugMode::Summary);
    else if (debugModeStr == "Trace")
        diag.SetDebugMode(DebugMode::Trace);
    else if (debugModeStr == "DryRun")
        diag.SetDebugMode(DebugMode::DryRun);
    else
        diag.SetDebugMode(DebugMode::Off);

    diag.SetMaxEventsPerSecond(static_cast<uint32_t>(GetInternalInt("BotLevelBrackets.Debug.MaxEventsPerSecond", 50)));
    diag.SetRingBufferCapacity(static_cast<size_t>(GetInternalInt("BotLevelBrackets.Debug.RingBufferEvents", 2000)));

    policy.IncrementConfigGeneration();
    RequestLedger::Instance().WithdrawStaleProposals(policy.GetConfigGeneration());
}

} // anonymous namespace

class BotLevelBracketsWorldScript : public WorldScript
{
public:
    BotLevelBracketsWorldScript() : WorldScript("BotLevelBracketsWorldScript") {}

    void OnStartup() override
    {
        sLog.outString("======================================================================");
        sLog.outString("  mod-twow-bot-level-brackets: Native-Task Population Orchestrator    ");
        sLog.outString("  Unchanged Playerbots Executes; Zero Core/Bot Modifications           ");
        sLog.outString("  Active Engine: %s", NativeTaskAdapter::GetEngineName());
        sLog.outString("======================================================================");

        LoadModuleConfiguration();
        ProtectionCache::Instance().Initialize();
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
        {
            sLog.outString("mod-twow-bot-level-brackets: Reloading configuration...");
            LoadModuleConfiguration();
            ProtectionCache::Instance().RefreshFromDatabase();
            sLog.outString("mod-twow-bot-level-brackets: Configuration reloaded successfully.");
        }
    }

    void OnUpdate(uint32 diff) override
    {
        Coordinator::Instance().Update(diff);
        LevelDistributionManager::Instance().Update(diff);
        ProtectionCache::Instance().Update(diff);
    }
};

class BotLevelBracketsPlayerScript : public PlayerScript
{
public:
    BotLevelBracketsPlayerScript() : PlayerScript("BotLevelBracketsPlayerScript") {}

    void OnLogin(Player* player) override
    {
        if (!player || NativeTaskAdapter::IsBot(player))
            return;

        uint32 guildId = player->GetGuildId();
        if (guildId != 0)
            ProtectionCache::Instance().OnGuildMemberAdd(guildId, true);
    }

    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        uint32_t guidLow = player->GetObjectGuid().GetCounter();
        LevelDistributionManager::Instance().RemoveBot(guidLow);
        NativeTaskAdapter::RemoveArrivalFloor(player->GetObjectGuid());
        const LedgerEntry* active = RequestLedger::Instance().GetActiveEntryForBot(guidLow);
        if (active)
        {
            RequestLedger::Instance().MarkReleased(
                active->requestId, "BotLogout", static_cast<uint32_t>(time(nullptr)));
        }
    }
};

class BotLevelBracketsGuildScript : public GuildScript
{
public:
    BotLevelBracketsGuildScript() : GuildScript("BotLevelBracketsGuildScript") {}

    void OnAddMember(Guild* guild, Player* player, uint8&) override
    {
        if (!guild || !player)
            return;

        if (!NativeTaskAdapter::IsBot(player))
            ProtectionCache::Instance().OnGuildMemberAdd(guild->GetId(), true);
    }

    void OnRemoveMember(Guild* guild, Player* player, bool, bool) override
    {
        if (!guild || !player)
            return;

        if (!NativeTaskAdapter::IsBot(player))
            ProtectionCache::Instance().OnGuildMemberRemove(guild->GetId(), true);
    }
};

class BotLevelBracketsCommandScript : public AllCommandScript
{
public:
    BotLevelBracketsCommandScript() : AllCommandScript("BotLevelBracketsCommandScript") {}

    bool CanExecuteCommand(ChatHandler* handler, const char* cmd, const char* args) override
    {
        if (!cmd)
            return true;

        std::string rootCommand = cmd;
        std::transform(rootCommand.begin(), rootCommand.end(), rootCommand.begin(), ::tolower);
        if (rootCommand != "blb" && rootCommand != "botbrackets" && rootCommand != "botlevelbrackets")
            return true;

        std::string rawArgs = args ? args : "";
        size_t first = rawArgs.find_first_not_of(" \t\r\n");
        std::string subCmd = "status";
        std::string extraArgs = "";

        if (first != std::string::npos)
        {
            rawArgs.erase(0, first);
            size_t spacePos = rawArgs.find_first_of(" \t\r\n");
            if (spacePos != std::string::npos)
            {
                subCmd = rawArgs.substr(0, spacePos);
                size_t restFirst = rawArgs.find_first_not_of(" \t\r\n", spacePos);
                if (restFirst != std::string::npos)
                    extraArgs = rawArgs.substr(restFirst);
            }
            else
            {
                subCmd = rawArgs;
            }
        }
        std::transform(subCmd.begin(), subCmd.end(), subCmd.begin(), ::tolower);

        WorldSession* session = handler ? handler->GetSession() : nullptr;
        auto hasSecurity = [session](AccountTypes security)
        {
            return !session || session->GetSecurity() >= security;
        };

        if (subCmd == "reload" || subCmd == "cleanup" || subCmd == "guildcleanup" || subCmd == "force")
        {
            if (!hasSecurity(SEC_ADMINISTRATOR))
            {
                handler->SendSysMessage("You do not have permission to execute this administrative command.");
                return false;
            }
        }
        else
        {
            if (!hasSecurity(SEC_MODERATOR))
            {
                handler->SendSysMessage("You do not have permission to execute BotLevelBrackets commands.");
                return false;
            }
        }

        BotBracketsCommands::HandleCommand(handler, subCmd.c_str(), extraArgs.c_str());
        return false; // Command fully consumed
    }
};

void Addmod_twow_bot_level_bracketsScripts()
{
    new BotLevelBracketsWorldScript();
    new BotLevelBracketsPlayerScript();
    new BotLevelBracketsGuildScript();
    new BotLevelBracketsCommandScript();
}
