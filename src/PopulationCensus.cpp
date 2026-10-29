#include "PopulationCensus.h"
#include "BracketsConfig.h"
#include "DestinationCatalog.h"
#include "OpportunityLedger.h"
#include "SocialProtection.h"
#include "BotEngineAdapter.h"
#include "ObjectAccessor.h"

#include <cmath>
#include <algorithm>
#include <numeric>

namespace Brackets
{
    CensusSnapshot PopulationCensus::TakeSnapshot()
    {
        CensusSnapshot snap;
        snap.timestamp = time(nullptr);

        const auto& config = ConfigManager::Instance().GetConfig();
        snap.generation = config.generation;

        // Initialize city census records
        snap.cities.resize(CITY_COUNT);
        for (uint8 i = 0; i < CITY_COUNT; ++i)
        {
            snap.cities[i].id = static_cast<CityId>(i);
            const CityInfo* ci = DestinationCatalog::Instance().GetCity(static_cast<CityId>(i));
            snap.cities[i].teamId = ci ? ci->teamId : (i < 4 ? 0 : 1);
            snap.cities[i].confirmedInbound = OpportunityLedger::Instance().GetConfirmedInboundCount(DOMAIN_CITY, i);
            snap.cities[i].pending = OpportunityLedger::Instance().GetPendingReservationCount(DOMAIN_CITY, i);
        }

        // Initialize outdoor region records
        const auto& allRegions = DestinationCatalog::Instance().GetOutdoorRegions();
        snap.regions.resize(allRegions.size());
        for (size_t i = 0; i < allRegions.size(); ++i)
        {
            const auto& r = allRegions[i];
            snap.regions[i].regionId = r.regionId;
            snap.regions[i].teamAccess = r.teamAccess;
            snap.regions[i].minLevel = r.minLevel;
            snap.regions[i].maxLevel = r.maxLevel;
            snap.regions[i].confirmedInbound = OpportunityLedger::Instance().GetConfirmedInboundCount(DOMAIN_WORLD, r.regionId);
            snap.regions[i].pending = OpportunityLedger::Instance().GetPendingReservationCount(DOMAIN_WORLD, r.regionId);
        }

        // Initialize bracket census records
        snap.allianceBrackets.resize(config.allianceLevelRanges.size());
        for (size_t i = 0; i < config.allianceLevelRanges.size(); ++i)
        {
            snap.allianceBrackets[i].lower = config.allianceLevelRanges[i].lower;
            snap.allianceBrackets[i].upper = config.allianceLevelRanges[i].upper;
            snap.allianceBrackets[i].staticPct = config.allianceLevelRanges[i].desiredPercent;
        }

        snap.hordeBrackets.resize(config.hordeLevelRanges.size());
        for (size_t i = 0; i < config.hordeLevelRanges.size(); ++i)
        {
            snap.hordeBrackets[i].lower = config.hordeLevelRanges[i].lower;
            snap.hordeBrackets[i].upper = config.hordeLevelRanges[i].upper;
            snap.hordeBrackets[i].staticPct = config.hordeLevelRanges[i].desiredPercent;
        }

        // Iterate all online players once
        HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
        for (const auto& pair : players)
        {
            Player* p = pair.second;
            if (!p || !p->IsInWorld() || !p->GetSession())
                continue;

            snap.totalPlayersOnline++;

            bool isBot = BotEngineAdapter::IsBot(p);
            if (!isBot)
            {
                snap.totalHumansOnline++;
                uint8 lvl = p->GetLevel();
                uint8 team = p->GetTeamId();

                if (team == 0)
                {
                    for (auto& b : snap.allianceBrackets)
                        if (lvl >= b.lower && lvl <= b.upper) b.observedHumans++;
                }
                else
                {
                    for (auto& b : snap.hordeBrackets)
                        if (lvl >= b.lower && lvl <= b.upper) b.observedHumans++;
                }
                continue;
            }

            snap.totalBotsOnline++;
            uint8 team = p->GetTeamId();
            if (team == 0)
                snap.allianceBotsOnline++;
            else
                snap.hordeBotsOnline++;

            // Check eligibility for scheduling
            bool isRandom = BotEngineAdapter::IsRandomBot(p);
            bool hasMaster = BotEngineAdapter::BotHasRealPlayerMaster(p);
            bool isProtected = SocialProtection::Instance().IsBotProtected(p);

            if (isRandom && !hasMaster && !isProtected)
            {
                if (team == 0)
                    snap.allianceEligibleBots++;
                else
                    snap.hordeEligibleBots++;
            }

            // Track level distribution
            uint8 lvl = p->GetLevel();
            if (team == 0)
            {
                for (auto& b : snap.allianceBrackets)
                    if (lvl >= b.lower && lvl <= b.upper) b.actualCount++;
            }
            else
            {
                for (auto& b : snap.hordeBrackets)
                    if (lvl >= b.lower && lvl <= b.upper) b.actualCount++;
            }

            // Track location (residents)
            uint32 mapId = p->GetMapId();
            uint32 areaId = p->GetAreaId();
            float x = p->GetPositionX();
            float y = p->GetPositionY();
            float z = p->GetPositionZ();

            const CityInfo* city = DestinationCatalog::Instance().GetCityForArea(mapId, areaId, x, y, z);
            if (city)
            {
                snap.cities[city->id].residents++;
            }
            else
            {
                const OutdoorRegion* reg = DestinationCatalog::Instance().GetRegionForLocation(mapId, areaId, areaId);
                if (reg)
                {
                    for (auto& rRec : snap.regions)
                    {
                        if (rRec.regionId == reg->regionId)
                        {
                            rRec.residents++;
                            break;
                        }
                    }
                }
            }
        }

        // Perform quota math
        CalculateCityQuotas(snap);
        CalculateOutdoorQuotas(snap);
        CalculateDynamicLevelWeights(snap);

        m_latestSnapshot = snap;
        return snap;
    }

