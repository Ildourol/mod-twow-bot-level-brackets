#include "OpportunityCoordinator.h"
#include "BracketsConfig.h"
#include "DestinationCatalog.h"
#include "ActivityRegistry.h"
#include "OpportunityLedger.h"
#include "CityRelocationFallback.h"
#include "SocialProtection.h"
#include "BracketsDebug.h"
#include "BracketsAiContext.h"
#include "BotEngineAdapter.h"
#include "ObjectAccessor.h"
#include "Log.h"

#include <algorithm>
#include <random>

namespace Brackets
{
    void OpportunityCoordinator::Initialize()
    {
        const auto& config = ConfigManager::Instance().GetConfig();
        m_cycleTimer = 0;
        m_maintenanceTimer = 0;
        m_nextCycleTime = time(nullptr) + config.checkSeconds;

        sLog.outString("BotLevelBrackets: Opportunity Coordinator initialized (Cadence=%us, SharedLimit=%u).",
                       config.checkSeconds, config.maxAdmissionsPerCycle);
    }

    void OpportunityCoordinator::Update(uint32 diff)
    {
        m_maintenanceTimer += diff;
        if (m_maintenanceTimer >= 1000)
        {
            m_maintenanceTimer = 0;
            RunMaintenanceTick();
        }

        m_cycleTimer += diff;
        const auto& config = ConfigManager::Instance().GetConfig();
        if (m_cycleTimer >= config.checkSeconds * 1000)
        {
            m_cycleTimer = 0;
            m_nextCycleTime = time(nullptr) + config.checkSeconds;
            RunDistributionCycle();
        }
    }

    void OpportunityCoordinator::ForceCycle()
    {
        m_cycleTimer = 0;
        m_nextCycleTime = time(nullptr) + ConfigManager::Instance().GetConfig().checkSeconds;
        RunDistributionCycle();
    }

    bool OpportunityCoordinator::HasProposedOpportunity(ObjectGuid botGuid, Domain domain) const
    {
        OpportunityIntent intent = OpportunityLedger::Instance().GetActiveIntent(botGuid);
        return (intent.intentId > 0 && intent.domain == domain && intent.state == INTENT_STATE_PROPOSED);
    }

    void OpportunityCoordinator::HandlePlayerLogin(Player* player)
    {
        if (!player || !player->GetSession())
            return;

        // If human player logins, refresh social cache
        if (!player->GetSession()->IsHeadless())
        {
            SocialProtection::Instance().RefreshCaches();
        }
#if defined(HAVE_TORTOISEBOTS)
        else if (BotEngineAdapter::IsRandomBot(player))
        {
            // Ensure newly logged-in bot receives the opportunities strategy
            PlayerbotAI* ai = BotEngineAdapter::GetBotAI(player);
            if (ai)
                EnsureBotStrategy(ai, ConfigManager::Instance().GetConfig().enabled);
        }
#endif
    }

    void OpportunityCoordinator::HandlePlayerLogout(Player* player)
    {
        if (!player)
            return;

        // Clean any active opportunity intent for logged out bot
        OpportunityLedger::Instance().CancelIntent(player->GetObjectGuid(), REASON_NONE, "PlayerLogout");
    }

    void OpportunityCoordinator::RunMaintenanceTick()
    {
        time_t now = time(nullptr);
        OpportunityLedger::Instance().ExpireStaleIntents(now);
        OpportunityLedger::Instance().CleanupTerminalIntents(now, 3600);

        auto activeIntents = OpportunityLedger::Instance().GetActiveIntentsSnapshot();
        for (const auto& intent : activeIntents)
        {
            Player* bot = sObjectAccessor.FindPlayer(intent.botGuid);
            if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            {
                OpportunityLedger::Instance().CancelIntent(intent.botGuid, REASON_DEAD, "BotOfflineOrDead");
                continue;
            }

            if (BotEngineAdapter::BotHasRealPlayerMaster(bot))
            {
                OpportunityLedger::Instance().CancelIntent(intent.botGuid, REASON_MASTER_CLAIMED, "HumanMasterClaimed");
                continue;
            }

            // Check arrival if bot is traveling
            if (intent.state == INTENT_STATE_TRAVELING)
            {
                uint32 mapId = bot->GetMapId();
                uint32 areaId = bot->GetAreaId();
                float x = bot->GetPositionX();
                float y = bot->GetPositionY();
                float z = bot->GetPositionZ();

                if (intent.domain == DOMAIN_CITY)
                {
                    const CityInfo* city = DestinationCatalog::Instance().GetCity(static_cast<CityId>(intent.destinationId));
                    if (city && DestinationCatalog::Instance().IsInCity(city->id, mapId, areaId, x, y, z))
                    {
                        OpportunityLedger::Instance().NotifyArrival(intent.botGuid);
                        BracketsDebug::Instance().LogEvent(CAT_CITY, DOMAIN_CITY, intent.intentId,
                                                           bot->GetObjectGuid().GetCounter(), city->name,
                                                           intent.activityName, "Arrived", "Success",
                                                           "Bot arrived at destination city");
                    }
                }
                else if (intent.domain == DOMAIN_WORLD)
                {
                    const OutdoorRegion* reg = DestinationCatalog::Instance().GetRegion(intent.destinationId);
                    if (reg && reg->mapId == mapId &&
                        (reg->zoneId == areaId || reg->areaIds.find(areaId) != reg->areaIds.end()))
                    {
                        OpportunityLedger::Instance().NotifyArrival(intent.botGuid);
                        BracketsDebug::Instance().LogEvent(CAT_WORLD, DOMAIN_WORLD, intent.intentId,
                                                           bot->GetObjectGuid().GetCounter(), reg->name,
                                                           intent.activityName, "Arrived", "Success",
                                                           "Bot arrived at destination outdoor region");
                    }
                }
            }
        }
    }

