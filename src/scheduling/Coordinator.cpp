#include "Coordinator.h"
#include "../policy/PopulationPolicy.h"
#include "../compat/NativeTaskAdapter.h"
#include "../social/ProtectionCache.h"
#include "../activities/ActivityRegistry.h"
#include "../debug/DiagnosticManager.h"
#include "RequestLedger.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#include "Log.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Log.h"
#else
// Test Doubles
#include "support/Framework.h"
#endif

#include <ctime>
#include <algorithm>
#include <vector>

namespace BotLevelBrackets
{

Coordinator& Coordinator::Instance()
{
    static Coordinator instance;
    return instance;
}

void Coordinator::Update(uint32_t diff)
{
    const auto& policy = PopulationPolicy::Instance();
    if (!policy.IsEnabled())
        return;

    m_elapsedMs += diff;
    uint32_t checkMs = policy.GetCheckCadenceSeconds() * 1000;
    if (m_elapsedMs >= checkMs)
    {
        m_elapsedMs = 0;
        RunSchedulingPass();
    }
}

void Coordinator::ForceExecutionCycle()
{
    RunSchedulingPass();
}

void Coordinator::CleanupStaleEntries()
{
    uint32_t now = static_cast<uint32_t>(time(nullptr));
    RequestLedger::Instance().Update(now, PopulationPolicy::Instance().GetDispatchTimeoutSeconds());
}

void Coordinator::ReconcileActiveLedger(uint32_t now)
{
    auto& ledger = RequestLedger::Instance();
    auto active = ledger.GetAllActiveEntries();

    for (const auto& entry : active)
    {
        Player* bot = sObjectAccessor.FindPlayer(ObjectGuid(entry.guidLow));
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
        {
            ledger.MarkReleased(entry.requestId, "BotOfflineOrDead", now);
            continue;
        }

        if (entry.domain == RequestDomain::City)
        {
            bool inCity = NativeTaskAdapter::IsBotInCity(bot, entry.intendedCity);
            if (inCity && entry.state == RequestState::NativeAccepted)
            {
                ledger.MarkArrived(entry.requestId, now);
                DiagnosticManager::Instance().LogEvent(
                    "Lifecycle", "ArrivalObserved", "Success", "Bot arrived at intended city",
                    entry.requestId, entry.guidLow, GetCityName(entry.intendedCity),
                    GetCityName(entry.intendedCity));
            }

            // Check if native visit completed
            if (!NativeTaskAdapter::HasActiveCityVisit(bot->GetObjectGuid()))
            {
                if (entry.state == RequestState::Arrived || entry.state == RequestState::NativeAccepted)
                {
                    ledger.MarkReleased(entry.requestId, "NativeVisitFinished", now);
                    ledger.ApplyCooldown(entry.guidLow, PopulationPolicy::Instance().GetRequestCooldownSeconds(), now);
                }
            }
        }
    }
}

void Coordinator::RunSchedulingPass()
{
    m_totalCyclesRun.fetch_add(1, std::memory_order_relaxed);
    uint32_t now = static_cast<uint32_t>(time(nullptr));

    // Reconcile ongoing dispatches and prune expired entries
    ReconcileActiveLedger(now);
    CleanupStaleEntries();

    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    uint32_t requestsRemaining = PopulationPolicy::Instance().GetMaxRequestsPerCycle();

    DiagnosticManager::Instance().LogEvent(
        "Census", "SnapshotTaken", "OK", "Census snapshot taken for scheduling pass",
        0, 0, "", "", "");

    // 1. Process city visit requests
    if (PopulationPolicy::Instance().IsCityEnabled() && requestsRemaining > 0)
    {
        ProcessCityDemands(snap, now, requestsRemaining);
    }

    // 2. Process outdoor activity requests if budget remains
    if (PopulationPolicy::Instance().IsWorldEnabled() && requestsRemaining > 0)
    {
        ProcessOutdoorActivities(snap, now, requestsRemaining);
    }
}

void Coordinator::ProcessCityDemands(const CensusSnapshot& snap, uint32_t now, uint32_t& requestsRemaining)
{
    const auto& policy = PopulationPolicy::Instance();
    auto& ledger = RequestLedger::Instance();
    const auto& catalog = ReadOnlyCatalog::Instance();
    bool isDryRun = policy.IsDryRun() || DiagnosticManager::Instance().IsDryRun();

    // Rotate through cities starting from m_cityCursor to prevent first-city bias
    size_t startIdx = m_cityCursor;
    m_cityCursor = (m_cityCursor + 1) % 8;

    for (size_t offset = 0; offset < 8 && requestsRemaining > 0; ++offset)
    {
        size_t cIdx = (startIdx + offset) % 8;
        const auto& c = snap.cities[cIdx];

        if (c.shortfall == 0)
            continue;

        uint8_t teamId = c.teamId;
        if (ledger.GetOutstandingCount(teamId) >= policy.GetMaxOutstandingPerFaction())
            continue;

        const CityDefinition* def = catalog.GetCity(c.id);
        if (!def)
            continue;

        // Collect eligible candidate bots for this city
        std::vector<Player*> candidates;
        const auto& playerMap = sObjectAccessor.GetPlayers();
        for (const auto& kv : playerMap)
        {
            Player* p = kv.second;
            if (!p || !p->IsInWorld() || !p->IsAlive())
                continue;

            if (p->GetTeamId() != teamId)
                continue;

            if (!NativeTaskAdapter::IsFreeBot(p))
                continue;

            uint32_t guidLow = p->GetObjectGuid().GetCounter();
            if (ledger.HasActiveRequest(guidLow))
                continue;

            if (ledger.IsOnCooldown(guidLow, now))
                continue;

            if (ProtectionCache::Instance().IsBotProtected(p))
                continue;

            // Must not already reside in the target city
            if (NativeTaskAdapter::IsBotInCity(p, c.id))
                continue;

            candidates.push_back(p);
        }

        if (candidates.empty())
        {
            DiagnosticManager::Instance().LogEvent(
                "Eligibility", "NoCandidates", "ShortfallPersists",
                "No eligible bots available for city demand",
                0, 0, def->name, "", "");
            continue;
        }

        // Bounded selection: request up to c.shortfall and up to requestsRemaining
        uint32_t toRequest = std::min(c.shortfall, requestsRemaining);
        for (size_t i = 0; i < toRequest && i < candidates.size(); ++i)
        {
            Player* bot = candidates[i];
            uint32_t guidLow = bot->GetObjectGuid().GetCounter();

            if (isDryRun)
            {
                DiagnosticManager::Instance().LogEvent(
                    "DryRun", "WouldRequestCity", "Simulated",
                    "DryRun mode: simulated city visit proposal",
                    0, guidLow, def->name, "", "");
                --requestsRemaining;
                continue;
            }

            uint64_t reqId = ledger.ProposeCityRequest(
                guidLow, teamId, c.id, SupportClass::RequestableExact,
                policy.GetConfigGeneration(), now);

            if (reqId == 0)
                continue;

            ledger.MarkSubmitted(reqId, now);
            m_totalRequestsSubmitted.fetch_add(1, std::memory_order_relaxed);

            uint64_t outNativeId = 0;
            TaskAdmissionResult res = NativeTaskAdapter::RequestCityVisit(
                bot, c.id, policy.GetCityDwellDurationMs(),
                policy.GetConfigGeneration(), outNativeId);

            if (res == TaskAdmissionResult::Accepted)
            {
                ledger.MarkAccepted(reqId, outNativeId, c.id, now);
                m_totalRequestsAccepted.fetch_add(1, std::memory_order_relaxed);
                --requestsRemaining;

                DiagnosticManager::Instance().LogEvent(
                    "NativeAcknowledgment", "RequestAccepted", "Success",
                    "Native CityVisitService accepted request",
                    reqId, guidLow, def->name, def->name, "");
            }
            else if (res == TaskAdmissionResult::AlreadyActive)
            {
                ledger.MarkAccepted(reqId, outNativeId, c.id, now);
                --requestsRemaining;
            }
            else
            {
                ledger.MarkRejected(reqId, GetTaskAdmissionResultName(res), now);
                ledger.ApplyCooldown(guidLow, policy.GetRejectionBackoffSeconds(), now);
                m_totalRequestsRejected.fetch_add(1, std::memory_order_relaxed);

                DiagnosticManager::Instance().LogEvent(
                    "NativeAcknowledgment", "RequestRejected", "Refused",
                    GetTaskAdmissionResultName(res),
                    reqId, guidLow, def->name, "", "");
            }
        }
    }
}

void Coordinator::ProcessOutdoorActivities(const CensusSnapshot& snap, uint32_t now, uint32_t& requestsRemaining)
{
    const auto& policy = PopulationPolicy::Instance();
    const auto& reg = ActivityRegistry::Instance();
    auto& ledger = RequestLedger::Instance();
    bool isDryRun = policy.IsDryRun() || DiagnosticManager::Instance().IsDryRun();

    if (!reg.IsActivitiesEnabled() || requestsRemaining == 0)
        return;

    // Pick requestable outdoor activities (e.g. GenericRpg, Grinding, FlightDiscovery)
    std::vector<const ActivityDefinition*> requestableActivities;
    for (const auto& act : reg.GetAllActivities())
    {
        if (act.enabled && act.worldEnabled &&
            (act.supportClass == SupportClass::RequestableNativeChoice ||
             act.supportClass == SupportClass::LocalOnly))
        {
            requestableActivities.push_back(&act);
        }
    }

    if (requestableActivities.empty())
        return;

    const auto& playerMap = sObjectAccessor.GetPlayers();
    for (const auto& kv : playerMap)
    {
        if (requestsRemaining == 0)
            break;

        Player* p = kv.second;
        if (!p || !p->IsInWorld() || !p->IsAlive())
            continue;

        if (!NativeTaskAdapter::IsFreeBot(p))
            continue;

        uint32_t guidLow = p->GetObjectGuid().GetCounter();
        if (ledger.HasActiveRequest(guidLow) || ledger.IsOnCooldown(guidLow, now))
            continue;

        if (ProtectionCache::Instance().IsBotProtected(p))
            continue;

        // Select an activity based on weights
        const ActivityDefinition* act = requestableActivities[guidLow % requestableActivities.size()];
        if (!act)
            continue;

        if (isDryRun)
        {
            DiagnosticManager::Instance().LogEvent(
                "DryRun", "WouldRequestActivity", "Simulated",
                "DryRun mode: simulated outdoor activity request",
                0, guidLow, "", "", act->id);
            --requestsRemaining;
            continue;
        }

        uint64_t reqId = ledger.ProposeActivityRequest(
            guidLow, p->GetTeamId(), act->id, act->supportClass,
            policy.GetConfigGeneration(), now);

        if (reqId == 0)
            continue;

        ledger.MarkSubmitted(reqId, now);
        m_totalRequestsSubmitted.fetch_add(1, std::memory_order_relaxed);

        bool ok = false;
        if (!act->nativeActionName.empty())
        {
            ok = NativeTaskAdapter::DispatchNativeAction(p, act->nativeActionName);
        }

        if (ok)
        {
            ledger.MarkActivityObserved(reqId, now);
            ledger.ApplyCooldown(guidLow, act->cooldownSeconds, now);
            m_totalRequestsAccepted.fetch_add(1, std::memory_order_relaxed);
            --requestsRemaining;

            DiagnosticManager::Instance().LogEvent(
                "ActivityRequests", "ActionDispatched", "Success",
                "Dispatched native action",
                reqId, guidLow, "", "", act->id);
        }
        else
        {
            ledger.MarkRejected(reqId, "ActionUselessOrImpossible", now);
            ledger.ApplyCooldown(guidLow, policy.GetRejectionBackoffSeconds(), now);
            m_totalRequestsRejected.fetch_add(1, std::memory_order_relaxed);

            DiagnosticManager::Instance().LogEvent(
                "ActivityRequests", "ActionRefused", "UselessOrImpossible",
                "Action isUseful/isPossible returned false",
                reqId, guidLow, "", "", act->id);
        }
    }
}

} // namespace BotLevelBrackets
