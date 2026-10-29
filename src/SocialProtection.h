#ifndef BRACKETS_SOCIAL_PROTECTION_H
#define BRACKETS_SOCIAL_PROTECTION_H

#include "BracketsCommon.h"
#include "Objects/Player.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <cstdint>

namespace Brackets
{
    class SocialProtection
    {
    public:
        static SocialProtection& Instance()
        {
            static SocialProtection instance;
            return instance;
        }

        void Initialize();
        void RefreshCaches();
        void LoadPersistentGuilds();

        bool IsBotProtected(Player* bot, std::string* outReason = nullptr) const;

        void OnGuildMemberAdd(uint32 guildId, Player* player);
        void OnGuildMemberRemove(uint32 guildId, Player* player, bool isDisband);

        void UpdateExcludeNames(const std::vector<std::string>& names);

        void AddSocialFriend(ObjectGuid /*humanGuid*/, ObjectGuid botGuid)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_friendsList.insert(botGuid.GetCounter());
        }

        void AddPersistentGuild(uint32 guildId)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_persistentRealPlayerGuildIds.insert(guildId);
        }

        bool IsSocialProtected(ObjectGuid botGuid) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_friendsList.find(botGuid.GetCounter()) != m_friendsList.end();
        }

        bool IsSocialProtectedGuild(uint32 guildId) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_persistentRealPlayerGuildIds.find(guildId) != m_persistentRealPlayerGuildIds.end() ||
                   m_onlineRealPlayerGuildIds.find(guildId) != m_onlineRealPlayerGuildIds.end();
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_friendsList.clear();
            m_onlineRealPlayerGuildIds.clear();
            m_persistentRealPlayerGuildIds.clear();
            m_excludeNames.clear();
        }

        size_t GetProtectedFriendsCount() const;
        size_t GetProtectedGuildsCount() const;

    private:
        SocialProtection() = default;
        ~SocialProtection() = default;

        mutable std::mutex m_mutex;

        // Character GUID counters of players who are friends of human players
        std::unordered_set<uint32> m_friendsList;

        // Guild IDs containing online human players
        std::unordered_set<uint32> m_onlineRealPlayerGuildIds;

        // Guild IDs containing human players (persisted from DB for offline protection)
        std::unordered_set<uint32> m_persistentRealPlayerGuildIds;

        // Excluded bot names
        std::vector<std::string> m_excludeNames;

        time_t m_lastRefreshTime{0};
    };
}

#endif // BRACKETS_SOCIAL_PROTECTION_H