    void PopulationCensus::CalculateCityQuotas(CensusSnapshot& snap)
    {
        const auto& config = ConfigManager::Instance().GetConfig();

        for (uint8 team = 0; team < 2; ++team)
        {
            uint32 denom = (team == 0) ? snap.allianceBotsOnline : snap.hordeBotsOnline;
            uint32 citySum = 0;

            for (uint8 i = 0; i < CITY_COUNT; ++i)
            {
                auto& rec = snap.cities[i];
                if (rec.teamId != team)
                    continue;

                rec.effectivePopulation = rec.residents + rec.confirmedInbound + rec.pending;

                uint32 configuredVal = config.hubCityPcts[i];
                if (configuredVal == 0 || denom == 0)
                {
                    rec.rawTarget = 0;
                    rec.boundedTarget = 0;
                    rec.feasibleTarget = 0;
                    rec.actionableDeficit = 0;
                    continue;
                }

                // 1. Raw target
                if (config.hubQuotaMode == 1) // Fixed headcount
                {
                    rec.rawTarget = configuredVal;
                }
                else // Percentage of online bots
                {
                    rec.rawTarget = (denom * configuredVal + 50) / 100;
                }

                // 2. Bounds
                uint32 minB = (config.hubLimitsMode == 1) ? config.hubMinBotsPerHub : (denom * config.hubMinBotsPerHub + 50) / 100;
                uint32 maxB = (config.hubLimitsMode == 1) ? config.hubMaxBotsPerHub : (denom * config.hubMaxBotsPerHub + 50) / 100;

                rec.boundedTarget = rec.rawTarget;
                if (config.hubMaxLimitEnabled && rec.boundedTarget > maxB)
                    rec.boundedTarget = maxB;
                if (config.hubMinLimitEnabled && rec.boundedTarget < minB)
                    rec.boundedTarget = minB;

                if (rec.boundedTarget > denom)
                    rec.boundedTarget = denom;

                citySum += rec.boundedTarget;
            }

            // 3. Feasible target: apply FieldReservePct (e.g. 70% reserved for field, max 30% for cities)
            uint32 maxTotalCityDemand = (denom * (100 - config.fieldReservePct)) / 100;
            float scale = 1.0f;
            if (citySum > maxTotalCityDemand && citySum > 0)
            {
                scale = static_cast<float>(maxTotalCityDemand) / static_cast<float>(citySum);
            }

            for (uint8 i = 0; i < CITY_COUNT; ++i)
            {
                auto& rec = snap.cities[i];
                if (rec.teamId != team)
                    continue;

                rec.feasibleTarget = static_cast<uint32>(rec.boundedTarget * scale);

                // 4. Actionable deficit with hysteresis
                if (rec.feasibleTarget > rec.effectivePopulation)
                {
                    uint32 deficit = rec.feasibleTarget - rec.effectivePopulation;
                    if (deficit >= config.hubHysteresisBots)
                        rec.actionableDeficit = deficit;
                    else
                        rec.actionableDeficit = 0;
                }
                else
                {
                    rec.actionableDeficit = 0;
                }
            }

            if (team == 0)
                snap.allianceFieldSupply = (denom > citySum) ? (denom - citySum) : 0;
            else
                snap.hordeFieldSupply = (denom > citySum) ? (denom - citySum) : 0;
        }
    }

