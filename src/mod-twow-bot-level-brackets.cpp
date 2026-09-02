#include "mod-twow-bot-level-brackets.h"
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
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
// PlayerBots headers depend on the ordered Tortoise compatibility bundle.
#include "botpch.h"
#include "playerbot/playerbot.h"
#include "playerbot/BotSlots.h"
#include "playerbot/PlayerbotAI.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/RandomPlayerbotMgr.h"
#include "playerbot/PlayerbotFactory.h"
#include "playerbot/ChatHelper.h"

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

// Forward declarations
static bool IsAlliancePlayerBot(Player* bot);
static bool IsHordePlayerBot(Player* bot);
static void ClampAndBalanceBrackets(bool warnLayout = true);

// -----------------------------------------------------------------------------
// LEVEL RANGE CONFIGURATION
// -----------------------------------------------------------------------------
struct LevelRangeConfig
{
    uint8 lower;          ///< Lower bound (inclusive)
    uint8 upper;          ///< Upper bound (inclusive)
    uint8 desiredPercent; ///< Desired percentage of bots in this range
};

// Configurable number of brackets (default 7 for Vanilla 1-60)
static uint32 g_NumRanges = 7;

// Global bot level bounds
static uint8 g_RandomBotMinLevel = 1;
static uint8 g_RandomBotMaxLevel = 60;

// Module toggles and settings
static bool   g_BotLevelBracketsEnabled           = true;
static bool   g_IgnoreGuildBotsWithRealPlayers   = true;
static bool   g_BotDistFullDebugMode             = false;
static bool   g_BotDistLiteDebugMode             = false;
static uint32 g_BotDistCheckFrequency            = 300; // in seconds
static uint32 g_BotDistFlaggedCheckFrequency     = 15;  // in seconds
static uint32 g_GuildTrackerUpdateFrequency      = 600; // in seconds (10 minutes)
static bool   g_UseDynamicDistribution           = false;
static bool   g_IgnoreFriendListed               = true;
static uint32 g_FlaggedProcessLimit              = 5;   // 0 = unlimited
static float  g_RealPlayerWeight                 = 1.0f;
static bool   g_SyncFactions                     = false;

// Vectors to store configured level ranges
static std::vector<LevelRangeConfig> g_AllianceLevelRanges;
static std::vector<LevelRangeConfig> g_HordeLevelRanges;

// Friends list GUIDs cache
static std::unordered_set<uint32> g_SocialFriendsList;

// Excluded bot names list
static std::vector<std::string> g_ExcludeBotNames;

// Guild ID caches
static std::unordered_set<uint32> g_RealPlayerGuildIds;
static std::unordered_set<uint32> g_PersistentRealPlayerGuildIds;

// Pending level reset entry
struct PendingResetEntry
{
    ObjectGuid botGuid;
    int targetRange;
    uint8 teamId;
};
static std::vector<PendingResetEntry> g_PendingLevelResets;

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

/**
 * Loads configuration values from the server config manager.
 */
