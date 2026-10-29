#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_COORDINATOR_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_COORDINATOR_H

#include "../world/Census.h"
#include <cstdint>
#include <atomic>

namespace BotLevelBrackets
{

class Coordinator
{
public:
    static Coordinator& Instance();

    void Update(uint32_t diff);
    void ForceExecutionCycle();
    void CleanupStaleEntries();

    uint64_t GetTotalCyclesRun() const { return m_totalCyclesRun.load(); }
    uint64_t GetTotalRequestsSubmitted() const { return m_totalRequestsSubmitted.load(); }
    uint64_t GetTotalRequestsAccepted() const { return m_totalRequestsAccepted.load(); }
    uint64_t GetTotalRequestsRejected() const { return m_totalRequestsRejected.load(); }

private:
    Coordinator() = default;

    void RunSchedulingPass();
    void ProcessCityDemands(const CensusSnapshot& snap, uint32_t now, uint32_t& requestsRemaining);
    void ProcessOutdoorActivities(const CensusSnapshot& snap, uint32_t now, uint32_t& requestsRemaining);
    void ReconcileActiveLedger(uint32_t now);

    uint32_t m_elapsedMs = 0;
    size_t m_cityCursor = 0;

    std::atomic<uint64_t> m_totalCyclesRun{0};
    std::atomic<uint64_t> m_totalRequestsSubmitted{0};
    std::atomic<uint64_t> m_totalRequestsAccepted{0};
    std::atomic<uint64_t> m_totalRequestsRejected{0};
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_COORDINATOR_H
