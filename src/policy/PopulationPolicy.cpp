#include "PopulationPolicy.h"
#include <algorithm>
#include <cmath>

#if defined(HAVE_TORTOISEBOTS) || defined(HAVE_LEGACY_PLAYERBOTS)
#include "Log.h"
#else
#include "Log.h"
#endif

namespace BotLevelBrackets
{

PopulationPolicy& PopulationPolicy::Instance()
{
    static PopulationPolicy instance;
    return instance;
}

PopulationPolicy::PopulationPolicy()
{
    LoadDefaults();
}

void PopulationPolicy::LoadDefaults()
{
    m_enabled = true;
    m_cityEnabled = true;
    m_worldEnabled = true;
    m_dryRun = false;

    m_cityDestPolicy = DestinationPolicy::RequireExact;
    m_worldDestPolicy = DestinationPolicy::RequireExact;

    m_checkSeconds = 90;
    m_maxRequestsPerCycle = 5;
    m_maxOutstandingPerFaction = 10;
    m_maxLooseRequestsPerCycle = 1;
    m_requestCooldownSeconds = 1800;
    m_rejectionBackoffSeconds = 600;
    m_stabilitySeconds = 60;
    m_dispatchTimeoutSeconds = 30;
    m_earlyDepartureBackoffSeconds = 3600;
    m_fieldReservePct = 70;
    m_cityDwellDurationMs = 300000;
    m_cityMinDwellDurationMs = 300000;
    m_cityMaxDwellDurationMs = 600000;
    m_cityTransitMode = TransitMode::Travel;
    m_cityPickAllBrackets = true;
    m_cityMinLevel = 1;

    m_cityQuotaMode = 2;
    m_cityMinLimitEnabled = false;
    m_cityMaxLimitEnabled = true;
    m_cityLimitsMode = 1;
    m_cityMinBots = 0;
    m_cityMaxBots = 60;
    m_cityHysteresis = 2;

    int32_t defaults[8] = { 10, 8, 6, 6, 10, 8, 6, 6 };
    for (size_t i = 0; i < 8; ++i)
        m_cityTargets[i] = defaults[i];

    // Default 7 brackets for 1-60 progression: 1-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60
    m_allianceBrackets = {
        { 1,   9,  16 },
        { 10, 19,  14 },
        { 20, 29,  14 },
        { 30, 39,  14 },
        { 40, 49,  14 },
        { 50, 59,  14 },
        { 60, 60,  14 }
    };
    m_hordeBrackets = m_allianceBrackets;

    m_useDynamicDistribution = false;
    m_realPlayerWeight = 1.0f;
    m_syncFactions = false;

    m_levelDistributionEnabled = true;
    m_distributionCheckFrequency = 300;
    m_checkFlaggedFrequency = 15;
    m_flaggedProcessLimit = 5;
    m_minBracketChangeThreshold = 2;
    m_maxPendingQueueSize = 0;
    m_pendingQueueTTL = 0;
    m_teleportOnLevelChange = true;
    m_arrivalFloorHoldEnabled = true;
    m_arrivalHoldDurationMs = 5000;
    m_maxFloorLifts = 6;

    m_startingZoneDisperseEnabled = true;
    m_startingZoneDisperseFrequency = 15;

    m_wrongMapDisperseEnabled = true;
    m_wrongMapDisperseFrequency = 60;

    m_cityLandingDestinations.clear();
    m_starterLocationPools.clear();
    m_allianceRangePools.clear();
    m_allianceRangePools.resize(7);
    m_hordeRangePools.clear();
    m_hordeRangePools.resize(7);
}

int32_t PopulationPolicy::GetCityTarget(size_t cityIndex) const
{
    if (cityIndex < 8)
        return m_cityTargets[cityIndex];
    return 0;
}

void PopulationPolicy::SetCityTarget(size_t cityIndex, int32_t val)
{
    if (cityIndex < 8)
        m_cityTargets[cityIndex] = val;
}

const std::vector<LevelBracketConfig>& PopulationPolicy::GetLevelBrackets(uint8_t team) const
{
    if (team == 1 && !m_syncFactions)
        return m_hordeBrackets;
    return m_allianceBrackets;
}

void PopulationPolicy::SetLevelBrackets(uint8_t team, const std::vector<LevelBracketConfig>& brackets)
{
    if (team == 1 && !m_syncFactions)
        m_hordeBrackets = brackets;
    else
    {
        m_allianceBrackets = brackets;
        if (m_syncFactions)
            m_hordeBrackets = brackets;
    }
}

void PopulationPolicy::ClampAndBalanceBrackets(bool warnLayout)
{
    auto clampAndBalance = [warnLayout](std::vector<LevelBracketConfig>& ranges, const char* factionName)
    {
        for (auto& r : ranges)
        {
            if (r.lower < 1)
                r.lower = 1;
            if (r.upper > 60)
                r.upper = 60;
            if (r.lower > r.upper)
                r.desiredPercent = 0;
        }

        uint32_t total = 0;
        std::vector<size_t> valid;
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            if (ranges[i].lower <= ranges[i].upper)
            {
                valid.push_back(i);
                total += ranges[i].desiredPercent;
            }
            else
                ranges[i].desiredPercent = 0;
        }

        if (valid.empty())
            return;

        std::vector<double> remainders(ranges.size(), 0.0);
        uint32_t assigned = 0;
        for (size_t index : valid)
        {
            double raw = total > 0
                ? (static_cast<double>(ranges[index].desiredPercent) * 100.0 / total)
                : (100.0 / valid.size());
            uint32_t base = static_cast<uint32_t>(std::floor(raw));
            ranges[index].desiredPercent = static_cast<uint8_t>(base);
            remainders[index] = raw - base;
            assigned += base;
        }

        std::stable_sort(valid.begin(), valid.end(), [&remainders](size_t left, size_t right)
        {
            return remainders[left] > remainders[right];
        });
        for (size_t i = 0; assigned < 100; ++i, ++assigned)
            ++ranges[valid[i % valid.size()]].desiredPercent;

        if (total != 100)
        {
            sLog.outString("[BotLevelBrackets] Normalized %s bracket percentages from %u to 100.", factionName, total);
        }

        if (warnLayout)
        {
            for (size_t i = 1; i < ranges.size(); ++i)
            {
                if (ranges[i].lower <= ranges[i - 1].upper)
                {
                    sLog.outError("[BotLevelBrackets] %s Range %u (%u-%u) overlaps with Range %u (%u-%u)!",
                        factionName, static_cast<unsigned>(i + 1), ranges[i].lower, ranges[i].upper,
                        static_cast<unsigned>(i), ranges[i - 1].lower, ranges[i - 1].upper);
                }
            }
        }
    };

    clampAndBalance(m_allianceBrackets, "Alliance");
    clampAndBalance(m_hordeBrackets, "Horde");
}

} // namespace BotLevelBrackets