static void LoadBotLevelBracketsConfig()
{
    g_BotLevelBracketsEnabled           = sConfig.GetBoolDefault("BotLevelBrackets.Enabled", true);
    g_IgnoreGuildBotsWithRealPlayers   = sConfig.GetBoolDefault("BotLevelBrackets.IgnoreGuildBotsWithRealPlayers", true);
    g_BotDistFullDebugMode             = sConfig.GetBoolDefault("BotLevelBrackets.FullDebugMode", false);
    g_BotDistLiteDebugMode             = sConfig.GetBoolDefault("BotLevelBrackets.LiteDebugMode", false);
    g_BotDistCheckFrequency            = ReadBoundedUInt("BotLevelBrackets.CheckFrequency", 300, 1, 86400);
    g_BotDistFlaggedCheckFrequency     = ReadBoundedUInt("BotLevelBrackets.CheckFlaggedFrequency", 15, 1, 86400);
    g_GuildTrackerUpdateFrequency      = ReadBoundedUInt("BotLevelBrackets.GuildTrackerUpdateFrequency", 600, 1, 86400);
    g_UseDynamicDistribution           = sConfig.GetBoolDefault("BotLevelBrackets.Dynamic.UseDynamicDistribution", false);
    g_RealPlayerWeight                 = std::max(0.0f, std::min(100.0f,
        sConfig.GetFloatDefault("BotLevelBrackets.Dynamic.RealPlayerWeight", 1.0f)));
    g_SyncFactions                     = sConfig.GetBoolDefault("BotLevelBrackets.Dynamic.SyncFactions", false);
    g_IgnoreFriendListed               = sConfig.GetBoolDefault("BotLevelBrackets.IgnoreFriendListed", true);
    g_FlaggedProcessLimit              = ReadBoundedUInt("BotLevelBrackets.FlaggedProcessLimit", 5, 0, 10000);

    std::string excludeNames = sConfig.GetStringDefault("BotLevelBrackets.ExcludeNames", "");
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

    // Load bot level restrictions from playerbot config or world config
    uint32 configuredMinLevel = sPlayerbotAIConfig.randomBotMinLevel > 0 ? sPlayerbotAIConfig.randomBotMinLevel : 1;
    uint32 configuredMaxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    uint32 maxCfgLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    if (maxCfgLevel == 0)
        maxCfgLevel = 60;
    if (configuredMaxLevel == 0 || configuredMaxLevel > maxCfgLevel)
        configuredMaxLevel = maxCfgLevel;
    configuredMinLevel = std::min(configuredMinLevel, configuredMaxLevel);
    g_RandomBotMinLevel = static_cast<uint8>(std::min<uint32>(configuredMinLevel, 255));
    g_RandomBotMaxLevel = static_cast<uint8>(std::min<uint32>(configuredMaxLevel, 255));

    // Number of brackets
    uint32 const maximumUsefulRanges = std::max<uint32>(1, g_RandomBotMaxLevel - g_RandomBotMinLevel + 1);
    g_NumRanges = ReadBoundedUInt("BotLevelBrackets.NumRanges", 7, 1, std::min<uint32>(60, maximumUsefulRanges));

    g_AllianceLevelRanges.resize(g_NumRanges);
    g_HordeLevelRanges.resize(g_NumRanges);

    // Load Alliance bracket configuration
    for (uint32 i = 0; i < g_NumRanges; ++i)
    {
        std::string idx = std::to_string(i + 1);
        uint32 defaultLower = (i == 0 ? 1 : i * 10);
        uint32 defaultUpper = (i < g_NumRanges - 1 ? i * 10 + 9 : g_RandomBotMaxLevel);
        uint32 defaultPct   = (i == 0 ? 16 : 14);

        g_AllianceLevelRanges[i].lower = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Alliance.Range" + idx + ".Lower").c_str(), defaultLower, 0, 255));
        g_AllianceLevelRanges[i].upper = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Alliance.Range" + idx + ".Upper").c_str(), defaultUpper, 0, 255));
        g_AllianceLevelRanges[i].desiredPercent = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Alliance.Range" + idx + ".Pct").c_str(), defaultPct, 0, 100));
    }

    // Load Horde bracket configuration
    for (uint32 i = 0; i < g_NumRanges; ++i)
    {
        std::string idx = std::to_string(i + 1);
        uint32 defaultLower = (i == 0 ? 1 : i * 10);
        uint32 defaultUpper = (i < g_NumRanges - 1 ? i * 10 + 9 : g_RandomBotMaxLevel);
        uint32 defaultPct   = (i == 0 ? 16 : 14);

        g_HordeLevelRanges[i].lower = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Horde.Range" + idx + ".Lower").c_str(), defaultLower, 0, 255));
        g_HordeLevelRanges[i].upper = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Horde.Range" + idx + ".Upper").c_str(), defaultUpper, 0, 255));
        g_HordeLevelRanges[i].desiredPercent = static_cast<uint8>(ReadBoundedUInt(("BotLevelBrackets.Horde.Range" + idx + ".Pct").c_str(), defaultPct, 0, 100));
    }

    // Validate synchronization if enabled
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
}

