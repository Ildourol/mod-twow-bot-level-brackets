#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_LEVEL_DISTRIBUTION_MANAGER_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_LEVEL_DISTRIBUTION_MANAGER_H

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

class Player;
class ChatHandler;
struct ObjectGuid;

namespace BotLevelBrackets
{

struct PendingResetEntry
{
    uint32_t guidLow;
    uint8_t targetRange;
    uint8_t targetLevel;
    uint8_t teamId;
    uint32_t enqueuedAt;
};

class LevelDistributionManager
{
public:
    static LevelDistributionManager& Instance();

    void Update(uint32_t diff);
    void ForceDistributionCycle(ChatHandler* handler = nullptr);
    void ForceDispersalCycle(ChatHandler* handler = nullptr);

    bool EnqueueReset(Player* bot, uint8_t targetRange, uint8_t teamId);
    bool BoostBotDirect(Player* bot, uint8_t targetLevel);
    void RemoveBot(uint32_t guidLow);

    size_t GetPendingQueueSize() const;
    std::vector<PendingResetEntry> GetPendingQueueSnapshot() const;
    void ClearPendingQueue();

    uint64_t GetTotalBotsAdjusted() const { return m_statsBotsAdjusted.load(); }
    uint64_t GetTotalBotsTeleported() const { return m_statsBotsTeleported.load(); }
    uint64_t GetTotalBotsSkipped() const { return m_statsBotsSkipped.load(); }
    uint64_t GetTotalDispersedStarter() const { return m_statsDispersedStarter.load(); }
    uint64_t GetTotalDispersedWrongMap() const { return m_statsDispersedWrongMap.load(); }
    uint64_t GetTotalCyclesRun() const { return m_statsCyclesRun.load(); }

private:
    LevelDistributionManager() = default;

    void RunDistributionPass(ChatHandler* handler = nullptr);
    void ProcessPendingResets();
    void ProcessStartingZoneDisperse();
    void ProcessWrongMapDisperse();

    uint32_t m_distributionTimer = 0;
    uint32_t m_flaggedTimer = 0;
    uint32_t m_starterZoneTimer = 0;
    uint32_t m_wrongMapTimer = 0;
    bool m_initialPassDone = false;

    mutable std::mutex m_queueMutex;
    std::unordered_map<uint32_t, PendingResetEntry> m_pendingResets;

    std::atomic<uint64_t> m_statsBotsAdjusted{0};
    std::atomic<uint64_t> m_statsBotsTeleported{0};
    std::atomic<uint64_t> m_statsBotsSkipped{0};
    std::atomic<uint64_t> m_statsDispersedStarter{0};
    std::atomic<uint64_t> m_statsDispersedWrongMap{0};
    std::atomic<uint64_t> m_statsCyclesRun{0};
    std::atomic<bool> m_distributionInProgress{false};
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_LEVEL_DISTRIBUTION_MANAGER_H
