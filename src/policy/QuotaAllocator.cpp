#include "QuotaAllocator.h"
#include "PopulationPolicy.h"
#include <algorithm>

namespace BotLevelBrackets
{

uint32_t QuotaAllocator::CalculateCityQuota(size_t cityIndex, uint32_t factionTotal,
                                           uint32_t quotaMode, bool minLimitEnabled,
                                           bool maxLimitEnabled, uint32_t limitsMode,
                                           uint32_t minBots, uint32_t maxBots,
                                           int32_t const* cityTargets)
{
    if (cityIndex >= 8 || factionTotal == 0 || !cityTargets)
        return 0;

    int32_t configuredValue = cityTargets[cityIndex];
    if (configuredValue <= 0)
        return 0;

    uint32_t val = static_cast<uint32_t>(configuredValue);
    // QuotaMode 1: fixed count; QuotaMode 2: percentage
    uint32_t quota = (quotaMode == 1) ? val : ((factionTotal * val + 50) / 100);

    // LimitsMode 1: fixed count; LimitsMode 2: percentage
    uint32_t minBound = (limitsMode == 1) ? minBots : ((factionTotal * minBots + 50) / 100);
    uint32_t maxBound = (limitsMode == 1) ? maxBots : ((factionTotal * maxBots + 50) / 100);

    if (maxLimitEnabled)
        quota = std::min(quota, maxBound);

    if (minLimitEnabled)
    {
        uint32_t effectiveMin = maxLimitEnabled ? std::min(minBound, maxBound) : minBound;
        quota = std::max(quota, effectiveMin);
    }

    return std::min(quota, factionTotal);
}

PopulationImbalance QuotaAllocator::ClassifyImbalance(uint32_t quota, uint32_t effectiveResidents,
                                                      uint32_t hysteresis)
{
    if (quota == 0)
        return (effectiveResidents > 0) ? PopulationImbalance::Overpopulated : PopulationImbalance::Neutral;

    if (effectiveResidents > quota + hysteresis)
        return PopulationImbalance::Overpopulated;

    if (effectiveResidents + hysteresis < quota)
        return PopulationImbalance::Underpopulated;

    return PopulationImbalance::Neutral;
}

QuotaCalculationResult QuotaAllocator::EvaluateCityDemand(size_t cityIndex, uint32_t factionTotal,
                                                          uint32_t actualResidents,
                                                          uint32_t confirmedInbound,
                                                          uint32_t pendingReservations,
                                                          uint32_t availableIdleSupply)
{
    QuotaCalculationResult result{};
    const auto& policy = PopulationPolicy::Instance();

    int32_t rawVal = policy.GetCityTarget(cityIndex);
    if (rawVal <= 0 || factionTotal == 0)
    {
        result.effectiveResidents = actualResidents + confirmedInbound + pendingReservations;
        result.imbalance = ClassifyImbalance(0, result.effectiveResidents, policy.GetCityHysteresis());
        return result;
    }

    result.rawTarget = (policy.GetCityQuotaMode() == 1) ?
        static_cast<uint32_t>(rawVal) : ((factionTotal * static_cast<uint32_t>(rawVal) + 50) / 100);

    int32_t targets[8];
    for (size_t i = 0; i < 8; ++i)
        targets[i] = policy.GetCityTarget(i);

    result.boundedTarget = CalculateCityQuota(
        cityIndex, factionTotal, policy.GetCityQuotaMode(),
        policy.IsCityMinLimitEnabled(), policy.IsCityMaxLimitEnabled(),
        policy.GetCityLimitsMode(), policy.GetCityMinBots(),
        policy.GetCityMaxBots(), targets);

    // Feasible target accounts for field reserve (leaving fieldReservePct in the open world)
    uint32_t maxCityPopulation = (factionTotal * (100 - policy.GetFieldReservePct()) + 50) / 100;
    result.feasibleTarget = std::min(result.boundedTarget, maxCityPopulation);

    result.effectiveResidents = actualResidents + confirmedInbound + pendingReservations;

    if (result.feasibleTarget > result.effectiveResidents)
        result.shortfall = result.feasibleTarget - result.effectiveResidents;
    else
        result.shortfall = 0;

    result.imbalance = ClassifyImbalance(result.feasibleTarget, result.effectiveResidents, policy.GetCityHysteresis());

    return result;
}

} // namespace BotLevelBrackets
