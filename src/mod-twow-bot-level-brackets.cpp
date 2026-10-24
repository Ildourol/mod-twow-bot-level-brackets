#include "mod-twow-bot-level-brackets.h"
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
#include <cmath>
#include <utility>
#include <limits>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <ctime>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -----------------------------------------------------------------------------
// DATA STRUCTURES
// -----------------------------------------------------------------------------

enum TeleportSource : uint8
{
    TELEPORT_SOURCE_LEVEL_BRACKET = 0,
    TELEPORT_SOURCE_STARTING_ZONE = 1,
    TELEPORT_SOURCE_WRONG_MAP     = 2,
    TELEPORT_SOURCE_HUB           = 3
};

struct LevelRangeConfig
{
    uint8 lower;          ///< Lower bound (inclusive)
    uint8 upper;          ///< Upper bound (inclusive)
    uint8 desiredPercent; ///< Desired percentage of bots in this range
};

struct PendingResetEntry
{
    ObjectGuid botGuid;
    int        targetRange;
    uint8      teamId;
    uint32     enqueuedAt;
};

struct PendingTeleportEntry
{
    ObjectGuid     botGuid;
    uint8          newLevel;
    uint8          teamId;
    uint32         enqueuedAt;
    bool           useHubDest;
    uint32         destMapId;
    float          destX;
    float          destY;
    float          destZ;
    float          destO;
    TeleportSource source{TELEPORT_SOURCE_LEVEL_BRACKET};
};

struct TeleportDestination
{
    uint32 mapId;
    float x, y, z, o;
};

struct HubArea
{
    uint32 mapId;
    float  cx, cy, cz;
    float  radius;
    uint8  teamId;
    uint8  maxLevel;
    uint8  minLevel;
};

// -----------------------------------------------------------------------------
// MODULE CONFIGURATION & RUNTIME STATE
// -----------------------------------------------------------------------------

static uint32 g_NumRanges = 7;
static uint8  g_RandomBotMinLevel = 1;
static uint8  g_RandomBotMaxLevel = 60;
static uint32 g_ConfigGeneration  = 0;

static bool   g_BotLevelBracketsEnabled           = true;
static bool   g_IgnoreGuildBotsWithRealPlayers   = true;
static bool   g_IgnoreArenaTeamBots              = true;
static bool   g_BotDistFullDebugMode             = false;
static bool   g_BotDistLiteDebugMode             = false;
static uint32 g_BotDistCheckFrequency            = 300; // in seconds
static uint32 g_BotDistFlaggedCheckFrequency     = 15;  // in seconds
static uint32 g_GuildTrackerUpdateFrequency      = 600; // in seconds (10 minutes)
static uint32 g_SocialListRefreshFrequency       = 300; // in seconds
static bool   g_UseDynamicDistribution           = false;
static bool   g_IgnoreFriendListed               = true;
static uint32 g_FlaggedProcessLimit              = 5;   // 0 = unlimited
static uint32 g_MaxPendingQueueSize              = 0;   // 0 = unlimited
static uint32 g_PendingQueueTTL                  = 0;   // 0 = disabled
static uint32 g_MinBracketChangeThreshold        = 0;   // 0 = disabled
static float  g_RealPlayerWeight                 = 1.0f;
static bool   g_SyncFactions                     = false;
static bool   g_TeleportOnLevelChange            = true;

static bool   g_HubDisperseEnabled               = true;
static uint32 g_HubDisperseQuotaMode             = 2;  // 1 = Total Number (MaxBotsPerHub), 2 = Percentage (per-city percentages)
static uint32 g_HubDisperseBotsPerCycle          = 50;
static uint32 g_HubDisperseMaxBotsPerHub         = 50;
static uint32 g_HubDisperseMaxBotsPerBracket     = 0;  // 0 = disabled / off
static uint32 g_HubDisperseMinBotsPerBracket     = 0;  // 0 = disabled / off
static uint32 g_HubPopulateFrequency             = 30;
static uint32 g_StartingZoneDisperseFrequency    = 15;
static uint32 g_WrongMapDisperseFrequency        = 60;
static int32  g_HubCityPcts[8]                   = { 7, 5, 4, 4, 7, 5, 4, 4 };
static uint32 g_HubDisperseHysteresisBots        = 2;
static uint32 g_HubDisperseStabilitySeconds      = 60;
static uint32 g_HubDisperseCooldownSeconds       = 180;
static bool        g_ManageCityStrategies          = true;
static std::string g_CityStrategies                = "+rpg,+rpg quest,+rpg vendor,+rpg explore,+rpg maintenance,+rpg craft,+emote,+sit,+custom::say";
static std::string g_CityStripStrategies           = "-grind,-wander,-loot,-tfish";
static bool        g_WorldTeleportResetStrategies  = true;

enum class HubImbalanceDirection : uint8
{
    Neutral = 0,
    Underpopulated = 1,
    Overpopulated = 2
};

struct HubStabilityRecord
{
    HubImbalanceDirection currentDirection{HubImbalanceDirection::Neutral};
    int64                 imbalanceStartTime{0};
    int32                 lastEffectivePopulation{0};
};

static HubStabilityRecord g_HubStability[8];

struct BotHubCooldownRecord
{
    int64 lastRelocationTime{0};
};

static std::unordered_map<ObjectGuid, BotHubCooldownRecord> g_HubBotCooldowns;
static std::mutex                                          g_HubCooldownMutex;
static std::atomic<uint64>                                 g_StatsCooldownSkips{0};

static inline int64 GetSteadySeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool IsBotOnHubCooldown(const ObjectGuid& guid, int64 now)
{
    if (g_HubDisperseCooldownSeconds == 0)
        return false;

    std::lock_guard<std::mutex> lock(g_HubCooldownMutex);
    auto it = g_HubBotCooldowns.find(guid);
    if (it == g_HubBotCooldowns.end())
        return false;

    return (now - it->second.lastRelocationTime < static_cast<int64>(g_HubDisperseCooldownSeconds));
}

// Configured level ranges
static std::vector<LevelRangeConfig> g_AllianceLevelRanges;
static std::vector<LevelRangeConfig> g_HordeLevelRanges;
static std::vector<uint8>             g_AllianceStaticPcts;
static std::vector<uint8>             g_HordeStaticPcts;

// Cached social data
static std::unordered_set<uint32>                            g_SocialFriendsList;
static std::vector<std::string>                              g_ExcludeBotNames;
static std::unordered_set<uint32>                            g_RealPlayerGuildIds;
static std::unordered_set<uint32>                            g_PersistentRealPlayerGuildIds;
static std::mutex                                            g_SocialDataMutex;

// Pending queues
static std::unordered_map<ObjectGuid, PendingResetEntry>     g_PendingLevelResets;
static std::mutex                                            g_PendingLevelResetsMutex;
static std::unordered_map<ObjectGuid, PendingTeleportEntry>   g_PendingTeleports;
static std::mutex                                            g_PendingTeleportsMutex;

// Runtime atomics and cumulative stats
static std::atomic<bool>                                     g_DistributionInProgress{false};
static std::atomic<uint64>                                   g_StatsBotsAdjusted{0};
static std::atomic<uint64>                                   g_StatsBotsTeleported{0};
static std::atomic<uint64>                                   g_StatsBotsSkipped{0};
static std::atomic<uint64>                                   g_StatsCyclesRun{0};

// Forward declarations
static bool IsAlliancePlayerBot(Player* bot);
static bool IsHordePlayerBot(Player* bot);
static void ClampAndBalanceBrackets(bool warnLayout = true);
static void RunDistributionCycle(ChatHandler* handler = nullptr);
static void CleanupGuildTracker();

// -----------------------------------------------------------------------------
// VANILLA / TURTLE WOW LEVELING ZONES & HUBS
// -----------------------------------------------------------------------------

static const TeleportDestination s_AllianceZones[] =
{
    { 0,  -9464.0f,    62.0f,  56.0f, 0.0f },     // 1-9:   Elwynn Forest (Goldshire)
    { 0, -10515.0f,  1070.0f,  55.0f, 0.0f },     // 10-19: Westfall (Sentinel Hill)
    { 0,  -9250.0f, -2160.0f,  64.0f, 0.0f },     // 20-29: Redridge Mountains (Lakeshire)
    { 0, -11340.0f,  -200.0f,  75.0f, 0.0f },     // 30-39: Northern STV (Rebel Camp)
    { 1,  -4380.0f,  2060.0f,  49.0f, 0.0f },     // 40-49: Feralas (Feathermoon Stronghold)
    { 0,    940.0f, -1450.0f,  63.0f, 0.0f },     // 50-59: Western Plaguelands (Chillwind Camp)
    { 0,  -8810.0f,   640.0f,  94.0f, 0.0f }      // 60:    Stormwind City
};
static constexpr size_t s_NumAllianceZones = sizeof(s_AllianceZones) / sizeof(s_AllianceZones[0]);

static const TeleportDestination s_HordeZones[] =
{
    { 1,   -618.0f, -4251.0f,  38.0f, 0.0f },     // 1-9:   Durotar (Valley of Trials)
    { 1,    310.0f, -4680.0f,  16.0f, 0.0f },     // 10-19: Durotar (Razor Hill)
    { 1,   -440.0f, -2650.0f,  96.0f, 0.0f },     // 20-29: The Barrens (Crossroads)
    { 0,    -24.0f,  -870.0f,  55.0f, 0.0f },     // 30-39: Hillsbrad Foothills (Tarren Mill)
    { 1,  -4400.0f,   190.0f,  25.0f, 0.0f },     // 40-49: Feralas (Camp Mojache)
    { 1,  -7180.0f, -1070.0f, -188.0f, 0.0f },    // 50-59: Un'Goro Crater (Entrance)
    { 1,   1570.0f, -4400.0f,   8.0f, 0.0f }      // 60:    Orgrimmar
};
static constexpr size_t s_NumHordeZones = sizeof(s_HordeZones) / sizeof(s_HordeZones[0]);

static const HubArea s_HubAreas[] =
{
    { 0, -8810.0f,   640.0f,   94.0f, 200.0f, TEAM_ALLIANCE, 0, 0 }, // Stormwind City
    { 0, -4980.0f,  -940.0f,  501.0f, 150.0f, TEAM_ALLIANCE, 0, 0 }, // Ironforge
    { 1,  9950.0f,  2330.0f, 1330.0f, 150.0f, TEAM_ALLIANCE, 0, 0 }, // Darnassus
    { 0,  3212.6f, -2501.4f,  111.7f, 180.0f, TEAM_ALLIANCE, 0, 0 }, // Alah'Thalas (High Elf Capital - Turtle WoW)
    { 1,  1570.0f, -4400.0f,    8.0f, 200.0f, TEAM_HORDE,    0, 0 }, // Orgrimmar
    { 0,  1620.0f,   240.0f,   60.0f, 150.0f, TEAM_HORDE,    0, 0 }, // Undercity
    { 1, -1290.0f,   150.0f,  130.0f, 150.0f, TEAM_HORDE,    0, 0 }, // Thunder Bluff
    { 1,   840.0f, -5060.0f,    6.0f, 180.0f, TEAM_HORDE,    0, 0 }  // Sparkwater Port (Goblin Capital - Turtle WoW)
};
static constexpr size_t s_NumHubAreas = sizeof(s_HubAreas) / sizeof(s_HubAreas[0]);

static const HubArea s_StartingAreas[] =
{
    { 0,  -8913.0f,  -133.0f,   80.0f, 250.0f, TEAM_ALLIANCE, 9, 0 }, // Northshire Valley (Human)
    { 0,  -6230.0f,   330.0f,  383.0f, 300.0f, TEAM_ALLIANCE, 9, 0 }, // Coldridge Valley (Dwarf/Gnome)
    { 1,  10330.0f,   830.0f, 1326.0f, 250.0f, TEAM_ALLIANCE, 9, 0 }, // Shadowglen (Night Elf)
    { 0,   3322.0f, -2448.0f,  107.2f, 250.0f, TEAM_ALLIANCE, 9, 0 }, // Thalassian Highlands (High Elf - Turtle WoW)
    { 1,   -620.0f, -4300.0f,   10.0f, 300.0f, TEAM_HORDE,    9, 0 }, // Valley of Trials (Orc/Troll)
    { 1,  -2900.0f, -1300.0f,   90.0f, 300.0f, TEAM_HORDE,    9, 0 }, // Red Cloud Mesa (Tauren)
    { 0,   2250.0f,   320.0f,   35.0f, 300.0f, TEAM_HORDE,    9, 0 }, // Deathknell (Undead)
    { 1,   -275.6f, -7474.6f,   73.2f, 250.0f, TEAM_HORDE,    9, 0 }  // Blackstone Island (Goblin - Turtle WoW)
};
static constexpr size_t s_NumStartingAreas = sizeof(s_StartingAreas) / sizeof(s_StartingAreas[0]);

// -----------------------------------------------------------------------------
// HELPER UTILITIES
// -----------------------------------------------------------------------------

static uint32 ReadBoundedUInt(char const* key, int32 defaultValue, uint32 minimum, uint32 maximum)
{
    int64 value = sConfig.GetIntDefault(key, defaultValue);
    if (value < static_cast<int64>(minimum))
        value = minimum;
    if (value > static_cast<int64>(maximum))
        value = maximum;
    return static_cast<uint32>(value);
}

static void ToLowerInPlace(std::string& value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
}

static bool IsPlayerBot(Player* player)
{
    return BotEngineAdapter::IsBot(player);
}

static bool IsPlayerRandomBot(Player* player)
{
    return BotEngineAdapter::IsRandomBot(player);
}

static bool IsAlliancePlayerBot(Player* bot)
{
    return bot && (bot->GetTeamId() == TEAM_ALLIANCE);
}

static bool IsHordePlayerBot(Player* bot)
{
    return bot && (bot->GetTeamId() == TEAM_HORDE);
}

static bool BotHasRealPlayerMaster(Player* bot)
{
    return BotEngineAdapter::BotHasRealPlayerMaster(bot);
}

