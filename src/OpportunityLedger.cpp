#include "OpportunityLedger.h"
#include <algorithm>

namespace Brackets
{
    uint64 OpportunityLedger::CreateIntent(ObjectGuid botGuid, Domain domain, uint32 destId,
                                           const std::string& destName, ActivityId actId,
                                           uint32 generation, uint8 botLevel, uint8 teamId,
                                           uint32 ttlSeconds)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Cancel any existing non-terminal intent for this bot
        auto it = m_activeIntents.find(botGuid);
        if (it != m_activeIntents.end())
        {
            it->second.state = INTENT_STATE_CANCELLED;
            it->second.terminalReason = REASON_TARGET_REPLACED;
            m_history.push_back(it->second);
            m_idToGuid.erase(it->second.intentId);
            m_activeIntents.erase(it);
        }

        uint64 intentId = m_nextIntentId++;
        time_t now = time(nullptr);

        OpportunityIntent intent;
        intent.intentId = intentId;
        intent.botGuid = botGuid;
        intent.domain = domain;
        intent.destinationId = destId;
        intent.destinationName = destName;
        intent.activityId = actId;

        const auto* actDef = ActivityRegistry::Instance().GetActivity(actId);
        intent.activityName = actDef ? actDef->name : "GenericRpg";

        intent.generation = generation;
        intent.token = std::to_string(intentId) + "_" + std::to_string(generation);
        intent.creationTime = now;
        intent.stateTime = now;
        intent.deadline = now + ttlSeconds;
        intent.state = INTENT_STATE_PROPOSED;
        intent.terminalReason = REASON_NONE;
        intent.botLevelAtCreation = botLevel;
        intent.teamId = teamId;

        m_activeIntents[botGuid] = intent;
        m_idToGuid[intentId] = botGuid;

        // Track domain switch
        m_lastDomainSwitch[botGuid] = now;