    Player* OpportunityCoordinator::FindBestCandidateBot(uint8 teamId, uint8 minLevel, uint8 maxLevel,
                                                         Domain domain, uint32 destinationId)
    {
        const auto& config = ConfigManager::Instance().GetConfig();
        time_t now = time(nullptr);

        HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
        for (const auto& pair : players)
        {
            Player* p = pair.second;
            if (!p || !p->IsInWorld() || !p->IsAlive() || p->IsInCombat())
                continue;

            if (p->GetTeamId() != teamId)
                continue;

            uint8 lvl = p->GetLevel();
            if (lvl < minLevel || lvl > maxLevel)
                continue;

            if (!BotEngineAdapter::IsRandomBot(p))
                continue;

            if (BotEngineAdapter::BotHasRealPlayerMaster(p))
                continue;

            if (SocialProtection::Instance().IsBotProtected(p))
                continue;

            if (p->IsTaxiFlying() || p->IsBeingTeleported())
                continue;

            // Check if bot already holds an active opportunity
            if (OpportunityLedger::Instance().HasActiveIntent(p->GetObjectGuid()))
                continue;

            // Check revisit cooldown
            time_t lastVisit = OpportunityLedger::Instance().GetLastVisitTime(p->GetObjectGuid(), domain, destinationId);
            if (lastVisit > 0 && (now - lastVisit) < static_cast<time_t>(config.stabilitySeconds))
                continue;

            // Check cross-domain cooldown
            time_t lastSwitch = OpportunityLedger::Instance().GetLastDomainSwitchTime(p->GetObjectGuid());
            if (lastSwitch > 0 && (now - lastSwitch) < static_cast<time_t>(config.crossDomainCooldownSeconds))
                continue;

            // Check failure backoff
            time_t lastFail = OpportunityLedger::Instance().GetLastFailureTime(p->GetObjectGuid());
            if (lastFail > 0 && (now - lastFail) < static_cast<time_t>(config.failureBackoffSeconds))
                continue;

            // Candidate found!
            return p;
        }

        return nullptr;
    }

    ActivityId OpportunityCoordinator::SelectWeightedActivity(Domain domain, uint32 /*destinationId*/, Player* /*bot*/)
    {
        auto& reg = ActivityRegistry::Instance();
        std::vector<std::pair<ActivityId, float>> candidates;
        float totalWeight = 0.0f;

        for (const auto& def : reg.GetAllDefinitions())
        {
            if (!reg.IsEnabledForDomain(def.id, domain))
                continue;

            if (def.support != SUPPORT_SCHEDULABLE)
                continue;

            float wt = reg.GetWeight(def.id);
            if (wt > 0.0f)
            {
                candidates.push_back({def.id, wt});
                totalWeight += wt;
            }
        }

        if (candidates.empty() || totalWeight <= 0.0f)
            return (domain == DOMAIN_CITY) ? ACTIVITY_GENERIC_RPG : ACTIVITY_GRINDING;

        float roll = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * totalWeight;
        float running = 0.0f;
        for (const auto& c : candidates)
        {
            running += c.second;
            if (roll <= running)
                return c.first;
        }

        return candidates.back().first;
    }

    void OpportunityCoordinator::RunDistributionCycle()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const auto& config = ConfigManager::Instance().GetConfig();
        if (!config.enabled)
            return;

        uint32 startTimeMs = WorldTimer::getMSTime();

        CensusSnapshot snap = PopulationCensus::Instance().TakeSnapshot();
        BracketsDebug::Instance().GetMetrics().lastCensusTimeMs = WorldTimer::getMSTime() - startTimeMs;

        uint32 admissionsThisCycle = 0;

        // Alternate factions for fairness
        m_factionCursor = 1 - m_factionCursor;