static bool IsBotInProtectedDuelZone(Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return false;
    uint32 areaId = bot->GetAreaId();
    // Racial Capital Cities & Sanctuaries:
    // Stormwind City (1519), Orgrimmar (1637), Ironforge (1537), Undercity (1497),
    // Darnassus (1657), Thunder Bluff (1638),
    // Alah'Thalas (2040) / Thalassian Highlands (5225), Sparkwater Port (4021)
    if (areaId == 1519 || areaId == 1637 || areaId == 1537 || areaId == 1497 ||
        areaId == 1657 || areaId == 1638 || areaId == 2040 || areaId == 5225 ||
        areaId == 4021)
        return true;
    // Gurubashi Arena (Area 2177, 1741)
    if (bot->InGurubashiArena(false) || areaId == 2177 || areaId == 1741)
        return true;
    // Blood Ring Arena (Map 26)
    if (bot->InArena() || (bot->GetMap() && bot->GetMap()->GetId() == 26))
        return true;
    return false;
}

static bool BotInArena(Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return false;
    if (bot->InArena() || bot->InGurubashiArena(false) || bot->InBattleGround() || bot->InBattleGroundQueue())
        return true;
    if (bot->GetMap() && (bot->GetMap()->GetId() == 26 || bot->GetMap()->GetId() == 27))
        return true;
    return false;
}

static bool BotInArenaTeam(Player* bot)
{
    return BotInArena(bot);
}

static bool IsBotExcluded(Player* bot)
{
    if (!bot)
        return false;

    std::string name = bot->GetName();
    ToLowerInPlace(name);

    std::lock_guard<std::mutex> lock(g_SocialDataMutex);
    for (const auto& excluded : g_ExcludeBotNames)
    {
        if (excluded == name)
            return true;
    }
    return false;
}

static bool BotInGuildWithRealPlayer(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return false;

    uint32 guildId = bot->GetGuildId();
    if (guildId == 0)
        return false;

    std::lock_guard<std::mutex> lock(g_SocialDataMutex);
    return g_RealPlayerGuildIds.count(guildId) > 0 || g_PersistentRealPlayerGuildIds.count(guildId) > 0;
}

static bool BotInFriendList(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return false;

    std::lock_guard<std::mutex> lock(g_SocialDataMutex);
    return g_SocialFriendsList.count(bot->GetObjectGuid().GetCounter()) > 0;
}

static bool IsBotSafeForLevelReset(Player* bot)
{
    if (BotEngineAdapter::IsRandomLevelsDisabled() || !bot || !bot->GetSession() ||
        bot->GetSession()->isLogingOut())
        return false;

    if (!bot->IsInWorld() || !bot->IsAlive())
        return false;

    if (bot->IsInCombat())
        return false;

    if (BotInArena(bot))
        return false;

    if (bot->IsTaxiFlying())
        return false;

    if (bot->IsBeingTeleported())
        return false;

    if (bot->GetTrader() || !bot->GetLootGuid().IsEmpty())
        return false;

    if (bot->GetCurrentSpell(CURRENT_GENERIC_SPELL) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    if (IsBotInProtectedDuelZone(bot))
        return false;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsInWorld() && !IsPlayerBot(member))
                return false;
        }
    }

    if (BotHasRealPlayerMaster(bot))
        return false;

    return true;
}

static bool IsBotSafeForTeleport(Player* bot)
{
    if (!bot || !bot->GetSession() || bot->GetSession()->isLogingOut())
        return false;

    if (!bot->IsInWorld() || !bot->IsAlive())
        return false;

    if (bot->IsInCombat())
        return false;

    if (BotInArena(bot))
        return false;

    if (bot->IsTaxiFlying())
        return false;

    if (bot->IsBeingTeleported())
        return false;

    if (bot->GetTrader() || !bot->GetLootGuid().IsEmpty())
        return false;

    if (bot->GetCurrentSpell(CURRENT_GENERIC_SPELL) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsInWorld() && !IsPlayerBot(member))
                return false;
        }
    }

    if (BotHasRealPlayerMaster(bot))
        return false;

    return true;
}

static uint32 GetHubQuota(size_t hubIdx, uint32 factionTotal)
{
    if (g_HubDisperseQuotaMode == 1) // Mode 1: Total number (absolute headcount)
    {
        return g_HubDisperseMaxBotsPerHub;
    }

    // Mode 2: Percentage of total faction bots
    if (hubIdx >= s_NumHubAreas || factionTotal == 0)
        return 0;

    int32 configuredPct = g_HubCityPcts[hubIdx];
    if (configuredPct <= 0)
        return 0;

    uint32 pct = static_cast<uint32>(configuredPct);

    // Standard rounding: (factionTotal * pct + 50) / 100
    uint32 quota = (factionTotal * pct + 50) / 100;
    return std::min<uint32>(quota, factionTotal);
}

static bool ShouldExcludeBot(Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return true;
    if (!IsPlayerBot(bot) || !IsPlayerRandomBot(bot))
        return true;
    if (IsBotExcluded(bot))
        return true;
    if (g_IgnoreGuildBotsWithRealPlayers && BotInGuildWithRealPlayer(bot))
        return true;
    if (g_IgnoreFriendListed && BotInFriendList(bot))
        return true;
    if (g_IgnoreArenaTeamBots && BotInArenaTeam(bot))
        return true;
    if (BotHasRealPlayerMaster(bot))
        return true;
    return false;
}

// -----------------------------------------------------------------------------
// CONFIGURATION LOADER
// -----------------------------------------------------------------------------

static void LoadBotLevelBracketsConfig()
{
    g_BotLevelBracketsEnabled           = sConfig.GetBoolDefault("BotLevelBrackets.Enabled", true);
    g_IgnoreGuildBotsWithRealPlayers   = sConfig.GetBoolDefault("BotLevelBrackets.IgnoreGuildBotsWithRealPlayers", true);
    g_IgnoreArenaTeamBots              = sConfig.GetBoolDefault("BotLevelBrackets.IgnoreArenaTeamBots", true);
    g_BotDistFullDebugMode             = sConfig.GetBoolDefault("BotLevelBrackets.FullDebugMode", false);
    g_BotDistLiteDebugMode             = sConfig.GetBoolDefault("BotLevelBrackets.LiteDebugMode", false);
    g_BotDistCheckFrequency            = ReadBoundedUInt("BotLevelBrackets.CheckFrequency", 300, 1, 86400);
    g_BotDistFlaggedCheckFrequency     = ReadBoundedUInt("BotLevelBrackets.CheckFlaggedFrequency", 15, 1, 86400);
    g_GuildTrackerUpdateFrequency      = ReadBoundedUInt("BotLevelBrackets.GuildTrackerUpdateFrequency", 600, 1, 86400);
    g_SocialListRefreshFrequency       = ReadBoundedUInt("BotLevelBrackets.SocialListRefreshFrequency", 300, 1, 86400);
    g_UseDynamicDistribution           = sConfig.GetBoolDefault("BotLevelBrackets.Dynamic.UseDynamicDistribution", false);
    g_RealPlayerWeight                 = std::max(0.0f, std::min(100.0f,
        sConfig.GetFloatDefault("BotLevelBrackets.Dynamic.RealPlayerWeight", 1.0f)));
    g_SyncFactions                     = sConfig.GetBoolDefault("BotLevelBrackets.Dynamic.SyncFactions", false);
    g_IgnoreFriendListed               = sConfig.GetBoolDefault("BotLevelBrackets.IgnoreFriendListed", true);
    g_FlaggedProcessLimit              = ReadBoundedUInt("BotLevelBrackets.FlaggedProcessLimit", 5, 0, 10000);
    g_MaxPendingQueueSize              = ReadBoundedUInt("BotLevelBrackets.MaxPendingQueueSize", 0, 0, 100000);
    g_PendingQueueTTL                  = ReadBoundedUInt("BotLevelBrackets.PendingQueueTTL", 0, 0, 86400);
    g_MinBracketChangeThreshold        = ReadBoundedUInt("BotLevelBrackets.MinBracketChangeThreshold", 0, 0, 1000);
    g_TeleportOnLevelChange            = sConfig.GetBoolDefault("BotLevelBrackets.TeleportOnLevelChange", true);

    g_HubDisperseEnabled               = sConfig.GetBoolDefault("BotLevelBrackets.HubDisperse.Enabled", true);
    g_HubDisperseQuotaMode             = ReadBoundedUInt("BotLevelBrackets.HubDisperse.QuotaMode", 2, 1, 2);
    g_HubDisperseBotsPerCycle          = ReadBoundedUInt("BotLevelBrackets.HubDisperse.BotsPerCycle", 50, 1, 500);
    g_HubDisperseMaxBotsPerHub         = ReadBoundedUInt("BotLevelBrackets.HubDisperse.MaxBotsPerHub", 50, 0, 500);
    g_HubDisperseMaxBotsPerBracket     = ReadBoundedUInt("BotLevelBrackets.HubDisperse.MaxBotsPerBracket", 0, 0, 500);
    g_HubDisperseMinBotsPerBracket     = ReadBoundedUInt("BotLevelBrackets.HubDisperse.MinBotsPerBracket", 0, 0, 500);

    if (g_HubDisperseMaxBotsPerBracket > 0 && g_HubDisperseMinBotsPerBracket > g_HubDisperseMaxBotsPerBracket)
    {
        sLog.outError("[BotLevelBrackets] WARNING: HubDisperse.MinBotsPerBracket (%u) exceeds MaxBotsPerBracket (%u). Clamping Min to Max.",
            g_HubDisperseMinBotsPerBracket, g_HubDisperseMaxBotsPerBracket);
        g_HubDisperseMinBotsPerBracket = g_HubDisperseMaxBotsPerBracket;
    }

    int32 swPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.StormwindPct", 7);
    int32 ifPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.IronforgePct", 5);
    int32 daPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.DarnassusPct", 4);
    int32 atPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.AlahThalasPct", 4);

    int32 orPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.OrgrimmarPct", 7);
    int32 ucPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.UndercityPct", 5);
    int32 tbPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.ThunderBluffPct", 4);
    int32 spPct = sConfig.GetIntDefault("BotLevelBrackets.HubDisperse.SparkwaterPct", 4);

    g_HubCityPcts[0] = (swPct >= 0) ? std::min(swPct, 100) : 0;
    g_HubCityPcts[1] = (ifPct >= 0) ? std::min(ifPct, 100) : 0;
    g_HubCityPcts[2] = (daPct >= 0) ? std::min(daPct, 100) : 0;
    g_HubCityPcts[3] = (atPct >= 0) ? std::min(atPct, 100) : 0;

    g_HubCityPcts[4] = (orPct >= 0) ? std::min(orPct, 100) : 0;
    g_HubCityPcts[5] = (ucPct >= 0) ? std::min(ucPct, 100) : 0;
    g_HubCityPcts[6] = (tbPct >= 0) ? std::min(tbPct, 100) : 0;
    g_HubCityPcts[7] = (spPct >= 0) ? std::min(spPct, 100) : 0;

    g_HubPopulateFrequency             = ReadBoundedUInt("BotLevelBrackets.HubPopulate.Frequency", 30, 1, 86400);
    g_StartingZoneDisperseFrequency    = ReadBoundedUInt("BotLevelBrackets.StartingZoneDisperse.Frequency", 15, 1, 86400);
    g_WrongMapDisperseFrequency        = ReadBoundedUInt("BotLevelBrackets.WrongMapDisperse.Frequency", 60, 1, 86400);
    g_HubDisperseHysteresisBots        = ReadBoundedUInt("BotLevelBrackets.HubDisperse.HysteresisBots", 2, 0, 100);
    g_HubDisperseStabilitySeconds      = ReadBoundedUInt("BotLevelBrackets.HubDisperse.StabilitySeconds", 60, 0, 3600);
    g_HubDisperseCooldownSeconds       = ReadBoundedUInt("BotLevelBrackets.HubDisperse.CooldownSeconds", 180, 0, 86400);
    g_ManageCityStrategies             = sConfig.GetBoolDefault("BotLevelBrackets.HubDisperse.ManageCityStrategies", true);
    g_CityStrategies                   = sConfig.GetStringDefault("BotLevelBrackets.HubDisperse.CityStrategies",
        "+rpg,+rpg quest,+rpg vendor,+rpg explore,+rpg maintenance,+rpg craft,+emote,+sit,+custom::say");
    g_CityStripStrategies              = sConfig.GetStringDefault("BotLevelBrackets.HubDisperse.CityStripStrategies",
        "-grind,-wander,-loot,-tfish");
    g_WorldTeleportResetStrategies     = sConfig.GetBoolDefault("BotLevelBrackets.WorldTeleport.ResetStrategies", true);

    for (size_t h = 0; h < s_NumHubAreas; ++h)
    {
        g_HubStability[h].currentDirection = HubImbalanceDirection::Neutral;
        g_HubStability[h].imbalanceStartTime = 0;
        g_HubStability[h].lastEffectivePopulation = 0;
    }

    std::string excludeNames = sConfig.GetStringDefault("BotLevelBrackets.ExcludeNames", "");
    {
        std::lock_guard<std::mutex> lock(g_SocialDataMutex);
        g_ExcludeBotNames.clear();
        std::istringstream f(excludeNames);
        std::string s;
        while (std::getline(f, s, ','))
        {
            s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; }), s.end());
            if (!s.empty())
            {
                ToLowerInPlace(s);
                g_ExcludeBotNames.push_back(s);
            }
        }
    }

    uint32 configuredMinLevel = BotEngineAdapter::GetConfigMinLevel();
    uint32 configuredMaxLevel = BotEngineAdapter::GetConfigMaxLevel();
    uint32 maxCfgLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    if (maxCfgLevel == 0)
        maxCfgLevel = 60;
    if (configuredMaxLevel == 0 || configuredMaxLevel > maxCfgLevel)
        configuredMaxLevel = maxCfgLevel;
    configuredMinLevel = std::min(configuredMinLevel, configuredMaxLevel);
    g_RandomBotMinLevel = static_cast<uint8>(std::min<uint32>(configuredMinLevel, 255));
    g_RandomBotMaxLevel = static_cast<uint8>(std::min<uint32>(configuredMaxLevel, 255));

    uint32 const maximumUsefulRanges = std::max<uint32>(1, g_RandomBotMaxLevel - g_RandomBotMinLevel + 1);
    g_NumRanges = ReadBoundedUInt("BotLevelBrackets.NumRanges", 7, 1, std::min<uint32>(60, maximumUsefulRanges));

    g_AllianceLevelRanges.resize(g_NumRanges);
    g_HordeLevelRanges.resize(g_NumRanges);

    for (uint32 i = 0; i < g_NumRanges; ++i)
    {
        std::string idx = std::to_string(i + 1);
        uint32 defaultLower = (i == 0 ? 1 : i * 10);
        uint32 defaultUpper = (i < g_NumRanges - 1 ? i * 10 + 9 : g_RandomBotMaxLevel);
        uint32 defaultPct   = (i == 0 ? 16 : 14);

        g_AllianceLevelRanges[i].lower = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Alliance.Range" + idx + ".Lower").c_str(), defaultLower, 0, 255));
        g_AllianceLevelRanges[i].upper = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Alliance.Range" + idx + ".Upper").c_str(), defaultUpper, 0, 255));
        g_AllianceLevelRanges[i].desiredPercent = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Alliance.Range" + idx + ".Pct").c_str(), defaultPct, 0, 100));

        g_HordeLevelRanges[i].lower = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Horde.Range" + idx + ".Lower").c_str(), defaultLower, 0, 255));
        g_HordeLevelRanges[i].upper = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Horde.Range" + idx + ".Upper").c_str(), defaultUpper, 0, 255));
        g_HordeLevelRanges[i].desiredPercent = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Horde.Range" + idx + ".Pct").c_str(), defaultPct, 0, 100));
    }

    if (g_SyncFactions)
    {
        for (uint32 i = 0; i < g_NumRanges; ++i)
        {
            if (g_AllianceLevelRanges[i].lower != g_HordeLevelRanges[i].lower ||
                g_AllianceLevelRanges[i].upper != g_HordeLevelRanges[i].upper)
            {
                sLog.outError("[BotLevelBrackets] WARNING: Bracket mismatch between factions at index %u (Alliance: %u-%u, Horde: %u-%u).",
                    i + 1, g_AllianceLevelRanges[i].lower, g_AllianceLevelRanges[i].upper,
                    g_HordeLevelRanges[i].lower, g_HordeLevelRanges[i].upper);
            }
        }
    }

    ClampAndBalanceBrackets();

    g_AllianceStaticPcts.resize(g_NumRanges);
    g_HordeStaticPcts.resize(g_NumRanges);
    for (uint32 i = 0; i < g_NumRanges; ++i)
    {
        g_AllianceStaticPcts[i] = g_AllianceLevelRanges[i].desiredPercent;
        g_HordeStaticPcts[i]    = g_HordeLevelRanges[i].desiredPercent;
    }

    ++g_ConfigGeneration;
}

