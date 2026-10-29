#include "DiagnosticManager.h"
#include "../policy/PopulationPolicy.h"
#include "../world/ReadOnlyCatalog.h"
#include "../world/Census.h"
#include "../scheduling/RequestLedger.h"
#include "../social/ProtectionCache.h"
#include "../compat/NativeTaskAdapter.h"

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

#include <sstream>
#include <ctime>

namespace BotLevelBrackets
{

const char* GetDebugModeName(DebugMode mode)
{
    switch (mode)
    {
        case DebugMode::Off:     return "Off";
        case DebugMode::Summary: return "Summary";
        case DebugMode::Trace:   return "Trace";
        case DebugMode::DryRun:  return "DryRun";
        default:                 return "Unknown";
    }
}

DiagnosticManager& DiagnosticManager::Instance()
{
    static DiagnosticManager instance;
    return instance;
}

DiagnosticManager::DiagnosticManager()
{
    m_mode = DebugMode::Off;
}

void DiagnosticManager::LogEvent(const std::string& category, const std::string& eventType,
                                 const std::string& outcome, const std::string& reason,
                                 uint64_t reqId, uint32_t guidLow,
                                 const std::string& intendedDest,
                                 const std::string& actualDest,
                                 const std::string& actId)
{
    if (m_mode == DebugMode::Off && category != "Error")
        return;

    uint32_t now = static_cast<uint32_t>(time(nullptr));

    std::lock_guard<std::mutex> lock(m_mutex);

    // Rate limiter
    if (now == m_currentSecond)
    {
        if (m_eventsInCurrentSecond >= m_maxEventsPerSec)
            return;
        ++m_eventsInCurrentSecond;
    }
    else
    {
        m_currentSecond = now;
        m_eventsInCurrentSecond = 1;
    }

    DiagnosticEvent ev;
    ev.sequence = m_nextSequence++;
    ev.timestamp = now;
    ev.configGeneration = PopulationPolicy::Instance().GetConfigGeneration();
    ev.requestId = reqId;
    ev.botGuid = guidLow;
    ev.category = category;
    ev.eventType = eventType;
    ev.outcome = outcome;
    ev.reason = reason;
    ev.intendedDestination = intendedDest;
    ev.actualDestination = actualDest;
    ev.activityId = actId;

    m_ringBuffer.push_back(ev);
    while (m_ringBuffer.size() > m_ringBufferCapacity)
        m_ringBuffer.pop_front();

    if (m_mode == DebugMode::Trace || m_mode == DebugMode::DryRun)
    {
        sLog.outString("[BLB-%s] %s | bot=%u req=%llu | %s : %s | intended=%s actual=%s",
                       category.c_str(), eventType.c_str(), guidLow,
                       static_cast<unsigned long long>(reqId), outcome.c_str(), reason.c_str(),
                       intendedDest.c_str(), actualDest.c_str());
    }
}

std::vector<DiagnosticEvent> DiagnosticManager::GetRecentEvents(size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DiagnosticEvent> result;
    size_t count = std::min(maxCount, m_ringBuffer.size());
    auto it = m_ringBuffer.rbegin();
    for (size_t i = 0; i < count && it != m_ringBuffer.rend(); ++i, ++it)
        result.push_back(*it);
    return result;
}

void DiagnosticManager::ClearRingBuffer()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ringBuffer.clear();
}

