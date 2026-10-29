#ifndef BRACKETS_OPPORTUNITY_LEDGER_H
#define BRACKETS_OPPORTUNITY_LEDGER_H

#include "BracketsCommon.h"
#include "ActivityRegistry.h"
#include "Objects/Player.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <ctime>
#include <cstdint>

namespace Brackets
{
    struct OpportunityIntent
    {
        uint64         intentId{0};
        ObjectGuid     botGuid;
        Domain         domain{DOMAIN_NONE};
        uint32         destinationId{0};
        std::string    destinationName{""};
        ActivityId     activityId{ACTIVITY_GENERIC_RPG};
        std::string    activityName{""};
        std::string    token{""};
        uint32         generation{0};
        time_t         creationTime{0};
        time_t         stateTime{0};
        time_t         deadline{0};
        IntentState    state{INTENT_STATE_NONE};
        TerminalReason terminalReason{REASON_NONE};
        std::string    terminalDetail{""};
        uint8          botLevelAtCreation{0};
        uint8          teamId{0};
    };

    struct VisitHistoryRecord
    {
        Domain domain;
        uint32 destinationId;
        time_t timestamp;
    };

    class OpportunityLedger
    {
    public:
        static OpportunityLedger& Instance()
        {
            static OpportunityLedger instance;
            return instance;
        }

        uint64 CreateIntent(ObjectGuid botGuid, Domain domain, uint32 destId,
                            const std::string& destName, ActivityId actId,
                            uint32 generation, uint8 botLevel, uint8 teamId,
                            uint32 ttlSeconds);

        bool HasActiveIntent(ObjectGuid botGuid) const;
        OpportunityIntent GetActiveIntent(ObjectGuid botGuid) const;
        bool IsIntentValid(ObjectGuid botGuid, const std::string& token) const;

        bool TransitionState(uint64 intentId, IntentState newState,
                             TerminalReason reason = REASON_NONE,
                             const std::string& detail = "");

        bool NotifyTravelAdopted(ObjectGuid botGuid, void* travelTargetPtr = nullptr);
        bool NotifyArrival(ObjectGuid botGuid);
        bool NotifyActivityObserved(ObjectGuid botGuid, ActivityId actId);
        bool CancelIntent(ObjectGuid botGuid, TerminalReason reason, const std::string& detail = "");

        void ExpireStaleIntents(time_t now);
        void CleanupTerminalIntents(time_t now, uint32 retentionSeconds = 3600);

        uint32 GetActiveIntentsCount(Domain domain, uint8 teamId) const;
        uint32 GetConfirmedInboundCount(Domain domain, uint32 destId) const;
        uint32 GetPendingReservationCount(Domain domain, uint32 destId) const;

        time_t GetLastVisitTime(ObjectGuid botGuid, Domain domain, uint32 destId) const;
        time_t GetLastDomainSwitchTime(ObjectGuid botGuid) const;
        time_t GetLastFailureTime(ObjectGuid botGuid) const;
        void RecordFailure(ObjectGuid botGuid, time_t now);

        std::vector<OpportunityIntent> GetActiveIntentsSnapshot() const;
        std::vector<OpportunityIntent> GetRecentHistorySnapshot(size_t maxCount = 100) const;

        void InvalidateAllForReload(uint32 oldGeneration);
        void ClearAll();
        void Clear() { ClearAll(); }

    private:
        OpportunityLedger() : m_nextIntentId(1) {}
        ~OpportunityLedger() = default;

        mutable std::mutex m_mutex;
        uint64 m_nextIntentId;

        // Active non-terminal intents keyed by bot ObjectGuid
        std::unordered_map<ObjectGuid, OpportunityIntent> m_activeIntents;

        // Reverse map from intentId to botGuid
        std::unordered_map<uint64, ObjectGuid> m_idToGuid;

        // Ring buffer of completed / terminal intents for telemetry
        std::deque<OpportunityIntent> m_history;

        // Anti-churn history: per-bot recent destination visits
        std::unordered_map<ObjectGuid, std::vector<VisitHistoryRecord>> m_visitHistory;
        std::unordered_map<ObjectGuid, time_t> m_lastDomainSwitch;
        std::unordered_map<ObjectGuid, time_t> m_lastFailure;
    };
}

#endif // BRACKETS_OPPORTUNITY_LEDGER_H