// -----------------------------------------------------------------------------
// BRACKET NORMALIZATION & BALANCING
// -----------------------------------------------------------------------------

static void ClampAndBalanceBrackets(bool warnLayout)
{
    for (uint32 i = 0; i < g_NumRanges; ++i)
    {
        if (g_AllianceLevelRanges[i].lower < g_RandomBotMinLevel)
            g_AllianceLevelRanges[i].lower = g_RandomBotMinLevel;
        if (g_AllianceLevelRanges[i].upper > g_RandomBotMaxLevel)
            g_AllianceLevelRanges[i].upper = g_RandomBotMaxLevel;
        if (g_AllianceLevelRanges[i].lower > g_AllianceLevelRanges[i].upper)
            g_AllianceLevelRanges[i].desiredPercent = 0;

        if (g_HordeLevelRanges[i].lower < g_RandomBotMinLevel)
            g_HordeLevelRanges[i].lower = g_RandomBotMinLevel;
        if (g_HordeLevelRanges[i].upper > g_RandomBotMaxLevel)
            g_HordeLevelRanges[i].upper = g_RandomBotMaxLevel;
        if (g_HordeLevelRanges[i].lower > g_HordeLevelRanges[i].upper)
            g_HordeLevelRanges[i].desiredPercent = 0;
    }

    auto balancePercentages = [](std::vector<LevelRangeConfig>& ranges, const char* factionName)
    {
        uint32 total = 0;
        std::vector<uint32> valid;
        for (uint32 i = 0; i < g_NumRanges; ++i)
        {
            if (ranges[i].lower <= ranges[i].upper)
            {
                valid.push_back(i);
                total += ranges[i].desiredPercent;
            }
            else
                ranges[i].desiredPercent = 0;
        }

        if (valid.empty())
            return;

        std::vector<double> remainders(g_NumRanges, 0.0);
        uint32 assigned = 0;
        for (uint32 index : valid)
        {
            double raw = total > 0
                ? (static_cast<double>(ranges[index].desiredPercent) * 100.0 / total)
                : (100.0 / valid.size());
            uint32 base = static_cast<uint32>(std::floor(raw));
            ranges[index].desiredPercent = static_cast<uint8>(base);
            remainders[index] = raw - base;
            assigned += base;
        }

        std::stable_sort(valid.begin(), valid.end(), [&remainders](uint32 left, uint32 right)
        {
            return remainders[left] > remainders[right];
        });
        for (uint32 i = 0; assigned < 100; ++i, ++assigned)
            ++ranges[valid[i % valid.size()]].desiredPercent;

        if (total != 100 && (g_BotDistFullDebugMode || g_BotDistLiteDebugMode))
            sLog.outString("[BotLevelBrackets] Normalized %s bracket percentages from %u to 100.", factionName, total);
    };

    auto warnAboutLayout = [](std::vector<LevelRangeConfig> const& ranges, char const* factionName)
    {
        for (uint32 i = 1; i < ranges.size(); ++i)
        {
            if (ranges[i - 1].lower <= ranges[i - 1].upper && ranges[i].lower <= ranges[i].upper &&
                ranges[i].lower <= ranges[i - 1].upper)
            {
                sLog.outError("[BotLevelBrackets] WARNING: %s brackets %u and %u overlap; the first matching bracket wins.",
                    factionName, i, i + 1);
            }
        }
    };

    balancePercentages(g_AllianceLevelRanges, "Alliance");
    balancePercentages(g_HordeLevelRanges, "Horde");
    if (warnLayout)
    {
        warnAboutLayout(g_AllianceLevelRanges, "Alliance");
        warnAboutLayout(g_HordeLevelRanges, "Horde");
    }
}

// -----------------------------------------------------------------------------
// BRACKET LOOKUP & SELECTION
// -----------------------------------------------------------------------------

static int GetLevelRangeIndex(uint8 level, uint8 teamID)
{
    if (level < g_RandomBotMinLevel || level > g_RandomBotMaxLevel)
        return -1;

    static thread_local int8 allianceLut[256];
    static thread_local int8 hordeLut[256];
    static thread_local bool lutInit = false;
    static thread_local uint32 lutGeneration = 0;

    if (!lutInit || lutGeneration != g_ConfigGeneration)
    {
        std::fill_n(allianceLut, 256, int8(-1));
        std::fill_n(hordeLut, 256, int8(-1));
        for (uint32 i = 0; i < g_NumRanges; ++i)
        {
            for (uint32 lv = g_AllianceLevelRanges[i].lower; lv <= g_AllianceLevelRanges[i].upper && lv < 256; ++lv)
                allianceLut[lv] = static_cast<int8>(i);
            for (uint32 lv = g_HordeLevelRanges[i].lower; lv <= g_HordeLevelRanges[i].upper && lv < 256; ++lv)
                hordeLut[lv] = static_cast<int8>(i);
        }
        lutInit = true;
        lutGeneration = g_ConfigGeneration;
    }

    if (teamID == TEAM_ALLIANCE)
        return allianceLut[level];
    else if (teamID == TEAM_HORDE)
        return hordeLut[level];

    return -1;
}

static uint8 GetRandomLevelInRange(const LevelRangeConfig& range)
{
    return static_cast<uint8>(urand(range.lower, range.upper));
}

static std::vector<LevelRangeConfig> const* GetFactionRanges(Player const* bot)
{
    if (!bot)
        return nullptr;
    if (bot->GetTeamId() == TEAM_ALLIANCE)
        return &g_AllianceLevelRanges;
    if (bot->GetTeamId() == TEAM_HORDE)
        return &g_HordeLevelRanges;
    return nullptr;
}

// -----------------------------------------------------------------------------
// TELEPORTATION SYSTEM & CAPITAL HUB DISPERSAL
// -----------------------------------------------------------------------------

static bool TeleportBotToLevelZone(Player* bot, uint8 newLevel, uint8 teamId)
{
    if (!bot || !bot->IsInWorld() || !g_TeleportOnLevelChange)
        return false;

    const TeleportDestination* zones = nullptr;
    size_t zoneCount = 0;
    if (teamId == TEAM_ALLIANCE)
    {
        zones = s_AllianceZones;
        zoneCount = s_NumAllianceZones;
    }
    else if (teamId == TEAM_HORDE)
    {
        zones = s_HordeZones;
        zoneCount = s_NumHordeZones;
    }
    else
        return false;

    int rangeIndex = GetLevelRangeIndex(newLevel, teamId);
    if (rangeIndex < 0)
        rangeIndex = 0;

    // Racial starter zone awareness for level 1-9 (bracket 0)
    if (rangeIndex == 0)
    {
        uint8 race = bot->GetRace();
        TeleportDestination racialDest{0, 0.0f, 0.0f, 0.0f, 0.0f};
        bool hasRacialDest = true;

        switch (race)
        {
            case RACE_HIGH_ELF: // High Elf (Turtle WoW) - Thalassian Highlands
                racialDest = { 0, 3322.0f, -2448.0f, 107.2f, 3.47f };
                break;
            case RACE_GOBLIN: // Goblin (Turtle WoW) - Blackstone Island
                racialDest = { 1, -275.6f, -7474.6f, 73.2f, 2.51f };
                break;
            case RACE_HUMAN: // Human - Goldshire
                racialDest = { 0, -9464.0f, 62.0f, 56.0f, 0.0f };
                break;
            case RACE_DWARF:
            case RACE_GNOME: // Dwarf & Gnome - Kharanos
                racialDest = { 0, -5597.0f, -483.0f, 397.0f, 3.17f };
                break;
            case RACE_NIGHTELF: // Night Elf - Dolanaar
                racialDest = { 1, 9813.0f, 951.0f, 1308.0f, 0.0f };
                break;
            case RACE_ORC:
            case RACE_TROLL: // Orc & Troll - Razor Hill
                racialDest = { 1, 310.0f, -4680.0f, 16.0f, 4.16f };
                break;
            case RACE_TAUREN: // Tauren - Bloodhoof Village
                racialDest = { 1, -2323.0f, -399.0f, -9.0f, 0.0f };
                break;
            case RACE_UNDEAD: // Undead - Brill / Deathknell
                racialDest = { 0, 2250.0f, 320.0f, 35.0f, 0.0f };
                break;
            default:
                hasRacialDest = false;
                break;
        }

        if (hasRacialDest)
        {
            bool ok = BotEngineAdapter::TeleportBot(bot, racialDest.mapId, racialDest.x, racialDest.y, racialDest.z, racialDest.o);
            if (ok && g_BotDistFullDebugMode)
            {
                sLog.outString("[BotLevelBrackets] Teleported %s bot '%s' (Lvl %u, Race %u) to racial starting zone (Map %u, %.1f, %.1f, %.1f).",
                    (teamId == TEAM_ALLIANCE ? "Alliance" : "Horde"), bot->GetName(), newLevel, race, racialDest.mapId, racialDest.x, racialDest.y, racialDest.z);
            }
            return ok;
        }
    }

    size_t targetIdx = std::min<size_t>(static_cast<size_t>(rangeIndex), zoneCount - 1);
    const TeleportDestination& dest = zones[targetIdx];

    bool ok = BotEngineAdapter::TeleportBot(bot, dest.mapId, dest.x, dest.y, dest.z, dest.o);

    if (ok && g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Teleported %s bot '%s' (Lvl %u) to leveling zone (Map %u, %.1f, %.1f, %.1f).",
            (teamId == TEAM_ALLIANCE ? "Alliance" : "Horde"), bot->GetName(), newLevel, dest.mapId, dest.x, dest.y, dest.z);
    }
    return ok;
}

static bool EnqueuePendingTeleport(Player* bot, uint8 newLevel, uint8 teamId, TeleportSource source = TELEPORT_SOURCE_LEVEL_BRACKET)
{
    if (!bot || !g_TeleportOnLevelChange)
        return false;

    {
        std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
        if (g_PendingTeleports.find(bot->GetObjectGuid()) != g_PendingTeleports.end())
            return false;

        PendingTeleportEntry entry;
        entry.botGuid     = bot->GetObjectGuid();
        entry.newLevel    = newLevel;
        entry.teamId      = teamId;
        entry.enqueuedAt  = static_cast<uint32>(time(nullptr));
        entry.useHubDest  = false;
        entry.destMapId   = 0;
        entry.destX       = 0.0f;
        entry.destY       = 0.0f;
        entry.destZ       = 0.0f;
        entry.destO       = 0.0f;
        entry.source      = source;

        g_PendingTeleports.emplace(bot->GetObjectGuid(), entry);
    }

    if (g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Enqueued bot '%s' for teleport to level %u zone (source: %u).",
            bot->GetName(), newLevel, static_cast<uint32>(source));
    }
    return true;
}

static bool EnqueuePendingHubTeleport(Player* bot, uint32 mapId, float x, float y, float z, float o, TeleportSource source = TELEPORT_SOURCE_HUB)
{
    if (!bot || !g_TeleportOnLevelChange)
        return false;

    {
        std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
        if (g_PendingTeleports.find(bot->GetObjectGuid()) != g_PendingTeleports.end())
            return false;

        PendingTeleportEntry entry;
        entry.botGuid     = bot->GetObjectGuid();
        entry.newLevel    = bot->GetLevel();
        entry.teamId      = bot->GetTeamId();
        entry.enqueuedAt  = static_cast<uint32>(time(nullptr));
        entry.useHubDest  = true;
        entry.destMapId   = mapId;
        entry.destX       = x;
        entry.destY       = y;
        entry.destZ       = z;
        entry.destO       = o;
        entry.source      = source;

        g_PendingTeleports.emplace(bot->GetObjectGuid(), entry);
    }

    if (g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Enqueued bot '%s' for teleport to hub (Map %u, %.1f, %.1f, %.1f, source: %u).",
            bot->GetName(), mapId, x, y, z, static_cast<uint32>(source));
    }
    return true;
}

static void RemoveBotFromPendingTeleports(Player* bot)
{
    if (bot)
    {
        std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
        g_PendingTeleports.erase(bot->GetObjectGuid());
    }
}

