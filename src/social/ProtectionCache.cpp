#include "ProtectionCache.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "Objects/Player.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Player.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#else
// Generic / Test doubles
#include "Objects/Player.h"
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
    RefreshFromDatabase();
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

    // 1. Refresh friends list: characters friended by non-headless players
    // character_social: guid (player), friend (target), flags (1 = friend)
    QueryResult* result = CharacterDatabase.Query(
        "SELECT DISTINCT friend FROM character_social WHERE flags & 1");
    if (result)
    {
        std::unordered_set<uint32_t> newFriends;
        do
        {
            Field* fields = result->Fetch();
            if (fields)
                newFriends.insert(fields[0].GetUInt32());
        } while (result->NextRow());
        delete result;
        m_friends = std::move(newFriends);
    }

    // 2. Refresh human guilds: guilds that have at least one character on a normal account (accountId < 100 or non-bot)
    QueryResult* gResult = CharacterDatabase.Query(
        "SELECT DISTINCT guildid FROM guild_member gm "
        "JOIN characters c ON gm.guid = c.guid "
        "WHERE c.account < 100");
    if (gResult)
    {
        std::unordered_set<uint32_t> newGuilds;
        do
        {
            Field* fields = gResult->Fetch();
            if (fields)
                newGuilds.insert(fields[0].GetUInt32());
        } while (gResult->NextRow());
        delete gResult;
        m_guilds = std::move(newGuilds);
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
    return m_guilds.find(guildId) != m_guilds.end();
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
}

void ProtectionCache::OnGuildMemberRemove(uint32_t guildId, bool isRealPlayer)
{
    (void)guildId;
    (void)isRealPlayer;
    // On member removal, we do not immediately evict the guild to prevent race conditions;
    // the periodic DB refresh cleanly syncs it.
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

} // namespace BotLevelBrackets
