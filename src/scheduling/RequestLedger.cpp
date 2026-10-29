#include "RequestLedger.h"
#include <algorithm>

namespace BotLevelBrackets
{

const char* GetRequestDomainName(RequestDomain dom)
{
    switch (dom)
    {
        case RequestDomain::City:     return "City";
        case RequestDomain::World:    return "World";
        case RequestDomain::Activity: return "Activity";
        default:                      return "Unknown";
    }
}

const char* GetRequestStateName(RequestState st)
{
    switch (st)
    {
        case RequestState::Proposed:               return "Proposed";
        case RequestState::Submitted:              return "Submitted";
        case RequestState::NativeAccepted:          return "NativeAccepted";
        case RequestState::Arrived:                return "Arrived";
        case RequestState::NativeActivityObserved: return "NativeActivityObserved";
        case RequestState::Released:               return "Released";
        case RequestState::Rejected:               return "Rejected";
        case RequestState::Expired:                return "Expired";
        default:                                   return "Unknown";
    }
}

RequestLedger& RequestLedger::Instance()
{
    static RequestLedger instance;
    return instance;
}

bool RequestLedger::HasActiveRequest(uint32_t guidLow) const
{
    return m_activeBotToReqId.find(guidLow) != m_activeBotToReqId.end();
}

bool RequestLedger::IsOnCooldown(uint32_t guidLow, uint32_t now) const
{
    auto it = m_botCooldowns.find(guidLow);
    if (it != m_botCooldowns.end())
        return now < it->second;
    return false;
}

uint32_t RequestLedger::GetCooldownRemaining(uint32_t guidLow, uint32_t now) const
{
    auto it = m_botCooldowns.find(guidLow);
    if (it != m_botCooldowns.end() && now < it->second)
        return it->second - now;
    return 0;
}

void RequestLedger::ApplyCooldown(uint32_t guidLow, uint32_t cooldownSeconds, uint32_t now)
{
    if (cooldownSeconds > 0)
        m_botCooldowns[guidLow] = now + cooldownSeconds;
}

uint64_t RequestLedger::ProposeCityRequest(uint32_t guidLow, uint8_t teamId, CityId cityId,
                                           SupportClass sc, uint32_t generation, uint32_t now)
{
    if (HasActiveRequest(guidLow))
        return 0;

    uint64_t reqId = m_nextRequestId++;
    LedgerEntry entry{};
    entry.requestId = reqId;
    entry.guidLow = guidLow;
    entry.teamId = teamId;
    entry.domain = RequestDomain::City;
    entry.intendedCity = cityId;
    entry.supportClass = sc;
    entry.state = RequestState::Proposed;
    entry.configGeneration = generation;
    entry.submittedAt = now;
    entry.lastObservedAt = now;

    m_entries[reqId] = entry;
    m_activeBotToReqId[guidLow] = reqId;
    return reqId;
}

uint64_t RequestLedger::ProposeActivityRequest(uint32_t guidLow, uint8_t teamId,
                                               const std::string& activityId,
                                               SupportClass sc, uint32_t generation, uint32_t now)
{
    if (HasActiveRequest(guidLow))
        return 0;

    uint64_t reqId = m_nextRequestId++;
    LedgerEntry entry{};
    entry.requestId = reqId;
    entry.guidLow = guidLow;
    entry.teamId = teamId;
    entry.domain = RequestDomain::Activity;
    entry.activityId = activityId;
    entry.supportClass = sc;
    entry.state = RequestState::Proposed;
    entry.configGeneration = generation;
    entry.submittedAt = now;
    entry.lastObservedAt = now;

    m_entries[reqId] = entry;
    m_activeBotToReqId[guidLow] = reqId;
    return reqId;
}

bool RequestLedger::MarkSubmitted(uint64_t reqId, uint32_t now)
{
    auto it = m_entries.find(reqId);
    if (it == m_entries.end())
        return false;

    it->second.state = RequestState::Submitted;
    it->second.lastObservedAt = now;
    return true;
}

bool RequestLedger::MarkAccepted(uint64_t reqId, uint64_t nativeTaskId, CityId actualCity, uint32_t now)
{
    auto it = m_entries.find(reqId);
    if (it == m_entries.end())
        return false;

    it->second.state = RequestState::NativeAccepted;
    it->second.nativeTaskId = nativeTaskId;
    it->second.observedActualCity = actualCity;
    it->second.lastObservedAt = now;
    return true;
}

bool RequestLedger::MarkRejected(uint64_t reqId, const std::string& reason, uint32_t now)
{
    auto it = m_entries.find(reqId);
    if (it == m_entries.end())
        return false;

    it->second.state = RequestState::Rejected;
    it->second.terminalReason = reason;
    it->second.lastObservedAt = now;

    m_activeBotToReqId.erase(it->second.guidLow);
    return true;
}

bool RequestLedger::MarkArrived(uint64_t reqId, uint32_t now)
{
    auto it = m_entries.find(reqId);
    if (it == m_entries.end())
        return false;

    it->second.state = RequestState::Arrived;
    it->second.lastObservedAt = now;
    return true;
}

bool RequestLedger::MarkActivityObserved(uint64_t reqId, uint32_t now)
{
    auto it = m_entries.find(reqId);
    if (it == m_entries.end())
        return false;

    it->second.state = RequestState::NativeActivityObserved;
    it->second.lastObservedAt = now;
    return true;
}

bool RequestLedger::MarkReleased(uint64_t reqId, const std::string& reason, uint32_t now)
{
    auto it = m_entries.find(reqId);
    if (it == m_entries.end())
        return false;

    it->second.state = RequestState::Released;
    it->second.terminalReason = reason;
    it->second.lastObservedAt = now;

    m_activeBotToReqId.erase(it->second.guidLow);
    return true;
}

uint32_t RequestLedger::GetOutstandingCount(uint8_t teamId) const
{
    uint32_t count = 0;
    for (const auto& kv : m_entries)
    {
        if (kv.second.teamId == teamId &&
            (kv.second.state == RequestState::Proposed ||
             kv.second.state == RequestState::Submitted ||
             kv.second.state == RequestState::NativeAccepted))
        {
            ++count;
        }
    }
    return count;
}

uint32_t RequestLedger::GetPendingCityInbound(CityId cityId) const
{
    uint32_t count = 0;
    for (const auto& kv : m_entries)
    {
        if (kv.second.domain == RequestDomain::City &&
            (kv.second.intendedCity == cityId || kv.second.observedActualCity == cityId) &&
            (kv.second.state == RequestState::Proposed ||
             kv.second.state == RequestState::Submitted ||
             kv.second.state == RequestState::NativeAccepted))
        {
            ++count;
        }
    }
    return count;
}

const LedgerEntry* RequestLedger::GetEntry(uint64_t reqId) const
{
    auto it = m_entries.find(reqId);
    if (it != m_entries.end())
        return &it->second;
    return nullptr;
}

const LedgerEntry* RequestLedger::GetActiveEntryForBot(uint32_t guidLow) const
{
    auto it = m_activeBotToReqId.find(guidLow);
    if (it != m_activeBotToReqId.end())
        return GetEntry(it->second);
    return nullptr;
}

std::vector<LedgerEntry> RequestLedger::GetAllActiveEntries() const
{
    std::vector<LedgerEntry> list;
    for (const auto& kv : m_activeBotToReqId)
    {
        auto it = m_entries.find(kv.second);
        if (it != m_entries.end())
            list.push_back(it->second);
    }
    return list;
}

void RequestLedger::Update(uint32_t now, uint32_t dispatchTimeoutSeconds)
{
    // Expire stuck unconfirmed submissions
    for (auto& kv : m_entries)
    {
        if ((kv.second.state == RequestState::Proposed || kv.second.state == RequestState::Submitted) &&
            (now - kv.second.submittedAt) > dispatchTimeoutSeconds)
        {
            kv.second.state = RequestState::Expired;
            kv.second.terminalReason = "DispatchTimeout";
            m_activeBotToReqId.erase(kv.second.guidLow);
        }
    }

    // Clean up ancient cooldowns
    for (auto it = m_botCooldowns.begin(); it != m_botCooldowns.end(); )
    {
        if (now >= it->second)
            it = m_botCooldowns.erase(it);
        else
            ++it;
    }
}

void RequestLedger::WithdrawStaleProposals(uint32_t currentGeneration)
{
    for (auto& kv : m_entries)
    {
        if (kv.second.state == RequestState::Proposed && kv.second.configGeneration != currentGeneration)
        {
            kv.second.state = RequestState::Released;
            kv.second.terminalReason = "ConfigGenerationStale";
            m_activeBotToReqId.erase(kv.second.guidLow);
        }
    }
}

void RequestLedger::Clear()
{
    m_entries.clear();
    m_activeBotToReqId.clear();
    m_botCooldowns.clear();
}

} // namespace BotLevelBrackets