        return intentId;
    }

    bool OpportunityLedger::HasActiveIntent(ObjectGuid botGuid) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeIntents.find(botGuid) != m_activeIntents.end();
    }

    OpportunityIntent OpportunityLedger::GetActiveIntent(ObjectGuid botGuid) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_activeIntents.find(botGuid);
        if (it != m_activeIntents.end())
            return it->second;
        return OpportunityIntent{};
    }

    bool OpportunityLedger::IsIntentValid(ObjectGuid botGuid, const std::string& token) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_activeIntents.find(botGuid);
        if (it == m_activeIntents.end())
            return false;

        if (IsTerminalState(it->second.state))
            return false;

        if (token.empty())
            return true;

        return it->second.token == token;
    }

    bool OpportunityLedger::TransitionState(uint64 intentId, IntentState newState,
                                            TerminalReason reason, const std::string& detail)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto idIt = m_idToGuid.find(intentId);
        if (idIt == m_idToGuid.end())
            return false;

        ObjectGuid botGuid = idIt->second;
        auto actIt = m_activeIntents.find(botGuid);
        if (actIt == m_activeIntents.end())
            return false;

        time_t now = time(nullptr);
        actIt->second.state = newState;
        actIt->second.stateTime = now;

        if (IsTerminalState(newState))
        {
            actIt->second.terminalReason = reason;
            actIt->second.terminalDetail = detail;

            // Record visit if completed/arrived
            if (reason == REASON_COMPLETED)
            {
                VisitHistoryRecord rec;
                rec.domain = actIt->second.domain;
                rec.destinationId = actIt->second.destinationId;
                rec.timestamp = now;
                m_visitHistory[botGuid].push_back(rec);
            }
            else if (reason == REASON_FAILED || reason == REASON_SEARCH_EMPTY)
            {
                m_lastFailure[botGuid] = now;
            }

            m_history.push_back(actIt->second);
            if (m_history.size() > 500)
                m_history.pop_front();

            m_idToGuid.erase(idIt);
            m_activeIntents.erase(actIt);
        }

        return true;
    }

    bool OpportunityLedger::NotifyTravelAdopted(ObjectGuid botGuid, void* /*travelTargetPtr*/)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_activeIntents.find(botGuid);
        if (it == m_activeIntents.end())
            return false;

        it->second.state = INTENT_STATE_TRAVELING;
        it->second.stateTime = time(nullptr);
        return true;
    }

    bool OpportunityLedger::NotifyArrival(ObjectGuid botGuid)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_activeIntents.find(botGuid);
        if (it == m_activeIntents.end())
            return false;

        time_t now = time(nullptr);
        it->second.state = INTENT_STATE_RELEASED;
        it->second.stateTime = now;
        it->second.terminalReason = REASON_COMPLETED;

        VisitHistoryRecord rec;
        rec.domain = it->second.domain;
        rec.destinationId = it->second.destinationId;
        rec.timestamp = now;
        m_visitHistory[botGuid].push_back(rec);

        m_history.push_back(it->second);
        if (m_history.size() > 500)
            m_history.pop_front();

        m_idToGuid.erase(it->second.intentId);
        m_activeIntents.erase(it);
        return true;
    }

    bool OpportunityLedger::NotifyActivityObserved(ObjectGuid botGuid, ActivityId /*actId*/)
    {
        return NotifyArrival(botGuid);
    }

    bool OpportunityLedger::CancelIntent(ObjectGuid botGuid, TerminalReason reason, const std::string& detail)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_activeIntents.find(botGuid);
        if (it == m_activeIntents.end())
            return false;

        time_t now = time(nullptr);
        it->second.state = INTENT_STATE_CANCELLED;
        it->second.stateTime = now;
        it->second.terminalReason = reason;
        it->second.terminalDetail = detail;

        m_history.push_back(it->second);
        if (m_history.size() > 500)
            m_history.pop_front();

        m_idToGuid.erase(it->second.intentId);
        m_activeIntents.erase(it);
        return true;
    }

    void OpportunityLedger::ExpireStaleIntents(time_t now)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ObjectGuid> expiredGuids;

        for (const auto& pair : m_activeIntents)
        {
            if (pair.second.deadline > 0 && now >= pair.second.deadline)
            {
                expiredGuids.push_back(pair.first);
            }
        }

        for (ObjectGuid guid : expiredGuids)
        {
            auto it = m_activeIntents.find(guid);
            if (it != m_activeIntents.end())
            {
                it->second.state = INTENT_STATE_EXPIRED;
                it->second.stateTime = now;
                it->second.terminalReason = REASON_DEADLINE_EXPIRED;
                m_history.push_back(it->second);
                if (m_history.size() > 500)
                    m_history.pop_front();

                m_idToGuid.erase(it->second.intentId);
                m_activeIntents.erase(it);
            }
        }
    }

    void OpportunityLedger::CleanupTerminalIntents(time_t now, uint32 retentionSeconds)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_history.empty() && (now - m_history.front().stateTime > static_cast<time_t>(retentionSeconds)))
        {
            m_history.pop_front();
        }

        // Clean visit history older than 24 hours
        time_t cutoff = now - 86400;
        for (auto& pair : m_visitHistory)
        {
            auto& list = pair.second;
            list.erase(std::remove_if(list.begin(), list.end(), [cutoff](const VisitHistoryRecord& r) {
                return r.timestamp < cutoff;
            }), list.end());
        }
    }

    uint32 OpportunityLedger::GetActiveIntentsCount(Domain domain, uint8 teamId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32 count = 0;
        for (const auto& pair : m_activeIntents)
        {
            if (pair.second.domain == domain && pair.second.teamId == teamId)
                count++;
        }
        return count;
    }

    uint32 OpportunityLedger::GetConfirmedInboundCount(Domain domain, uint32 destId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32 count = 0;
        for (const auto& pair : m_activeIntents)
        {
            if (pair.second.domain == domain && pair.second.destinationId == destId &&
                pair.second.state == INTENT_STATE_TRAVELING)
            {
                count++;
            }
        }
        return count;
    }

    uint32 OpportunityLedger::GetPendingReservationCount(Domain domain, uint32 destId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32 count = 0;
        for (const auto& pair : m_activeIntents)
        {
            if (pair.second.domain == domain && pair.second.destinationId == destId &&
                (pair.second.state == INTENT_STATE_PROPOSED || pair.second.state == INTENT_STATE_PLANNING))
            {
                count++;
            }
        }
        return count;
    }

    time_t OpportunityLedger::GetLastVisitTime(ObjectGuid botGuid, Domain domain, uint32 destId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_visitHistory.find(botGuid);
        if (it == m_visitHistory.end())
            return 0;

        time_t lastTime = 0;
        for (const auto& r : it->second)
        {
            if (r.domain == domain && r.destinationId == destId && r.timestamp > lastTime)
                lastTime = r.timestamp;
        }
        return lastTime;
    }

    time_t OpportunityLedger::GetLastDomainSwitchTime(ObjectGuid botGuid) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lastDomainSwitch.find(botGuid);
        if (it != m_lastDomainSwitch.end())
            return it->second;
        return 0;
    }

    time_t OpportunityLedger::GetLastFailureTime(ObjectGuid botGuid) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lastFailure.find(botGuid);
        if (it != m_lastFailure.end())
            return it->second;
        return 0;
    }

    void OpportunityLedger::RecordFailure(ObjectGuid botGuid, time_t now)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastFailure[botGuid] = now;
    }

    std::vector<OpportunityIntent> OpportunityLedger::GetActiveIntentsSnapshot() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<OpportunityIntent> result;
        result.reserve(m_activeIntents.size());
        for (const auto& p : m_activeIntents)
            result.push_back(p.second);
        return result;
    }

    std::vector<OpportunityIntent> OpportunityLedger::GetRecentHistorySnapshot(size_t maxCount) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<OpportunityIntent> result;
        size_t count = std::min(maxCount, m_history.size());
        auto it = m_history.rbegin();
        for (size_t i = 0; i < count && it != m_history.rend(); ++i, ++it)
        {
            result.push_back(*it);
        }
        return result;
    }

    void OpportunityLedger::InvalidateAllForReload(uint32 /*oldGeneration*/)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        time_t now = time(nullptr);
        std::vector<ObjectGuid> toCancel;

        for (auto& pair : m_activeIntents)
        {
            // If intent is merely proposed, cancel it so it will be re-evaluated under new config
            if (pair.second.state == INTENT_STATE_PROPOSED)
            {
                toCancel.push_back(pair.first);
            }
        }

        for (ObjectGuid guid : toCancel)
        {
            auto it = m_activeIntents.find(guid);
            if (it != m_activeIntents.end())
            {
                it->second.state = INTENT_STATE_CANCELLED;
                it->second.stateTime = now;
                it->second.terminalReason = REASON_CONFIG_RELOAD;
                m_history.push_back(it->second);
                m_idToGuid.erase(it->second.intentId);
                m_activeIntents.erase(it);
            }
        }
    }

    void OpportunityLedger::ClearAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeIntents.clear();
        m_idToGuid.clear();
        m_history.clear();
    }
}
