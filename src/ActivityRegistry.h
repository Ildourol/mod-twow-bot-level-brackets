#ifndef BRACKETS_ACTIVITY_REGISTRY_H
#define BRACKETS_ACTIVITY_REGISTRY_H

#include "BracketsCommon.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Brackets
{
    enum ActivityId : uint8
    {
        ACTIVITY_QUEST_ACCEPT = 0,
        ACTIVITY_QUEST_OBJECTIVE,
        ACTIVITY_QUEST_TURN_IN,
        ACTIVITY_QUEST_REPEAT,
        ACTIVITY_GRINDING,
        ACTIVITY_EXPLORATION,
        ACTIVITY_GENERIC_RPG,
        ACTIVITY_VENDOR_SELL,
        ACTIVITY_VENDOR_BUY,
        ACTIVITY_REPAIR,
        ACTIVITY_CLASS_TRAINING,
        ACTIVITY_PROFESSION_TRAINING,
        ACTIVITY_MOUNT_TRAINING,
        ACTIVITY_MOUNT_PURCHASE,
        ACTIVITY_REAGENT_PURCHASE,
        ACTIVITY_AUCTION_SELL,
        ACTIVITY_AUCTION_BUY,
        ACTIVITY_MAILBOX,
        ACTIVITY_GATHER_HERBALISM,
        ACTIVITY_GATHER_MINING,
        ACTIVITY_SKINNING,
        ACTIVITY_FISHING,
        ACTIVITY_CRAFTING,
        ACTIVITY_GOSSIP,
        ACTIVITY_EMOTE,
        ACTIVITY_REST_SIT,
        ACTIVITY_SOCIAL_HEAL,
        ACTIVITY_NPC_USE,
        ACTIVITY_FLIGHT_DISCOVERY,
        ACTIVITY_GUILD_MEETING,
        ACTIVITY_GUILD_ORDER,
        ACTIVITY_GUILD_PETITION,
        ACTIVITY_PLAYER_TRADE,
        ACTIVITY_ENCHANT_SERVICE,
        ACTIVITY_DUEL,
        ACTIVITY_WORLD_PVP,
        ACTIVITY_BOSS_HUNT,
        ACTIVITY_BATTLEGROUND_QUEUE,
        ACTIVITY_DUNGEON_QUEUE,
        ACTIVITY_HOME_BIND,
        ACTIVITY_AMBIENT_CHAT,
        ACTIVITY_RANDOM_JUMP,
        ACTIVITY_COUNT
    };

    enum SupportLevel : uint8
    {
        SUPPORT_SCHEDULABLE = 0,
        SUPPORT_LOCAL_ONLY  = 1,
        SUPPORT_OBSERVE_ONLY = 2,
        SUPPORT_UNAVAILABLE = 3
    };

    inline const char* SupportLevelToString(SupportLevel l)
    {
        switch (l)
        {
            case SUPPORT_SCHEDULABLE:  return "Schedulable";
            case SUPPORT_LOCAL_ONLY:   return "LocalOnly";
            case SUPPORT_OBSERVE_ONLY: return "ObserveOnly";
            case SUPPORT_UNAVAILABLE:  return "Unavailable";
            default:                   return "Unknown";
        }
    }

    struct ActivityConfig
    {
        bool   enabled{true};
        bool   cityEnabled{true};
        bool   worldEnabled{true};
        float  weight{1.0f};
        uint32 cooldownSeconds{300};
        uint32 maxOutstandingPerFaction{2};
    };

    struct ActivityDefinition
    {
        ActivityId   id;
        const char*  name;
        SupportLevel support;
        uint8        domainMask; // Bitmask: DOMAIN_CITY (1), DOMAIN_WORLD (2)
        uint32       nativePurposeFlag; // TravelDestinationPurpose bitmask
        const char*  description;
        ActivityConfig defaultConfig;
    };

    class ActivityRegistry
    {
    public:
        static ActivityRegistry& Instance()
        {
            static ActivityRegistry instance;
            return instance;
        }

        void Initialize();

        const ActivityDefinition* GetActivity(ActivityId id) const;
        const ActivityDefinition* FindByName(const std::string& name) const;

        bool IsEnabledForDomain(ActivityId id, Domain domain) const;
        float GetWeight(ActivityId id) const;
        uint32 GetCooldownSeconds(ActivityId id) const;
        uint32 GetMaxOutstanding(ActivityId id) const;

        void SetConfig(ActivityId id, const ActivityConfig& cfg);
        const ActivityConfig& GetConfig(ActivityId id) const;

        const std::vector<ActivityDefinition>& GetAllDefinitions() const { return m_definitions; }

        uint32 GetIntendedCount(ActivityId id) const { return m_intendedCounts[id]; }
        uint32 GetObservedCount(ActivityId id) const { return m_observedCounts[id]; }
        void IncrementIntended(ActivityId id) { m_intendedCounts[id]++; }
        void IncrementObserved(ActivityId id) { m_observedCounts[id]++; }
        void ResetCounters();

    private:
        ActivityRegistry();
        ~ActivityRegistry() = default;

        std::vector<ActivityDefinition> m_definitions;
        std::unordered_map<std::string, ActivityId> m_nameToId;
        std::vector<ActivityConfig> m_runtimeConfigs;
        std::vector<uint32> m_intendedCounts;
        std::vector<uint32> m_observedCounts;
    };
}

#endif // BRACKETS_ACTIVITY_REGISTRY_H
