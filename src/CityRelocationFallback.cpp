#include "CityRelocationFallback.h"
#include "BracketsConfig.h"
#include "DestinationCatalog.h"
#include "OpportunityLedger.h"
#include "BotEngineAdapter.h"
#include "SocialProtection.h"
#include "Log.h"

namespace Brackets
{
    bool CityRelocationFallback::CanRelocate(Player* bot, CityId cityId, TerminalReason reason, std::string* outRejectionReason)
    {
        if (!bot)
        {
            if (outRejectionReason) *outRejectionReason = "NullBot";
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& config = ConfigManager::Instance().GetConfig();

        // 1. Global enable
        if (!config.fallbackEnabled)
        {
            if (outRejectionReason) *outRejectionReason = "FallbackDisabled";
            return false;
        }

        // 2. City allowlist check
        const CityInfo* city = DestinationCatalog::Instance().GetCity(cityId);
        if (!city || !ConfigManager::Instance().IsFallbackAllowedForCity(city->name))
        {
            if (outRejectionReason) *outRejectionReason = "CityNotAllowlisted";
            return false;
        }

        // 3. Allowed reason check
        if (reason != REASON_ROUTE_UNSUPPORTED_BEFORE_START)
        {
            if (outRejectionReason) *outRejectionReason = "ReasonNotAllowlisted";
            return false;
        }

        // 4. Bot state and protections
        if (bot->IsInCombat() || !bot->IsAlive() || bot->IsTaxiFlying() || bot->IsBeingTeleported())
        {
            if (outRejectionReason) *outRejectionReason = "BusyState";
            return false;
        }

        if (BotEngineAdapter::BotHasRealPlayerMaster(bot))
        {
            if (outRejectionReason) *outRejectionReason = "MasterClaimed";
            return false;
        }

        if (SocialProtection::Instance().IsBotProtected(bot))
        {
            if (outRejectionReason) *outRejectionReason = "Protected";
            return false;
        }

        time_t now = time(nullptr);

        // 5. Early departure backoff check
        auto depIt = m_botEarlyDepartureBackoff.find(bot->GetObjectGuid());
        if (depIt != m_botEarlyDepartureBackoff.end() && now < depIt->second)
        {
            if (outRejectionReason) *outRejectionReason = "EarlyDepartureBackoff";
            return false;
        }

        // 6. Bot/city cooldown check
        auto cdIt = m_botCityCooldown.find(bot->GetObjectGuid());
        if (cdIt != m_botCityCooldown.end() && now < cdIt->second)
        {
            if (outRejectionReason) *outRejectionReason = "BotCityCooldown";
            return false;
        }

        // 7. Rolling hourly limit check
        while (!m_recentRelocationTimes.empty() && now - m_recentRelocationTimes.front() > 3600)
        {
            m_recentRelocationTimes.pop_front();
        }

        if (m_recentRelocationTimes.size() >= config.fallbackMaxPerHour)
        {
            if (outRejectionReason) *outRejectionReason = "HourlyLimitExceeded";
            return false;
        }

        return true;
    }

    bool CityRelocationFallback::ExecuteRelocation(Player* bot, CityId cityId, TerminalReason reason)
    {
        std::string rejection;
        if (!CanRelocate(bot, cityId, reason, &rejection))
        {
            sLog.outError("BotLevelBrackets: Fallback relocation rejected for bot %s to city %u: %s",
                          bot ? bot->GetName() : "null", cityId, rejection.c_str());
            return false;
        }

        const CityInfo* city = DestinationCatalog::Instance().GetCity(cityId);
        if (!city)
            return false;

        const auto& landing = city->landing;
        bool ok = BotEngineAdapter::TeleportBot(bot, landing.mapId, landing.x, landing.y, landing.z, landing.o);

        std::lock_guard<std::mutex> lock(m_mutex);
        time_t now = time(nullptr);

        if (ok)
        {
            m_totalRelocations++;
            m_recentRelocationTimes.push_back(now);

            const auto& config = ConfigManager::Instance().GetConfig();
            m_botCityCooldown[bot->GetObjectGuid()] = now + config.fallbackCooldownSeconds;

            sLog.outString("BotLevelBrackets: Fallback relocated bot %s (%u) to %s (Reason: NativeRouteUnsupportedBeforeStart).",
                           bot->GetName(), bot->GetObjectGuid().GetCounter(), city->name);
            return true;
        }
        else
        {
            m_failedRelocations++;
            sLog.outError("BotLevelBrackets: Fallback teleport failed for bot %s to %s.",
                          bot->GetName(), city->name);
            return false;
        }
    }

    void CityRelocationFallback::OnBotDeparture(ObjectGuid botGuid, CityId /*cityId*/, uint32 dwellSeconds)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& config = ConfigManager::Instance().GetConfig();

        if (dwellSeconds < config.fallbackEarlyDepartureSeconds)
        {
            time_t now = time(nullptr);
            m_botEarlyDepartureBackoff[botGuid] = now + config.fallbackEarlyDepartureBackoffSeconds;
        }
    }

    uint32 CityRelocationFallback::GetRelocationsInPastHour() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        time_t now = time(nullptr);
        uint32 count = 0;
        for (time_t t : m_recentRelocationTimes)
        {
            if (now - t <= 3600)
                count++;
        }
        return count;
    }

    void CityRelocationFallback::ResetCounters()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_recentRelocationTimes.clear();
        m_botCityCooldown.clear();
        m_botEarlyDepartureBackoff.clear();
        m_totalRelocations = 0;
        m_failedRelocations = 0;
    }
}
