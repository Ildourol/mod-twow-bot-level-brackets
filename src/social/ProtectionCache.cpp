#include "ProtectionCache.h"
#include "../compat/NativeTaskAdapter.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#else
// Generic / Test doubles
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#endif

#include <sstream>
#include <ctime>
#include <algorithm>
#include <cstring>

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif

namespace BotLevelBrackets
{

ProtectionCache& ProtectionCache::Instance()
{
    static ProtectionCache instance;
    return instance;
}

void ProtectionCache::Initialize()
{
    LoadPersistentGuildTracker();
    RefreshFromDatabase();
    UpdatePersistentGuildTracker();
}

void ProtectionCache::Update(uint32_t diff)
{
    m_socialTimer += diff;
    m_guildTrackerTimer += diff;

    if (m_socialRefreshCadence > 0 && m_socialTimer >= m_socialRefreshCadence * 1000)
    {
        m_socialTimer = 0;
        RefreshFromDatabase();
    }

    if (m_guildTrackerCadence > 0 && m_guildTrackerTimer >= m_guildTrackerCadence * 1000)
    {
        m_guildTrackerTimer = 0;
        UpdatePersistentGuildTracker();
    }
}

void ProtectionCache::LoadPersistentGuildTracker(bool force)
{
    if (!force && !m_ignoreRealPlayerGuilds)
        return;

    QueryResult* result = CharacterDatabase.Query(
        "SELECT guild_id FROM bot_level_brackets_guild_tracker WHERE has_real_players = 1");

    std::unordered_set<uint32_t> tempSet;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            if (fields)
                tempSet.insert(fields[0].GetUInt32());
        } while (result->NextRow());
        delete result;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_persistentGuilds = std::move(tempSet);
    }
}

void ProtectionCache::UpdatePersistentGuildTracker()
{
    if (!m_ignoreRealPlayerGuilds)
        return;

    std::unordered_set<uint32_t> currentRealPlayerGuilds;
    const auto& players = sObjectAccessor.GetPlayers();

    for (const auto& itr : players)
    {
        Player* player = itr.second;
        if (!player || !player->IsInWorld())
            continue;

        if (!NativeTaskAdapter::IsBot(player))
        {
            uint32_t guildId = player->GetGuildId();
            if (guildId != 0)
                currentRealPlayerGuilds.insert(guildId);
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint32_t guildId : currentRealPlayerGuilds)
    {
        m_guilds.insert(guildId);
        if (m_persistentGuilds.find(guildId) == m_persistentGuilds.end())
        {
            CharacterDatabase.PExecute(
                "REPLACE INTO bot_level_brackets_guild_tracker (guild_id, has_real_players) VALUES (%u, 1)", guildId);
            m_persistentGuilds.insert(guildId);
        }
    }
}

uint32_t ProtectionCache::CleanupGuildTracker()
{
    LoadPersistentGuildTracker(true);

    std::unordered_set<uint32_t> guildsWithHumanAccountMembers;
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
            if (fields)
            {
                uint32_t guildId = fields[0].GetUInt32();
                uint32_t accountId = fields[1].GetUInt32();
                if (!NativeTaskAdapter::IsInRandomAccountList(accountId))
                    guildsWithHumanAccountMembers.insert(guildId);
            }
        } while (members->NextRow());
        delete members;
    }

    std::vector<uint32_t> guildsToRemove;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (uint32_t trackedGuildId : m_persistentGuilds)
        {
            if (guildsWithHumanAccountMembers.count(trackedGuildId) == 0)
                guildsToRemove.push_back(trackedGuildId);
        }

        for (uint32_t guildId : guildsToRemove)
        {
            CharacterDatabase.PExecute(
                "UPDATE bot_level_brackets_guild_tracker SET has_real_players = 0 WHERE guild_id = %u", guildId);
            m_persistentGuilds.erase(guildId);
            m_guilds.erase(guildId);
        }
    }

    return static_cast<uint32_t>(guildsToRemove.size());
}