/**
 * Checks if the given player is a bot.
 */
static bool IsPlayerBot(Player* player)
{
    return player && GetBotAI(player) != nullptr;
}

/**
 * Checks if the given player is a random bot.
 */
static bool IsPlayerRandomBot(Player* player)
{
    return player && sRandomPlayerbotMgr.IsRandomBot(player);
}

/**
 * Checks if the bot belongs to the Alliance team.
 */
static bool IsAlliancePlayerBot(Player* bot)
{
    return bot && (bot->GetTeamId() == TEAM_ALLIANCE);
}

/**
 * Checks if the bot belongs to the Horde team.
 */
static bool IsHordePlayerBot(Player* bot)
{
    return bot && (bot->GetTeamId() == TEAM_HORDE);
}

/**
 * Removes a bot from the pending level resets list.
 */
static void RemoveBotFromPendingResets(Player* bot)
{
    if (!bot)
        return;
    ObjectGuid guid = bot->GetObjectGuid();
    g_PendingLevelResets.erase(
        std::remove_if(
            g_PendingLevelResets.begin(),
            g_PendingLevelResets.end(),
            [guid](const PendingResetEntry& entry) { return entry.botGuid == guid; }
        ),
        g_PendingLevelResets.end()
    );
}

/**
 * Loads the social friends list from the database.
 */
static void LoadSocialFriendList()
{
    g_SocialFriendsList.clear();
    if (!g_IgnoreFriendListed)
        return;

    // Only relationships created by human-account characters protect a bot.
    // Bot-to-bot friend links must not remove random bots from distribution.
    std::unique_ptr<QueryResult> result(CharacterDatabase.Query(
        "SELECT cs.friend, c.account "
        "FROM character_social cs "
        "INNER JOIN characters c ON c.guid = cs.guid "
        "WHERE (cs.flags & 1) <> 0"));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 friendGuid = fields[0].GetUInt32();
        uint32 ownerAccount = fields[1].GetUInt32();
        if (!sPlayerbotAIConfig.IsInRandomAccountList(ownerAccount))
            g_SocialFriendsList.insert(friendGuid);
    } while (result->NextRow());

    if (g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Loaded %u human-owned friend relations from character_social.", (uint32)g_SocialFriendsList.size());
    }
}

/**
 * Loads persistent guild tracker data from the database.
 */
static void LoadPersistentGuildTracker(bool force = false)
{
    g_PersistentRealPlayerGuildIds.clear();
    if (!force && !g_IgnoreGuildBotsWithRealPlayers)
        return;

    std::unique_ptr<QueryResult> result(CharacterDatabase.Query("SELECT guild_id FROM bot_level_brackets_guild_tracker WHERE has_real_players = 1"));
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 guildId = fields[0].GetUInt32();
        g_PersistentRealPlayerGuildIds.insert(guildId);
    } while (result->NextRow());

    if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Loaded %u guilds with real players from persistent tracker.", (uint32)g_PersistentRealPlayerGuildIds.size());
    }
}

/**
 * Updates persistent guild tracker table based on online players.
 */
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
    for (uint32 guildId : currentRealPlayerGuilds)
    {
        CharacterDatabase.PExecute("REPLACE INTO bot_level_brackets_guild_tracker (guild_id, has_real_players) VALUES (%u, 1)", guildId);
        g_PersistentRealPlayerGuildIds.insert(guildId);
        addedCount++;
    }

    if (g_BotDistFullDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Guild tracker update complete: %u active guilds tracked.", (uint32)g_PersistentRealPlayerGuildIds.size());
    }
}

/**
 * Cleans up guilds with no online real players.
 */
