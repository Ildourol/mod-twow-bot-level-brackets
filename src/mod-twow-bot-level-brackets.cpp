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

void LoadModuleConfiguration()
{
    auto& policy = PopulationPolicy::Instance();
    auto& reg = ActivityRegistry::Instance();
    auto& diag = DiagnosticManager::Instance();
    auto& prot = ProtectionCache::Instance();

    // 1. Core Orchestration Settings
    policy.SetEnabled(sConfig.GetBoolDefault("BotLevelBrackets.Enabled", true));
    policy.SetDryRun(sConfig.GetBoolDefault("BotLevelBrackets.Orchestration.DryRun", false));
    policy.SetCheckCadenceSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.CheckSeconds", 90)));
    policy.SetMaxRequestsPerCycle(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.MaxRequestsPerCycle", 5)));
    policy.SetMaxOutstandingPerFaction(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.MaxOutstandingPerFaction", 10)));
    policy.SetMaxLooseRequestsPerCycle(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.MaxLooseRequestsPerCycle", 1)));
    policy.SetRequestCooldownSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.RequestCooldownSeconds", 1800)));
    policy.SetRejectionBackoffSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.RejectionBackoffSeconds", 600)));
    policy.SetStabilitySeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.StabilitySeconds", 60)));
    policy.SetDispatchTimeoutSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.DispatchTimeoutSeconds", 30)));
    policy.SetEarlyDepartureBackoffSeconds(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Orchestration.EarlyDepartureBackoffSeconds", 3600)));

    // 2. City Orchestration Settings
    policy.SetCityEnabled(sConfig.GetBoolDefault("BotLevelBrackets.City.Enabled", true));

    std::string cityDestStr = sConfig.GetStringDefault("BotLevelBrackets.City.DestinationPolicy", "RequireExact");
    policy.SetCityDestinationPolicy((cityDestStr == "AllowNativeChoice") ? DestinationPolicy::AllowNativeChoice : DestinationPolicy::RequireExact);

    policy.SetCityQuotaMode(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.QuotaMode", 2)));
    policy.SetCityMinLimitEnabled(sConfig.GetBoolDefault("BotLevelBrackets.City.MinLimitEnabled", false));
    policy.SetCityMaxLimitEnabled(sConfig.GetBoolDefault("BotLevelBrackets.City.MaxLimitEnabled", true));
    policy.SetCityLimitsMode(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.LimitsMode", 1)));
    policy.SetCityMinBots(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MinBotsPerCity", 0)));
    policy.SetCityMaxBots(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MaxBotsPerCity", 60)));
    policy.SetCityHysteresis(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.HysteresisBots", 2)));
    policy.SetCityTransitMode(static_cast<TransitMode>(sConfig.GetIntDefault("BotLevelBrackets.City.TransitMode", 2)));
    policy.SetCityPickAllBrackets(sConfig.GetBoolDefault("BotLevelBrackets.City.PickAllBrackets", true));
    policy.SetCityMinLevel(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MinLevel", 10)));
    policy.SetCityDwellDurationMs(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.DwellDurationMs", 300000)));
    policy.SetCityMinDwellDurationMs(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MinDwellDurationMs", 300000)));
    policy.SetCityMaxDwellDurationMs(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.City.MaxDwellDurationMs", 600000)));

    policy.SetCityTarget(0, sConfig.GetIntDefault("BotLevelBrackets.City.StormwindTarget", 10));
    policy.SetCityTarget(1, sConfig.GetIntDefault("BotLevelBrackets.City.IronforgeTarget", 8));
    policy.SetCityTarget(2, sConfig.GetIntDefault("BotLevelBrackets.City.DarnassusTarget", 6));
    policy.SetCityTarget(3, sConfig.GetIntDefault("BotLevelBrackets.City.AlahThalasTarget", 6));
    policy.SetCityTarget(4, sConfig.GetIntDefault("BotLevelBrackets.City.OrgrimmarTarget", 10));
    policy.SetCityTarget(5, sConfig.GetIntDefault("BotLevelBrackets.City.UndercityTarget", 8));
    policy.SetCityTarget(6, sConfig.GetIntDefault("BotLevelBrackets.City.ThunderBluffTarget", 6));
    policy.SetCityTarget(7, sConfig.GetIntDefault("BotLevelBrackets.City.SparkwaterTarget", 6));

    auto loadCityLoc = [&](CityId cId, const char* key, const char* defaultVal) {
        std::string locStr = sConfig.GetStringDefault(key, defaultVal);
        TeleportDestinationConfig dest;
        if (NativeTaskAdapter::ResolveTeleportLocation(locStr, dest))
            policy.SetCityLandingDestination(cId, dest);
    };

    loadCityLoc(CityId::Stormwind, "BotLevelBrackets.City.Stormwind.Location", "0 -8935.27 613.13 99.61 0.45");
    loadCityLoc(CityId::Ironforge, "BotLevelBrackets.City.Ironforge.Location", "0 -4886.55 -997.59 504.02 2.23");
    loadCityLoc(CityId::Darnassus, "BotLevelBrackets.City.Darnassus.Location", "1 9938.84 2521.53 1317.66 4.38");
    loadCityLoc(CityId::AlahThalas, "BotLevelBrackets.City.AlahThalas.Location", "0 4261.59 -2842.02 8.45 0.08");
    loadCityLoc(CityId::Orgrimmar, "BotLevelBrackets.City.Orgrimmar.Location", "1 1627.32 -4376.07 11.81 3.44");
    loadCityLoc(CityId::Undercity, "BotLevelBrackets.City.Undercity.Location", "0 1599.96 240.65 -52.06 0.09");
    loadCityLoc(CityId::ThunderBluff, "BotLevelBrackets.City.ThunderBluff.Location", "1 -1257.63 19.56 128.27 1.73");
    loadCityLoc(CityId::Sparkwater, "BotLevelBrackets.City.Sparkwater.Location", "1 838.27 -5048.73 6.03 1.92");

    // 3. Outdoor World Distribution Settings
    policy.SetWorldEnabled(sConfig.GetBoolDefault("BotLevelBrackets.World.Enabled", true));
    std::string worldDestStr = sConfig.GetStringDefault("BotLevelBrackets.World.DestinationPolicy", "RequireExact");
    policy.SetWorldDestinationPolicy((worldDestStr == "AllowNativeChoice") ? DestinationPolicy::AllowNativeChoice : DestinationPolicy::RequireExact);
    policy.SetFieldReservePct(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.World.FieldReservePct", 70)));

    // 4. Level & Population Policy Settings & Active Redistribution
    policy.SetLevelDistributionEnabled(sConfig.GetBoolDefault("BotLevelBrackets.LevelDistribution.Enabled", true));
    policy.SetDistributionCheckFrequency(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.LevelDistribution.CheckFrequency", 300)));
    policy.SetCheckFlaggedFrequency(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.LevelDistribution.CheckFlaggedFrequency", 15)));
    policy.SetFlaggedProcessLimit(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.LevelDistribution.FlaggedProcessLimit", 5)));
    policy.SetMinBracketChangeThreshold(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.LevelDistribution.MinBracketChangeThreshold", 2)));
    policy.SetMaxPendingQueueSize(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.LevelDistribution.MaxPendingQueueSize", 0)));
    policy.SetPendingQueueTTL(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.LevelDistribution.PendingQueueTTL", 0)));
    policy.SetTeleportOnLevelChange(sConfig.GetBoolDefault("BotLevelBrackets.TeleportOnLevelChange", true));
    policy.SetArrivalFloorHoldEnabled(sConfig.GetBoolDefault("BotLevelBrackets.Teleport.ArrivalFloorHold", true));
    policy.SetArrivalHoldDurationMs(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Teleport.ArrivalHoldDurationMs", 5000)));
    policy.SetMaxFloorLifts(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Teleport.MaxFloorLifts", 6)));

    // Starting-Zone and Wrong-Map Dispersal Settings
    policy.SetStartingZoneDisperseEnabled(sConfig.GetBoolDefault("BotLevelBrackets.StartingZoneDisperse.Enabled", true));
    policy.SetStartingZoneDisperseFrequency(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.StartingZoneDisperse.Frequency", 15)));
    policy.SetWrongMapDisperseEnabled(sConfig.GetBoolDefault("BotLevelBrackets.WrongMapDisperse.Enabled", true));
    policy.SetWrongMapDisperseFrequency(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.WrongMapDisperse.Frequency", 60)));

    policy.SetUseDynamicDistribution(sConfig.GetBoolDefault("BotLevelBrackets.LevelPolicy.UseDynamic", false));
    policy.SetRealPlayerWeight(sConfig.GetFloatDefault("BotLevelBrackets.LevelPolicy.HumanWeight", 1.0f));
    policy.SetSyncFactions(sConfig.GetBoolDefault("BotLevelBrackets.LevelPolicy.SyncFactions", false));

    // 5. Explicit Level Bracket Definitions (Alliance and Horde)
    uint32_t numRanges = static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.NumRanges", 7));
    (void)numRanges;

    std::vector<LevelBracketConfig> allianceBrackets;
    allianceBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range1.Lower", 1)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range1.Upper", 9)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range1.Pct", 16))
    });
    allianceBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range2.Lower", 10)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range2.Upper", 19)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range2.Pct", 14))
    });
    allianceBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range3.Lower", 20)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range3.Upper", 29)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range3.Pct", 14))
    });
    allianceBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range4.Lower", 30)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range4.Upper", 39)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range4.Pct", 14))
    });
    allianceBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range5.Lower", 40)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range5.Upper", 49)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range5.Pct", 14))
    });
    allianceBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range6.Lower", 50)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range6.Upper", 59)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range6.Pct", 14))
    });
    allianceBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range7.Lower", 60)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range7.Upper", 60)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Alliance.Range7.Pct", 14))
    });
    if (numRanges > 0 && numRanges < allianceBrackets.size())
        allianceBrackets.resize(numRanges);
    policy.SetLevelBrackets(0, allianceBrackets);

    std::vector<LevelBracketConfig> hordeBrackets;
    hordeBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range1.Lower", 1)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range1.Upper", 9)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range1.Pct", 16))
    });
    hordeBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range2.Lower", 10)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range2.Upper", 19)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range2.Pct", 14))
    });
    hordeBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range3.Lower", 20)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range3.Upper", 29)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range3.Pct", 14))
    });
    hordeBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range4.Lower", 30)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range4.Upper", 39)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range4.Pct", 14))
    });
    hordeBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range5.Lower", 40)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range5.Upper", 49)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range5.Pct", 14))
    });
    hordeBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range6.Lower", 50)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range6.Upper", 59)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range6.Pct", 14))
    });
    hordeBrackets.push_back({
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range7.Lower", 60)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range7.Upper", 60)),
        static_cast<uint8_t>(sConfig.GetIntDefault("BotLevelBrackets.Horde.Range7.Pct", 14))
    });
    if (numRanges > 0 && numRanges < hordeBrackets.size())
        hordeBrackets.resize(numRanges);
    policy.SetLevelBrackets(1, hordeBrackets);
    policy.ClampAndBalanceBrackets(true);

    // Starter race leveling destination pools (Levels 1-9)
    auto loadStarterPool = [&](uint8_t race, const char* key, const char* defaultPool) {
        std::string poolStr = sConfig.GetStringDefault(key, defaultPool);
        auto pool = NativeTaskAdapter::ParseTeleportLocationPool(poolStr);
        policy.SetStarterLocationPool(race, pool);
    };

    loadStarterPool(RACE_HUMAN, "BotLevelBrackets.Starter.Human.Locations", "northshire");
    loadStarterPool(RACE_DWARF, "BotLevelBrackets.Starter.Dwarf.Locations", "coldridge");
    loadStarterPool(RACE_GNOME, "BotLevelBrackets.Starter.Gnome.Locations", "coldridge");
    loadStarterPool(RACE_NIGHTELF, "BotLevelBrackets.Starter.NightElf.Locations", "shadowglen");
    loadStarterPool(RACE_HIGH_ELF, "BotLevelBrackets.Starter.HighElf.Locations", "highelf");
    loadStarterPool(RACE_ORC, "BotLevelBrackets.Starter.Orc.Locations", "valleyoftrials");
    loadStarterPool(RACE_TROLL, "BotLevelBrackets.Starter.Troll.Locations", "valleyoftrials");
    loadStarterPool(RACE_TAUREN, "BotLevelBrackets.Starter.Tauren.Locations", "redcloud");
    loadStarterPool(RACE_UNDEAD, "BotLevelBrackets.Starter.Undead.Locations", "deathknell");
    loadStarterPool(RACE_GOBLIN, "BotLevelBrackets.Starter.Goblin.Locations", "blackstone");

    // Leveling zone destination pools (Turtle WoW Zone Level Chart)
    auto loadRangePool = [&](uint8_t team, size_t rangeIndex, const char* key, const char* defaultPool) {
        std::string poolStr = sConfig.GetStringDefault(key, defaultPool);
        auto pool = NativeTaskAdapter::ParseTeleportLocationPool(poolStr);
        policy.SetRangeLocationPool(team, rangeIndex, pool);
    };

    loadRangePool(0, 0, "BotLevelBrackets.Alliance.Range1.Locations", "goldshire");
    loadRangePool(0, 1, "BotLevelBrackets.Alliance.Range2.Locations", "westfall, thelsamar, darkshore");
    loadRangePool(0, 2, "BotLevelBrackets.Alliance.Range3.Locations", "redridge, duskwood, wetlands, ashenvale, balorhorde");
    loadRangePool(0, 3, "BotLevelBrackets.Alliance.Range4.Locations", "rebelcamp, refugepointe, southshore, alteracmountains, gilneas");
    loadRangePool(0, 4, "BotLevelBrackets.Alliance.Range5.Locations", "feathermoon, gadgetzan, aeriepeak, swampofsorrows, icepointrock, lapidisisle");
    loadRangePool(0, 5, "BotLevelBrackets.Alliance.Range6.Locations", "chillwind, lightshopechapel, winterspring, felwood, morgansvigil, thoriumpoint, ungoro, blastedlands, telabim");
    loadRangePool(0, 6, "BotLevelBrackets.Alliance.Range7.Locations", "lightshopechapel, silithus, winterspring, chillwind, stormwind");

    loadRangePool(1, 0, "BotLevelBrackets.Horde.Range1.Locations", "valleyoftrials");
    loadRangePool(1, 1, "BotLevelBrackets.Horde.Range2.Locations", "thecrossroads, silverpineforest, razorhill");
    loadRangePool(1, 2, "BotLevelBrackets.Horde.Range3.Locations", "camptaurajo, tarrenmill, ashenvale, stonetalonmountains, balorhorde");
    loadRangePool(1, 3, "BotLevelBrackets.Horde.Range4.Locations", "gromgol, hammerfall, ghostwalker, freewindpost, kargath, gilneas");
    loadRangePool(1, 4, "BotLevelBrackets.Horde.Range5.Locations", "campmojache, gadgetzan, hinterlands, stonard, icepointrock, lapidisisle");
    loadRangePool(1, 5, "BotLevelBrackets.Horde.Range6.Locations", "ungoro, lightshopechapel, westernplaguelands, winterspring, felwood, flamecrest, thoriumpoint, silithus, telabim");
    loadRangePool(1, 6, "BotLevelBrackets.Horde.Range7.Locations", "lightshopechapel, silithus, winterspring, ungoro, orgrimmar");

    // 6. Social & Identity Protections
    prot.SetIgnoreFriendListed(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreFriendListed", true));
    prot.SetIgnoreRealPlayerGuilds(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreRealPlayerGuilds", true));
    prot.SetIgnoreArenaTeams(sConfig.GetBoolDefault("BotLevelBrackets.Protection.IgnoreArenaTeams", true));
    prot.SetSocialRefreshCadence(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Protection.SocialRefreshSeconds", 300)));
    prot.SetGuildTrackerCadence(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Protection.GuildTrackerUpdateSeconds", 600)));

    std::string exclNames = sConfig.GetStringDefault("BotLevelBrackets.Protection.ExcludedBotNames", "");
    if (exclNames.empty())
        exclNames = sConfig.GetStringDefault("BotLevelBrackets.ExcludeNames", "");
    prot.SetExcludedNames(exclNames);

    // 7. Activity Registry Controls
    reg.SetActivitiesEnabled(sConfig.GetBoolDefault("BotLevelBrackets.Activities.Enabled", true));
    reg.SetRequireSupported(sConfig.GetBoolDefault("BotLevelBrackets.Activities.RequireSupported", false));

    auto loadAct = [&reg](const std::string& id)
    {
        ActivityDefinition* a = reg.GetActivity(id);
        if (!a)
            return;
        std::string prefix = "BotLevelBrackets.Activities." + id + ".";
        a->enabled = sConfig.GetBoolDefault((prefix + "Enabled").c_str(), a->enabled);
        a->cityEnabled = sConfig.GetBoolDefault((prefix + "CityEnabled").c_str(), a->cityEnabled);
        a->worldEnabled = sConfig.GetBoolDefault((prefix + "WorldEnabled").c_str(), a->worldEnabled);
        a->weight = sConfig.GetFloatDefault((prefix + "Weight").c_str(), a->weight);
        a->cooldownSeconds = static_cast<uint32_t>(sConfig.GetIntDefault((prefix + "CooldownSeconds").c_str(), a->cooldownSeconds));
        a->maxOutstandingPerFaction = static_cast<uint32_t>(sConfig.GetIntDefault((prefix + "MaxOutstandingPerFaction").c_str(), a->maxOutstandingPerFaction));
    };

    loadAct("QuestAccept");
    loadAct("QuestObjective");
    loadAct("QuestTurnIn");
    loadAct("Grinding");
    loadAct("Exploration");
    loadAct("GenericRpg");
    loadAct("VendorSell");
    loadAct("VendorBuy");
    loadAct("Repair");
    loadAct("ClassTraining");
    loadAct("ProfessionTraining");
    loadAct("AuctionSell");
    loadAct("Mailbox");
    loadAct("FlightDiscovery");

    // 7. Debug Settings
    std::string debugModeStr = sConfig.GetStringDefault("BotLevelBrackets.Debug.Mode", "Off");
    if (debugModeStr == "Summary")
        diag.SetDebugMode(DebugMode::Summary);
    else if (debugModeStr == "Trace")
        diag.SetDebugMode(DebugMode::Trace);
    else if (debugModeStr == "DryRun")
        diag.SetDebugMode(DebugMode::DryRun);
    else
        diag.SetDebugMode(DebugMode::Off);

    diag.SetMaxEventsPerSecond(static_cast<uint32_t>(sConfig.GetIntDefault("BotLevelBrackets.Debug.MaxEventsPerSecond", 50)));
    diag.SetRingBufferCapacity(static_cast<size_t>(sConfig.GetIntDefault("BotLevelBrackets.Debug.RingBufferEvents", 2000)));

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