        for (uint8 f = 0; f < 2; ++f)
        {
            uint8 team = (m_factionCursor + f) % 2;

            uint32 outstandingCity = OpportunityLedger::Instance().GetActiveIntentsCount(DOMAIN_CITY, team);
            uint32 outstandingWorld = OpportunityLedger::Instance().GetActiveIntentsCount(DOMAIN_WORLD, team);
            uint32 totalOutstanding = outstandingCity + outstandingWorld;

            if (totalOutstanding >= config.maxOutstandingPerFaction)
                continue;

            // 1. Process City Opportunities
            if (config.cityEnabled)
            {
                // Find city with maximum actionable deficit for this faction
                uint32 maxDeficit = 0;
                CityId bestCityId = CITY_STORMWIND;
                bool cityFound = false;

                for (uint8 c = 0; c < CITY_COUNT; ++c)
                {
                    const auto& crec = snap.cities[c];
                    if (crec.teamId == team && crec.actionableDeficit > maxDeficit)
                    {
                        maxDeficit = crec.actionableDeficit;
                        bestCityId = static_cast<CityId>(c);
                        cityFound = true;
                    }
                }

                if (cityFound && admissionsThisCycle < config.maxAdmissionsPerCycle)
                {
                    const CityInfo* city = DestinationCatalog::Instance().GetCity(bestCityId);
                    if (city)
                    {
                        Player* candidate = FindBestCandidateBot(team, 1, 60, DOMAIN_CITY, bestCityId);
                        if (candidate)
                        {
                            ActivityId actId = SelectWeightedActivity(DOMAIN_CITY, bestCityId, candidate);

                            // Check restricted Option 3 fallback allowlist
                            std::string rejection;
                            if (config.fallbackEnabled &&
                                CityRelocationFallback::Instance().CanRelocate(candidate, bestCityId, REASON_ROUTE_UNSUPPORTED_BEFORE_START, &rejection))
                            {
                                CityRelocationFallback::Instance().ExecuteRelocation(candidate, bestCityId, REASON_ROUTE_UNSUPPORTED_BEFORE_START);
                                admissionsThisCycle++;
                            }
                            else
                            {
                                // Standard Option 2 registered opportunity
                                uint64 intentId = OpportunityLedger::Instance().CreateIntent(
                                    candidate->GetObjectGuid(), DOMAIN_CITY, bestCityId, city->name,
                                    actId, config.generation, candidate->GetLevel(), team, config.stabilitySeconds);

                                ActivityRegistry::Instance().IncrementIntended(actId);
                                admissionsThisCycle++;

                                BracketsDebug::Instance().LogEvent(
                                    CAT_CITY, DOMAIN_CITY, intentId, candidate->GetObjectGuid().GetCounter(),
                                    city->name, ActivityRegistry::Instance().GetActivity(actId)->name,
                                    "Proposed", "CityDeficit", "Proposed native city visit opportunity");
                            }
                        }
                    }
                }
            }

            // 2. Process Outdoor World Opportunities
            if (config.worldEnabled && admissionsThisCycle < config.maxAdmissionsPerCycle)
            {
                uint32 maxRegDeficit = 0;
                const OutdoorRegion* bestRegion = nullptr;

                for (const auto& rrec : snap.regions)
                {
                    if ((rrec.teamAccess == 2 || rrec.teamAccess == team) && rrec.actionableDeficit > maxRegDeficit)
                    {
                        maxRegDeficit = rrec.actionableDeficit;
                        bestRegion = DestinationCatalog::Instance().GetRegion(rrec.regionId);
                    }
                }

                if (bestRegion)
                {
                    Player* candidate = FindBestCandidateBot(team, bestRegion->minLevel, bestRegion->maxLevel,
                                                             DOMAIN_WORLD, bestRegion->regionId);
                    if (candidate)
                    {
                        ActivityId actId = SelectWeightedActivity(DOMAIN_WORLD, bestRegion->regionId, candidate);

                        uint64 intentId = OpportunityLedger::Instance().CreateIntent(
                            candidate->GetObjectGuid(), DOMAIN_WORLD, bestRegion->regionId, bestRegion->name,
                            actId, config.generation, candidate->GetLevel(), team, config.stabilitySeconds);

                        ActivityRegistry::Instance().IncrementIntended(actId);
                        admissionsThisCycle++;

                        BracketsDebug::Instance().LogEvent(
                            CAT_WORLD, DOMAIN_WORLD, intentId, candidate->GetObjectGuid().GetCounter(),
                            bestRegion->name, ActivityRegistry::Instance().GetActivity(actId)->name,
                            "Proposed", "WorldDeficit", "Proposed native outdoor world opportunity");
                    }
                }
            }
        }

        BracketsDebug::Instance().GetMetrics().admissionsThisCycle = admissionsThisCycle;
        BracketsDebug::Instance().GetMetrics().cumulativeAdmissions += admissionsThisCycle;
        BracketsDebug::Instance().GetMetrics().lastSchedulerTimeMs = WorldTimer::getMSTime() - startTimeMs;
    }
}