static void CleanupGuildTracker()
{
    // The feature may currently be disabled, so force a fresh snapshot for
    // this explicit administrative cleanup operation.
    LoadPersistentGuildTracker(true);

    std::unordered_set<uint32> guildsWithHumanAccountMembers;
    std::unique_ptr<QueryResult> members(CharacterDatabase.Query(
        "SELECT DISTINCT gm.guildid, c.account "
        "FROM guild_member gm "
        "INNER JOIN characters c ON c.guid = gm.guid "
        "INNER JOIN bot_level_brackets_guild_tracker t ON t.guild_id = gm.guildid "
        "WHERE t.has_real_players = 1"));
    if (members)
    {
        do
        {
            Field* fields = members->Fetch();
            uint32 guildId = fields[0].GetUInt32();
            uint32 accountId = fields[1].GetUInt32();
            if (!sPlayerbotAIConfig.IsInRandomAccountList(accountId))
                guildsWithHumanAccountMembers.insert(guildId);
        } while (members->NextRow());
    }

    std::vector<uint32> guildsToRemove;
    for (uint32 trackedGuildId : g_PersistentRealPlayerGuildIds)
    {
        if (guildsWithHumanAccountMembers.count(trackedGuildId) == 0)
            guildsToRemove.push_back(trackedGuildId);
    }

    for (uint32 guildId : guildsToRemove)
    {
        CharacterDatabase.PExecute("UPDATE bot_level_brackets_guild_tracker SET has_real_players = 0 WHERE guild_id = %u", guildId);
        g_PersistentRealPlayerGuildIds.erase(guildId);
        g_RealPlayerGuildIds.erase(guildId);
    }

    if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
    {
        sLog.outString("[BotLevelBrackets] Guild tracker cleanup: removed %u guilds without human-account members.", (uint32)guildsToRemove.size());
    }
}

/**
 * Refreshes online real player guild cache.
 */
static void LoadRealPlayerGuildIds()
{
    g_RealPlayerGuildIds.clear();
    if (!g_IgnoreGuildBotsWithRealPlayers)
        return;

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
                g_RealPlayerGuildIds.insert(guildId);
        }
    }
}

/**
 * Returns the bracket index for a given level and faction.
 */
static int GetLevelRangeIndex(uint8 level, uint8 teamID)
{
    if (level < g_RandomBotMinLevel || level > g_RandomBotMaxLevel)
        return -1;

    if (teamID == TEAM_ALLIANCE)
    {
        for (uint32 i = 0; i < g_NumRanges; ++i)
        {
            if (level >= g_AllianceLevelRanges[i].lower && level <= g_AllianceLevelRanges[i].upper)
                return i;
        }
    }
    else if (teamID == TEAM_HORDE)
    {
        for (uint32 i = 0; i < g_NumRanges; ++i)
        {
            if (level >= g_HordeLevelRanges[i].lower && level <= g_HordeLevelRanges[i].upper)
                return i;
        }
    }

    return -1;
}

/**
 * Returns a random level within a given range.
 */
static uint8 GetRandomLevelInRange(const LevelRangeConfig& range)
{
    return static_cast<uint8>(urand(range.lower, range.upper));
}

/**
 * Adjusts a bot's level to a random level inside target bracket and randomizes it.
 */
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

