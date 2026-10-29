#include "BracketsDebug.h"
#include "BracketsConfig.h"
#include "DestinationCatalog.h"
#include "PopulationCensus.h"
#include "OpportunityLedger.h"
#include "SocialProtection.h"
#include "BotEngineAdapter.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "World.h"

#include <fstream>
#include <sstream>
#include <iomanip>

namespace Brackets
{
    void BracketsDebug::LogEvent(DebugCategory cat, Domain domain, uint64 intentId, uint32 botGuidLow,
                                const std::string& destination, const std::string& activity,
                                const std::string& eventType, const std::string& resultReason,
                                const std::string& details)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const auto& config = ConfigManager::Instance().GetConfig();

        // 1. Check if tracing or debug mode active
        time_t now = time(nullptr);
        bool isTracing = (m_activeTraceGuid > 0 && m_activeTraceGuid == botGuidLow && now < m_traceExpiryTime);

        if (config.debugMode == DEBUG_MODE_OFF && !isTracing)
            return;

        // 2. Rate limit per second
        if (now != m_currentSecond)
        {
            m_currentSecond = now;
            m_eventsThisSecond = 0;
        }

        if (m_eventsThisSecond >= config.debugMaxEventsPerSecond && !isTracing)
        {
            m_metrics.droppedEvents++;
            return;
        }
        m_eventsThisSecond++;

        // 3. Build event record
        DebugEvent ev;
        ev.sequenceId = m_nextSeqId++;
        ev.monotonicMs = WorldTimer::getMSTime();
        ev.wallTime = now;
        ev.generation = config.generation;
        ev.intentId = intentId;
        ev.botGuidLow = botGuidLow;
        ev.category = cat;
        ev.domain = domain;
        ev.destination = destination;
        ev.activity = activity;
        ev.eventType = eventType;
        ev.resultReason = resultReason;
        ev.details = details;

        // 4. Store into ring buffer
        m_ringBuffer.push_back(ev);
        while (m_ringBuffer.size() > config.debugRingBufferEvents)
        {
            m_ringBuffer.pop_front();
        }

