#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_QUOTA_ALLOCATOR_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_QUOTA_ALLOCATOR_H

#include <cstdint>
#include <cstddef>

namespace BotLevelBrackets
{

enum class PopulationImbalance : uint8_t
{
    Neutral = 0,
    Underpopulated = 1,
    Overpopulated = 2
};

struct QuotaCalculationResult
{
    uint32_t rawTarget;
    uint32_t boundedTarget;
    uint32_t feasibleTarget;
    uint32_t effectiveResidents;
    uint32_t shortfall;
    PopulationImbalance imbalance;
};

class QuotaAllocator
{
public:
    static uint32_t CalculateCityQuota(size_t cityIndex, uint32_t factionTotal,
                                      uint32_t quotaMode, bool minLimitEnabled,
                                      bool maxLimitEnabled, uint32_t limitsMode,
                                      uint32_t minBots, uint32_t maxBots,
                                      int32_t const* cityTargets);

    static QuotaCalculationResult EvaluateCityDemand(size_t cityIndex, uint32_t factionTotal,
                                                     uint32_t actualResidents,
                                                     uint32_t confirmedInbound,
                                                     uint32_t pendingReservations,
                                                     uint32_t availableIdleSupply);

    static PopulationImbalance ClassifyImbalance(uint32_t quota, uint32_t effectiveResidents,
                                                 uint32_t hysteresis);
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_QUOTA_ALLOCATOR_H