static bool AdjustBotToRange(Player* bot, int targetRangeIndex)
{
    if (sPlayerbotAIConfig.disableRandomLevels || !bot || !bot->IsInWorld() || !bot->GetSession() ||
        bot->GetSession()->isLogingOut() || bot->IsStunnedByLogout())
        return false;

    std::vector<LevelRangeConfig> const* factionRanges = GetFactionRanges(bot);
    if (!factionRanges || targetRangeIndex < 0 || static_cast<uint32>(targetRangeIndex) >= factionRanges->size())
        return false;

    uint8 botOriginalLevel = bot->GetLevel();
    uint8 newLevel = 0;

    // Death Knight minimum level safeguard
    if (bot->getClass() == CLASS_DEATH_KNIGHT)
    {
        uint8 lowerBound = (*factionRanges)[targetRangeIndex].lower;
        uint8 upperBound = (*factionRanges)[targetRangeIndex].upper;
        if (upperBound < 55)
            return false;
        if (lowerBound < 55)
            lowerBound = 55;
        if (lowerBound > upperBound)
            return false;
        newLevel = static_cast<uint8>(urand(lowerBound, upperBound));
    }
    else
    {
        const LevelRangeConfig& range = (*factionRanges)[targetRangeIndex];
        if (range.lower > range.upper)
            return false;
        newLevel = GetRandomLevelInRange(range);
    }

    if (newLevel < g_RandomBotMinLevel)
        newLevel = g_RandomBotMinLevel;
    if (newLevel > g_RandomBotMaxLevel)
        newLevel = g_RandomBotMaxLevel;

    // Re-level and randomize bot via PlayerbotFactory
    PlayerbotFactory newFactory(bot, newLevel);
    newFactory.Randomize(false, false);

    if (g_BotDistFullDebugMode)
    {
        const char* playerFaction = IsAlliancePlayerBot(bot) ? "Alliance" : "Horde";
        std::string className = ChatHelper::formatClass(bot->getClass());
        sLog.outString("[BotLevelBrackets] %s bot '%s' (%s, Lvl %u) adjusted to Level %u (Bracket %u-%u).",
            playerFaction, bot->GetName(), className.c_str(), botOriginalLevel, newLevel,
            (*factionRanges)[targetRangeIndex].lower, (*factionRanges)[targetRangeIndex].upper);
    }

    ChatHandler(bot->GetSession()).SendSysMessage("[mod-twow-bot-level-brackets] Your level has been reset.");
    return true;
}

/**
 * Checks if a bot is in a guild with real players.
 */
static bool BotInGuildWithRealPlayer(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return false;

    uint32 guildId = bot->GetGuildId();
    if (guildId == 0)
        return false;

    return g_RealPlayerGuildIds.count(guildId) > 0 || g_PersistentRealPlayerGuildIds.count(guildId) > 0;
}

/**
 * Checks if a bot is on a real player's friend list.
 */
static bool BotInFriendList(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->GetSession())
        return false;

    return g_SocialFriendsList.count(bot->GetObjectGuid().GetCounter()) > 0;
}

/**
 * Clamps bracket boundaries and balances percentage sum to 100%.
 */
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

        // Produce an exact 100% sum even for malformed or rounded input.
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

/**
 * Checks if a bot is currently in a safe state for a level change.
 */
static bool IsBotSafeForLevelReset(Player* bot)
{
    if (sPlayerbotAIConfig.disableRandomLevels || !bot || !bot->GetSession() ||
        bot->GetSession()->isLogingOut() || bot->IsStunnedByLogout())
        return false;

    if (!bot->IsInWorld() || !bot->IsAlive())
        return false;

    if (bot->IsInCombat())
        return false;

    if (bot->InBattleGround() || bot->InBattleGroundQueue())
        return false;

    if (bot->IsTaxiFlying())
        return false;

    if (bot->IsBeingTeleported())
        return false;

    if (bot->IsMounted() || bot->GetTrader() || !bot->GetLootGuid().IsEmpty())
        return false;

    if (bot->IsNonMeleeSpellCasted(false, false, false))
        return false;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsInWorld() && !IsPlayerBot(member))
            {
                return false;
            }
        }
    }

    return true;
}

static bool BotHasRealPlayerMaster(Player* bot)
{
    PlayerbotAI* ai = bot ? GetBotAI(bot) : nullptr;
    return ai && ai->HasRealPlayerMaster();
}

/**
 * Checks if a bot is excluded by name.
 */
static bool IsBotExcluded(Player* bot)
{
    if (!bot)
        return false;

    std::string name = bot->GetName();
    ToLowerInPlace(name);

    for (const auto& excluded : g_ExcludeBotNames)
    {
        if (excluded == name)
            return true;
    }
    return false;
}