static bool IsInArea(const Player* bot, const HubArea& area)
{
    if (!bot || bot->GetMapId() != area.mapId)
        return false;
    if (bot->GetTeamId() != area.teamId)
        return false;

    float dx = bot->GetPositionX() - area.cx;
    if (dx > area.radius || dx < -area.radius)
        return false;
    float dy = bot->GetPositionY() - area.cy;
    if (dy > area.radius || dy < -area.radius)
        return false;
    float dz = bot->GetPositionZ() - area.cz;
    if (dz > area.radius || dz < -area.radius)
        return false;

    return (dx * dx + dy * dy + dz * dz) <= area.radius * area.radius;
}

static bool IsInAnyHub(const Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return false;

    for (size_t h = 0; h < s_NumHubAreas; ++h)
    {
        if (IsInArea(bot, s_HubAreas[h]))
            return true;
    }
    return false;
}

static void ProcessPendingTeleports()
{
    struct TeleportCandidate
    {
        ObjectGuid           guid;
        PendingTeleportEntry entry;
    };

    std::vector<TeleportCandidate> candidates;

    {
        std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
        if (g_PendingTeleports.empty())
            return;

        uint32 now = static_cast<uint32>(time(nullptr));

        for (auto it = g_PendingTeleports.begin(); it != g_PendingTeleports.end();)
        {
            Player* bot = sObjectAccessor.FindPlayer(it->first);
            if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut())
            {
                it = g_PendingTeleports.erase(it);
                continue;
            }

            if (bot->IsBeingTeleported())
            {
                ++it;
                continue;
            }

            if (!IsBotSafeForTeleport(bot))
            {
                if (g_PendingQueueTTL > 0 && (now - it->second.enqueuedAt) > g_PendingQueueTTL)
                {
                    if (g_BotDistFullDebugMode)
                    {
                        sLog.outString("[BotLevelBrackets] Bot '%s' pending teleport expired (TTL %us), dropping.",
                            bot->GetName(), g_PendingQueueTTL);
                    }
                    it = g_PendingTeleports.erase(it);
                }
                else
                    ++it;
                continue;
            }

            candidates.push_back({it->first, it->second});
            it = g_PendingTeleports.erase(it);
        }
    }

    for (const auto& cand : candidates)
    {
        Player* bot = sObjectAccessor.FindPlayer(cand.guid);
        if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut())
            continue;

        bool success = false;
        if (cand.entry.useHubDest)
        {
            success = BotEngineAdapter::TeleportBot(bot, cand.entry.destMapId, cand.entry.destX, cand.entry.destY, cand.entry.destZ, cand.entry.destO);
            if (success)
            {
                if (g_ManageCityStrategies)
                {
                    BotEngineAdapter::ApplyCityStrategies(bot, g_CityStripStrategies, g_CityStrategies);
                }
                if (g_BotDistFullDebugMode)
                {
                    sLog.outString("[BotLevelBrackets] Teleported bot '%s' to hub (Map %u, %.1f, %.1f, %.1f) - applied city strategies.",
                        bot->GetName(), cand.entry.destMapId, cand.entry.destX, cand.entry.destY, cand.entry.destZ);
                }
            }
        }
        else
        {
            success = TeleportBotToLevelZone(bot, cand.entry.newLevel, cand.entry.teamId);
            if (success)
            {
                if (IsInAnyHub(bot))
                {
                    if (g_ManageCityStrategies)
                    {
                        BotEngineAdapter::ApplyCityStrategies(bot, g_CityStripStrategies, g_CityStrategies);
                    }
                    if (g_BotDistFullDebugMode)
                    {
                        sLog.outString("[BotLevelBrackets] Teleported bot '%s' (Lvl %u) to capital hub zone - applied city strategies.",
                            bot->GetName(), cand.entry.newLevel);
                    }
                }
                else
                {
                    if (g_WorldTeleportResetStrategies)
                    {
                        BotEngineAdapter::ResetDefaultStrategies(bot);
                    }
                    if (g_BotDistFullDebugMode)
                    {
                        sLog.outString("[BotLevelBrackets] Teleported bot '%s' (Lvl %u) out to world (source=%u) - reset NC & CO strategies to default.",
                            bot->GetName(), cand.entry.newLevel, static_cast<uint32>(cand.entry.source));
                    }
                }
            }
        }

        if (success)
        {
            if (cand.entry.source == TELEPORT_SOURCE_HUB && g_HubDisperseCooldownSeconds > 0)
            {
                int64 steadyNow = GetSteadySeconds();
                std::lock_guard<std::mutex> lock(g_HubCooldownMutex);
                g_HubBotCooldowns[cand.guid] = BotHubCooldownRecord{ steadyNow };
            }
            g_StatsBotsTeleported.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

static bool IsBotDispersable(Player* bot, const std::unordered_set<ObjectGuid>* pendingTeleportGuids)
{
    if (!bot || !bot->IsInWorld())
        return false;
    if (!IsPlayerBot(bot) || !IsPlayerRandomBot(bot))
        return false;
    if (IsBotExcluded(bot))
        return false;
    if (g_IgnoreGuildBotsWithRealPlayers && BotInGuildWithRealPlayer(bot))
        return false;
    if (g_IgnoreFriendListed && BotInFriendList(bot))
        return false;
    if (BotHasRealPlayerMaster(bot))
        return false;
    if (bot->IsBeingTeleported())
        return false;
    if (!bot->GetSession() || bot->GetSession()->isLogingOut())
        return false;
    if (pendingTeleportGuids && pendingTeleportGuids->count(bot->GetObjectGuid()) > 0)
        return false;
    if (g_IgnoreArenaTeamBots && BotInArenaTeam(bot))
        return false;
    if (BotInArena(bot))
        return false;
    return true;
}

static void ProcessHubPopulate()
{
    if (!g_HubDisperseEnabled || !g_TeleportOnLevelChange)
        return;

    int64 now = GetSteadySeconds();

    // Prune expired bot relocation cooldown entries
    {
        std::lock_guard<std::mutex> lock(g_HubCooldownMutex);
        if (!g_HubBotCooldowns.empty())
        {
            for (auto it = g_HubBotCooldowns.begin(); it != g_HubBotCooldowns.end();)
            {
                if (now - it->second.lastRelocationTime >= static_cast<int64>(g_HubDisperseCooldownSeconds))
                    it = g_HubBotCooldowns.erase(it);
                else
                    ++it;
            }
        }
    }

    uint32 processed = 0;

    struct BotInfo {
        Player* bot;
        int8 hubIdx;
        bool inStarting;
    };

    HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
    std::unordered_set<ObjectGuid> pendingTeleportGuids;
    std::vector<uint32> pendingInbound(s_NumHubAreas, 0);
    {
        std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
        for (const auto& kv : g_PendingTeleports)
        {
            pendingTeleportGuids.insert(kv.first);
            const PendingTeleportEntry& entry = kv.second;
            if (entry.useHubDest)
            {
                for (size_t h = 0; h < s_NumHubAreas; ++h)
                {
                    if (entry.destMapId == s_HubAreas[h].mapId)
                    {
                        float dx = entry.destX - s_HubAreas[h].cx;
                        float dy = entry.destY - s_HubAreas[h].cy;
                        float dz = entry.destZ - s_HubAreas[h].cz;
                        if ((dx * dx + dy * dy + dz * dz) <= s_HubAreas[h].radius * s_HubAreas[h].radius)
                        {
                            ++pendingInbound[h];
                            break;
                        }
                    }
                }
            }
        }
    }

    std::vector<BotInfo> allBots;
    allBots.reserve(players.size());
    std::vector<uint32> hubCounts(s_NumHubAreas, 0);

    for (const auto& itr : players)
    {
        Player* bot = itr.second;
        if (!IsBotDispersable(bot, &pendingTeleportGuids))
            continue;

        BotInfo info{bot, -1, false};
        for (size_t h = 0; h < s_NumHubAreas; ++h)
        {
            if (IsInArea(bot, s_HubAreas[h]))
            {
                info.hubIdx = static_cast<int8>(h);
                ++hubCounts[h];
                break;
            }
        }
        for (size_t s = 0; s < s_NumStartingAreas; ++s)
        {
            if (IsInArea(bot, s_StartingAreas[s]))
            {
                info.inStarting = true;
                break;
            }
        }
        allBots.push_back(info);

        // Active city strategy synchronization for bots residing in hubs:
        if (g_ManageCityStrategies && info.hubIdx >= 0)
        {
            if (!BotEngineAdapter::HasStrategy(bot, "rpg", BotState::BOT_STATE_NON_COMBAT))
            {
                BotEngineAdapter::ApplyCityStrategies(bot, g_CityStripStrategies, g_CityStrategies);
                if (g_BotDistFullDebugMode)
                {
                    sLog.outString("[BotLevelBrackets] HubPopulate: Residing bot '%s' in hub %d missing 'rpg' - applied city strategies.",
                        bot->GetName(), info.hubIdx);
                }
            }
        }
        else if (g_WorldTeleportResetStrategies && info.hubIdx < 0)
        {
            // If the bot is out in the open world but still retains city-only strategies, reset both NC and CO
            if (BotEngineAdapter::HasStrategy(bot, "sit", BotState::BOT_STATE_NON_COMBAT) ||
                BotEngineAdapter::HasStrategy(bot, "emote", BotState::BOT_STATE_NON_COMBAT) ||
                !BotEngineAdapter::HasStrategy(bot, "grind", BotState::BOT_STATE_NON_COMBAT))
            {
                BotEngineAdapter::ResetDefaultStrategies(bot);
                if (g_BotDistFullDebugMode)
                {
                    sLog.outString("[BotLevelBrackets] HubPopulate: Wild bot '%s' outside hubs had stale city strategies - reset NC & CO strategies.",
                        bot->GetName());
                }
            }
        }
    }

    if (allBots.empty())
        return;

    uint32 totalAllianceBots = 0;
    uint32 totalHordeBots    = 0;
    for (const auto& itr : players)
    {
        Player* p = itr.second;
        if (!p || !p->IsInWorld() || !IsPlayerBot(p) || !IsPlayerRandomBot(p) || IsBotExcluded(p))
            continue;
        if (p->GetTeamId() == TEAM_ALLIANCE)
            ++totalAllianceBots;
        else if (p->GetTeamId() == TEAM_HORDE)
            ++totalHordeBots;
    }

    struct HubEval
    {
        uint32                 quota{0};
        int32                  effectiveCount{0};
        HubImbalanceDirection dir{HubImbalanceDirection::Neutral};
        bool                   stabilityPermitted{false};
    };

    std::vector<HubEval> hubEvals(s_NumHubAreas);
    for (size_t hubIdx = 0; hubIdx < s_NumHubAreas; ++hubIdx)
    {
        const HubArea& hub = s_HubAreas[hubIdx];
        uint32 factionTotal = (hub.teamId == TEAM_ALLIANCE) ? totalAllianceBots : totalHordeBots;
        uint32 quota = GetHubQuota(hubIdx, factionTotal);
        int32 effectiveCount = static_cast<int32>(hubCounts[hubIdx] + pendingInbound[hubIdx]);
        int32 deviation = effectiveCount - static_cast<int32>(quota);

        HubImbalanceDirection dir = HubImbalanceDirection::Neutral;
        if (quota == 0)
        {
            // Zero-Target Bypass: if quota is 0, any managed bot in the city is excess
            if (effectiveCount > 0)
                dir = HubImbalanceDirection::Overpopulated;
            else
                dir = HubImbalanceDirection::Neutral;
        }
        else
        {
            if (std::abs(deviation) <= static_cast<int32>(g_HubDisperseHysteresisBots))
                dir = HubImbalanceDirection::Neutral;
            else if (deviation > static_cast<int32>(g_HubDisperseHysteresisBots))
                dir = HubImbalanceDirection::Overpopulated;
            else
                dir = HubImbalanceDirection::Underpopulated;
        }

        HubStabilityRecord& stab = g_HubStability[hubIdx];
        bool stabilityPermitted = false;

        if (dir == HubImbalanceDirection::Neutral)
        {
            stab.currentDirection = HubImbalanceDirection::Neutral;
            stab.imbalanceStartTime = 0;
            stab.lastEffectivePopulation = effectiveCount;
        }
        else if (dir != stab.currentDirection)
        {
            // Direction changed or new imbalance started
            stab.currentDirection = dir;
            stab.imbalanceStartTime = now;
            stab.lastEffectivePopulation = effectiveCount;
            if (g_HubDisperseStabilitySeconds == 0)
                stabilityPermitted = true;
        }
        else
        {
            // Direction unchanged: imbalance persisted across check intervals
            stab.lastEffectivePopulation = effectiveCount;
            int64 elapsed = now - stab.imbalanceStartTime;
            if (elapsed >= static_cast<int64>(g_HubDisperseStabilitySeconds))
                stabilityPermitted = true;
        }

        hubEvals[hubIdx] = {quota, effectiveCount, dir, stabilityPermitted};
    }

    std::unordered_set<ObjectGuid> alreadyProcessed;

    // Part A: Disperse wrong-level and excess bots from hubs
    for (size_t hubIdx = 0; hubIdx < s_NumHubAreas && processed < g_HubDisperseBotsPerCycle; ++hubIdx)
    {
        const HubArea& hub = s_HubAreas[hubIdx];
        const HubEval& eval = hubEvals[hubIdx];

        std::vector<const BotInfo*> botsInHub;
        botsInHub.reserve(30);

        for (const auto& info : allBots)
        {
            if (info.hubIdx == static_cast<int8>(hubIdx))
                botsInHub.push_back(&info);
        }

        if (botsInHub.empty())
            continue;

        // Disperse wrong-level bots (if configured on hub)
        for (const BotInfo* info : botsInHub)
        {
            if (processed >= g_HubDisperseBotsPerCycle)
                break;
            if (alreadyProcessed.count(info->bot->GetObjectGuid()) > 0)
                continue;

            uint8 level = info->bot->GetLevel();
            bool wrongLevel = false;
            if (hub.minLevel > 0 && level < hub.minLevel)
                wrongLevel = true;
            if (hub.maxLevel > 0 && level > hub.maxLevel)
                wrongLevel = true;

            if (!wrongLevel)
                continue;

            if (!IsBotSafeForTeleport(info->bot))
                continue;

            EnqueuePendingTeleport(info->bot, level, info->bot->GetTeamId(), TELEPORT_SOURCE_HUB);
            alreadyProcessed.insert(info->bot->GetObjectGuid());
            ++processed;
        }

        // Disperse excess bots per bracket if MaxBotsPerBracket is configured
        if (g_HubDisperseMaxBotsPerBracket > 0)
        {
            std::vector<uint32> bracketCounts(g_NumRanges, 0);
            for (const BotInfo* info : botsInHub)
            {
                if (alreadyProcessed.count(info->bot->GetObjectGuid()) > 0)
                    continue;
                int r = GetLevelRangeIndex(info->bot->GetLevel(), hub.teamId);
                if (r >= 0 && static_cast<uint32>(r) < g_NumRanges)
                    bracketCounts[r]++;
            }

            for (const BotInfo* info : botsInHub)
            {
                if (processed >= g_HubDisperseBotsPerCycle)
                    break;
                if (alreadyProcessed.count(info->bot->GetObjectGuid()) > 0)
                    continue;

                int r = GetLevelRangeIndex(info->bot->GetLevel(), hub.teamId);
                if (r >= 0 && static_cast<uint32>(r) < g_NumRanges && bracketCounts[r] > g_HubDisperseMaxBotsPerBracket)
                {
                    if (IsBotOnHubCooldown(info->bot->GetObjectGuid(), now))
                    {
                        g_StatsCooldownSkips.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }

                    if (!IsBotSafeForTeleport(info->bot))
                        continue;

                    EnqueuePendingTeleport(info->bot, info->bot->GetLevel(), info->bot->GetTeamId(), TELEPORT_SOURCE_HUB);
                    alreadyProcessed.insert(info->bot->GetObjectGuid());
                    ++processed;
                    --bracketCounts[r];

                    if (g_BotDistFullDebugMode)
                    {
                        sLog.outString("[BotLevelBrackets] HubPopulate: Dispersed bot '%s' (Lvl %u, Bracket %u) from hub %u exceeding MaxBotsPerBracket cap (%u).",
                            info->bot->GetName(), info->bot->GetLevel(), r + 1, static_cast<uint32>(hubIdx), g_HubDisperseMaxBotsPerBracket);
                    }
                }
            }
        }

        // Only disperse excess if hub is confirmed Overpopulated and stability window is satisfied
        if (eval.dir != HubImbalanceDirection::Overpopulated || !eval.stabilityPermitted)
            continue;

        uint32 remaining = 0;
        for (const BotInfo* info : botsInHub)
        {
            if (alreadyProcessed.count(info->bot->GetObjectGuid()) > 0)
                continue;
            ++remaining;
        }

        if (remaining <= eval.quota)
            continue;

        uint32 toDisperse = remaining - eval.quota;
        for (const BotInfo* info : botsInHub)
        {
            if (processed >= g_HubDisperseBotsPerCycle || toDisperse == 0)
                break;
            if (alreadyProcessed.count(info->bot->GetObjectGuid()) > 0)
                continue;

            if (IsBotOnHubCooldown(info->bot->GetObjectGuid(), now))
            {
                g_StatsCooldownSkips.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            if (!IsBotSafeForTeleport(info->bot))
                continue;

            EnqueuePendingTeleport(info->bot, info->bot->GetLevel(), info->bot->GetTeamId(), TELEPORT_SOURCE_HUB);
            alreadyProcessed.insert(info->bot->GetObjectGuid());
            ++processed;
            --toDisperse;
        }
    }

    // Part B: Fill hubs below quota by pulling idle bots from the wild
    for (size_t hubIdx = 0; hubIdx < s_NumHubAreas && processed < g_HubDisperseBotsPerCycle; ++hubIdx)
    {
        const HubArea& hub = s_HubAreas[hubIdx];
        const HubEval& eval = hubEvals[hubIdx];

        // Only fill if hub is confirmed Underpopulated and stability window is satisfied
        if (eval.dir != HubImbalanceDirection::Underpopulated || !eval.stabilityPermitted)
            continue;

        if (eval.effectiveCount >= static_cast<int32>(eval.quota))
            continue;

        uint32 needed = eval.quota - eval.effectiveCount;

        // Track live bracket counts inside this hub
        std::vector<uint32> currentHubBracketCounts(g_NumRanges, 0);
        for (const auto& info : allBots)
        {
            if (info.hubIdx == static_cast<int8>(hubIdx) && alreadyProcessed.count(info.bot->GetObjectGuid()) == 0)
            {
                int r = GetLevelRangeIndex(info.bot->GetLevel(), hub.teamId);
                if (r >= 0 && static_cast<uint32>(r) < g_NumRanges)
                    currentHubBracketCounts[r]++;
            }
        }

        auto tryTeleportBotToHub = [&](Player* candidateBot, int rangeIdx) -> bool
        {
            if (IsBotOnHubCooldown(candidateBot->GetObjectGuid(), now))
            {
                g_StatsCooldownSkips.fetch_add(1, std::memory_order_relaxed);
                return false;
            }

            if (!IsBotSafeForTeleport(candidateBot))
                return false;

            float angle  = static_cast<float>(rand_norm()) * 2.0f * static_cast<float>(M_PI);
            float offset = static_cast<float>(rand_norm()) * 30.0f;
            float dx = hub.cx + std::cos(angle) * offset;
            float dy = hub.cy + std::sin(angle) * offset;

            EnqueuePendingHubTeleport(candidateBot, hub.mapId, dx, dy, hub.cz, angle, TELEPORT_SOURCE_HUB);
            alreadyProcessed.insert(candidateBot->GetObjectGuid());
            if (rangeIdx >= 0 && static_cast<uint32>(rangeIdx) < g_NumRanges)
                currentHubBracketCounts[rangeIdx]++;
            ++processed;
            --needed;
            return true;
        };

        // Phase 1: Prioritize filling brackets below MinBotsPerBracket floor
        if (g_HubDisperseMinBotsPerBracket > 0)
        {
            for (const auto& info : allBots)
            {
                if (processed >= g_HubDisperseBotsPerCycle || needed == 0)
                    break;
                if (alreadyProcessed.count(info.bot->GetObjectGuid()) > 0)
                    continue;
                if (info.bot->GetTeamId() != hub.teamId)
                    continue;
                if (info.hubIdx >= 0 || info.inStarting)
                    continue;

                uint8 level = info.bot->GetLevel();
                if (hub.minLevel > 0 && level < hub.minLevel)
                    continue;
                if (hub.maxLevel > 0 && level > hub.maxLevel)
                    continue;

                int rangeIdx = GetLevelRangeIndex(level, hub.teamId);
                if (rangeIdx < 0 || static_cast<uint32>(rangeIdx) >= g_NumRanges)
                    continue;

                if (currentHubBracketCounts[rangeIdx] >= g_HubDisperseMinBotsPerBracket)
                    continue;

                if (g_HubDisperseMaxBotsPerBracket > 0 && currentHubBracketCounts[rangeIdx] >= g_HubDisperseMaxBotsPerBracket)
                    continue;

                tryTeleportBotToHub(info.bot, rangeIdx);
            }
        }

        // Phase 2: Fill remaining needed quota with eligible wild bots respecting MaxBotsPerBracket
        for (const auto& info : allBots)
        {
            if (processed >= g_HubDisperseBotsPerCycle || needed == 0)
                break;
            if (alreadyProcessed.count(info.bot->GetObjectGuid()) > 0)
                continue;
            if (info.bot->GetTeamId() != hub.teamId)
                continue;
            if (info.hubIdx >= 0 || info.inStarting)
                continue;

            uint8 level = info.bot->GetLevel();
            if (hub.minLevel > 0 && level < hub.minLevel)
                continue;
            if (hub.maxLevel > 0 && level > hub.maxLevel)
                continue;

            int rangeIdx = GetLevelRangeIndex(level, hub.teamId);
            if (g_HubDisperseMaxBotsPerBracket > 0 && rangeIdx >= 0 && static_cast<uint32>(rangeIdx) < g_NumRanges)
            {
                if (currentHubBracketCounts[rangeIdx] >= g_HubDisperseMaxBotsPerBracket)
                    continue;
            }

            tryTeleportBotToHub(info.bot, rangeIdx);
        }
    }

    if (processed > 0 && g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] HubPopulate: processed %u bots this cycle (Mode: %s, MinPerBracket: %u, MaxPerBracket: %u).",
            processed, (g_HubDisperseQuotaMode == 1 ? "Headcount" : "Percentage"),
            g_HubDisperseMinBotsPerBracket, g_HubDisperseMaxBotsPerBracket);
    }
}

