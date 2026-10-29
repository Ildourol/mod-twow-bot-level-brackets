#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_DIAGNOSTIC_MANAGER_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_DIAGNOSTIC_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <mutex>

namespace BotLevelBrackets
{

enum class DebugMode : uint8_t
{
    Off = 0,
    Summary = 1,
    Trace = 2,
    DryRun = 3
};

const char* GetDebugModeName(DebugMode mode);

struct DiagnosticEvent
{
    uint64_t sequence;
    uint32_t timestamp;
    uint32_t configGeneration;
    uint64_t requestId;
    uint32_t botGuid;
    std::string category;
    std::string eventType;
    std::string outcome;
    std::string reason;
    std::string intendedDestination;
    std::string actualDestination;
    std::string activityId;
};

class DiagnosticManager
{
public:
    static DiagnosticManager& Instance();

    void SetDebugMode(DebugMode mode) { m_mode = mode; }
    DebugMode GetDebugMode() const { return m_mode; }
    bool IsTracing() const { return m_mode == DebugMode::Trace; }
    bool IsDryRun() const { return m_mode == DebugMode::DryRun; }

    void SetMaxEventsPerSecond(uint32_t val) { m_maxEventsPerSec = val; }
    void SetRingBufferCapacity(size_t cap) { m_ringBufferCapacity = cap; }

    void LogEvent(const std::string& category, const std::string& eventType,
                  const std::string& outcome, const std::string& reason,
                  uint64_t reqId = 0, uint32_t guidLow = 0,
                  const std::string& intendedDest = "",
                  const std::string& actualDest = "",
                  const std::string& actId = "");

    std::vector<DiagnosticEvent> GetRecentEvents(size_t maxCount = 100) const;
    void ClearRingBuffer();

    std::string ExplainBot(uint32_t guidLow) const;
    std::string ExplainCity(size_t cityIndex) const;
    std::string ExplainRegion(uint32_t regionId) const;

private:
    DiagnosticManager();

    DebugMode m_mode = DebugMode::Off;
    uint32_t m_maxEventsPerSec = 50;
    size_t m_ringBufferCapacity = 2000;

    mutable std::mutex m_mutex;
    uint64_t m_nextSequence = 1;
    uint32_t m_currentSecond = 0;
    uint32_t m_eventsInCurrentSecond = 0;
    std::deque<DiagnosticEvent> m_ringBuffer;
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_DIAGNOSTIC_MANAGER_H
