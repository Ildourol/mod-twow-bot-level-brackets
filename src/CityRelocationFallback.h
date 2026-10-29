#ifndef BRACKETS_CITY_RELOCATION_FALLBACK_H
#define BRACKETS_CITY_RELOCATION_FALLBACK_H

#include "BracketsCommon.h"
#include "Objects/Player.h"
#include <deque>
#include <unordered_map>
#include <mutex>
#include <ctime>
#include <cstdint>

namespace Brackets
{
    struct RelocationRecord
    {
        ObjectGuid botGuid;
        CityId     cityId;
        time_t     timestamp;
    };

    class CityRelocationFallback
    {
    public:
        static CityRelocationFallback& Instance()
        {
            static CityRelocationFallback instance;
            return instance;
        }

        bool CanRelocate(Player* bot, CityId cityId, TerminalReason reason, std::string* outRejectionReason = nullptr);
        bool ExecuteRelocation(Player* bot, CityId cityId, TerminalReason reason);

        void OnBotDeparture(ObjectGuid botGuid, CityId cityId, uint32 dwellSeconds);

        uint32 GetRelocationsInPastHour() const;
        uint32 GetTotalRelocations() const { return m_totalRelocations; }
        uint32 GetFailedRelocations() const { return m_failedRelocations; }

        void ResetCounters();
        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_recentRelocationTimes.clear();
            m_botCityCooldown.clear();
            m_botEarlyDepartureBackoff.clear();
            m_totalRelocations = 0;
            m_failedRelocations = 0;
        }

    private:
        CityRelocationFallback() = default;
        ~CityRelocationFallback() = default;

        mutable std::mutex m_mutex;

        std::deque<time_t> m_recentRelocationTimes;
        std::unordered_map<ObjectGuid, time_t> m_botCityCooldown;
        std::unordered_map<ObjectGuid, time_t> m_botEarlyDepartureBackoff;

        uint32 m_totalRelocations{0};
        uint32 m_failedRelocations{0};
    };
}

#endif // BRACKETS_CITY_RELOCATION_FALLBACK_H
