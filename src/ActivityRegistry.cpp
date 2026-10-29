#include "ActivityRegistry.h"
#include <algorithm>

namespace Brackets
{
    // Native purpose flags bitmask mirror from TravelValues.h
    static constexpr uint32 PURPOSE_NONE              = 0;
    static constexpr uint32 PURPOSE_QUEST_GIVER       = 1 << 0;
    static constexpr uint32 PURPOSE_QUEST_OBJECTIVES  = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
    static constexpr uint32 PURPOSE_QUEST_TAKER       = 1 << 5;
    static constexpr uint32 PURPOSE_GENERIC_RPG       = 1 << 6;
    static constexpr uint32 PURPOSE_TRAINER           = 1 << 7;
    static constexpr uint32 PURPOSE_REPAIR            = 1 << 8;
    static constexpr uint32 PURPOSE_VENDOR            = 1 << 9;
    static constexpr uint32 PURPOSE_AH                = 1 << 10;
    static constexpr uint32 PURPOSE_MAIL              = 1 << 11;
    static constexpr uint32 PURPOSE_GRIND             = 1 << 12;
    static constexpr uint32 PURPOSE_BOSS              = 1 << 13;
    static constexpr uint32 PURPOSE_GATHER_SKINNING   = 1 << 14;
    static constexpr uint32 PURPOSE_GATHER_MINING     = 1 << 15;
    static constexpr uint32 PURPOSE_GATHER_HERBALISM  = 1 << 16;
    static constexpr uint32 PURPOSE_GATHER_FISHING    = 1 << 17;
    static constexpr uint32 PURPOSE_EXPLORE           = 1 << 18;

    ActivityRegistry::ActivityRegistry()
    {
        Initialize();
    }