static void ProcessStartingZoneDisperse()
{
    if (!g_HubDisperseEnabled || !g_TeleportOnLevelChange)
        return;

    uint32 processed = 0;
    HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
    std::unordered_set<ObjectGuid> pendingTeleportGuids;
    {
        std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
        for (const auto& kv : g_PendingTeleports)
            pendingTeleportGuids.insert(kv.first);
    }

    for (const auto& itr : players)
    {
        if (processed >= g_HubDisperseBotsPerCycle)
            break;

        Player* bot = itr.second;
        if (!IsBotDispersable(bot, &pendingTeleportGuids))
            continue;

        for (size_t i = 0; i < s_NumStartingAreas; ++i)
        {
            if (!IsInArea(bot, s_StartingAreas[i]))
                continue;

            uint8 level = bot->GetLevel();
            if (level <= s_StartingAreas[i].maxLevel)
                break;

            if (!IsBotSafeForTeleport(bot))
                break;

            EnqueuePendingTeleport(bot, level, bot->GetTeamId(), TELEPORT_SOURCE_STARTING_ZONE);
            ++processed;
            if (g_BotDistFullDebugMode)
            {
                sLog.outString("[BotLevelBrackets] StartingZoneDisperse: bot '%s' (Lvl %u) dispersed from starter zone %u to leveling zone.",
                    bot->GetName(), level, (uint32)i);
            }
            break;
        }
    }

    if (processed > 0 && g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] StartingZoneDisperse: processed %u bots this cycle.", processed);
    }
}

static void ProcessWrongMapDisperse()
{
    if (!g_HubDisperseEnabled || !g_TeleportOnLevelChange)
        return;

    uint32 processed = 0;
    HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
    std::unordered_set<ObjectGuid> pendingTeleportGuids;
    {
        std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
        for (const auto& kv : g_PendingTeleports)
            pendingTeleportGuids.insert(kv.first);
    }

    for (const auto& itr : players)
    {
        if (processed >= g_HubDisperseBotsPerCycle)
            break;

        Player* bot = itr.second;
        if (!IsBotDispersable(bot, &pendingTeleportGuids))
            continue;

        uint32 mapId = bot->GetMapId();
        bool isInstanceOrBg = bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsBattleGround() || bot->GetMap()->IsRaid());
        if (isInstanceOrBg || mapId == 0 || mapId == 1 || mapId == 369 || mapId == 815 || mapId == 816 || mapId == 813 || mapId == 26 || mapId == 27)
            continue;

        if (!IsBotSafeForTeleport(bot))
            continue;

        EnqueuePendingTeleport(bot, bot->GetLevel(), bot->GetTeamId(), TELEPORT_SOURCE_WRONG_MAP);
        ++processed;
        if (g_BotDistFullDebugMode)
        {
            sLog.outString("[BotLevelBrackets] WrongMapDisperse: bot '%s' on invalid/stranded Map %u dispersed to leveling zone.",
                bot->GetName(), mapId);
        }
    }

    if (processed > 0 && g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] WrongMapDisperse: processed %u bots this cycle.", processed);
    }
}

// -----------------------------------------------------------------------------
// BOT LEVEL ADJUSTMENT EXECUTION
// -----------------------------------------------------------------------------

static bool AdjustBotToRange(Player* bot, int targetRangeIndex)
{
    if (BotEngineAdapter::IsRandomLevelsDisabled() || !bot || !bot->IsInWorld() || !bot->GetSession() ||
        bot->GetSession()->isLogingOut())
        return false;

    std::vector<LevelRangeConfig> const* factionRanges = GetFactionRanges(bot);
    if (!factionRanges || targetRangeIndex < 0 || static_cast<uint32>(targetRangeIndex) >= factionRanges->size())
        return false;

    uint8 botOriginalLevel = bot->GetLevel();
    const LevelRangeConfig& range = (*factionRanges)[targetRangeIndex];
    if (range.lower > range.upper)
        return false;

    uint8 newLevel = GetRandomLevelInRange(range);

    if (newLevel < g_RandomBotMinLevel)
        newLevel = g_RandomBotMinLevel;
    if (newLevel > g_RandomBotMaxLevel)
        newLevel = g_RandomBotMaxLevel;

    if (!BotEngineAdapter::AdjustBotLevel(bot, targetRangeIndex, newLevel))
        return false;

    if (g_BotDistFullDebugMode)
    {
        const char* playerFaction = IsAlliancePlayerBot(bot) ? "Alliance" : "Horde";
        std::string className = BotEngineAdapter::GetClassName(bot->GetClass());
        sLog.outString("[BotLevelBrackets] %s bot '%s' (%s, Lvl %u) adjusted to Level %u (Bracket %u-%u).",
            playerFaction, bot->GetName(), className.c_str(), botOriginalLevel, newLevel,
            (*factionRanges)[targetRangeIndex].lower, (*factionRanges)[targetRangeIndex].upper);
    }

    if (bot->GetSession() && !bot->GetSession()->IsHeadless())
    {
        ChatHandler(bot->GetSession()).SendSysMessage("[mod-twow-bot-level-brackets] Your level has been reset.");
    }
    return true;
}

// -----------------------------------------------------------------------------
// PENDING RESET QUEUE WITH PERSISTENCE
// -----------------------------------------------------------------------------

static void DeletePendingFromDB(ObjectGuid guid)
{
    CharacterDatabase.PExecute(
        "DELETE FROM bot_level_brackets_pending_resets WHERE bot_guid = " UI64FMTD,
        guid.GetRawValue());
}