void ProtectionCache::SetExcludedNames(const std::string& commaSeparated)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_excludedNames.clear();
    std::stringstream ss(commaSeparated);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        // trim whitespace
        size_t first = item.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            continue;
        size_t last = item.find_last_not_of(" \t\r\n");
        m_excludedNames.push_back(item.substr(first, (last - first + 1)));
    }
}

void ProtectionCache::RefreshFromDatabase()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastRefreshTime = static_cast<uint32_t>(time(nullptr));

    // 1. Refresh friends list: characters friended by non-bot players
    QueryResult* result = CharacterDatabase.Query(
        "SELECT cs.friend, c.account "
        "FROM character_social cs "
        "INNER JOIN characters c ON c.guid = cs.guid "
        "WHERE (cs.flags & 1) <> 0");
    if (result)
    {
        std::unordered_set<uint32_t> newFriends;
        do
        {
            Field* fields = result->Fetch();
            if (fields)
            {
                uint32_t friendGuid = fields[0].GetUInt32();
                uint32_t ownerAccount = fields[1].GetUInt32();
                if (!NativeTaskAdapter::IsInRandomAccountList(ownerAccount))
                    newFriends.insert(friendGuid);
            }
        } while (result->NextRow());
        delete result;
        m_friends = std::move(newFriends);
    }
}

bool ProtectionCache::IsFriendListed(uint32_t guidLow) const
{
    if (!m_ignoreFriendListed)
        return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_friends.find(guidLow) != m_friends.end();
}

bool ProtectionCache::IsRealPlayerGuild(uint32_t guildId) const
{
    if (!m_ignoreRealPlayerGuilds || guildId == 0)
        return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_guilds.find(guildId) != m_guilds.end() || m_persistentGuilds.find(guildId) != m_persistentGuilds.end();
}

bool ProtectionCache::IsExcludedName(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& excl : m_excludedNames)
    {
        if (strcasecmp(excl.c_str(), name.c_str()) == 0)
            return true;
    }
    return false;
}

bool ProtectionCache::IsBotProtected(Player* bot) const
{
    if (!bot)
        return true;

    uint32_t guidLow = bot->GetObjectGuid().GetCounter();
    if (IsFriendListed(guidLow))
        return true;

    if (bot->GetGuildId() != 0 && IsRealPlayerGuild(bot->GetGuildId()))
        return true;

    if (IsExcludedName(bot->GetName()))
        return true;

    if (NativeTaskAdapter::HasRealPlayerMaster(bot) || NativeTaskAdapter::HasRealPlayerInParty(bot))
        return true;

#if !defined(TEST_DOUBLES_MODE)
    if (m_ignoreArenaTeams)
    {
        if (bot->InArena() || bot->InBattleGroundQueue())
            return true;
    }
#endif

    return false;
}

void ProtectionCache::OnGuildMemberAdd(uint32_t guildId, bool isRealPlayer)
{
    if (guildId == 0 || !isRealPlayer)
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_guilds.insert(guildId);
    m_persistentGuilds.insert(guildId);
    CharacterDatabase.PExecute(
        "REPLACE INTO bot_level_brackets_guild_tracker (guild_id, has_real_players) VALUES (%u, 1)", guildId);
}

void ProtectionCache::OnGuildMemberRemove(uint32_t guildId, bool isRealPlayer)
{
    (void)guildId;
    (void)isRealPlayer;
    // On member removal, we do not immediately evict the guild to prevent race conditions;
    // the periodic CleanupGuildTracker (.blb cleanup) safely prunes empty guilds.
}

uint32_t ProtectionCache::GetProtectedFriendsCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_friends.size());
}

uint32_t ProtectionCache::GetProtectedGuildsCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_guilds.size());
}

uint32_t ProtectionCache::GetProtectedPersistentGuildsCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_persistentGuilds.size());
}

} // namespace BotLevelBrackets
