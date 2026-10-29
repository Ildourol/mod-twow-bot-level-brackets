#include "ActivityRegistry.h"
#include <algorithm>

namespace BotLevelBrackets
{

const char* GetSupportClassName(SupportClass sc)
{
    switch (sc)
    {
        case SupportClass::RequestableExact:        return "RequestableExact";
        case SupportClass::RequestableNativeChoice: return "RequestableNativeChoice";
        case SupportClass::LocalOnly:               return "LocalOnly";
        case SupportClass::ObserveOnly:             return "ObserveOnly";
        case SupportClass::Unavailable:             return "Unavailable";
        default:                                    return "Unknown";
    }
}

ActivityRegistry& ActivityRegistry::Instance()
{
    static ActivityRegistry instance;
    return instance;
}

ActivityRegistry::ActivityRegistry()
{
    LoadDefaults();
}

void ActivityRegistry::LoadDefaults()
{
    m_activitiesMasterEnabled = true;
    m_requireSupported = false;
    m_activities.clear();
    m_indexMap.clear();

    auto add = [this](std::string const& id, SupportClass sc, bool en, bool cityEn, bool worldEn,
                      float wt, uint32_t cd, uint32_t maxOut, std::string const& action, std::string const& desc)
    {
        m_indexMap[id] = m_activities.size();
        m_activities.push_back({ id, sc, en, cityEn, worldEn, wt, cd, maxOut, action, desc });
    };

    // The 42 audited native activities (from Section 26)
    add("QuestAccept",        SupportClass::RequestableNativeChoice, true,  true,  true,  1.0f, 300,  3, "accept quest",              "Accept nearby available quest");
    add("QuestObjective",     SupportClass::RequestableNativeChoice, true,  false, true,  1.2f, 180,  5, "choose travel target",      "Work on quest objectives");
    add("QuestTurnIn",        SupportClass::RequestableNativeChoice, true,  true,  true,  1.5f, 300,  3, "talk to quest giver",       "Turn in completed quest");
    add("QuestRepeat",        SupportClass::RequestableNativeChoice, false, false, false, 0.5f, 600,  1, "talk to quest giver",       "Repeatable quest interaction");
    add("Grinding",           SupportClass::RequestableNativeChoice, true,  false, true,  1.0f, 600,  5, "choose travel target",      "Grind mobs at appropriate level");
    add("Exploration",        SupportClass::RequestableNativeChoice, true,  true,  true,  0.8f, 900,  2, "choose travel target",      "Explore uncharted map areas");
    add("GenericRpg",         SupportClass::RequestableNativeChoice, true,  true,  true,  0.9f, 300,  2, "rpg",                       "General RPG world interaction");
    add("VendorSell",         SupportClass::RequestableNativeChoice, true,  true,  true,  1.0f, 600,  2, "sell",                      "Sell vendor junk and grey items");
    add("VendorBuy",          SupportClass::RequestableNativeChoice, true,  true,  true,  0.8f, 600,  2, "buy",                       "Purchase consumable food and drinks");
    add("Repair",             SupportClass::RequestableNativeChoice, true,  true,  true,  1.2f, 600,  2, "repair",                    "Repair damaged durability");
    add("ClassTraining",      SupportClass::RequestableNativeChoice, true,  true,  false, 1.0f, 1800, 2, "trainer",                   "Visit class trainer for new ranks");
    add("ProfessionTraining", SupportClass::RequestableNativeChoice, true,  true,  false, 0.7f, 1800, 2, "trainer",                   "Train primary/secondary professions");
    add("MountTraining",      SupportClass::RequestableNativeChoice, false, true,  false, 0.5f, 3600, 1, "trainer",                   "Riding skill training");
    add("MountPurchase",      SupportClass::RequestableNativeChoice, false, true,  false, 0.5f, 3600, 1, "buy",                       "Purchase racial mount");
    add("ReagentPurchase",    SupportClass::RequestableNativeChoice, true,  true,  true,  0.8f, 900,  2, "buy",                       "Purchase class reagents and ammo");
    add("AuctionSell",        SupportClass::RequestableExact,        true,  true,  false, 1.0f, 1200, 2, "ah",                        "Post valuable items to AH");
    add("AuctionBuy",         SupportClass::RequestableExact,        false, true,  false, 0.5f, 1800, 1, "ah",                        "Browse/buy upgrades from AH");
    add("Mailbox",            SupportClass::RequestableNativeChoice, true,  true,  true,  0.8f, 900,  2, "mail",                      "Retrieve gold/items from mailbox");
    add("GatherHerbalism",    SupportClass::RequestableNativeChoice, true,  false, true,  0.8f, 600,  2, "reveal gathering item",     "Herbalism node collection");
    add("GatherMining",       SupportClass::RequestableNativeChoice, true,  false, true,  0.8f, 600,  2, "reveal gathering item",     "Mining vein harvesting");
    add("Skinning",           SupportClass::LocalOnly,               true,  false, true,  0.8f, 300,  2, "skin",                      "Skin slain beasts");
    add("Fishing",            SupportClass::LocalOnly,               false, false, false, 0.5f, 600,  1, "fish",                      "Fish in calm open waters");
    add("Crafting",           SupportClass::LocalOnly,               false, true,  false, 0.5f, 1200, 1, "craft",                     "Craft trade skill recipes");
    add("Gossip",             SupportClass::LocalOnly,               false, true,  true,  0.3f, 300,  1, "gossip hello",              "Chat with ambient NPCs");
    add("Emote",              SupportClass::LocalOnly,               false, true,  true,  0.2f, 180,  1, "emote",                     "Expressive social emotes");
    add("RestSit",            SupportClass::LocalOnly,               false, true,  true,  0.2f, 300,  1, "sit",                       "Sit down to rest in inn/town");
    add("SocialHeal",         SupportClass::LocalOnly,               false, true,  true,  0.5f, 300,  1, "heal",                      "Provide altruistic out-of-combat heals");
    add("NpcUse",             SupportClass::LocalOnly,               false, true,  true,  0.4f, 300,  1, "talk to quest giver",       "Interact with world game objects");
    add("FlightDiscovery",    SupportClass::RequestableNativeChoice, true,  true,  true,  0.9f, 1800, 2, "remember taxi",             "Discover new taxi flight paths");
    add("GuildMeeting",       SupportClass::LocalOnly,               false, true,  false, 0.2f, 3600, 1, "guild accept",              "Socialize at guild gathering spot");
    add("GuildOrder",         SupportClass::LocalOnly,               false, true,  false, 0.2f, 3600, 1, "guild accept quest order",  "Fulfill guild crafting orders");
    add("GuildPetition",      SupportClass::LocalOnly,               false, true,  false, 0.1f, 3600, 1, "guild create",              "Sign prospective guild charter");
    add("PlayerTrade",        SupportClass::LocalOnly,               false, true,  true,  0.2f, 3600, 1, "trade",                     "Friendly trade bartering");
    add("EnchantService",     SupportClass::LocalOnly,               false, true,  false, 0.2f, 3600, 1, "craft",                     "Perform enchanting on gear");
    add("Duel",               SupportClass::LocalOnly,               false, false, false, 0.1f, 3600, 1, "accept duel",               "Casual outside-gate duel");
    add("WorldPvp",           SupportClass::ObserveOnly,             false, false, false, 0.5f, 1800, 1, "",                          "Contested zone territorial defense");
    add("BossHunt",           SupportClass::RequestableNativeChoice, false, false, true,  0.5f, 3600, 1, "choose travel target",      "Elite outdoor boss skirmish");
    add("BattlegroundQueue",  SupportClass::ObserveOnly,             false, false, false, 0.5f, 1800, 1, "",                          "Queue for Warsong or Arathi");
    add("DungeonQueue",       SupportClass::ObserveOnly,             false, false, false, 0.5f, 1800, 1, "",                          "Looking For Turtles dungeon queue");
    add("HomeBind",           SupportClass::Unavailable,             false, false, false, 0.0f, 86400,0, "",                          "Hearthstone location binding");
    add("AmbientChat",        SupportClass::LocalOnly,               false, true,  true,  0.1f, 300,  1, "say",                       "Occasional ambient chatter");
    add("RandomJump",         SupportClass::LocalOnly,               false, true,  true,  0.1f, 120,  1, "",                          "Playful random jump while running");
}

ActivityDefinition* ActivityRegistry::GetActivity(const std::string& id)
{
    auto it = m_indexMap.find(id);
    if (it != m_indexMap.end())
        return &m_activities[it->second];
    return nullptr;
}

const ActivityDefinition* ActivityRegistry::GetActivity(const std::string& id) const
{
    auto it = m_indexMap.find(id);
    if (it != m_indexMap.end())
        return &m_activities[it->second];
    return nullptr;
}

bool ActivityRegistry::IsRequestable(const std::string& id, bool isCity) const
{
    if (!m_activitiesMasterEnabled)
        return false;

    const ActivityDefinition* def = GetActivity(id);
    if (!def || !def->enabled)
        return false;

    if (isCity && !def->cityEnabled)
        return false;

    if (!isCity && !def->worldEnabled)
        return false;

    if (def->supportClass == SupportClass::Unavailable || def->supportClass == SupportClass::ObserveOnly)
        return false;

    return true;
}

} // namespace BotLevelBrackets