static bool EnqueuePendingReset(ObjectGuid guid, int targetRange, uint8 teamId)
{
    std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
    if (g_MaxPendingQueueSize > 0 && g_PendingLevelResets.size() >= g_MaxPendingQueueSize)
    {
        if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
        {
            sLog.outString("[BotLevelBrackets] Pending reset queue is full (%u/%u). Bot " UI64FMTD " not enqueued.",
                (uint32)g_PendingLevelResets.size(), g_MaxPendingQueueSize, guid.GetRawValue());
        }
        return false;
    }

    uint32 now = static_cast<uint32>(time(nullptr));
    auto res = g_PendingLevelResets.emplace(guid, PendingResetEntry{guid, targetRange, teamId, now});
    if (!res.second)
    {
        if (g_BotDistFullDebugMode)
            sLog.outString("[BotLevelBrackets] Bot " UI64FMTD " already in pending reset queue, skipping.", guid.GetRawValue());
        return false;
    }

    CharacterDatabase.PExecute(
        "REPLACE INTO bot_level_brackets_pending_resets (bot_guid, target_range, is_alliance, enqueued_at) VALUES (" UI64FMTD ", %d, %u, %u)",
        guid.GetRawValue(), targetRange, (teamId == TEAM_ALLIANCE ? 1 : 0), now);
    return true;
}

static void RemoveBotFromPendingResets(Player* bot)
{
    if (!bot)
        return;
    ObjectGuid guid = bot->GetObjectGuid();
    {
        std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
        g_PendingLevelResets.erase(guid);
    }
    DeletePendingFromDB(guid);
}

static void LoadPendingResetsFromDB()
{
    QueryResult* result = CharacterDatabase.Query(
        "SELECT bot_guid, target_range, is_alliance, enqueued_at FROM bot_level_brackets_pending_resets");
    if (!result)
        return;

    std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
    g_PendingLevelResets.clear();
    do
    {
        Field* fields = result->Fetch();
        uint64 rawGuid     = fields[0].GetUInt64();
        int    targetRange = static_cast<int>(fields[1].GetInt32());
        uint8  isAlliance  = fields[2].GetUInt8();
        uint32 enqueuedAt  = fields[3].GetUInt32();

        ObjectGuid guid(rawGuid);
        uint8 teamId = (isAlliance != 0) ? TEAM_ALLIANCE : TEAM_HORDE;
        g_PendingLevelResets.emplace(guid, PendingResetEntry{guid, targetRange, teamId, enqueuedAt});
    } while (result->NextRow());

    delete result;

    if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Loaded %u pending resets from database.", (uint32)g_PendingLevelResets.size());
    }
}

static void ProcessPendingLevelResets()
{
    struct ProcessCandidate {
        ObjectGuid guid;
        int        targetRange;
        uint8      teamId;
    };

    std::vector<ProcessCandidate> candidates;

    {
        std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
        if (g_PendingLevelResets.empty())
            return;

        uint32 now = static_cast<uint32>(time(nullptr));

        for (auto it = g_PendingLevelResets.begin(); it != g_PendingLevelResets.end();)
        {
            if (g_FlaggedProcessLimit > 0 && candidates.size() >= g_FlaggedProcessLimit)
                break;

            if (g_PendingQueueTTL > 0 && (now - it->second.enqueuedAt) > g_PendingQueueTTL)
            {
                if (g_BotDistFullDebugMode)
                {
                    sLog.outString("[BotLevelBrackets] Pending entry for bot " UI64FMTD " expired (TTL %us), dropping.",
                        it->first.GetRawValue(), g_PendingQueueTTL);
                }
                DeletePendingFromDB(it->first);
                it = g_PendingLevelResets.erase(it);
                continue;
            }

            Player* bot = sObjectAccessor.FindPlayer(it->first);
            if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut())
            {
                DeletePendingFromDB(it->first);
                it = g_PendingLevelResets.erase(it);
                continue;
            }

            if (IsBotExcluded(bot) ||
                (g_IgnoreGuildBotsWithRealPlayers && BotInGuildWithRealPlayer(bot)) ||
                (g_IgnoreFriendListed && BotInFriendList(bot)) ||
                (g_IgnoreArenaTeamBots && BotInArenaTeam(bot)) ||
                BotHasRealPlayerMaster(bot))
            {
                DeletePendingFromDB(it->first);
                it = g_PendingLevelResets.erase(it);
                continue;
            }

            if (it->second.targetRange < 0 || static_cast<uint32>(it->second.targetRange) >= g_NumRanges)
            {
                DeletePendingFromDB(it->first);
                it = g_PendingLevelResets.erase(it);
                continue;
            }

            if (IsBotSafeForLevelReset(bot))
            {
                candidates.push_back({it->first, it->second.targetRange, it->second.teamId});
            }
            else
            {
                g_StatsBotsSkipped.fetch_add(1, std::memory_order_relaxed);
            }

            ++it;
        }
    }

    std::vector<ObjectGuid> processed;
    for (const auto& cand : candidates)
    {
        Player* bot = sObjectAccessor.FindPlayer(cand.guid);
        if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut())
            continue;

        if (AdjustBotToRange(bot, cand.targetRange))
        {
            processed.push_back(cand.guid);
            g_StatsBotsAdjusted.fetch_add(1, std::memory_order_relaxed);
            if (g_TeleportOnLevelChange)
                EnqueuePendingTeleport(bot, bot->GetLevel(), cand.teamId);
        }
    }

    if (!processed.empty())
    {
        std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
        for (const auto& guid : processed)
        {
            g_PendingLevelResets.erase(guid);
            DeletePendingFromDB(guid);
        }
    }
}

// -----------------------------------------------------------------------------
// SOCIAL FRIENDS & PERSISTENT GUILD TRACKING
// -----------------------------------------------------------------------------

static void LoadSocialFriendList()
{
    QueryResult* result = CharacterDatabase.Query(
        "SELECT cs.friend, c.account "
        "FROM character_social cs "
        "INNER JOIN characters c ON c.guid = cs.guid "
        "WHERE (cs.flags & 1) <> 0");

    std::unordered_set<uint32> tempSet;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 friendGuid = fields[0].GetUInt32();
            uint32 ownerAccount = fields[1].GetUInt32();
            if (!BotEngineAdapter::IsInRandomAccountList(ownerAccount))
                tempSet.insert(friendGuid);
        } while (result->NextRow());

        delete result;
    }

    {
        std::lock_guard<std::mutex> lock(g_SocialDataMutex);
        g_SocialFriendsList = std::move(tempSet);
    }

    if (g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Loaded %u human-owned friend relations from character_social.",
            (uint32)g_SocialFriendsList.size());
    }
}

static void LoadPersistentGuildTracker(bool force = false)
{
    if (!force && !g_IgnoreGuildBotsWithRealPlayers)
        return;

    QueryResult* result = CharacterDatabase.Query(
        "SELECT guild_id FROM bot_level_brackets_guild_tracker WHERE has_real_players = 1");

    std::unordered_set<uint32> tempSet;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guildId = fields[0].GetUInt32();
            tempSet.insert(guildId);
        } while (result->NextRow());

        delete result;
    }

    {
        std::lock_guard<std::mutex> lock(g_SocialDataMutex);
        g_PersistentRealPlayerGuildIds = std::move(tempSet);
    }

    if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Loaded %u guilds with real players from persistent tracker.",
            (uint32)g_PersistentRealPlayerGuildIds.size());
    }
}

static void UpdatePersistentGuildTracker()
{
    if (!g_IgnoreGuildBotsWithRealPlayers)
        return;

    std::unordered_set<uint32> currentRealPlayerGuilds;
    HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();

    for (const auto& itr : players)
    {
        Player* player = itr.second;
        if (!player || !player->IsInWorld())
            continue;

        if (!IsPlayerBot(player))
        {
            uint32 guildId = player->GetGuildId();
            if (guildId != 0)
                currentRealPlayerGuilds.insert(guildId);
        }
    }

    uint32 addedCount = 0;
    {
        std::lock_guard<std::mutex> lock(g_SocialDataMutex);
        for (uint32 guildId : currentRealPlayerGuilds)
        {
            if (g_PersistentRealPlayerGuildIds.find(guildId) == g_PersistentRealPlayerGuildIds.end())
            {
                CharacterDatabase.PExecute(
                    "REPLACE INTO bot_level_brackets_guild_tracker (guild_id, has_real_players) VALUES (%u, 1)", guildId);
                g_PersistentRealPlayerGuildIds.insert(guildId);
                addedCount++;
            }
        }
    }

    if (g_BotDistFullDebugMode && addedCount > 0)
    {
        sLog.outString("[BotLevelBrackets] Added %u new active guilds to persistent tracker.", addedCount);
    }
}

static void CleanupGuildTracker()
{
    LoadPersistentGuildTracker(true);

    std::unordered_set<uint32> guildsWithHumanAccountMembers;
    QueryResult* members = CharacterDatabase.Query(
        "SELECT DISTINCT gm.guildid, c.account "
        "FROM guild_member gm "
        "INNER JOIN characters c ON c.guid = gm.guid "
        "INNER JOIN bot_level_brackets_guild_tracker t ON t.guild_id = gm.guildid "
        "WHERE t.has_real_players = 1");
    if (members)
    {
        do
        {
            Field* fields = members->Fetch();
            uint32 guildId = fields[0].GetUInt32();
            uint32 accountId = fields[1].GetUInt32();
            if (!BotEngineAdapter::IsInRandomAccountList(accountId))
                guildsWithHumanAccountMembers.insert(guildId);
        } while (members->NextRow());

        delete members;
    }

    std::vector<uint32> guildsToRemove;
    {
        std::lock_guard<std::mutex> lock(g_SocialDataMutex);
        for (uint32 trackedGuildId : g_PersistentRealPlayerGuildIds)
        {
            if (guildsWithHumanAccountMembers.count(trackedGuildId) == 0)
                guildsToRemove.push_back(trackedGuildId);
        }

        for (uint32 guildId : guildsToRemove)
        {
            CharacterDatabase.PExecute(
                "UPDATE bot_level_brackets_guild_tracker SET has_real_players = 0 WHERE guild_id = %u", guildId);
            g_PersistentRealPlayerGuildIds.erase(guildId);
            g_RealPlayerGuildIds.erase(guildId);
        }
    }

    if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Guild tracker cleanup: removed %u guilds without human-account members.",
            (uint32)guildsToRemove.size());
    }
}