    void ActivityRegistry::Initialize()
    {
        m_definitions.clear();
        m_definitions.reserve(ACTIVITY_COUNT);

        auto add = [this](ActivityId id, const char* name, SupportLevel sup, uint8 dMask,
                          uint32 purpose, const char* desc, bool en, bool cEn, bool wEn,
                          float wt, uint32 cd, uint32 maxOut)
        {
            ActivityConfig cfg;
            cfg.enabled = en;
            cfg.cityEnabled = cEn;
            cfg.worldEnabled = wEn;
            cfg.weight = wt;
            cfg.cooldownSeconds = cd;
            cfg.maxOutstandingPerFaction = maxOut;

            ActivityDefinition def;
            def.id = id;
            def.name = name;
            def.support = sup;
            def.domainMask = dMask;
            def.nativePurposeFlag = purpose;
            def.description = desc;
            def.defaultConfig = cfg;

            m_definitions.push_back(def);
            m_nameToId[name] = id;
        };

        // 1. QuestAccept
        add(ACTIVITY_QUEST_ACCEPT, "QuestAccept", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_QUEST_GIVER, "Accept available quests matching level and race/class",
            true, true, true, 1.0f, 300, 2);

        // 2. QuestObjective
        add(ACTIVITY_QUEST_OBJECTIVE, "QuestObjective", SUPPORT_SCHEDULABLE, DOMAIN_WORLD,
            PURPOSE_QUEST_OBJECTIVES, "Travel to outdoor quest objectives for active quests",
            true, false, true, 1.0f, 300, 2);

        // 3. QuestTurnIn
        add(ACTIVITY_QUEST_TURN_IN, "QuestTurnIn", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_QUEST_TAKER, "Turn in completed quests to quest takers",
            true, true, true, 1.0f, 300, 2);

        // 4. QuestRepeat
        add(ACTIVITY_QUEST_REPEAT, "QuestRepeat", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_QUEST_TAKER, "Repeatable quest turn-ins with anti-loop checks",
            false, false, false, 0.5f, 600, 1);

        // 5. Grinding
        add(ACTIVITY_GRINDING, "Grinding", SUPPORT_SCHEDULABLE, DOMAIN_WORLD,
            PURPOSE_GRIND, "Level-appropriate mob grinding in underpopulated outdoor spots",
            true, false, true, 1.0f, 300, 3);

        // 6. Exploration
        add(ACTIVITY_EXPLORATION, "Exploration", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_EXPLORE, "Uncharted area and capital city exploration",
            true, true, true, 0.8f, 600, 2);

        // 7. GenericRpg
        add(ACTIVITY_GENERIC_RPG, "GenericRpg", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_GENERIC_RPG, "Civic interactions with townspeople and landmark NPCs",
            true, true, true, 1.0f, 300, 2);

        // 8. VendorSell
        add(ACTIVITY_VENDOR_SELL, "VendorSell", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_VENDOR, "Sell junk and unwanted items to merchants",
            true, true, true, 1.0f, 300, 2);

        // 9. VendorBuy
        add(ACTIVITY_VENDOR_BUY, "VendorBuy", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_VENDOR, "Purchase supplies, drinks, and food from merchants",
            true, true, true, 1.0f, 300, 2);

        // 10. Repair
        add(ACTIVITY_REPAIR, "Repair", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_REPAIR, "Visit blacksmiths and armorers for genuine equipment repair",
            true, true, true, 1.0f, 300, 2);

        // 11. ClassTraining
        add(ACTIVITY_CLASS_TRAINING, "ClassTraining", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_TRAINER, "Visit class trainers to learn new spells and abilities",
            true, true, true, 1.0f, 600, 2);

        // 12. ProfessionTraining
        add(ACTIVITY_PROFESSION_TRAINING, "ProfessionTraining", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_TRAINER, "Visit profession trainers to learn recipe tiers and apprentice skills",
            true, true, true, 0.8f, 600, 2);

        // 13. MountTraining
        add(ACTIVITY_MOUNT_TRAINING, "MountTraining", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_TRAINER, "Visit riding trainers when eligible for mount skill",
            false, false, false, 0.5f, 1800, 1);

        // 14. MountPurchase
        add(ACTIVITY_MOUNT_PURCHASE, "MountPurchase", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_VENDOR, "Visit mount vendors when funds and riding skill permit",
            false, false, false, 0.5f, 1800, 1);

        // 15. ReagentPurchase
        add(ACTIVITY_REAGENT_PURCHASE, "ReagentPurchase", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_VENDOR, "Stock up on class reagents (candles, feathers, ankhs, symbols)",
            true, true, true, 1.0f, 600, 2);

        // 16. AuctionSell
        add(ACTIVITY_AUCTION_SELL, "AuctionSell", SUPPORT_SCHEDULABLE, DOMAIN_CITY,
            PURPOSE_AH, "Deposit trade items and crafted goods on capital auction house",
            true, true, false, 1.0f, 600, 2);

        // 17. AuctionBuy
        add(ACTIVITY_AUCTION_BUY, "AuctionBuy", SUPPORT_SCHEDULABLE, DOMAIN_CITY,
            PURPOSE_AH, "Browse and purchase upgrades on capital auction house",
            false, false, false, 0.5f, 1200, 1);

        // 18. Mailbox
        add(ACTIVITY_MAILBOX, "Mailbox", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_MAIL, "Check mailbox for auction proceeds, letters, and items",
            true, true, true, 1.0f, 300, 2);

        // 19. GatherHerbalism
        add(ACTIVITY_GATHER_HERBALISM, "GatherHerbalism", SUPPORT_SCHEDULABLE, DOMAIN_WORLD,
            PURPOSE_GATHER_HERBALISM, "Harvest herbs in native level-appropriate zones",
            true, false, true, 1.0f, 300, 2);

        // 20. GatherMining
        add(ACTIVITY_GATHER_MINING, "GatherMining", SUPPORT_SCHEDULABLE, DOMAIN_WORLD,
            PURPOSE_GATHER_MINING, "Mine mineral veins and ore deposits in outdoor zones",
            true, false, true, 1.0f, 300, 2);

        // 21. Skinning
        add(ACTIVITY_SKINNING, "Skinning", SUPPORT_LOCAL_ONLY, DOMAIN_WORLD,
            PURPOSE_GATHER_SKINNING, "Skin defeated beasts in immediate combat vicinity",
            true, false, true, 1.0f, 60, 2);

        // 22. Fishing
        add(ACTIVITY_FISHING, "Fishing", SUPPORT_LOCAL_ONLY, DOMAIN_WORLD,
            PURPOSE_GATHER_FISHING, "Fish at safe nearby water bodies",
            false, false, false, 0.5f, 600, 1);

        // 23. Crafting
        add(ACTIVITY_CRAFTING, "Crafting", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Craft learned items at anvils or fires when materials exist",
            false, false, false, 0.5f, 600, 1);

        // 24. Gossip
        add(ACTIVITY_GOSSIP, "Gossip", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_GENERIC_RPG, "Converse with innkeepers and civic citizens",
            true, true, true, 1.0f, 120, 2);

        // 25. Emote
        add(ACTIVITY_EMOTE, "Emote", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Trigger expressive emotes at nearby friendly NPCs or bots",
            false, false, false, 0.3f, 300, 1);

        // 26. RestSit
        add(ACTIVITY_REST_SIT, "RestSit", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Rest at taverns, benches, and campfires",
            false, false, false, 0.5f, 600, 1);

        // 27. SocialHeal
        add(ACTIVITY_SOCIAL_HEAL, "SocialHeal", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Assist friendly bots or NPCs with beneficial buffs and heals",
            false, false, false, 0.5f, 180, 1);

        // 28. NpcUse
        add(ACTIVITY_NPC_USE, "NpcUse", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Interact with specialized world game objects and quest devices",
            false, false, false, 0.5f, 300, 1);

        // 29. FlightDiscovery
        add(ACTIVITY_FLIGHT_DISCOVERY, "FlightDiscovery", SUPPORT_SCHEDULABLE, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_GENERIC_RPG, "Discover flight masters and unlock regional flight paths",
            true, true, true, 1.0f, 600, 2);

        // 30. GuildMeeting
        add(ACTIVITY_GUILD_MEETING, "GuildMeeting", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Gather at designated guild meeting points",
            false, false, false, 0.3f, 1800, 1);

        // 31. GuildOrder
        add(ACTIVITY_GUILD_ORDER, "GuildOrder", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Fulfill requested guild bank or officer assignments",
            false, false, false, 0.3f, 1800, 1);

        // 32. GuildPetition
        add(ACTIVITY_GUILD_PETITION, "GuildPetition", SUPPORT_LOCAL_ONLY, DOMAIN_CITY,
            PURPOSE_NONE, "Seek signatures for chartered guild petitions",
            false, false, false, 0.2f, 3600, 1);

        // 33. PlayerTrade
        add(ACTIVITY_PLAYER_TRADE, "PlayerTrade", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Participate in consented trading interactions",
            false, false, false, 0.2f, 1800, 1);

        // 34. EnchantService
        add(ACTIVITY_ENCHANT_SERVICE, "EnchantService", SUPPORT_LOCAL_ONLY, DOMAIN_CITY,
            PURPOSE_NONE, "Provide or request enchantment enhancements",
            false, false, false, 0.2f, 1800, 1);

        // 35. Duel
        add(ACTIVITY_DUEL, "Duel", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Engage in friendly dueling challenges outside capital gates",
            false, false, false, 0.2f, 1800, 1);

        // 36. WorldPvp
        add(ACTIVITY_WORLD_PVP, "WorldPvp", SUPPORT_LOCAL_ONLY, DOMAIN_WORLD,
            PURPOSE_NONE, "Contested zone outdoor skirmishes and tower defense",
            false, false, false, 0.4f, 1200, 1);

        // 37. BossHunt
        add(ACTIVITY_BOSS_HUNT, "BossHunt", SUPPORT_SCHEDULABLE, DOMAIN_WORLD,
            PURPOSE_BOSS, "Seek outdoor rare spawns and world bosses with group",
            false, false, false, 0.3f, 3600, 1);

        // 38. BattlegroundQueue
        add(ACTIVITY_BATTLEGROUND_QUEUE, "BattlegroundQueue", SUPPORT_OBSERVE_ONLY, DOMAIN_NONE,
            PURPOSE_NONE, "Observed battleground matchmaking queue state",
            false, false, false, 0.0f, 0, 0);

        // 39. DungeonQueue
        add(ACTIVITY_DUNGEON_QUEUE, "DungeonQueue", SUPPORT_OBSERVE_ONLY, DOMAIN_NONE,
            PURPOSE_NONE, "Observed Looking For Turtles (LFT) dungeon queue state",
            false, false, false, 0.0f, 0, 0);

        // 40. HomeBind
        add(ACTIVITY_HOME_BIND, "HomeBind", SUPPORT_UNAVAILABLE, DOMAIN_NONE,
            PURPOSE_NONE, "Hearthstone binding (managed natively by AI)",
            false, false, false, 0.0f, 0, 0);

        // 41. AmbientChat
        add(ACTIVITY_AMBIENT_CHAT, "AmbientChat", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Periodic native ambient chat utterances",
            false, false, false, 0.2f, 600, 1);

        // 42. RandomJump
        add(ACTIVITY_RANDOM_JUMP, "RandomJump", SUPPORT_LOCAL_ONLY, DOMAIN_CITY | DOMAIN_WORLD,
            PURPOSE_NONE, "Playful jumping during movement",
            false, false, false, 0.2f, 300, 1);

        m_runtimeConfigs.resize(ACTIVITY_COUNT);
        m_intendedCounts.assign(ACTIVITY_COUNT, 0);
        m_observedCounts.assign(ACTIVITY_COUNT, 0);

        for (size_t i = 0; i < m_definitions.size(); ++i)
        {
            m_runtimeConfigs[i] = m_definitions[i].defaultConfig;
        }
    }

