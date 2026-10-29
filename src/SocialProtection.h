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
