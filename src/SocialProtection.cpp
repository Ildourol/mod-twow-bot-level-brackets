#include "SocialProtection.h"
#include "BracketsConfig.h"
#include "ObjectAccessor.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"

namespace Brackets
{
    void SocialProtection::Initialize()
    {
        LoadPersistentGuilds();
        RefreshCaches();
    }

    void SocialProtection::LoadPersistentGuilds()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_persistentRealPlayerGuildIds.clear();

        QueryResult* result = CharacterDatabase.Query(
            "SELECT guild_id FROM bot_level_brackets_guild_tracker WHERE has_real_players = 1");

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[0].GetUInt32();
                m_persistentRealPlayerGuildIds.insert(guildId);
            } while (result->NextRow());

            delete result;
        }

        sLog.outString("BotLevelBrackets: Loaded %zu persistent human guilds from database.",
                       m_persistentRealPlayerGuildIds.size());
    }

    void SocialProtection::RefreshCaches()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_onlineRealPlayerGuildIds.clear();
        m_friendsList.clear();

        const auto& config = ConfigManager::Instance().GetConfig();

        // 1. Scan online players for real players and their guilds
        HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
        for (const auto& pair : players)
        {
            Player* p = pair.second;
            if (!p || !p->IsInWorld() || !p->GetSession())
                continue;

            // Real player detected: session is not headless
            if (!p->GetSession()->IsHeadless())
            {
                uint32 gid = p->GetGuildId();
                if (gid > 0)
                {
                    m_onlineRealPlayerGuildIds.insert(gid);
                    m_persistentRealPlayerGuildIds.insert(gid);
                }
            }
        }

        // 2. Query character_social table for friends of real players
        if (config.protectFriends)
        {
            QueryResult* result = CharacterDatabase.Query(
                "SELECT cs.friend FROM character_social cs "
                "JOIN characters c ON cs.guid = c.guid "
                "JOIN `tw_logon`.`account` a ON c.account = a.id "
                "WHERE a.username NOT LIKE 'BOT%' AND a.username NOT LIKE 'RNDBOT%' AND cs.flags & 1");

            if (result)
            {
                do
                {
                    Field* fields = result->Fetch();
                    uint32 friendCounter = fields[0].GetUInt32();
                    m_friendsList.insert(friendCounter);
                } while (result->NextRow());

                delete result;
            }
        }

        m_lastRefreshTime = time(nullptr);
    }

    bool SocialProtection::IsBotProtected(Player* bot, std::string* outReason) const
    {
        if (!bot)
            return false;

        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& config = ConfigManager::Instance().GetConfig();

        // 1. Name exclusion
        const char* bName = bot->GetName();
        if (bName)
        {
            for (const auto& excluded : m_excludeNames)
            {
                if (excluded == bName)
                {
                    if (outReason) *outReason = "ProtectedName";
                    return true;
                }
            }
        }

        // 2. Friend list of human players
        if (config.protectFriends)
        {
            uint32 counter = bot->GetObjectGuid().GetCounter();
            if (m_friendsList.find(counter) != m_friendsList.end())
            {
                if (outReason) *outReason = "ProtectedFriend";
                return true;
            }
        }

        // 3. Human guild protection
        if (config.protectGuilds)
        {
            uint32 gid = bot->GetGuildId();
            if (gid > 0)
            {
                if (m_onlineRealPlayerGuildIds.find(gid) != m_onlineRealPlayerGuildIds.end())
                {
                    if (outReason) *outReason = "ProtectedGuild";
                    return true;
                }

                if (config.protectOfflineHumans &&
                    m_persistentRealPlayerGuildIds.find(gid) != m_persistentRealPlayerGuildIds.end())
                {
                    if (outReason) *outReason = "ProtectedGuildOffline";
                    return true;
                }
            }
        }

        return false;
    }

    void SocialProtection::OnGuildMemberAdd(uint32 guildId, Player* player)
    {
        if (!player || guildId == 0 || !player->GetSession())
            return;

        if (!player->GetSession()->IsHeadless())
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_onlineRealPlayerGuildIds.insert(guildId);
            m_persistentRealPlayerGuildIds.insert(guildId);

            CharacterDatabase.PExecute(
                "REPLACE INTO bot_level_brackets_guild_tracker (guild_id, has_real_players) VALUES (%u, 1)",
                guildId);
        }
    }

    void SocialProtection::OnGuildMemberRemove(uint32 guildId, Player* /*player*/, bool isDisband)
    {
        if (guildId == 0)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (isDisband)
        {
            m_onlineRealPlayerGuildIds.erase(guildId);
            m_persistentRealPlayerGuildIds.erase(guildId);

            CharacterDatabase.PExecute(
                "DELETE FROM bot_level_brackets_guild_tracker WHERE guild_id = %u",
                guildId);
        }
    }

    void SocialProtection::UpdateExcludeNames(const std::vector<std::string>& names)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_excludeNames = names;
    }

    size_t SocialProtection::GetProtectedFriendsCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_friendsList.size();
    }

    size_t SocialProtection::GetProtectedGuildsCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_persistentRealPlayerGuildIds.size();
    }
}