static void LoadRealPlayerGuildIds()
{
    std::unordered_set<uint32> tempSet;
    if (g_IgnoreGuildBotsWithRealPlayers)
    {
        HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
        for (const auto& itr : players)
        {
            Player* player = itr.second;
            if (!player || !player->IsInWorld())
                continue;

            if (!IsPlayerBot(player))
            {
                uint32 guildId = player->GetGuildId();
                if (guildId != 0)
                    tempSet.insert(guildId);
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_SocialDataMutex);
        g_RealPlayerGuildIds = std::move(tempSet);
    }
}

// -----------------------------------------------------------------------------
// BOT DISTRIBUTION CALCULATION & REDISTRIBUTION
// -----------------------------------------------------------------------------

static int GetOrFlagPlayerBracket(Player* player)
{
    if (IsPlayerBot(player))
    {
        if (IsBotExcluded(player))
            return -1;
        if (g_IgnoreGuildBotsWithRealPlayers && BotInGuildWithRealPlayer(player))
            return -1;
        if (g_IgnoreFriendListed && BotInFriendList(player))
            return -1;
        if (g_IgnoreArenaTeamBots && BotInArenaTeam(player))
            return -1;
        if (BotHasRealPlayerMaster(player))
            return -1;
    }

    int rangeIndex = GetLevelRangeIndex(player->GetLevel(), player->GetTeamId());
    if (rangeIndex >= 0)
        return rangeIndex;

    if (!IsPlayerBot(player))
        return -1;

    std::vector<LevelRangeConfig> const* factionRangesVector = GetFactionRanges(player);
    if (!factionRangesVector)
        return -1;
    LevelRangeConfig const* factionRanges = factionRangesVector->data();

    int targetRange = -1;
    int smallestDiff = std::numeric_limits<int>::max();

    for (uint32 i = 0; i < g_NumRanges; ++i)
    {
        if (factionRanges[i].lower > factionRanges[i].upper)
            continue;

        int diff = 0;
        if (player->GetLevel() < factionRanges[i].lower)
            diff = factionRanges[i].lower - player->GetLevel();
        else if (player->GetLevel() > factionRanges[i].upper)
            diff = player->GetLevel() - factionRanges[i].upper;

        if (diff < smallestDiff)
        {
            smallestDiff = diff;
            targetRange = static_cast<int>(i);
        }
    }

    if (targetRange >= 0)
    {
        EnqueuePendingReset(player->GetObjectGuid(), targetRange, player->GetTeamId());
    }

    return -1;
}

static std::vector<int> CalculateDesiredCounts(uint32 totalBots, std::vector<LevelRangeConfig> const& ranges)
{
    std::vector<int> desired(ranges.size(), 0);
    std::vector<double> remainder(ranges.size(), 0.0);
    std::vector<uint32> indices(ranges.size());
    uint32 assigned = 0;

    for (uint32 i = 0; i < ranges.size(); ++i)
    {
        indices[i] = i;
        double raw = static_cast<double>(ranges[i].desiredPercent) * totalBots / 100.0;
        desired[i] = static_cast<int>(std::floor(raw));
        remainder[i] = raw - desired[i];
        assigned += static_cast<uint32>(desired[i]);
    }

    std::stable_sort(indices.begin(), indices.end(), [&remainder](uint32 left, uint32 right)
    {
        return remainder[left] > remainder[right];
    });

    for (uint32 i = 0; assigned < totalBots && !indices.empty(); ++i, ++assigned)
        ++desired[indices[i % indices.size()]];

    return desired;
}

static void QueueFactionRedistribution(
    uint32 totalBots,
    std::vector<LevelRangeConfig> const& ranges,
    std::vector<int>& actualCounts,
    std::vector<std::vector<Player*>> const& botsByRange,
    bool isAlliance)
{
    if (totalBots == 0)
        return;

    std::vector<int> desiredCounts = CalculateDesiredCounts(totalBots, ranges);
    for (uint32 source = 0; source < g_NumRanges; ++source)
    {
        if (g_MinBracketChangeThreshold > 0 &&
            (actualCounts[source] - desiredCounts[source]) <= static_cast<int>(g_MinBracketChangeThreshold))
            continue;

        std::vector<Player*> candidates;
        candidates.reserve(botsByRange[source].size());

        std::vector<Player*> unsafe;
        unsafe.reserve(botsByRange[source].size());
        for (Player* bot : botsByRange[source])
        {
            if (IsBotSafeForLevelReset(bot))
                candidates.push_back(bot);
            else
                unsafe.push_back(bot);
        }
        candidates.insert(candidates.end(), unsafe.begin(), unsafe.end());

        size_t candidateIndex = 0;
        while (actualCounts[source] > desiredCounts[source] && candidateIndex < candidates.size())
        {
            if (g_MinBracketChangeThreshold > 0 &&
                (actualCounts[source] - desiredCounts[source]) <= static_cast<int>(g_MinBracketChangeThreshold))
                break;

            int target = -1;
            for (uint32 i = 0; i < g_NumRanges; ++i)
            {
                if (actualCounts[i] < desiredCounts[i])
                {
                    target = static_cast<int>(i);
                    break;
                }
            }
            if (target < 0)
                break;

            Player* bot = candidates[candidateIndex++];
            if (!EnqueuePendingReset(bot->GetObjectGuid(), target, isAlliance ? TEAM_ALLIANCE : TEAM_HORDE))
                continue;

            --actualCounts[source];
            ++actualCounts[target];
        }
    }
}

static void RunDistributionCycle(ChatHandler* handler)
{
    bool expected = false;
    if (!g_DistributionInProgress.compare_exchange_strong(expected, true))
    {
        if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
            sLog.outString("[BotLevelBrackets] Distribution cycle already in progress, skipping.");
        if (handler)
            handler->SendSysMessage("[BotBrackets] Distribution cycle already in progress, skipping.");
        return;
    }

    LoadRealPlayerGuildIds();
    LoadSocialFriendList();

    HashMapHolder<Player>::MapType const& allPlayers = sObjectAccessor.GetPlayers();

    if (g_UseDynamicDistribution)
    {
        std::vector<int> allianceRealCounts(g_NumRanges, 0);
        std::vector<int> hordeRealCounts(g_NumRanges, 0);
        uint32 totalAllianceReal = 0;
        uint32 totalHordeReal    = 0;

        for (const auto& itr : allPlayers)
        {
            Player* player = itr.second;
            if (!player || !player->IsInWorld() || IsPlayerBot(player))
                continue;

            int rangeIndex = GetLevelRangeIndex(player->GetLevel(), player->GetTeamId());
            if (rangeIndex < 0)
                continue;

            if (player->GetTeamId() == TEAM_ALLIANCE)
            {
                allianceRealCounts[rangeIndex]++;
                totalAllianceReal++;
            }
            else if (player->GetTeamId() == TEAM_HORDE)
            {
                hordeRealCounts[rangeIndex]++;
                totalHordeReal++;
            }
        }

        const float baseline = 1.0f;
        std::vector<float> allianceWeights(g_NumRanges, 0.0f);
        std::vector<float> hordeWeights(g_NumRanges, 0.0f);

        if (g_SyncFactions)
        {
            uint32 totalCombined = totalAllianceReal + totalHordeReal;
            for (uint32 i = 0; i < g_NumRanges; ++i)
            {
                int combined = allianceRealCounts[i] + hordeRealCounts[i];
                float weight = baseline + g_RealPlayerWeight *
                    (totalCombined > 0 ? (1.0f / float(totalCombined)) : 1.0f) *
                    std::log(1.0f + combined);
                allianceWeights[i] = weight;
                hordeWeights[i]    = weight;
            }
        }
        else
        {
            for (uint32 i = 0; i < g_NumRanges; ++i)
            {
                allianceWeights[i] = (g_AllianceLevelRanges[i].lower > g_AllianceLevelRanges[i].upper) ? 0.0f :
                    (baseline + g_RealPlayerWeight * (totalAllianceReal > 0 ? (1.0f / totalAllianceReal) : 1.0f) * std::log(1.0f + allianceRealCounts[i]));

                hordeWeights[i] = (g_HordeLevelRanges[i].lower > g_HordeLevelRanges[i].upper) ? 0.0f :
                    (baseline + g_RealPlayerWeight * (totalHordeReal > 0 ? (1.0f / totalHordeReal) : 1.0f) * std::log(1.0f + hordeRealCounts[i]));
            }
        }

        auto applyWeights = [](std::vector<LevelRangeConfig>& ranges, const std::vector<float>& weights)
        {
            float total = 0.0f;
            for (uint32 i = 0; i < g_NumRanges; ++i)
                total += weights[i];

            int pctSum = 0;
            for (uint32 i = 0; i < g_NumRanges; ++i)
            {
                uint8 pct = (total > 0.0f) ? static_cast<uint8>(std::round((weights[i] / total) * 100)) : 0;
                ranges[i].desiredPercent = pct;
                pctSum += pct;
            }

            int missing = 100 - pctSum;
            for (uint32 i = 0; i < g_NumRanges && missing > 0; ++i)
            {
                if (ranges[i].lower <= ranges[i].upper && ranges[i].desiredPercent > 0)
                {
                    ranges[i].desiredPercent++;
                    missing--;
                }
            }
        };

        applyWeights(g_AllianceLevelRanges, allianceWeights);
        applyWeights(g_HordeLevelRanges, hordeWeights);
        ClampAndBalanceBrackets(false);
    }
    else
    {
        for (uint32 i = 0; i < g_NumRanges; ++i)
        {
            g_AllianceLevelRanges[i].desiredPercent = g_AllianceStaticPcts[i];
            g_HordeLevelRanges[i].desiredPercent    = g_HordeStaticPcts[i];
        }
    }

    uint32 totalAllianceBots = 0;
    uint32 totalHordeBots    = 0;
    std::vector<int> allianceActualCounts(g_NumRanges, 0);
    std::vector<int> hordeActualCounts(g_NumRanges, 0);
    std::vector<std::vector<Player*>> allianceBotsByRange(g_NumRanges);
    std::vector<std::vector<Player*>> hordeBotsByRange(g_NumRanges);

    for (const auto& itr : allPlayers)
    {
        Player* player = itr.second;
        if (!player || !player->IsInWorld() || ShouldExcludeBot(player))
            continue;

        if (IsAlliancePlayerBot(player))
        {
            totalAllianceBots++;
            int rangeIdx = GetOrFlagPlayerBracket(player);
            if (rangeIdx >= 0)
            {
                allianceActualCounts[rangeIdx]++;
                allianceBotsByRange[rangeIdx].push_back(player);
            }
        }
        else if (IsHordePlayerBot(player))
        {
            totalHordeBots++;
            int rangeIdx = GetOrFlagPlayerBracket(player);
            if (rangeIdx >= 0)
            {
                hordeActualCounts[rangeIdx]++;
                hordeBotsByRange[rangeIdx].push_back(player);
            }
        }
    }

    QueueFactionRedistribution(totalAllianceBots, g_AllianceLevelRanges, allianceActualCounts, allianceBotsByRange, true);
    QueueFactionRedistribution(totalHordeBots,    g_HordeLevelRanges,    hordeActualCounts,    hordeBotsByRange,    false);

    size_t pendingCount = 0;
    {
        std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
        pendingCount = g_PendingLevelResets.size();
    }

    if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Distribution cycle complete. Alliance bots: %u, Horde bots: %u, Pending resets: %u.",
            totalAllianceBots, totalHordeBots, (uint32)pendingCount);
    }

    if (handler)
    {
        handler->PSendSysMessage("[BotBrackets] Alliance: %u bots  Horde: %u bots  Pending: %u",
            totalAllianceBots, totalHordeBots, (uint32)pendingCount);
    }

    g_StatsCyclesRun.fetch_add(1, std::memory_order_relaxed);
    g_DistributionInProgress.store(false);
}

// -----------------------------------------------------------------------------
// WORLD SCRIPT: Periodic Distribution Management & Schedulers
// -----------------------------------------------------------------------------

class BotLevelBracketsWorldScript : public WorldScript
{
public:
    BotLevelBracketsWorldScript()
        : WorldScript("mod_twow_bot_level_brackets_world", { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_AFTER_CONFIG_LOAD }),
          m_timer(0), m_flaggedTimer(0), m_guildTrackerTimer(0), m_socialListTimer(0), m_teleportTimer(1000),
          m_hubPopulateTimer(500), m_startingZoneTimer(1000), m_wrongMapTimer(2000)
    {
    }

    void OnStartup() override
    {
        LoadBotLevelBracketsConfig();
        LoadSocialFriendList();
        LoadPersistentGuildTracker();
        LoadPendingResetsFromDB();

        if (!g_BotLevelBracketsEnabled)
        {
            sLog.outString("[BotLevelBrackets] Module disabled via configuration.");
            return;
        }

        if (BotEngineAdapter::IsRandomLevelsDisabled())
            sLog.outError("[BotLevelBrackets] Distribution paused because AiPlayerbot.DisableRandomLevels is enabled.");

        sLog.outString("[BotLevelBrackets] Module loaded (%s). Check: %us, Flagged: %us, Hub: %us, Starter: %us, WrongMap: %us.",
            BotEngineAdapter::GetEngineName(), g_BotDistCheckFrequency, g_BotDistFlaggedCheckFrequency,
            g_HubPopulateFrequency, g_StartingZoneDisperseFrequency, g_WrongMapDisperseFrequency);
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        LoadBotLevelBracketsConfig();
        LoadSocialFriendList();
        LoadPersistentGuildTracker();
        sLog.outString("[BotLevelBrackets] Configuration reloaded (%s).", BotEngineAdapter::GetEngineName());
    }

    void OnUpdate(uint32 diff) override
    {
        if (!g_BotLevelBracketsEnabled || BotEngineAdapter::IsRandomLevelsDisabled())
            return;

        m_timer             += diff;
        m_flaggedTimer      += diff;
        m_guildTrackerTimer += diff;
        m_socialListTimer   += diff;
        m_teleportTimer     += diff;
        m_hubPopulateTimer  += diff;
        m_startingZoneTimer += diff;
        m_wrongMapTimer     += diff;

        // Pending level resets processing
        if (m_flaggedTimer >= static_cast<uint64>(g_BotDistFlaggedCheckFrequency) * 1000ULL)
        {
            ProcessPendingLevelResets();
            m_flaggedTimer = 0;
        }

        // Pending teleports processing (2 seconds tick)
        if (m_teleportTimer >= 2000ULL)
        {
            ProcessPendingTeleports();
            m_teleportTimer = 0;
        }

        // Capital city hub ambiance population and dispersal
        if (m_hubPopulateTimer >= static_cast<uint64>(g_HubPopulateFrequency) * 1000ULL)
        {
            ProcessHubPopulate();
            m_hubPopulateTimer = 0;
        }

        // Starter zone dispersal (evacuates over-leveled bots)
        if (m_startingZoneTimer >= static_cast<uint64>(g_StartingZoneDisperseFrequency) * 1000ULL)
        {
            ProcessStartingZoneDisperse();
            m_startingZoneTimer = 0;
        }

        // Wrong-map dispersal (evacuates stranded bots)
        if (m_wrongMapTimer >= static_cast<uint64>(g_WrongMapDisperseFrequency) * 1000ULL)
        {
            ProcessWrongMapDisperse();
            m_wrongMapTimer = 0;
        }

        // Periodic guild tracker refresh
        if (g_IgnoreGuildBotsWithRealPlayers &&
            m_guildTrackerTimer >= static_cast<uint64>(g_GuildTrackerUpdateFrequency) * 1000ULL)
        {
            UpdatePersistentGuildTracker();
            m_guildTrackerTimer = 0;
        }

        // Decoupled social friend list refresh
        if (m_socialListTimer >= static_cast<uint64>(g_SocialListRefreshFrequency) * 1000ULL)
        {
            LoadSocialFriendList();
            m_socialListTimer = 0;
        }

        // Periodic full distribution check
        if (m_timer >= static_cast<uint64>(g_BotDistCheckFrequency) * 1000ULL)
        {
            RunDistributionCycle(nullptr);
            m_timer = 0;
        }
    }

private:
    uint64 m_timer;
    uint64 m_flaggedTimer;
    uint64 m_guildTrackerTimer;
    uint64 m_socialListTimer;
    uint64 m_teleportTimer;
    uint64 m_hubPopulateTimer;
    uint64 m_startingZoneTimer;
    uint64 m_wrongMapTimer;
};

// -----------------------------------------------------------------------------
// PLAYER SCRIPT: Session & State Lifecycle
// -----------------------------------------------------------------------------

class BotLevelBracketsPlayerScript : public PlayerScript
{
public:
    BotLevelBracketsPlayerScript()
        : PlayerScript("mod_twow_bot_level_brackets_player", { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT })
    {
    }

    void OnLogin(Player* player) override
    {
        if (!g_BotLevelBracketsEnabled || !g_IgnoreGuildBotsWithRealPlayers)
            return;
        if (!player || IsPlayerBot(player))
            return;

        uint32 guildId = player->GetGuildId();
        if (guildId == 0)
            return;

        {
            std::lock_guard<std::mutex> lock(g_SocialDataMutex);
            g_RealPlayerGuildIds.insert(guildId);
            g_PersistentRealPlayerGuildIds.insert(guildId);
        }
        CharacterDatabase.PExecute(
            "REPLACE INTO bot_level_brackets_guild_tracker (guild_id, has_real_players) VALUES (%u, 1)", guildId);

        if (g_BotDistFullDebugMode)
        {
            sLog.outString("[BotLevelBrackets] Real player '%s' logged in - guild %u marked as having real players.",
                player->GetName(), guildId);
        }
    }

    void OnLogout(Player* player) override
    {
        RemoveBotFromPendingResets(player);
        RemoveBotFromPendingTeleports(player);
        if (player)
        {
            std::lock_guard<std::mutex> lock(g_HubCooldownMutex);
            g_HubBotCooldowns.erase(player->GetObjectGuid());
        }
    }
};

// -----------------------------------------------------------------------------
// GUILD SCRIPT: Dual-Lifecycle Guild Membership Tracking
// -----------------------------------------------------------------------------

class BotLevelBracketsGuildScript : public GuildScript
{
public:
    BotLevelBracketsGuildScript() : GuildScript("mod_twow_bot_level_brackets_guild") {}

