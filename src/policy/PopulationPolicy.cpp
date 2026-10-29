#include "PopulationPolicy.h"
#include <algorithm>

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
    m_cityDwellDurationMs = 180000;

    m_cityQuotaMode = 2;
    m_cityMinLimitEnabled = false;
    m_cityMaxLimitEnabled = true;
    m_cityLimitsMode = 1;
    m_cityMinBots = 0;
    m_cityMaxBots = 50;
    m_cityHysteresis = 2;

    int32_t defaults[8] = { 7, 5, 4, 4, 7, 5, 4, 4 };
    for (size_t i = 0; i < 8; ++i)
        m_cityTargets[i] = defaults[i];

    // Default 7 brackets for 1-60 progression: 1-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60
    m_allianceBrackets = {
        { 1,   9,  10 },
        { 10, 19,  15 },
        { 20, 29,  15 },
        { 30, 39,  15 },
        { 40, 49,  15 },
        { 50, 59,  15 },
        { 60, 60,  15 }
    };
    m_hordeBrackets = m_allianceBrackets;

    m_useDynamicDistribution = false;
    m_realPlayerWeight = 1.0f;
    m_syncFactions = false;
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

} // namespace BotLevelBrackets