        // 5. Output to core log if Trace or Tracing active
        if (config.debugMode == DEBUG_MODE_TRACE || isTracing)
        {
            sLog.outString("[BLB Debug #%llu] [%s] [%s] Bot=%u Dest='%s' Act='%s' Event='%s' Reason='%s': %s",
                           ev.sequenceId, DomainToString(domain), eventType.c_str(),
                           botGuidLow, destination.c_str(), activity.c_str(),
                           eventType.c_str(), resultReason.c_str(), details.c_str());
        }
    }

    void BracketsDebug::StartTrace(uint32 botGuidLow, uint32 seconds)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeTraceGuid = botGuidLow;
        m_traceExpiryTime = time(nullptr) + seconds;
        sLog.outString("BotLevelBrackets: Started detailed trace for Bot GUID Low %u for %u seconds.",
                       botGuidLow, seconds);
    }

    void BracketsDebug::StopTrace()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeTraceGuid = 0;
        m_traceExpiryTime = 0;
        sLog.outString("BotLevelBrackets: Stopped active debug trace.");
    }

    bool BracketsDebug::IsTracingBot(uint32 botGuidLow) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeTraceGuid > 0 && m_activeTraceGuid == botGuidLow && time(nullptr) < m_traceExpiryTime;
    }

    std::string BracketsDebug::ExplainBot(uint32 botGuidLow) const
    {
        std::ostringstream ss;
        ss << "=== Bot #" << botGuidLow << " Diagnostic Snapshot ===\n";

        Player* p = sObjectAccessor.FindPlayer(ObjectGuid(botGuidLow));
        if (!p)
        {
            ss << "Status: Bot is not online or not found in ObjectAccessor.\n";
            return ss.str();
        }

        ss << "Name: " << p->GetName() << "\n";
        ss << "Faction: " << (p->GetTeamId() == 0 ? "Alliance" : "Horde") << "\n";
        ss << "Level: " << static_cast<uint32>(p->GetLevel()) << "\n";
        ss << "Map: " << p->GetMapId() << ", Area: " << p->GetAreaId() << "\n";

        bool isBot = BotEngineAdapter::IsBot(p);
        bool isRandom = BotEngineAdapter::IsRandomBot(p);
        bool hasMaster = BotEngineAdapter::BotHasRealPlayerMaster(p);
        std::string protReason;
        bool isProtected = SocialProtection::Instance().IsBotProtected(p, &protReason);

        ss << "IsBot: " << (isBot ? "YES" : "NO") << "\n";
        ss << "IsRandomPool: " << (isRandom ? "YES" : "NO") << "\n";
        ss << "HasRealMaster: " << (hasMaster ? "YES" : "NO") << "\n";
        ss << "IsProtected: " << (isProtected ? ("YES (" + protReason + ")") : "NO") << "\n";

        ss << "InCombat: " << (p->IsInCombat() ? "YES" : "NO") << "\n";
        ss << "IsAlive: " << (p->IsAlive() ? "YES" : "NO") << "\n";
        ss << "OnTaxi: " << (p->IsTaxiFlying() ? "YES" : "NO") << "\n";
        ss << "Teleporting: " << (p->IsBeingTeleported() ? "YES" : "NO") << "\n";

        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(p->GetObjectGuid());
        if (intent.intentId > 0)
        {
            ss << "Active Intent: ID=" << intent.intentId << " Domain=" << DomainToString(intent.domain)
               << " Dest='" << intent.destinationName << "' Act='" << intent.activityName
               << "' State=" << IntentStateToString(intent.state) << "\n";
        }
        else
        {
            ss << "Active Intent: None (Idle)\n";
        }

        return ss.str();
    }

    std::string BracketsDebug::ExplainCity(uint32 cityId) const
    {
        std::ostringstream ss;
        if (cityId >= CITY_COUNT)
        {
            ss << "Invalid city ID " << cityId << ". Valid range: 0 to 7.\n";
            return ss.str();
        }

        const auto* city = DestinationCatalog::Instance().GetCity(static_cast<CityId>(cityId));
        if (!city)
            return "City not found.\n";

        const auto& snap = PopulationCensus::Instance().GetLatestSnapshot();
        const auto& rec = snap.cities[cityId];

        ss << "=== City: " << city->name << " (ID: " << cityId << ") ===\n";
        ss << "Team: " << (city->teamId == 0 ? "Alliance" : "Horde") << "\n";
        ss << "Residents: " << rec.residents << "\n";
        ss << "Confirmed Inbound: " << rec.confirmedInbound << "\n";
        ss << "Pending Reservations: " << rec.pending << "\n";
        ss << "Effective Population: " << rec.effectivePopulation << "\n";
        ss << "Raw Target: " << rec.rawTarget << "\n";
        ss << "Bounded Target: " << rec.boundedTarget << "\n";
        ss << "Feasible Target: " << rec.feasibleTarget << "\n";
        ss << "Actionable Deficit: " << rec.actionableDeficit << "\n";
        ss << "Fallback Allowlisted: " << (ConfigManager::Instance().IsFallbackAllowedForCity(city->name) ? "YES" : "NO") << "\n";

        return ss.str();
    }

    std::string BracketsDebug::ExplainRegion(uint32 regionId) const
    {
        std::ostringstream ss;
        const auto* r = DestinationCatalog::Instance().GetRegion(regionId);
        if (!r)
        {
            ss << "Region ID " << regionId << " not found in catalog.\n";
            return ss.str();
        }

        const auto& snap = PopulationCensus::Instance().GetLatestSnapshot();
        const RegionCensusRecord* rec = nullptr;
        for (const auto& rr : snap.regions)
        {
            if (rr.regionId == regionId)
            {
                rec = &rr;
                break;
            }
        }

        ss << "=== Region: " << r->name << " (ID: " << regionId << ") ===\n";
        ss << "Team Access: " << (r->teamAccess == 0 ? "Alliance" : r->teamAccess == 1 ? "Horde" : "Both/Contested") << "\n";
        ss << "Safe Level Envelope: " << static_cast<uint32>(r->minLevel) << " - " << static_cast<uint32>(r->maxLevel) << "\n";
        if (rec)
        {
            ss << "Residents: " << rec->residents << "\n";
            ss << "Confirmed Inbound: " << rec->confirmedInbound << "\n";
            ss << "Pending Reservations: " << rec->pending << "\n";
            ss << "Effective Population: " << rec->effectivePopulation << "\n";
            ss << "Desired Demand: " << rec->desired << "\n";
            ss << "Actionable Deficit: " << rec->actionableDeficit << "\n";
        }

        return ss.str();
    }

    bool BracketsDebug::ExportJsonl(const std::string& filepath, std::string* outError)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream out(filepath);
        if (!out.is_open())
        {
            if (outError) *outError = "Failed to open export file: " + filepath;
            return false;
        }

        for (const auto& ev : m_ringBuffer)
        {
            out << "{\"seq\":" << ev.sequenceId
                << ",\"time\":" << ev.wallTime
                << ",\"gen\":" << ev.generation
                << ",\"intent\":" << ev.intentId
                << ",\"bot\":" << ev.botGuidLow
                << ",\"domain\":\"" << DomainToString(ev.domain) << "\""
                << ",\"dest\":\"" << ev.destination << "\""
                << ",\"act\":\"" << ev.activity << "\""
                << ",\"event\":\"" << ev.eventType << "\""
                << ",\"reason\":\"" << ev.resultReason << "\""
                << ",\"detail\":\"" << ev.details << "\"}\n";
        }

        out.close();
        return true;
    }

    std::vector<DebugEvent> BracketsDebug::GetRecentEventsSnapshot(size_t limit) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<DebugEvent> result;
        size_t count = std::min(limit, m_ringBuffer.size());
        auto it = m_ringBuffer.rbegin();
        for (size_t i = 0; i < count && it != m_ringBuffer.rend(); ++i, ++it)
            result.push_back(*it);
        return result;
    }

    void BracketsDebug::ResetCounters()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metrics = PerformanceMetrics{};
    }
}