static bool IsPendingReset(ObjectGuid guid)
{
    return std::find_if(g_PendingLevelResets.begin(), g_PendingLevelResets.end(), [guid](PendingResetEntry const& entry)
    {
        return entry.botGuid == guid;
    }) != g_PendingLevelResets.end();
}

static bool QueuePendingReset(Player* bot, int targetRange)
{
    if (!bot || targetRange < 0 || static_cast<uint32>(targetRange) >= g_NumRanges)
        return false;

    ObjectGuid guid = bot->GetObjectGuid();
    if (IsPendingReset(guid))
        return false;

    g_PendingLevelResets.push_back({guid, targetRange, static_cast<uint8>(bot->GetTeamId())});
    return true;
}

/**
 * Processes pending level resets for bots that were previously flagged.
 */
static void ProcessPendingLevelResets()
{
    if (g_PendingLevelResets.empty())
        return;

    uint32 processed = 0;
    for (auto it = g_PendingLevelResets.begin(); it != g_PendingLevelResets.end();)
    {
        if (g_FlaggedProcessLimit > 0 && processed >= g_FlaggedProcessLimit)
            break;

        Player* bot = sObjectAccessor.FindPlayer(it->botGuid);
        if (!bot || !bot->IsInWorld() || !bot->GetSession() || bot->GetSession()->isLogingOut() ||
            !IsPlayerBot(bot) || !IsPlayerRandomBot(bot) || static_cast<uint8>(bot->GetTeamId()) != it->teamId)
        {
            it = g_PendingLevelResets.erase(it);
            continue;
        }

        if (IsBotExcluded(bot) ||
            (g_IgnoreGuildBotsWithRealPlayers && BotInGuildWithRealPlayer(bot)) ||
            (g_IgnoreFriendListed && BotInFriendList(bot)) ||
            BotHasRealPlayerMaster(bot))
        {
            it = g_PendingLevelResets.erase(it);
            continue;
        }

        if (IsBotSafeForLevelReset(bot))
        {
            if (AdjustBotToRange(bot, it->targetRange))
            {
                it = g_PendingLevelResets.erase(it);
                ++processed;
            }
            else
                it = g_PendingLevelResets.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

/**
 * Determines a player's bracket or flags a bot for redistribution if out of bounds.
 */
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
        if (BotHasRealPlayerMaster(player))
            return -1;
    }

    int rangeIndex = GetLevelRangeIndex(player->GetLevel(), player->GetTeamId());
    if (rangeIndex >= 0)
        return rangeIndex;

    if (!IsPlayerBot(player))
        return -1;

    std::vector<LevelRangeConfig> const* factionRangesVector = GetFactionRanges(player);
    LevelRangeConfig const* factionRanges = nullptr;
    if (!factionRangesVector)
        return -1;
    factionRanges = factionRangesVector->data();

    int targetRange = -1;
    int smallestDiff = std::numeric_limits<int>::max();

    for (uint32 i = 0; i < g_NumRanges; ++i)
    {
        if (factionRanges[i].lower > factionRanges[i].upper)
            continue;

        if (player->getClass() == CLASS_DEATH_KNIGHT && factionRanges[i].upper < 55)
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
        QueuePendingReset(player, targetRange);
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
    std::vector<std::vector<Player*>> const& botsByRange)
{
    if (totalBots == 0)
        return;

    std::vector<int> desiredCounts = CalculateDesiredCounts(totalBots, ranges);
    for (uint32 source = 0; source < g_NumRanges; ++source)
    {
        std::vector<Player*> candidates;
        candidates.reserve(botsByRange[source].size());

        // Prefer bots that are safe now, but retain unsafe surplus bots in the
        // pending queue so they can be handled once combat/travel/group state clears.
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
            if (!QueuePendingReset(bot, target))
                continue;

            --actualCounts[source];
            ++actualCounts[target];
        }
    }
}

// -----------------------------------------------------------------------------
// WORLD SCRIPT: Periodic Distribution Management
// -----------------------------------------------------------------------------
class BotLevelBracketsWorldScript : public WorldScript
{
public:
    BotLevelBracketsWorldScript()
        : WorldScript("mod_twow_bot_level_brackets_world", { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_AFTER_CONFIG_LOAD }),
          m_timer(0), m_flaggedTimer(0), m_guildTrackerTimer(0)
    {
    }

    void OnStartup() override
    {
        LoadBotLevelBracketsConfig();
        LoadSocialFriendList();
        LoadPersistentGuildTracker();

        if (!g_BotLevelBracketsEnabled)
        {
            sLog.outString("[BotLevelBrackets] Module disabled via configuration.");
            return;
        }

        if (sPlayerbotAIConfig.disableRandomLevels)
            sLog.outError("[BotLevelBrackets] Distribution paused because AiPlayerbot.DisableRandomLevels is enabled.");

        sLog.outString("[BotLevelBrackets] Module loaded. Check frequency: %u s, Flagged check frequency: %u s.",
            g_BotDistCheckFrequency, g_BotDistFlaggedCheckFrequency);
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        LoadBotLevelBracketsConfig();
        LoadSocialFriendList();
        LoadPersistentGuildTracker();
        // Targets are bracket indices. Recompute them after any config reload.
        g_PendingLevelResets.clear();
        sLog.outString("[BotLevelBrackets] Configuration reloaded.");
    }

    void OnUpdate(uint32 diff) override
    {
        if (!g_BotLevelBracketsEnabled || sPlayerbotAIConfig.disableRandomLevels)
            return;

        m_timer += diff;
        m_flaggedTimer += diff;
        m_guildTrackerTimer += diff;

        // Process pending resets
        if (m_flaggedTimer >= static_cast<uint64>(g_BotDistFlaggedCheckFrequency) * 1000ULL)
        {
            ProcessPendingLevelResets();
            m_flaggedTimer = 0;
        }

        // Periodic guild tracker update
        if (g_IgnoreGuildBotsWithRealPlayers &&
            m_guildTrackerTimer >= static_cast<uint64>(g_GuildTrackerUpdateFrequency) * 1000ULL)
        {
            UpdatePersistentGuildTracker();
            m_guildTrackerTimer = 0;
        }

        // Periodic bot distribution check
        if (m_timer < static_cast<uint64>(g_BotDistCheckFrequency) * 1000ULL)
            return;
        m_timer = 0;

        LoadRealPlayerGuildIds();
        LoadSocialFriendList();

        HashMapHolder<Player>::MapType const& allPlayers = sObjectAccessor.GetPlayers();

        // Rebuild the pending plan from current world state. This keeps queued
        // targets consistent with dynamic weights and avoids duplicate/stale work.
        g_PendingLevelResets.clear();

        // Dynamic Distribution Logic
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

                int rangeIndex = GetOrFlagPlayerBracket(player);
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

        // Count bots in each bracket
        uint32 totalAllianceBots = 0;
        uint32 totalHordeBots    = 0;
        std::vector<int> allianceActualCounts(g_NumRanges, 0);
        std::vector<int> hordeActualCounts(g_NumRanges, 0);
        std::vector<std::vector<Player*>> allianceBotsByRange(g_NumRanges);
        std::vector<std::vector<Player*>> hordeBotsByRange(g_NumRanges);

        for (const auto& itr : allPlayers)
        {
            Player* player = itr.second;
            if (!player || !player->IsInWorld() || !IsPlayerBot(player) || !IsPlayerRandomBot(player))
                continue;

            if (IsBotExcluded(player) ||
                (g_IgnoreGuildBotsWithRealPlayers && BotInGuildWithRealPlayer(player)) ||
                (g_IgnoreFriendListed && BotInFriendList(player)) ||
                BotHasRealPlayerMaster(player))
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
                else
                {
                    auto pending = std::find_if(g_PendingLevelResets.begin(), g_PendingLevelResets.end(), [player](PendingResetEntry const& entry)
                    {
                        return entry.botGuid == player->GetObjectGuid();
                    });
                    if (pending != g_PendingLevelResets.end() && pending->targetRange >= 0 &&
                        static_cast<uint32>(pending->targetRange) < g_NumRanges)
                        ++allianceActualCounts[pending->targetRange];
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
                else
                {
                    auto pending = std::find_if(g_PendingLevelResets.begin(), g_PendingLevelResets.end(), [player](PendingResetEntry const& entry)
                    {
                        return entry.botGuid == player->GetObjectGuid();
                    });
                    if (pending != g_PendingLevelResets.end() && pending->targetRange >= 0 &&
                        static_cast<uint32>(pending->targetRange) < g_NumRanges)
                        ++hordeActualCounts[pending->targetRange];
                }
            }
        }

        QueueFactionRedistribution(totalAllianceBots, g_AllianceLevelRanges, allianceActualCounts, allianceBotsByRange);
        QueueFactionRedistribution(totalHordeBots, g_HordeLevelRanges, hordeActualCounts, hordeBotsByRange);

        if (g_BotDistFullDebugMode || g_BotDistLiteDebugMode)
        {
            sLog.outString("[BotLevelBrackets] Distribution check complete. Alliance bots: %u, Horde bots: %u.",
                totalAllianceBots, totalHordeBots);
        }
    }

private:
    uint64 m_timer;
    uint64 m_flaggedTimer;
    uint64 m_guildTrackerTimer;
};

// -----------------------------------------------------------------------------
// PLAYER SCRIPT: State Cleanup
// -----------------------------------------------------------------------------
class BotLevelBracketsPlayerScript : public PlayerScript
{
public:
    BotLevelBracketsPlayerScript()
        : PlayerScript("mod_twow_bot_level_brackets_player", { PLAYERHOOK_ON_LOGOUT })
    {
    }

    void OnLogout(Player* player) override
    {
        RemoveBotFromPendingResets(player);
    }

};

// Tortoise's ChatCommand table binds ChatHandler member-function pointers, so
// module-owned commands use the pre-dispatch AllCommandScript hook.
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
        if (rootCommand != "blb" && rootCommand != "botlevelbrackets")
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

            sWorld.LoadConfigSettings(true);
            handler->SendSysMessage("[BotLevelBrackets] World and module configuration reloaded successfully.");
        }
        else if (subcommand == "cleanup")
        {
            if (!hasSecurity(SEC_ADMINISTRATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            CleanupGuildTracker();
            handler->SendSysMessage("[BotLevelBrackets] Guild tracker cleaned up.");
        }
        else if (subcommand == "status")
        {
            if (!hasSecurity(SEC_MODERATOR))
            {
                handler->SendSysMessage("You are not allowed to do that.");
                return false;
            }

            char const* status = !g_BotLevelBracketsEnabled ? "DISABLED" :
                (sPlayerbotAIConfig.disableRandomLevels ? "PAUSED (AiPlayerbot.DisableRandomLevels=1)" : "ENABLED");
            handler->PSendSysMessage("[BotLevelBrackets] Status: %s, CheckFreq: %us, FlaggedFreq: %us, PendingResets: %u",
                status,
                g_BotDistCheckFrequency,
                g_BotDistFlaggedCheckFrequency,
                (uint32)g_PendingLevelResets.size());
        }
        else
            handler->SendSysMessage("Usage: .blb reload | cleanup | status");

        return false; // Command handled by this module.
    }
};

// -----------------------------------------------------------------------------
// ENTRY POINT: Register Scripts
// -----------------------------------------------------------------------------
void Addmod_twow_bot_level_bracketsScripts()
{
    new BotLevelBracketsWorldScript();
    new BotLevelBracketsPlayerScript();
    new BotLevelBracketsCommandScript();
}