std::string DiagnosticManager::ExplainBot(uint32_t guidLow) const
{
    std::ostringstream ss;
    Player* bot = sObjectAccessor.FindPlayer(ObjectGuid(guidLow));
    if (!bot)
    {
        ss << "Bot [GUID " << guidLow << "] is offline or not found in ObjectAccessor.\n";
        return ss.str();
    }

    bool isBot = NativeTaskAdapter::IsBot(bot);
    bool isRandom = NativeTaskAdapter::IsRandomBot(bot);
    bool isFree = NativeTaskAdapter::IsFreeBot(bot);
    bool isProtected = ProtectionCache::Instance().IsBotProtected(bot);
    bool isFriendListed = ProtectionCache::Instance().IsFriendListed(guidLow);
    bool isRealGuild = bot->GetGuildId() != 0 && ProtectionCache::Instance().IsRealPlayerGuild(bot->GetGuildId());
    bool isReady = NativeTaskAdapter::IsInWorldAndReady(bot);
    bool isOnCd = RequestLedger::Instance().IsOnCooldown(guidLow, static_cast<uint32_t>(time(nullptr)));
    uint32_t cdRem = RequestLedger::Instance().GetCooldownRemaining(guidLow, static_cast<uint32_t>(time(nullptr)));

    ss << "=== Diagnostic Report for Bot: " << bot->GetName() << " (GUID: " << guidLow << ") ===\n";
    ss << "Level: " << static_cast<unsigned>(bot->GetLevel())
       << " | Team: " << (bot->GetTeamId() == 0 ? "Alliance" : "Horde")
       << " | Map: " << bot->GetMapId() << " | Pos: ("
       << bot->GetPositionX() << ", " << bot->GetPositionY() << ", " << bot->GetPositionZ() << ")\n";
    ss << "Engine Classification: IsBot=" << (isBot ? "YES" : "NO")
       << " | IsRandomBot=" << (isRandom ? "YES" : "NO")
       << " | IsFreeBot=" << (isFree ? "YES" : "NO") << "\n";
    ss << "Protections: OverallProtected=" << (isProtected ? "YES" : "NO")
       << " | FriendListed=" << (isFriendListed ? "YES" : "NO")
       << " | RealPlayerGuild=" << (isRealGuild ? "YES" : "NO") << "\n";
    ss << "State: InWorldAndReady=" << (isReady ? "YES" : "NO")
       << " | InCombat=" << (bot->IsInCombat() ? "YES" : "NO")
       << " | Alive=" << (bot->IsAlive() ? "YES" : "NO") << "\n";
    ss << "Travel Status: " << NativeTaskAdapter::GetBotTravelStatusString(bot) << "\n";
    ss << "Cooldown: " << (isOnCd ? "ACTIVE (" + std::to_string(cdRem) + "s remaining)" : "None") << "\n";

    const LedgerEntry* active = RequestLedger::Instance().GetActiveEntryForBot(guidLow);
    if (active)
    {
        ss << "Active Ledger Request #" << active->requestId << ":\n";
        ss << "  Domain: " << GetRequestDomainName(active->domain)
           << " | State: " << GetRequestStateName(active->state)
           << " | Support: " << GetSupportClassName(active->supportClass) << "\n";
        if (active->domain == RequestDomain::City)
            ss << "  Intended City: " << GetCityName(active->intendedCity)
               << " | Actual City: " << GetCityName(active->observedActualCity) << "\n";
        else
            ss << "  Activity: " << active->activityId << "\n";
    }
    else
    {
        ss << "Active Ledger Request: None (Idle)\n";
    }

    return ss.str();
}

std::string DiagnosticManager::ExplainCity(size_t cityIndex) const
{
    std::ostringstream ss;
    if (cityIndex >= 8)
    {
        ss << "Invalid city index: " << cityIndex << " (valid: 0 to 7)\n";
        return ss.str();
    }

    const auto& catalog = ReadOnlyCatalog::Instance();
    const auto& cities = catalog.GetCities();
    if (cityIndex >= cities.size())
        return "City index not in catalog.\n";

    const auto& def = cities[cityIndex];
    CensusSnapshot snap = Census::Instance().TakeSnapshot();
    const auto& c = snap.cities[cityIndex];

    ss << "=== City Analysis: " << def.name << " (Team: " << (def.teamId == 0 ? "Alliance" : "Horde") << ") ===\n";
    ss << "Center: (" << def.centerX << ", " << def.centerY << ", " << def.centerZ << ") Radius: " << def.radius << "\n";
    ss << "Census Metrics:\n";
    ss << "  Actual Residents: " << c.residents << "\n";
    ss << "  Confirmed Native Inbound: " << c.confirmedInbound << "\n";
    ss << "  Pending Module Reservations: " << c.pendingReservations << "\n";
    ss << "  Effective Population: " << c.effective << "\n";
    ss << "Targets & Allocation:\n";
    ss << "  Raw Configured Target: " << c.rawTarget << "\n";
    ss << "  Feasible Target (Field Reserve bounded): " << c.feasibleTarget << "\n";
    ss << "  Current Actionable Shortfall: " << c.shortfall << "\n";
    ss << "  Imbalance Status: " << (c.imbalance == PopulationImbalance::Underpopulated ? "Underpopulated" :
                                   (c.imbalance == PopulationImbalance::Overpopulated ? "Overpopulated" : "Neutral")) << "\n";

    return ss.str();
}

std::string DiagnosticManager::ExplainRegion(uint32_t regionId) const
{
    std::ostringstream ss;
    const auto* reg = ReadOnlyCatalog::Instance().GetRegion(regionId);
    if (!reg)
    {
        ss << "Outdoor region ID " << regionId << " not found in catalog.\n";
        return ss.str();
    }

    ss << "=== Outdoor Region Analysis: " << reg->name << " (ID: " << reg->regionId << ") ===\n";
    ss << "Map: " << reg->mapId << " | Zone: " << reg->zoneId
       << " | Level Band: " << static_cast<unsigned>(reg->minLevel) << " - " << static_cast<unsigned>(reg->maxLevel)
       << " | Team: " << (reg->teamId == 0 ? "Alliance" : (reg->teamId == 1 ? "Horde" : "Contested")) << "\n";
    ss << "Destination Control: " << (PopulationPolicy::Instance().GetWorldDestinationPolicy() == DestinationPolicy::RequireExact ?
                                      "RequireExact (Reports NoExactDestinationControl)" : "AllowNativeChoice") << "\n";
    return ss.str();
}

} // namespace BotLevelBrackets