    const ActivityDefinition* ActivityRegistry::GetActivity(ActivityId id) const
    {
        if (id < m_definitions.size())
            return &m_definitions[id];
        return nullptr;
    }

    const ActivityDefinition* ActivityRegistry::FindByName(const std::string& name) const
    {
        auto it = m_nameToId.find(name);
        if (it != m_nameToId.end())
            return &m_definitions[it->second];
        return nullptr;
    }

    bool ActivityRegistry::IsEnabledForDomain(ActivityId id, Domain domain) const
    {
        if (id >= m_runtimeConfigs.size())
            return false;

        const auto& cfg = m_runtimeConfigs[id];
        if (!cfg.enabled)
            return false;

        const auto& def = m_definitions[id];
        if (domain == DOMAIN_CITY)
            return cfg.cityEnabled && (def.domainMask & DOMAIN_CITY);
        if (domain == DOMAIN_WORLD)
            return cfg.worldEnabled && (def.domainMask & DOMAIN_WORLD);

        return false;
    }

    float ActivityRegistry::GetWeight(ActivityId id) const
    {
        if (id < m_runtimeConfigs.size())
            return m_runtimeConfigs[id].weight;
        return 0.0f;
    }

    uint32 ActivityRegistry::GetCooldownSeconds(ActivityId id) const
    {
        if (id < m_runtimeConfigs.size())
            return m_runtimeConfigs[id].cooldownSeconds;
        return 300;
    }

    uint32 ActivityRegistry::GetMaxOutstanding(ActivityId id) const
    {
        if (id < m_runtimeConfigs.size())
            return m_runtimeConfigs[id].maxOutstandingPerFaction;
        return 2;
    }

    void ActivityRegistry::SetConfig(ActivityId id, const ActivityConfig& cfg)
    {
        if (id < m_runtimeConfigs.size())
            m_runtimeConfigs[id] = cfg;
    }

    const ActivityConfig& ActivityRegistry::GetConfig(ActivityId id) const
    {
        return m_runtimeConfigs[id];
    }

    void ActivityRegistry::ResetCounters()
    {
        std::fill(m_intendedCounts.begin(), m_intendedCounts.end(), 0);
        std::fill(m_observedCounts.begin(), m_observedCounts.end(), 0);
    }
}
