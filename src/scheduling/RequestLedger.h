#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_REQUEST_LEDGER_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_REQUEST_LEDGER_H

#include "../world/ReadOnlyCatalog.h"
#include "../activities/ActivityRegistry.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <ctime>

namespace BotLevelBrackets
{

enum class RequestDomain : uint8_t
{
    City = 0,
    World = 1,
    Activity = 2
};

const char* GetRequestDomainName(RequestDomain dom);

enum class RequestState : uint8_t
{
    Proposed = 0,
    Submitted = 1,
    NativeAccepted = 2,
    Arrived = 3,
    NativeActivityObserved = 4,
    Released = 5,
    Rejected = 6,
    Expired = 7
};

const char* GetRequestStateName(RequestState st);

struct LedgerEntry
{
    uint64_t requestId = 0;
    uint32_t guidLow = 0;
    uint8_t teamId = 0;
    RequestDomain domain = RequestDomain::City;
    CityId intendedCity = CityId::None;
    CityId observedActualCity = CityId::None;
    uint32_t intendedRegionId = 0;
    std::string activityId;
    SupportClass supportClass = SupportClass::RequestableExact;
    RequestState state = RequestState::Proposed;
    uint32_t configGeneration = 0;
    uint32_t submittedAt = 0;
    uint32_t lastObservedAt = 0;
    uint64_t nativeTaskId = 0;
    std::string terminalReason;
};

class RequestLedger
{
public:
    static RequestLedger& Instance();

    bool HasActiveRequest(uint32_t guidLow) const;
    bool IsOnCooldown(uint32_t guidLow, uint32_t now) const;
    uint32_t GetCooldownRemaining(uint32_t guidLow, uint32_t now) const;

    uint64_t ProposeCityRequest(uint32_t guidLow, uint8_t teamId, CityId cityId,
                                SupportClass sc, uint32_t generation, uint32_t now);

    uint64_t ProposeActivityRequest(uint32_t guidLow, uint8_t teamId, const std::string& activityId,
                                    SupportClass sc, uint32_t generation, uint32_t now);

    bool MarkSubmitted(uint64_t reqId, uint32_t now);
    bool MarkAccepted(uint64_t reqId, uint64_t nativeTaskId, CityId actualCity, uint32_t now);
    bool MarkRejected(uint64_t reqId, const std::string& reason, uint32_t now);
    bool MarkArrived(uint64_t reqId, uint32_t now);
    bool MarkActivityObserved(uint64_t reqId, uint32_t now);
    bool MarkReleased(uint64_t reqId, const std::string& reason, uint32_t now);

    void ApplyCooldown(uint32_t guidLow, uint32_t cooldownSeconds, uint32_t now);

    uint32_t GetOutstandingCount(uint8_t teamId) const;
    uint32_t GetPendingCityInbound(CityId cityId) const;

    const LedgerEntry* GetEntry(uint64_t reqId) const;
    const LedgerEntry* GetActiveEntryForBot(uint32_t guidLow) const;
    std::vector<LedgerEntry> GetAllActiveEntries() const;

    void Update(uint32_t now, uint32_t dispatchTimeoutSeconds);
    void WithdrawStaleProposals(uint32_t currentGeneration);
    void Clear();

private:
    RequestLedger() = default;

    uint64_t m_nextRequestId = 1;
    std::unordered_map<uint64_t, LedgerEntry> m_entries;
    std::unordered_map<uint32_t, uint64_t> m_activeBotToReqId; // guidLow -> reqId
    std::unordered_map<uint32_t, uint32_t> m_botCooldowns;     // guidLow -> expireTimestamp
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_REQUEST_LEDGER_H
