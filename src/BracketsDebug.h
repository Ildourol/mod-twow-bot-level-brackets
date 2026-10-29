#ifndef BRACKETS_DEBUG_H
#define BRACKETS_DEBUG_H

#include "BracketsCommon.h"
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <mutex>
#include <ctime>
#include <cstdint>

namespace Brackets
{
    enum DebugCategory : uint32
    {
        CAT_CAPABILITIES       = 1 << 0,
        CAT_CONFIGURATION      = 1 << 1,
        CAT_LIFECYCLE          = 1 << 2,
        CAT_CENSUS             = 1 << 3,
        CAT_QUOTAS             = 1 << 4,
        CAT_ACTIVITIES         = 1 << 5,
        CAT_ELIGIBILITY        = 1 << 6,
        CAT_SCHEDULER          = 1 << 7,
        CAT_REGISTRATION       = 1 << 8,
        CAT_THREADS            = 1 << 9,
        CAT_NATIVE_TRAVEL      = 1 << 10,
        CAT_OWNERSHIP          = 1 << 11,
        CAT_CITY               = 1 << 12,
        CAT_WORLD              = 1 << 13,
        CAT_SOCIAL_PROTECTION  = 1 << 14,
        CAT_LEVEL_POLICY       = 1 << 15,
        CAT_FALLBACK           = 1 << 16,
        CAT_PERSISTENCE        = 1 << 17,
        CAT_COMMANDS           = 1 << 18,
        CAT_PERFORMANCE        = 1 << 19,
        CAT_ASSERTIONS         = 1 << 20,
        CAT_ALL                = 0xFFFFFFFF
    };

    struct DebugEvent
    {
        uint64         sequenceId{0};
        uint32         monotonicMs{0};
        time_t         wallTime{0};
        uint32         generation{0};
        uint64         intentId{0};
        uint32         botGuidLow{0};
        DebugCategory  category{CAT_SCHEDULER};
        Domain         domain{DOMAIN_NONE};
        std::string    destination{""};
        std::string    activity{""};
        std::string    eventType{""};
        std::string    resultReason{""};
        std::string    details{""};
    };

    struct PerformanceMetrics
    {
        uint32 lastCensusTimeMs{0};
        uint32 lastSchedulerTimeMs{0};
        uint32 totalProbesRun{0};
        uint32 admissionsThisCycle{0};
        uint32 cumulativeAdmissions{0};
        uint32 cumulativeArrivals{0};
        uint32 cumulativeCancellations{0};
        uint32 cumulativeFallbacks{0};
        uint32 dryRunDecisions{0};
        uint32 droppedEvents{0};
    };

    class BracketsDebug
    {
    public:
        static BracketsDebug& Instance()
        {
            static BracketsDebug instance;
            return instance;
        }

        void LogEvent(DebugCategory cat, Domain domain, uint64 intentId, uint32 botGuidLow,
                      const std::string& destination, const std::string& activity,
                      const std::string& eventType, const std::string& resultReason,
                      const std::string& details);

        void StartTrace(uint32 botGuidLow, uint32 seconds);
        void StopTrace();
        bool IsTracingBot(uint32 botGuidLow) const;

        std::string ExplainBot(uint32 botGuidLow) const;
        std::string ExplainCity(uint32 cityId) const;
        std::string ExplainRegion(uint32 regionId) const;

        bool ExportJsonl(const std::string& filepath, std::string* outError = nullptr);

        PerformanceMetrics& GetMetrics() { return m_metrics; }
        const PerformanceMetrics& GetMetrics() const { return m_metrics; }
        void ResetCounters();

        std::vector<DebugEvent> GetRecentEventsSnapshot(size_t limit = 100) const;

    private:
        BracketsDebug() = default;
        ~BracketsDebug() = default;

        bool ShouldLog(DebugCategory cat, uint32 botGuidLow) const;

        mutable std::mutex m_mutex;
        uint64 m_nextSeqId{1};

        std::deque<DebugEvent> m_ringBuffer;
        PerformanceMetrics m_metrics;

        uint32 m_activeTraceGuid{0};
        time_t m_traceExpiryTime{0};

        uint32 m_eventsThisSecond{0};
        time_t m_currentSecond{0};
    };
}

#endif // BRACKETS_DEBUG_H