    void OnAddMember(Guild* guild, Player* player, uint8& /*rank*/) override
    {
        if (!g_BotLevelBracketsEnabled || !g_IgnoreGuildBotsWithRealPlayers)
            return;
        if (!player || IsPlayerBot(player))
            return;

        uint32 guildId = guild ? guild->GetId() : 0;
        if (guildId == 0)
            return;

        {
            std::lock_guard<std::mutex> lock(g_SocialDataMutex);
            g_RealPlayerGuildIds.insert(guildId);
            g_PersistentRealPlayerGuildIds.insert(guildId);
        }
        CharacterDatabase.PExecute(
            "REPLACE INTO bot_level_brackets_guild_tracker (guild_id, has_real_players) VALUES (%u, 1)", guildId);

        if (g_BotDistFullDebugMode)
        {
            sLog.outString("[BotLevelBrackets] Real player '%s' joined guild %u - marked as having real players.",
                player->GetName(), guildId);
        }
    }

    void OnRemoveMember(Guild* guild, Player* player, bool /*isDisbanding*/, bool /*isKicked*/) override
    {
        if (!g_BotLevelBracketsEnabled || !g_IgnoreGuildBotsWithRealPlayers)
            return;
        if (!player || IsPlayerBot(player))
            return;

        uint32 guildId = guild ? guild->GetId() : 0;
        if (guildId == 0)
            return;

        bool otherRealPlayerOnline = false;
        HashMapHolder<Player>::MapType const& allPlayers = sObjectAccessor.GetPlayers();
        for (const auto& itr : allPlayers)
        {
            Player* p = itr.second;
            if (!p || !p->IsInWorld() || p == player)
                continue;
            if (!IsPlayerBot(p) && p->GetGuildId() == guildId)
            {
                otherRealPlayerOnline = true;
                break;
            }
        }

        if (!otherRealPlayerOnline)
        {
            std::lock_guard<std::mutex> lock(g_SocialDataMutex);
            g_RealPlayerGuildIds.erase(guildId);
            if (g_BotDistFullDebugMode)
            {
                sLog.outString("[BotLevelBrackets] Real player '%s' left guild %u with no other online real players - removed from online cache.",
                    player->GetName(), guildId);
            }
        }
    }
};

// -----------------------------------------------------------------------------
// COMMAND SCRIPT: Pre-dispatch Chat & Console Command Interface
// -----------------------------------------------------------------------------

class BotLevelBracketsCommandScript : public AllCommandScript
{
public:
    BotLevelBracketsCommandScript() : AllCommandScript("mod_twow_bot_level_brackets_commands") {}

    bool CanExecuteCommand(ChatHandler* handler, char const* command, char const* args) override
    {
        if (!handler || !command)
            return true;

        std::string rootCommand = command;
        ToLowerInPlace(rootCommand);
        if (rootCommand != "blb" && rootCommand != "botbrackets" && rootCommand != "botlevelbrackets")
            return true;

        std::string subcommand = args ? args : "";
        size_t first = subcommand.find_first_not_of(' ');
        if (first == std::string::npos)
            subcommand.clear();
        else
        {
            subcommand.erase(0, first);
            size_t end = subcommand.find(' ');
            if (end != std::string::npos)
                subcommand.erase(end);
            ToLowerInPlace(subcommand);
        }

        WorldSession* session = handler->GetSession();
        auto hasSecurity = [session](AccountTypes security)
        {
            return !session || session->GetSecurity() >= security;
        };

        if (subcommand == "reload")
        {
            if (!hasSecurity(SEC_ADMINISTRATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            LoadBotLevelBracketsConfig();
            handler->SendSysMessage("[BotBrackets] Configuration reloaded successfully.");
        }
        else if (subcommand == "cleanup" || subcommand == "guildcleanup")
        {
            if (!hasSecurity(SEC_ADMINISTRATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            size_t before;
            {
                std::lock_guard<std::mutex> lock(g_SocialDataMutex);
                before = g_PersistentRealPlayerGuildIds.size();
            }
            CleanupGuildTracker();
            size_t after;
            {
                std::lock_guard<std::mutex> lock(g_SocialDataMutex);
                after = g_PersistentRealPlayerGuildIds.size();
            }
            handler->PSendSysMessage("[BotBrackets] Guild cleanup complete. Removed %u guild(s). %u guild(s) remain tracked.",
                (uint32)(before - after), (uint32)after);
        }
        else if (subcommand == "force")
        {
            if (!hasSecurity(SEC_ADMINISTRATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            handler->SendSysMessage("[BotBrackets] Forcing full distribution cycle...");
            RunDistributionCycle(handler);
            handler->SendSysMessage("[BotBrackets] Full distribution cycle complete.");
        }
        else if (subcommand == "status")
        {
            if (!hasSecurity(SEC_MODERATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            if (!g_BotLevelBracketsEnabled)
            {
                handler->SendSysMessage("[BotBrackets] Module is disabled.");
                return false;
            }

            uint32 totalAlliance = 0, totalHorde = 0;
            std::vector<int> allianceCounts(g_NumRanges, 0);
            std::vector<int> hordeCounts(g_NumRanges, 0);

            HashMapHolder<Player>::MapType const& allPlayers = sObjectAccessor.GetPlayers();
            for (const auto& itr : allPlayers)
            {
                Player* player = itr.second;
                if (!player || !player->IsInWorld() || ShouldExcludeBot(player))
                    continue;

                int rangeIndex = GetLevelRangeIndex(player->GetLevel(), player->GetTeamId());
                if (IsAlliancePlayerBot(player))
                {
                    totalAlliance++;
                    if (rangeIndex >= 0) allianceCounts[rangeIndex]++;
                }
                else if (IsHordePlayerBot(player))
                {
                    totalHorde++;
                    if (rangeIndex >= 0) hordeCounts[rangeIndex]++;
                }
            }

            handler->PSendSysMessage("[BotBrackets] Status (%s) - Alliance: %u bots, Horde: %u bots",
                BotEngineAdapter::GetEngineName(), totalAlliance, totalHorde);
            handler->SendSysMessage("Idx  Range    A.Des  A.Act  H.Des  H.Act");

            for (uint32 i = 0; i < g_NumRanges; ++i)
            {
                int aDes = (totalAlliance > 0)
                    ? static_cast<int>(std::round((g_AllianceLevelRanges[i].desiredPercent / 100.0) * totalAlliance))
                    : 0;
                int hDes = (totalHorde > 0)
                    ? static_cast<int>(std::round((g_HordeLevelRanges[i].desiredPercent / 100.0) * totalHorde))
                    : 0;

                handler->PSendSysMessage("%-4u %2u-%-5u %-6d %-6d %-6d %-6d",
                    i + 1,
                    g_AllianceLevelRanges[i].lower,
                    g_AllianceLevelRanges[i].upper,
                    aDes,
                    allianceCounts[i],
                    hDes,
                    hordeCounts[i]);
            }

            size_t pendingResets = 0, pendingTeleports = 0;
            {
                std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
                pendingResets = g_PendingLevelResets.size();
            }
            {
                std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
                pendingTeleports = g_PendingTeleports.size();
            }
            handler->PSendSysMessage("[BotBrackets] Pending Resets: %u, Pending Teleports: %u",
                (uint32)pendingResets, (uint32)pendingTeleports);
            handler->PSendSysMessage("[BotBrackets] Hub Mode: %s | Safeguards (Hysteresis:%u bots, Stability:%us, Cooldown:%us)",
                (g_HubDisperseQuotaMode == 1 ? "Headcount" : "Percentage"),
                g_HubDisperseHysteresisBots, g_HubDisperseStabilitySeconds, g_HubDisperseCooldownSeconds);
            if (g_HubDisperseMaxBotsPerBracket > 0 || g_HubDisperseMinBotsPerBracket > 0)
            {
                handler->PSendSysMessage("[BotBrackets] Hub Bracket Limits: Min=%u, Max=%u per bracket",
                    g_HubDisperseMinBotsPerBracket, g_HubDisperseMaxBotsPerBracket);
            }
            handler->PSendSysMessage("[BotBrackets] Quotas (SW:%u IF:%u DA:%u AT:%u | OR:%u UC:%u TB:%u SP:%u)",
                GetHubQuota(0, totalAlliance), GetHubQuota(1, totalAlliance), GetHubQuota(2, totalAlliance), GetHubQuota(3, totalAlliance),
                GetHubQuota(4, totalHorde), GetHubQuota(5, totalHorde), GetHubQuota(6, totalHorde), GetHubQuota(7, totalHorde));
        }
        else if (subcommand == "pending")
        {
            if (!hasSecurity(SEC_MODERATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            size_t resetQueueSize = 0, teleportQueueSize = 0;
            uint32 oldest = std::numeric_limits<uint32>::max();
            uint32 newest = 0;
            {
                std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
                resetQueueSize = g_PendingLevelResets.size();
                for (const auto& kv : g_PendingLevelResets)
                {
                    if (kv.second.enqueuedAt < oldest) oldest = kv.second.enqueuedAt;
                    if (kv.second.enqueuedAt > newest) newest = kv.second.enqueuedAt;
                }
            }
            {
                std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
                teleportQueueSize = g_PendingTeleports.size();
            }

            handler->PSendSysMessage("[BotBrackets] Pending Resets Queue: %u entries (Cap: %u, TTL: %us)",
                (uint32)resetQueueSize, g_MaxPendingQueueSize, g_PendingQueueTTL);
            handler->PSendSysMessage("[BotBrackets] Pending Teleports Queue: %u entries", (uint32)teleportQueueSize);

            if (resetQueueSize > 0)
            {
                uint32 now = static_cast<uint32>(time(nullptr));
                handler->PSendSysMessage("[BotBrackets] Oldest reset: %us ago. Newest reset: %us ago.",
                    now - oldest, now - newest);
            }
        }
        else if (subcommand == "stats")
        {
            if (!hasSecurity(SEC_MODERATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            handler->SendSysMessage("[BotBrackets] Cumulative Statistics Since Startup:");
            handler->PSendSysMessage("  Cycles Run:        %u", (uint32)g_StatsCyclesRun.load());
            handler->PSendSysMessage("  Bots Adjusted:     %u", (uint32)g_StatsBotsAdjusted.load());
            handler->PSendSysMessage("  Bots Teleported:   %u", (uint32)g_StatsBotsTeleported.load());
            handler->PSendSysMessage("  Bots Skipped:      %u", (uint32)g_StatsBotsSkipped.load());
            handler->PSendSysMessage("  Cooldown Skips:    %u", (uint32)g_StatsCooldownSkips.load());

            size_t resetCount = 0, teleportCount = 0, cooldownCount = 0;
            {
                std::lock_guard<std::mutex> lock(g_PendingLevelResetsMutex);
                resetCount = g_PendingLevelResets.size();
            }
            {
                std::lock_guard<std::mutex> lock(g_PendingTeleportsMutex);
                teleportCount = g_PendingTeleports.size();
            }
            {
                std::lock_guard<std::mutex> lock(g_HubCooldownMutex);
                cooldownCount = g_HubBotCooldowns.size();
            }
            handler->PSendSysMessage("  Active Queue:      Resets=%u, Teleports=%u, BotCooldowns=%u",
                (uint32)resetCount, (uint32)teleportCount, (uint32)cooldownCount);
        }
        else if (subcommand == "hubs" || subcommand == "hub")
        {
            if (!hasSecurity(SEC_MODERATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            uint32 totalAlliance = 0, totalHorde = 0;
            HashMapHolder<Player>::MapType const& allPlayers = sObjectAccessor.GetPlayers();
            for (const auto& itr : allPlayers)
            {
                Player* player = itr.second;
                if (!player || !player->IsInWorld() || ShouldExcludeBot(player))
                    continue;
                if (IsAlliancePlayerBot(player))
                    totalAlliance++;
                else if (IsHordePlayerBot(player))
                    totalHorde++;
            }

            static const char* hubNames[8] = {
                "Stormwind", "Ironforge", "Darnassus", "Alah'Thalas",
                "Orgrimmar", "Undercity", "Thunder Bluff", "Sparkwater"
            };

            int64 now = GetSteadySeconds();
            handler->PSendSysMessage("[BotBrackets] Capital Hub Diagnostics (Hysteresis: %u bots, Stability: %us, Cooldown: %us | Min/Max per Bracket: %u/%u):",
                g_HubDisperseHysteresisBots, g_HubDisperseStabilitySeconds, g_HubDisperseCooldownSeconds,
                g_HubDisperseMinBotsPerBracket, g_HubDisperseMaxBotsPerBracket);
            handler->SendSysMessage("Hub            Team  Target  EffPop  State           Duration  Status");

            for (size_t i = 0; i < s_NumHubAreas; ++i)
            {
                uint32 factionTotal = (s_HubAreas[i].teamId == TEAM_ALLIANCE) ? totalAlliance : totalHorde;
                uint32 quota = GetHubQuota(i, factionTotal);
                const HubStabilityRecord& stab = g_HubStability[i];

                const char* stateStr = "Neutral";
                if (stab.currentDirection == HubImbalanceDirection::Underpopulated)
                    stateStr = "Deficit";
                else if (stab.currentDirection == HubImbalanceDirection::Overpopulated)
                    stateStr = "Surplus";

                int64 duration = (stab.imbalanceStartTime > 0) ? (now - stab.imbalanceStartTime) : 0;
                const char* statusStr = "Balanced";
                if (stab.currentDirection != HubImbalanceDirection::Neutral)
                {
                    if (duration >= static_cast<int64>(g_HubDisperseStabilitySeconds))
                        statusStr = "Actionable";
                    else
                        statusStr = "Waiting";
                }

                handler->PSendSysMessage("%-14s %-5s %-7u %-7d %-15s %-9d %s",
                    hubNames[i],
                    (s_HubAreas[i].teamId == TEAM_ALLIANCE ? "Alli" : "Horde"),
                    quota,
                    stab.lastEffectivePopulation,
                    stateStr,
                    (int32)duration,
                    statusStr);
            }
        }
        else
        {
            handler->SendSysMessage("Usage: .blb <reload | status | force | pending | cleanup | stats | hubs>");
        }

        return false;
    }
};

// -----------------------------------------------------------------------------
// ENTRY POINT: Register All Scripts
// -----------------------------------------------------------------------------

void Addmod_twow_bot_level_bracketsScripts()
{
    new BotLevelBracketsWorldScript();
    new BotLevelBracketsPlayerScript();
    new BotLevelBracketsGuildScript();
    new BotLevelBracketsCommandScript();
}
