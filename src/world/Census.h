#ifndef MOD_TWOW_BOT_LEVEL_BRACKETS_CENSUS_H
#define MOD_TWOW_BOT_LEVEL_BRACKETS_CENSUS_H

#include "ReadOnlyCatalog.h"
#include "../policy/QuotaAllocator.h"
#include <cstdint>
#include <vector>
#include <array>

namespace BotLevelBrackets
{

struct BracketCensusItem
{
    uint8_t lower;
    uint8_t upper;
    uint32_t botCount;
    uint32_t humanCount;
    float botPercent;
    float desiredPercent;
    int32_t shortage; // positive = shortage, negative = surplus
};

struct CityCensusItem
{
    CityId id;
    const char* name;
    uint8_t teamId;
    uint32_t residents;
    uint32_t confirmedInbound;
    uint32_t pendingReservations;
    uint32_t effective;
    uint32_t rawTarget;
    uint32_t feasibleTarget;
    uint32_t shortfall;
    PopulationImbalance imbalance;
};

struct CensusSnapshot
{
    uint32_t timestamp = 0;
    uint32_t allianceTotalBots = 0;
    uint32_t hordeTotalBots = 0;
    uint32_t allianceTotalHumans = 0;
    uint32_t hordeTotalHumans = 0;
    uint32_t allianceFieldBots = 0;
    uint32_t hordeFieldBots = 0;

    std::array<CityCensusItem, 8> cities{};
    std::vector<BracketCensusItem> allianceBrackets;
    std::vector<BracketCensusItem> hordeBrackets;
};

class Census
{
public:
    static Census& Instance();

    CensusSnapshot TakeSnapshot();
    const CensusSnapshot& GetLastSnapshot() const { return m_lastSnapshot; }

private:
    Census() = default;
    CensusSnapshot m_lastSnapshot;
};

} // namespace BotLevelBrackets

#endif // MOD_TWOW_BOT_LEVEL_BRACKETS_CENSUS_H
