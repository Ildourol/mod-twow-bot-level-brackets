#include "Census.h"
#include "../policy/PopulationPolicy.h"
#include "../policy/QuotaAllocator.h"
#include "../compat/NativeTaskAdapter.h"
#include "../scheduling/RequestLedger.h"

#ifdef HAVE_TORTOISEBOTS
#include "Common.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#elif defined(HAVE_LEGACY_PLAYERBOTS)
#include "Common.h"
#include "Player.h"
#include "ObjectAccessor.h"
#else
// Test Doubles
#include "support/Framework.h"
#endif

#include <ctime>
#include <algorithm>

namespace BotLevelBrackets
{

Census& Census::Instance()
{
    static Census instance;
    return instance;
}

CensusSnapshot Census::TakeSnapshot()
{
    CensusSnapshot snap{};
    snap.timestamp = static_cast<uint32_t>(time(nullptr));

    const auto& catalog = ReadOnlyCatalog::Instance();
    const auto& policy = PopulationPolicy::Instance();
    const auto& cities = catalog.GetCities();

    for (size_t i = 0; i < 8 && i < cities.size(); ++i)
    {
        snap.cities[i].id = cities[i].id;
        snap.cities[i].name = cities[i].name;
        snap.cities[i].teamId = cities[i].teamId;
    }

    // Set up bracket containers
    const auto& aBConfig = policy.GetLevelBrackets(0);
    snap.allianceBrackets.resize(aBConfig.size());
    for (size_t i = 0; i < aBConfig.size(); ++i)
    {
        snap.allianceBrackets[i].lower = aBConfig[i].lower;
        snap.allianceBrackets[i].upper = aBConfig[i].upper;
        snap.allianceBrackets[i].desiredPercent = static_cast<float>(aBConfig[i].desiredPercent);
    }

    const auto& hBConfig = policy.GetLevelBrackets(1);
    snap.hordeBrackets.resize(hBConfig.size());
    for (size_t i = 0; i < hBConfig.size(); ++i)
    {
        snap.hordeBrackets[i].lower = hBConfig[i].lower;
        snap.hordeBrackets[i].upper = hBConfig[i].upper;
        snap.hordeBrackets[i].desiredPercent = static_cast<float>(hBConfig[i].desiredPercent);
    }

    // Iterate through all players online
    const auto& playerMap = sObjectAccessor.GetPlayers();
    for (const auto& kv : playerMap)
    {
        Player* player = kv.second;
        if (!player || !player->IsInWorld())
            continue;

        uint8_t team = (player->GetTeamId() == 1) ? 1 : 0; // 0 = Alliance, 1 = Horde
        uint8_t level = player->GetLevel();
        bool isBot = NativeTaskAdapter::IsBot(player);

        if (isBot)
        {
            if (team == 0)
                ++snap.allianceTotalBots;
            else
                ++snap.hordeTotalBots;

            // Check level bracket
            auto& brackets = (team == 0) ? snap.allianceBrackets : snap.hordeBrackets;
            for (auto& b : brackets)
            {
                if (level >= b.lower && level <= b.upper)
                {
                    ++b.botCount;
                    break;
                }
            }

            // Check city residency
            const CityDefinition* cityDef = catalog.FindCityForLocation(
                player->GetMapId(), player->GetAreaId(), player->GetAreaId(),
                player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

            if (cityDef)
            {
                size_t cIdx = static_cast<size_t>(cityDef->id) - 1;
                if (cIdx < 8)
                    ++snap.cities[cIdx].residents;
            }
            else
            {
                if (team == 0)
                    ++snap.allianceFieldBots;
                else
                    ++snap.hordeFieldBots;
            }
        }
        else
        {
            // Human player
            if (team == 0)
                ++snap.allianceTotalHumans;
            else
                ++snap.hordeTotalHumans;

            auto& brackets = (team == 0) ? snap.allianceBrackets : snap.hordeBrackets;
            for (auto& b : brackets)
            {
                if (level >= b.lower && level <= b.upper)
                {
                    ++b.humanCount;
                    break;
                }
            }
        }
    }

    // Query RequestLedger and NativeTaskAdapter for inbound / pending counts
    const auto& ledger = RequestLedger::Instance();
    for (size_t i = 0; i < 8; ++i)
    {
        CityId cId = snap.cities[i].id;
        snap.cities[i].confirmedInbound = NativeTaskAdapter::GetCityInboundCount(cId);
        snap.cities[i].pendingReservations = ledger.GetPendingCityInbound(cId);

        uint32_t factionTotal = (snap.cities[i].teamId == 0) ? snap.allianceTotalBots : snap.hordeTotalBots;
        QuotaCalculationResult qr = QuotaAllocator::EvaluateCityDemand(
            i, factionTotal, snap.cities[i].residents,
            snap.cities[i].confirmedInbound, snap.cities[i].pendingReservations, 0);

        snap.cities[i].effective = qr.effectiveResidents;
        snap.cities[i].rawTarget = qr.rawTarget;
        snap.cities[i].feasibleTarget = qr.feasibleTarget;
        snap.cities[i].shortfall = qr.shortfall;
        snap.cities[i].imbalance = qr.imbalance;
    }

    // Calculate level bracket percentages and shortfalls
    auto calcBrackets = [](std::vector<BracketCensusItem>& brackets, uint32_t totalBots)
    {
        if (totalBots == 0)
            return;
        for (auto& b : brackets)
        {
            b.botPercent = (static_cast<float>(b.botCount) * 100.0f) / static_cast<float>(totalBots);
            uint32_t desiredCount = static_cast<uint32_t>((static_cast<float>(totalBots) * b.desiredPercent + 50.0f) / 100.0f);
            b.shortage = static_cast<int32_t>(desiredCount) - static_cast<int32_t>(b.botCount);
        }
    };
    calcBrackets(snap.allianceBrackets, snap.allianceTotalBots);
    calcBrackets(snap.hordeBrackets, snap.hordeTotalBots);

    m_lastSnapshot = snap;
    return snap;
}

} // namespace BotLevelBrackets
