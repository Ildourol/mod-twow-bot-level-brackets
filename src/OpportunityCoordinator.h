#ifndef BRACKETS_OPPORTUNITY_COORDINATOR_H
#define BRACKETS_OPPORTUNITY_COORDINATOR_H

#include "BracketsCommon.h"
#include "PopulationCensus.h"
#include "OpportunityLedger.h"
#include <mutex>
#include <cstdint>

namespace Brackets
{
    class OpportunityCoordinator
    {
    public:
        static OpportunityCoordinator& Instance()
        {
            static OpportunityCoordinator instance;
            return instance;
        }

        void Initialize();
        void Update(uint32 diff);
        void ForceCycle();

        bool HasProposedOpportunity(ObjectGuid botGuid, Domain domain) const;

        void HandlePlayerLogin(Player* player);
        void HandlePlayerLogout(Player* player);

        time_t GetNextCycleTime() const { return m_nextCycleTime; }

    private:
        OpportunityCoordinator() = default;
        ~OpportunityCoordinator() = default;

        void RunDistributionCycle();
        void RunMaintenanceTick();

        Player* FindBestCandidateBot(uint8 teamId, uint8 minLevel, uint8 maxLevel,
                                     Domain domain, uint32 destinationId);

        ActivityId SelectWeightedActivity(Domain domain, uint32 destinationId, Player* bot);

        uint32 m_cycleTimer{0};
        uint32 m_maintenanceTimer{0};
        time_t m_nextCycleTime{0};

        uint8 m_factionCursor{0};
        uint8 m_cityCursor[2]{0, 0};
        uint32 m_regionCursor[2]{0, 0};

        mutable std::mutex m_mutex;
    };
}

#endif // BRACKETS_OPPORTUNITY_COORDINATOR_H