    void PopulationCensus::CalculateOutdoorQuotas(CensusSnapshot& snap)
    {
        const auto& config = ConfigManager::Instance().GetConfig();

        for (auto& rRec : snap.regions)
        {
            rRec.effectivePopulation = rRec.residents + rRec.confirmedInbound + rRec.pending;

            // Simplified quota allocation for regional supply
            uint32 fieldSupply = (rRec.teamAccess == 0) ? snap.allianceFieldSupply :
                                 (rRec.teamAccess == 1) ? snap.hordeFieldSupply :
                                 (snap.allianceFieldSupply + snap.hordeFieldSupply) / 2;

            if (fieldSupply == 0)
            {
                rRec.desired = 0;
                rRec.actionableDeficit = 0;
                continue;
            }

            // Distribute supply evenly across regions matching level and faction
            rRec.desired = std::max<uint32>(2, fieldSupply / static_cast<uint32>(std::max<size_t>(1, snap.regions.size() / 2)));

            if (rRec.desired > rRec.effectivePopulation)
            {
                uint32 def = rRec.desired - rRec.effectivePopulation;
                rRec.actionableDeficit = std::min<uint32>(def, config.maxInboundPerRegion);
            }
            else
            {
                rRec.actionableDeficit = 0;
            }
        }
    }

    void PopulationCensus::CalculateDynamicLevelWeights(CensusSnapshot& snap)
    {
        const auto& config = ConfigManager::Instance().GetConfig();
        float weight = config.realPlayerWeight;
        float alpha = weight / (1.0f + weight);

        auto computeBlend = [&](std::vector<BracketCensusRecord>& brackets, uint32 onlineTotal)
        {
            if (brackets.empty() || onlineTotal == 0)
                return;

            uint32 totalHumans = 0;
            for (const auto& b : brackets)
                totalHumans += b.observedHumans;

            uint32 sumStatic = 0;
            for (const auto& b : brackets)
                sumStatic += b.staticPct;
            if (sumStatic == 0)
                sumStatic = 1;

            float currentAlpha = (totalHumans > 0) ? alpha : 0.0f;

            // Largest remainder allocation
            std::vector<float> desiredExact(brackets.size());
            std::vector<uint32> floored(brackets.size());
            std::vector<std::pair<float, size_t>> remainders(brackets.size());
            uint32 allocatedSum = 0;

            for (size_t i = 0; i < brackets.size(); ++i)
            {
                float staticShare = static_cast<float>(brackets[i].staticPct) / static_cast<float>(sumStatic);
                float humanShare = (totalHumans > 0) ? (static_cast<float>(brackets[i].observedHumans) / static_cast<float>(totalHumans)) : staticShare;

                float blendedShare = (1.0f - currentAlpha) * staticShare + currentAlpha * humanShare;
                brackets[i].desiredPct = blendedShare * 100.0f;

                desiredExact[i] = blendedShare * static_cast<float>(onlineTotal);
                floored[i] = static_cast<uint32>(std::floor(desiredExact[i]));
                allocatedSum += floored[i];
                remainders[i] = { desiredExact[i] - floored[i], i };
            }

            uint32 remainderCount = (onlineTotal >= allocatedSum) ? (onlineTotal - allocatedSum) : 0;
            std::sort(remainders.rbegin(), remainders.rend());

            for (uint32 r = 0; r < remainderCount && r < remainders.size(); ++r)
            {
                floored[remainders[r].second]++;
            }

            for (size_t i = 0; i < brackets.size(); ++i)
            {
                brackets[i].desiredCount = floored[i];
            }
        };

        computeBlend(snap.allianceBrackets, snap.allianceBotsOnline);
        computeBlend(snap.hordeBrackets, snap.hordeBotsOnline);
    }

    std::vector<uint32> PopulationCensus::CalculateLargestRemainderQuotas(const std::vector<float>& shares, uint32 total)
    {
        std::vector<uint32> floored(shares.size(), 0);
        if (shares.empty() || total == 0)
            return floored;

        float sumShares = 0.0f;
        for (float s : shares)
            sumShares += s;
        if (sumShares <= 0.0f)
            sumShares = 1.0f;

        std::vector<float> exact(shares.size());
        std::vector<std::pair<float, size_t>> remainders(shares.size());
        uint32 allocated = 0;

        for (size_t i = 0; i < shares.size(); ++i)
        {
            exact[i] = (shares[i] / sumShares) * static_cast<float>(total);
            floored[i] = static_cast<uint32>(std::floor(exact[i]));
            allocated += floored[i];
            remainders[i] = { exact[i] - static_cast<float>(floored[i]), i };
        }

        uint32 needed = (total >= allocated) ? (total - allocated) : 0;
        std::sort(remainders.rbegin(), remainders.rend());
        for (uint32 r = 0; r < needed && r < remainders.size(); ++r)
            floored[remainders[r].second]++;

        return floored;
    }

    int32 PopulationCensus::CalculateActionableDeficit(uint32 target, uint32 current, uint32 hysteresis)
    {
        if (target > current)
        {
            uint32 diff = target - current;
            if (diff >= hysteresis)
                return static_cast<int32>(diff);
        }
        return 0;
    }
}
