#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_PROTECTION_CACHE_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_PROTECTION_CACHE_H

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
#include <mutex>

class Player;

namespace BotLevelBrackets
{

class ProtectionCache
{
public:
    static ProtectionCache& Instance();

    void Initialize();
    void RefreshFromDatabase();

    bool IsBotProtected(Player* bot) const;
    bool IsFriendListed(uint32_t guidLow) const;
    bool IsRealPlayerGuild(uint32_t guildId) const;
    bool IsExcludedName(const std::string& name) const;

    void SetIgnoreFriendListed(bool val) { m_ignoreFriendListed = val; }
    void SetIgnoreRealPlayerGuilds(bool val) { m_ignoreRealPlayerGuilds = val; }
    void SetIgnoreArenaTeams(bool val) { m_ignoreArenaTeams = val; }
    void SetSocialRefreshCadence(uint32_t seconds) { m_socialRefreshCadence = seconds; }

    void SetExcludedNames(const std::string& commaSeparated);

    void OnGuildMemberAdd(uint32_t guildId, bool isRealPlayer);
    void OnGuildMemberRemove(uint32_t guildId, bool isRealPlayer);

    uint32_t GetProtectedFriendsCount() const;
    uint32_t GetProtectedGuildsCount() const;

private:
    ProtectionCache() = default;

    mutable std::mutex m_mutex;
    std::unordered_set<uint32_t> m_friends;
    std::unordered_set<uint32_t> m_guilds;
    std::vector<std::string> m_excludedNames;

    bool m_ignoreFriendListed = true;
    bool m_ignoreRealPlayerGuilds = true;
    bool m_ignoreArenaTeams = true;
    uint32_t m_socialRefreshCadence = 300;
    uint32_t m_lastRefreshTime = 0;
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_PROTECTION_CACHE_H
