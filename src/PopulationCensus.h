#ifndef BRACKETS_POPULATION_CENSUS_H
#define BRACKETS_POPULATION_CENSUS_H

#include "BracketsCommon.h"
#include "DestinationCatalog.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Brackets
{
    struct CityCensusRecord
    {
        CityId id;
        uint8  teamId;
        uint32 residents{0};
        uint32 confirmedInbound{0};
        uint32 pending{0};
        uint32 effectivePopulation{0};
        uint32 rawTarget{0};
        uint32 boundedTarget{0};
        uint32 feasibleTarget{0};
        uint32 actionableDeficit{0};
    };

    struct RegionCensusRecord
    {
        uint32 regionId{0};
        uint8  teamAccess{2};
        uint8  minLevel{1};
        uint8  maxLevel{60};
        uint32 residents{0};
        uint32 confirmedInbound{0};
        uint32 pending{0};
        uint32 effectivePopulation{0};
        uint32 desired{0};
        uint32 actionableDeficit{0};
    };

    struct BracketCensusRecord
    {
        uint8  lower{1};
        uint8  upper{60};
        uint8  staticPct{0};
        float  desiredPct{0.0f};
        uint32 desiredCount{0};
        uint32 actualCount{0};
        uint32 observedHumans{0};
    };

    struct CensusSnapshot
    {
        time_t timestamp{0};
        uint32 generation{0};

        // Total online counts
        uint32 totalPlayersOnline{0};
        uint32 totalBotsOnline{0};
        uint32 totalHumansOnline{0};

        uint32 allianceBotsOnline{0};
        uint32 hordeBotsOnline{0};
        uint32 allianceEligibleBots{0};
        uint32 hordeEligibleBots{0};

        // Level distribution
        std::vector<BracketCensusRecord> allianceBrackets;
        std::vector<BracketCensusRecord> hordeBrackets;

        // Capital Cities
        std::vector<CityCensusRecord> cities;

        // Outdoor Regions
        std::vector<RegionCensusRecord> regions;

        // Field reserve checks
        uint32 allianceFieldSupply{0};
        uint32 hordeFieldSupply{0};
    };

    class PopulationCensus
    {
    public:
        static PopulationCensus& Instance()
        {
            static PopulationCensus instance;
            return instance;
        }

        CensusSnapshot TakeSnapshot();
        const CensusSnapshot& GetLatestSnapshot() const { return m_latestSnapshot; }
        void Reset() { m_latestSnapshot = CensusSnapshot{}; }

        static std::vector<uint32> CalculateLargestRemainderQuotas(const std::vector<float>& shares, uint32 total);
        static int32 CalculateActionableDeficit(uint32 target, uint32 current, uint32 hysteresis);

    private:
        PopulationCensus() = default;
        ~PopulationCensus() = default;

        void CalculateCityQuotas(CensusSnapshot& snap);
        void CalculateOutdoorQuotas(CensusSnapshot& snap);
        void CalculateDynamicLevelWeights(CensusSnapshot& snap);

        CensusSnapshot m_latestSnapshot;
    };
}

#endif // BRACKETS_POPULATION_CENSUS_H
