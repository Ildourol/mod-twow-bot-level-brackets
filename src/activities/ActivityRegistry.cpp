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

// Total Native TortoiseBots Activities: 427
    add("QuestAccept", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "accept quest", "Accept nearby available quest");
    add("QuestObjective", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "choose travel target", "Work on quest objectives");
    add("QuestTurnIn", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "talk to quest giver", "Turn in completed quest");
    add("QuestRepeat", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "talk to quest giver", "Repeatable quest interaction");
    add("Grinding", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "choose travel target", "Grind mobs at appropriate level");
    add("Exploration", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "choose travel target", "Explore uncharted map areas");
    add("GenericRpg", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "rpg", "General RPG world interaction");
    add("VendorSell", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "sell", "Sell vendor junk and grey items");
    add("VendorBuy", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "buy", "Purchase consumable food and drinks");
    add("Repair", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "repair", "Repair damaged durability");
    add("ClassTraining", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "trainer", "Visit class trainer for new ranks");
    add("ProfessionTraining", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "trainer", "Train primary/secondary professions");
    add("MountTraining", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "trainer", "Riding skill training");
    add("MountPurchase", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "buy", "Purchase racial mount");
    add("ReagentPurchase", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "buy", "Purchase class reagents and ammo");
    add("AuctionSell", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "ah", "Post valuable items to AH");
    add("AuctionBuy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "ah", "Browse/buy upgrades from AH");
    add("Mailbox", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "mail", "Retrieve gold/items from mailbox");
    add("GatherHerbalism", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "reveal gathering item", "Herbalism node collection");
    add("GatherMining", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "reveal gathering item", "Mining vein harvesting");
    add("Skinning", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "skin", "Skin slain beasts");
    add("Fishing", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fish", "Fish in calm open waters");
    add("Crafting", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "craft", "Craft trade skill recipes");
    add("Gossip", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "gossip hello", "Chat with ambient NPCs");
    add("Emote", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "emote", "Expressive social emotes");
    add("RestSit", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "sit", "Sit down to rest in inn/town");
    add("SocialHeal", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "heal", "Provide altruistic out-of-combat heals");
    add("NpcUse", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "talk to quest giver", "Interact with world game objects");
    add("FlightDiscovery", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "remember taxi", "Discover new taxi flight paths");
    add("GuildMeeting", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guild accept", "Socialize at guild gathering spot");
    add("GuildOrder", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guild accept quest order", "Fulfill guild crafting orders");
    add("GuildPetition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guild create", "Sign prospective guild charter");
    add("PlayerTrade", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "trade", "Friendly trade bartering");
    add("EnchantService", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "craft", "Perform enchanting on gear");
    add("Duel", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "accept duel", "Casual outside-gate duel");
    add("WorldPvp", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "", "Contested zone territorial defense");
    add("BossHunt", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "choose travel target", "Elite outdoor boss skirmish");
    add("BattlegroundQueue", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "", "Queue for Warsong or Arathi");
    add("DungeonQueue", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "", "Looking For Turtles dungeon queue");
    add("HomeBind", SupportClass::Unavailable, true, true, true, 1.0f, 300, 5, "", "Hearthstone location binding");
    add("AmbientChat", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "say", "Occasional ambient chatter");
    add("RandomJump", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "", "Playful random jump while running");
    add("FankrissSwap", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fankriss swap", "Native TortoiseBots fankriss swap action");
    add("FankrissTankEngage", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fankriss tank engage", "Native TortoiseBots fankriss tank engage action");
    add("FankrissSpawnEngage", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fankriss spawn engage", "Native TortoiseBots fankriss spawn engage action");
    add("FankrissSpawnPickup", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fankriss spawn pickup", "Native TortoiseBots fankriss spawn pickup action");
    add("FankrissRearStack", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fankriss rear stack", "Native TortoiseBots fankriss rear stack action");
    add("FankrissControlSpawn", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fankriss control spawn", "Native TortoiseBots fankriss control spawn action");
    add("FankrissFocusSpawn", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fankriss focus spawn", "Native TortoiseBots fankriss focus spawn action");
    add("PatchwerkMeleeRetreat", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "patchwerk melee retreat", "Native TortoiseBots patchwerk melee retreat action");
    add("MarkRti", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "mark rti", "Native TortoiseBots mark rti action");
    add("SetReturnPosition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set return position", "Native TortoiseBots set return position action");
    add("Rpg", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg", "Native TortoiseBots rpg action");
    add("Crpg", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "crpg", "Native TortoiseBots crpg action");
    add("ChooseRpgTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "choose rpg target", "Native TortoiseBots choose rpg target action");
    add("MoveToRpgTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "move to rpg target", "Native TortoiseBots move to rpg target action");
    add("ServiceNearbyNpc", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "service nearby npc", "Native TortoiseBots service nearby npc action");
    add("Travel", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "travel", "Native TortoiseBots travel action");
    add("ChooseTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "choose travel target", "Native TortoiseBots choose travel target action");
    add("ChooseGroupTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "choose group travel target", "Native TortoiseBots choose group travel target action");
    add("RefreshTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "refresh travel target", "Native TortoiseBots refresh travel target action");
    add("RequestTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "request travel target", "Native TortoiseBots request travel target action");
    add("RequestNamedTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "request named travel target", "Native TortoiseBots request named travel target action");
    add("RequestQuestTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "request quest travel target", "Native TortoiseBots request quest travel target action");
    add("ResetTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "reset travel target", "Native TortoiseBots reset travel target action");
    add("MoveToTravelTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "move to travel target", "Native TortoiseBots move to travel target action");
    add("MoveOutOfCollision", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move out of collision", "Native TortoiseBots move out of collision action");
    add("MoveRandom", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move random", "Native TortoiseBots move random action");
    add("IdleWander", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "idle wander", "Native TortoiseBots idle wander action");
    add("Attack", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "attack", "Native TortoiseBots attack action");
    add("Melee", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "melee", "Native TortoiseBots melee action");
    add("SwitchToMelee", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "switch to melee", "Native TortoiseBots switch to melee action");
    add("SwitchToRanged", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "switch to ranged", "Native TortoiseBots switch to ranged action");
    add("ReachSpell", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reach spell", "Native TortoiseBots reach spell action");
    add("ReachCc", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reach cc", "Native TortoiseBots reach cc action");
    add("ReachMelee", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reach melee", "Native TortoiseBots reach melee action");
    add("ReachPull", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reach pull", "Native TortoiseBots reach pull action");
    add("ReachPartyMemberToHeal", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reach party member to heal", "Native TortoiseBots reach party member to heal action");
    add("ReachPartyMemberToResurrect", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reach party member to resurrect", "Native TortoiseBots reach party member to resurrect action");
    add("ReachPartyMemberForTotem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reach party member for totem", "Native TortoiseBots reach party member for totem action");
    add("Flee", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "flee", "Native TortoiseBots flee action");
    add("FleeFromArea", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "flee from area", "Native TortoiseBots flee from area action");
    add("FleeWithPet", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "flee with pet", "Native TortoiseBots flee with pet action");
    add("AvoidAoe", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "avoid aoe", "Native TortoiseBots avoid aoe action");
    add("AvoidElite", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "avoid elite", "Native TortoiseBots avoid elite action");
    add("WaitForAttackKeepSafeDistance", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "wait for attack keep safe distance", "Native TortoiseBots wait for attack keep safe distance action");
    add("Shoot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "shoot", "Native TortoiseBots shoot action");
    add("WhipperRootTuber", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "whipper root tuber", "Native TortoiseBots whipper root tuber action");
    add("Healthstone", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "healthstone", "Native TortoiseBots healthstone action");
    add("HealingPotion", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "healing potion", "Native TortoiseBots healing potion action");
    add("ManaPotion", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "mana potion", "Native TortoiseBots mana potion action");
    add("Food", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "food", "Native TortoiseBots food action");
    add("Drink", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "drink", "Native TortoiseBots drink action");
    add("TankAssist", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "tank assist", "Native TortoiseBots tank assist action");
    add("DpsAssist", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "dps assist", "Native TortoiseBots dps assist action");
    add("DpsAoe", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "dps aoe", "Native TortoiseBots dps aoe action");
    add("AttackRtiTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "attack rti target", "Native TortoiseBots attack rti target action");
    add("Loot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "loot", "Native TortoiseBots loot action");
    add("AddLoot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "add loot", "Native TortoiseBots add loot action");
    add("AddGatheringLoot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "add gathering loot", "Native TortoiseBots add gathering loot action");
    add("AddAllLoot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "add all loot", "Native TortoiseBots add all loot action");
    add("ReleaseLoot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "release loot", "Native TortoiseBots release loot action");
    add("AutoLootRoll", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "auto loot roll", "Native TortoiseBots auto loot roll action");
    add("Follow", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "follow", "Native TortoiseBots follow action");
    add("FormationStation", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "formation station", "Native TortoiseBots formation station action");
    add("StopFollow", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "stop follow", "Native TortoiseBots stop follow action");
    add("FleeToMaster", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "flee to master", "Native TortoiseBots flee to master action");
    add("Runaway", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "runaway", "Native TortoiseBots runaway action");
    add("Stay", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "stay", "Native TortoiseBots stay action");
    add("Sit", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "sit", "Native TortoiseBots sit action");
    add("AttackAnything", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "attack anything", "Native TortoiseBots attack anything action");
    add("AttackLeastHpTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "attack least hp target", "Native TortoiseBots attack least hp target action");
    add("AttackEnemyPlayer", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "attack enemy player", "Native TortoiseBots attack enemy player action");
    add("PullMyTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "pull my target", "Native TortoiseBots pull my target action");
    add("PullbackMyTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "pullback my target", "Native TortoiseBots pullback my target action");
    add("PullRtiTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "pull rti target", "Native TortoiseBots pull rti target action");
    add("PullNearestTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "pull nearest target", "Native TortoiseBots pull nearest target action");
    add("PullStart", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "pull start", "Native TortoiseBots pull start action");
    add("PullAction", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "pull action", "Native TortoiseBots pull action action");
    add("ReturnToPullPosition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "return to pull position", "Native TortoiseBots return to pull position action");
    add("PullEnd", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "pull end", "Native TortoiseBots pull end action");
    add("EndPull", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "end pull", "Native TortoiseBots end pull action");
    add("ReleasePullHold", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "release pull hold", "Native TortoiseBots release pull hold action");
    add("ActEmote", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "emote", "Native TortoiseBots emote action");
    add("Talk", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "talk", "Native TortoiseBots talk action");
    add("MountAnim", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "mount anim", "Native TortoiseBots mount anim action");
    add("Return", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "return", "Native TortoiseBots return action");
    add("MoveToLoot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move to loot", "Native TortoiseBots move to loot action");
    add("OpenLoot", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "open loot", "Native TortoiseBots open loot action");
    add("Guard", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guard", "Native TortoiseBots guard action");
    add("ReturnToStayPosition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "return to stay position", "Native TortoiseBots return to stay position action");
    add("MoveOutOfEnemyContact", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move out of enemy contact", "Native TortoiseBots move out of enemy contact action");
    add("SetFacing", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set facing", "Native TortoiseBots set facing action");
    add("TankFaceAway", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "tank face away", "Native TortoiseBots tank face away action");
    add("SetBehind", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set behind", "Native TortoiseBots set behind action");
    add("RearFlank", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rear flank", "Native TortoiseBots rear flank action");
    add("AttackDuelOpponent", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "attack duel opponent", "Native TortoiseBots attack duel opponent action");
    add("SelectNewTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "select new target", "Native TortoiseBots select new target action");
    add("CheckMail", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "check mail", "Native TortoiseBots check mail action");
    add("RevealGatheringItem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reveal gathering item", "Native TortoiseBots reveal gathering item action");
    add("Outfit", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "outfit", "Native TortoiseBots outfit action");
    add("RandomBotUpdate", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "random bot update", "Native TortoiseBots random bot update action");
    add("Delay", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "delay", "Native TortoiseBots delay action");
    add("CancelChannel", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "cancel channel", "Native TortoiseBots cancel channel action");
    add("Greet", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "greet", "Native TortoiseBots greet action");
    add("CheckValues", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "check values", "Native TortoiseBots check values action");
    add("SetAvoidArea", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set avoid area", "Native TortoiseBots set avoid area action");
    add("Ra", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "ra", "Native TortoiseBots ra action");
    add("RemoveBlessingOfSalvation", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "remove blessing of salvation", "Native TortoiseBots remove blessing of salvation action");
    add("RemoveGreaterBlessingOfSalvation", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "remove greater blessing of salvation", "Native TortoiseBots remove greater blessing of salvation action");
    add("ApplyStone", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "apply stone", "Native TortoiseBots apply stone action");
    add("ApplyOil", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "apply oil", "Native TortoiseBots apply oil action");
    add("TryEmergency", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "try emergency", "Native TortoiseBots try emergency action");
    add("GiveFood", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "give food", "Native TortoiseBots give food action");
    add("GiveWater", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "give water", "Native TortoiseBots give water action");
    add("Mount", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "mount", "Native TortoiseBots mount action");
    add("AutoTalents", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "auto talents", "Native TortoiseBots auto talents action");
    add("AutoLearnSpell", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "auto learn spell", "Native TortoiseBots auto learn spell action");
    add("AutoShareQuest", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "auto share quest", "Native TortoiseBots auto share quest action");
    add("XpGain", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "xp gain", "Native TortoiseBots xp gain action");
    add("HonorGain", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "honor gain", "Native TortoiseBots honor gain action");
    add("InviteNearby", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "invite nearby", "Native TortoiseBots invite nearby action");
    add("InviteGuild", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "invite guild", "Native TortoiseBots invite guild action");
    add("LeaveFarAway", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "leave far away", "Native TortoiseBots leave far away action");
    add("WorldBuff", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "world buff", "Native TortoiseBots world buff action");
    add("WorldBuffTravelApply", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel apply", "Native TortoiseBots world buff travel apply action");
    add("WorldBuffTravelCastPortal", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel cast portal", "Native TortoiseBots world buff travel cast portal action");
    add("WorldBuffTravelTakePortal", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel take portal", "Native TortoiseBots world buff travel take portal action");
    add("WorldBuffTravelFinish", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel finish", "Native TortoiseBots world buff travel finish action");
    add("WorldBuffTravelSetTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel set target", "Native TortoiseBots world buff travel set target action");
    add("WorldBuffTravelDmBuffed", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel dm buffed", "Native TortoiseBots world buff travel dm buffed action");
    add("WorldBuffTravelDmExited", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel dm exited", "Native TortoiseBots world buff travel dm exited action");
    add("WorldBuffTravelDmCastPortal", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel dm cast portal", "Native TortoiseBots world buff travel dm cast portal action");
    add("WorldBuffTravelDmTakePortal", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "world buff travel dm take portal", "Native TortoiseBots world buff travel dm take portal action");
    add("Hearthstone", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "hearthstone", "Native TortoiseBots hearthstone action");
    add("CastRandomSpell", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "cast random spell", "Native TortoiseBots cast random spell action");
    add("UseRandomRecipe", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use random recipe", "Native TortoiseBots use random recipe action");
    add("OpenRandomItem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "open random item", "Native TortoiseBots open random item action");
    add("UseRandomQuestItem", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "use random quest item", "Native TortoiseBots use random quest item action");
    add("CraftRandomItem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "craft random item", "Native TortoiseBots craft random item action");
    add("SmartDestroyItem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "smart destroy item", "Native TortoiseBots smart destroy item action");
    add("DisenchantRandomItem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disenchant random item", "Native TortoiseBots disenchant random item action");
    add("GuildDisenchantService", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guild disenchant service", "Native TortoiseBots guild disenchant service action");
    add("EnchantRandomItem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enchant random item", "Native TortoiseBots enchant random item action");
    add("ResetInstances", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reset instances", "Native TortoiseBots reset instances action");
    add("ResetRaids", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reset raids", "Native TortoiseBots reset raids action");
    add("UpdateGear", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "update gear", "Native TortoiseBots update gear action");
    add("BuyPetition", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "buy petition", "Native TortoiseBots buy petition action");
    add("OfferPetition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "offer petition", "Native TortoiseBots offer petition action");
    add("OfferPetitionNearby", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "offer petition nearby", "Native TortoiseBots offer petition nearby action");
    add("TurnInPetition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "turn in petition", "Native TortoiseBots turn in petition action");
    add("BuyTabard", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "buy tabard", "Native TortoiseBots buy tabard action");
    add("GuildManageNearby", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guild manage nearby", "Native TortoiseBots guild manage nearby action");
    add("GuildOrderBuy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "guild order buy", "Native TortoiseBots guild order buy action");
    add("GuildOrderTakeMail", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "guild order take mail", "Native TortoiseBots guild order take mail action");
    add("GuildOrderVendor", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guild order vendor", "Native TortoiseBots guild order vendor action");
    add("GuildOrderDeliver", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "guild order deliver", "Native TortoiseBots guild order deliver action");
    add("UseTrinket", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use trinket", "Native TortoiseBots use trinket action");
    add("Unstuck", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "unstuck", "Native TortoiseBots unstuck action");
    add("Reset", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "reset", "Native TortoiseBots reset action");
    add("InterruptCurrentSpell", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "interrupt current spell", "Native TortoiseBots interrupt current spell action");
    add("InitializePet", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "initialize pet", "Native TortoiseBots initialize pet action");
    add("TogglePetSpell", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "toggle pet spell", "Native TortoiseBots toggle pet spell action");
    add("SetPetStance", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set pet stance", "Native TortoiseBots set pet stance action");
    add("PetAttack", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "pet attack", "Native TortoiseBots pet attack action");
    add("GoblinSapper", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "goblin sapper", "Native TortoiseBots goblin sapper action");
    add("OilOfImmolation", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "oil of immolation", "Native TortoiseBots oil of immolation action");
    add("StoneshieldPotion", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "stoneshield potion", "Native TortoiseBots stoneshield potion action");
    add("ThistleTea", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "thistle tea", "Native TortoiseBots thistle tea action");
    add("DarkRune", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "dark rune", "Native TortoiseBots dark rune action");
    add("ThrowGrenade", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "throw grenade", "Native TortoiseBots throw grenade action");
    add("BgBanner", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg banner", "Native TortoiseBots bg banner action");
    add("UseBandage", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use bandage", "Native TortoiseBots use bandage action");
    add("RocketBoots", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rocket boots", "Native TortoiseBots rocket boots action");
    add("FireProtectionPotion", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fire protection potion", "Native TortoiseBots fire protection potion action");
    add("BossProtectionPotion", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss protection potion", "Native TortoiseBots boss protection potion action");
    add("FreeActionPotion", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "free action potion", "Native TortoiseBots free action potion action");
    add("UseConsumable", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use consumable", "Native TortoiseBots use consumable action");
    add("AntiVenom", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "anti-venom", "Native TortoiseBots anti-venom action");
    add("BgTactics", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg tactics", "Native TortoiseBots bg tactics action");
    add("BgMoveToStart", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg move to start", "Native TortoiseBots bg move to start action");
    add("BgMoveToObjective", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg move to objective", "Native TortoiseBots bg move to objective action");
    add("BgSelectObjective", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg select objective", "Native TortoiseBots bg select objective action");
    add("BgCheckObjective", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg check objective", "Native TortoiseBots bg check objective action");
    add("BgResetObjectiveForce", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg reset objective force", "Native TortoiseBots bg reset objective force action");
    add("BgAttackFc", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg attack fc", "Native TortoiseBots bg attack fc action");
    add("BgProtectFc", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg protect fc", "Native TortoiseBots bg protect fc action");
    add("BgUseBuff", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg use buff", "Native TortoiseBots bg use buff action");
    add("AttackEnemyFlagCarrier", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "attack enemy flag carrier", "Native TortoiseBots attack enemy flag carrier action");
    add("BgCheckFlag", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "bg check flag", "Native TortoiseBots bg check flag action");
    add("UseLightwell", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use lightwell", "Native TortoiseBots use lightwell action");
    add("RpgStay", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg stay", "Native TortoiseBots rpg stay action");
    add("RpgWork", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg work", "Native TortoiseBots rpg work action");
    add("RpgEmote", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg emote", "Native TortoiseBots rpg emote action");
    add("RpgCancel", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg cancel", "Native TortoiseBots rpg cancel action");
    add("RpgTaxi", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "rpg taxi", "Native TortoiseBots rpg taxi action");
    add("RpgDiscover", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg discover", "Native TortoiseBots rpg discover action");
    add("RpgStartQuest", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "rpg start quest", "Native TortoiseBots rpg start quest action");
    add("RpgEndQuest", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "rpg end quest", "Native TortoiseBots rpg end quest action");
    add("RpgBuy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "rpg buy", "Native TortoiseBots rpg buy action");
    add("RpgSell", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "rpg sell", "Native TortoiseBots rpg sell action");
    add("RpgAhSell", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "rpg ah sell", "Native TortoiseBots rpg ah sell action");
    add("RpgAhBuy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "rpg ah buy", "Native TortoiseBots rpg ah buy action");
    add("RpgGetMail", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "rpg get mail", "Native TortoiseBots rpg get mail action");
    add("RpgRepair", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "rpg repair", "Native TortoiseBots rpg repair action");
    add("RpgTrain", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg train", "Native TortoiseBots rpg train action");
    add("RpgHeal", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg heal", "Native TortoiseBots rpg heal action");
    add("RpgHomeBind", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg home bind", "Native TortoiseBots rpg home bind action");
    add("RpgQueueBg", SupportClass::ObserveOnly, true, true, true, 1.0f, 300, 5, "rpg queue bg", "Native TortoiseBots rpg queue bg action");
    add("RpgBuyPetition", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "rpg buy petition", "Native TortoiseBots rpg buy petition action");
    add("RpgUse", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg use", "Native TortoiseBots rpg use action");
    add("RpgSpell", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg spell", "Native TortoiseBots rpg spell action");
    add("RpgCraft", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg craft", "Native TortoiseBots rpg craft action");
    add("RpgTradeUseful", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg trade useful", "Native TortoiseBots rpg trade useful action");
    add("RpgEnchant", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg enchant", "Native TortoiseBots rpg enchant action");
    add("RpgDuel", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg duel", "Native TortoiseBots rpg duel action");
    add("RpgItem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg item", "Native TortoiseBots rpg item action");
    add("RpgGossipTalk", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "rpg gossip talk", "Native TortoiseBots rpg gossip talk action");
    add("AutoCompleteQuest", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "auto complete quest", "Native TortoiseBots auto complete quest action");
    add("MoveToFish", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move to fish", "Native TortoiseBots move to fish action");
    add("Fish", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "fish", "Native TortoiseBots fish action");
    add("UseFishingBobber", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use fishing bobber", "Native TortoiseBots use fishing bobber action");
    add("SetCombatState", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set combat state", "Native TortoiseBots set combat state action");
    add("SetNonCombatState", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set non combat state", "Native TortoiseBots set non combat state action");
    add("SetDeadState", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "set dead state", "Native TortoiseBots set dead state action");
    add("WarStomp", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "war stomp", "Native TortoiseBots war stomp action");
    add("Berserking", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "berserking", "Native TortoiseBots berserking action");
    add("BloodFury", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "blood fury", "Native TortoiseBots blood fury action");
    add("Cannibalize", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "cannibalize", "Native TortoiseBots cannibalize action");
    add("EscapeArtist", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "escape artist", "Native TortoiseBots escape artist action");
    add("Shadowmeld", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "shadowmeld", "Native TortoiseBots shadowmeld action");
    add("Stoneform", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "stoneform", "Native TortoiseBots stoneform action");
    add("Perception", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "perception", "Native TortoiseBots perception action");
    add("WillOfTheForsaken", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "will of the forsaken", "Native TortoiseBots will of the forsaken action");
    add("ManaTap", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "mana tap", "Native TortoiseBots mana tap action");
    add("ArcaneTorrent", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "arcane torrent", "Native TortoiseBots arcane torrent action");
    add("UseId", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use id", "Native TortoiseBots use id action");
    add("CommanderUse", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "commander use", "Native TortoiseBots commander use action");
    add("MoveTo", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move to", "Native TortoiseBots move to action");
    add("EnableBossTacticsStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable boss tactics strategy", "Native TortoiseBots enable boss tactics strategy action");
    add("DisableBossTacticsStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable boss tactics strategy", "Native TortoiseBots disable boss tactics strategy action");
    add("BossTacticsInterrupt", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics interrupt", "Native TortoiseBots boss tactics interrupt action");
    add("BossTacticsDispelSweep", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics dispel sweep", "Native TortoiseBots boss tactics dispel sweep action");
    add("BossTacticsKillFirst", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics kill first", "Native TortoiseBots boss tactics kill first action");
    add("BossTacticsHoldKill", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics hold kill", "Native TortoiseBots boss tactics hold kill action");
    add("BossTacticsLinkedDeath", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics linked death", "Native TortoiseBots boss tactics linked death action");
    add("BossTacticsDpsHold", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics dps hold", "Native TortoiseBots boss tactics dps hold action");
    add("BossTacticsStepBehind", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics step behind", "Native TortoiseBots boss tactics step behind action");
    add("BossTacticsAvoidCast", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics avoid cast", "Native TortoiseBots boss tactics avoid cast action");
    add("BossTacticsFreeze", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics freeze", "Native TortoiseBots boss tactics freeze action");
    add("BossTacticsFlee", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics flee", "Native TortoiseBots boss tactics flee action");
    add("BossTacticsStand", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics stand", "Native TortoiseBots boss tactics stand action");
    add("BossTacticsUseObject", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics use object", "Native TortoiseBots boss tactics use object action");
    add("BossTacticsTankSwap", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics tank swap", "Native TortoiseBots boss tactics tank swap action");
    add("BossTacticsTankSwapHold", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics tank swap hold", "Native TortoiseBots boss tactics tank swap hold action");
    add("BossTacticsBurnMana", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics burn mana", "Native TortoiseBots boss tactics burn mana action");
    add("BossTacticsRangedOnly", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics ranged only", "Native TortoiseBots boss tactics ranged only action");
    add("BossTacticsObjectAnchor", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics object anchor", "Native TortoiseBots boss tactics object anchor action");
    add("BossTacticsSeek", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics seek", "Native TortoiseBots boss tactics seek action");
    add("BossTacticsSeekTank", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics seek tank", "Native TortoiseBots boss tactics seek tank action");
    add("BossTacticsSeekKill", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics seek kill", "Native TortoiseBots boss tactics seek kill action");
    add("BossTacticsSeekPet", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics seek pet", "Native TortoiseBots boss tactics seek pet action");
    add("BossTacticsSeekTotem", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics seek totem", "Native TortoiseBots boss tactics seek totem action");
    add("BossTacticsTauntAfter", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "boss tactics taunt after", "Native TortoiseBots boss tactics taunt after action");
    add("EnableOnyxiaSLairStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable onyxia's lair strategy", "Native TortoiseBots enable onyxia's lair strategy action");
    add("DisableOnyxiaSLairStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable onyxia's lair strategy", "Native TortoiseBots disable onyxia's lair strategy action");
    add("EnableMoltenCoreStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable molten core strategy", "Native TortoiseBots enable molten core strategy action");
    add("DisableMoltenCoreStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable molten core strategy", "Native TortoiseBots disable molten core strategy action");
    add("EnableBlackwingLairStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable blackwing lair strategy", "Native TortoiseBots enable blackwing lair strategy action");
    add("DisableBlackwingLairStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable blackwing lair strategy", "Native TortoiseBots disable blackwing lair strategy action");
    add("EnableNaxxramasStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable naxxramas strategy", "Native TortoiseBots enable naxxramas strategy action");
    add("DisableNaxxramasStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable naxxramas strategy", "Native TortoiseBots disable naxxramas strategy action");
    add("EnableZulGurubStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable zul'gurub strategy", "Native TortoiseBots enable zul'gurub strategy action");
    add("DisableZulGurubStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable zul'gurub strategy", "Native TortoiseBots disable zul'gurub strategy action");
    add("EnableRuinsOfAhnQirajStrategy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "enable ruins of ahn'qiraj strategy", "Native TortoiseBots enable ruins of ahn'qiraj strategy action");
    add("DisableRuinsOfAhnQirajStrategy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "disable ruins of ahn'qiraj strategy", "Native TortoiseBots disable ruins of ahn'qiraj strategy action");
    add("EnableAhnQirajTempleStrategy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "enable ahn'qiraj temple strategy", "Native TortoiseBots enable ahn'qiraj temple strategy action");
    add("DisableAhnQirajTempleStrategy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "disable ahn'qiraj temple strategy", "Native TortoiseBots disable ahn'qiraj temple strategy action");
    add("EnableOssirianFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable ossirian fight strategy", "Native TortoiseBots enable ossirian fight strategy action");
    add("DisableOssirianFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable ossirian fight strategy", "Native TortoiseBots disable ossirian fight strategy action");
    add("UseOssirianCrystal", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use ossirian crystal", "Native TortoiseBots use ossirian crystal action");
    add("EnableEmeraldSanctumStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable emerald sanctum strategy", "Native TortoiseBots enable emerald sanctum strategy action");
    add("DisableEmeraldSanctumStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable emerald sanctum strategy", "Native TortoiseBots disable emerald sanctum strategy action");
    add("EnableLowerKarazhanStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable lower karazhan strategy", "Native TortoiseBots enable lower karazhan strategy action");
    add("DisableLowerKarazhanStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable lower karazhan strategy", "Native TortoiseBots disable lower karazhan strategy action");
    add("EnableKarazhanCryptStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable karazhan crypt strategy", "Native TortoiseBots enable karazhan crypt strategy action");
    add("DisableKarazhanCryptStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable karazhan crypt strategy", "Native TortoiseBots disable karazhan crypt strategy action");
    add("EnableOnyxiaFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable onyxia fight strategy", "Native TortoiseBots enable onyxia fight strategy action");
    add("DisableOnyxiaFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable onyxia fight strategy", "Native TortoiseBots disable onyxia fight strategy action");
    add("OnyxiaBreathSafeZone", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "onyxia breath safe zone", "Native TortoiseBots onyxia breath safe zone action");
    add("OnyxiaKillWhelps", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "onyxia kill whelps", "Native TortoiseBots onyxia kill whelps action");
    add("OnyxiaAvoidEggs", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "onyxia avoid eggs", "Native TortoiseBots onyxia avoid eggs action");
    add("EnableMagmadarFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable magmadar fight strategy", "Native TortoiseBots enable magmadar fight strategy action");
    add("DisableMagmadarFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable magmadar fight strategy", "Native TortoiseBots disable magmadar fight strategy action");
    add("MoveAwayFromMagmadar", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move away from magmadar", "Native TortoiseBots move away from magmadar action");
    add("EnableGarrFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable garr fight strategy", "Native TortoiseBots enable garr fight strategy action");
    add("DisableGarrFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable garr fight strategy", "Native TortoiseBots disable garr fight strategy action");
    add("EnableShazzrahFightStrategy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "enable shazzrah fight strategy", "Native TortoiseBots enable shazzrah fight strategy action");
    add("DisableShazzrahFightStrategy", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "disable shazzrah fight strategy", "Native TortoiseBots disable shazzrah fight strategy action");
    add("MoveAwayFromShazzrah", SupportClass::RequestableExact, true, true, true, 1.0f, 300, 5, "move away from shazzrah", "Native TortoiseBots move away from shazzrah action");
    add("EnableGolemaggFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable golemagg fight strategy", "Native TortoiseBots enable golemagg fight strategy action");
    add("DisableGolemaggFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable golemagg fight strategy", "Native TortoiseBots disable golemagg fight strategy action");
    add("BackOffGolemagg", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "back off golemagg", "Native TortoiseBots back off golemagg action");
    add("GolemaggHealerPosition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "golemagg healer position", "Native TortoiseBots golemagg healer position action");
    add("GolemaggTankHold", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "golemagg tank hold", "Native TortoiseBots golemagg tank hold action");
    add("EnableGeddonFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable geddon fight strategy", "Native TortoiseBots enable geddon fight strategy action");
    add("DisableGeddonFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable geddon fight strategy", "Native TortoiseBots disable geddon fight strategy action");
    add("MoveAwayFromLivingBomb", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move away from living bomb", "Native TortoiseBots move away from living bomb action");
    add("MoveAwayFromGeddon", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move away from geddon", "Native TortoiseBots move away from geddon action");
    add("ViscidusCombat", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "viscidus combat", "Native TortoiseBots viscidus combat action");
    add("ViscidusKeepDistance", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "viscidus keep distance", "Native TortoiseBots viscidus keep distance action");
    add("ViscidusToxinEscape", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "viscidus toxin escape", "Native TortoiseBots viscidus toxin escape action");
    add("MoveAwayFromHazard", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move away from hazard", "Native TortoiseBots move away from hazard action");
    add("RaidBombRunout", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "raid bomb runout", "Native TortoiseBots raid bomb runout action");
    add("FourHorsemenRotate", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "four horsemen rotate", "Native TortoiseBots four horsemen rotate action");
    add("VaelAdrenalineFlee", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "vael adrenaline flee", "Native TortoiseBots vael adrenaline flee action");
    add("DragonFlank", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "dragon flank", "Native TortoiseBots dragon flank action");
    add("RaidSpread", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "raid spread", "Native TortoiseBots raid spread action");
    add("SwapFireResistanceAura", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "swap fire resistance aura", "Native TortoiseBots swap fire resistance aura action");
    add("SwapShadowResistanceAura", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "swap shadow resistance aura", "Native TortoiseBots swap shadow resistance aura action");
    add("RestoreSpecAura", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "restore spec aura", "Native TortoiseBots restore spec aura action");
    add("DragonTankFaceAway", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "dragon tank face away", "Native TortoiseBots dragon tank face away action");
    add("MoveToMcRune", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move to mc rune", "Native TortoiseBots move to mc rune action");
    add("DouseMcRuneAqual", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "douse mc rune aqual", "Native TortoiseBots douse mc rune aqual action");
    add("DouseMcRuneEternal", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "douse mc rune eternal", "Native TortoiseBots douse mc rune eternal action");
    add("MoveToSuppressionDevice", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move to suppression device", "Native TortoiseBots move to suppression device action");
    add("StealthForSuppressionDevice", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "stealth for suppression device", "Native TortoiseBots stealth for suppression device action");
    add("DisarmSuppressionDevice", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disarm suppression device", "Native TortoiseBots disarm suppression device action");
    add("EnableChromaggusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable chromaggus fight strategy", "Native TortoiseBots enable chromaggus fight strategy action");
    add("DisableChromaggusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable chromaggus fight strategy", "Native TortoiseBots disable chromaggus fight strategy action");
    add("UseHourglassSand", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "use hourglass sand", "Native TortoiseBots use hourglass sand action");
    add("EnableRazorgoreFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable razorgore fight strategy", "Native TortoiseBots enable razorgore fight strategy action");
    add("DisableRazorgoreFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable razorgore fight strategy", "Native TortoiseBots disable razorgore fight strategy action");
    add("EscapeRazorgoreCone", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "escape razorgore cone", "Native TortoiseBots escape razorgore cone action");
    add("BackOffRazorgore", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "back off razorgore", "Native TortoiseBots back off razorgore action");
    add("RazorgoreEngage", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "razorgore engage", "Native TortoiseBots razorgore engage action");
    add("EnableBroodlordFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable broodlord fight strategy", "Native TortoiseBots enable broodlord fight strategy action");
    add("DisableBroodlordFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable broodlord fight strategy", "Native TortoiseBots disable broodlord fight strategy action");
    add("MoveAwayFromBroodlord", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move away from broodlord", "Native TortoiseBots move away from broodlord action");
    add("EnableNefarianFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable nefarian fight strategy", "Native TortoiseBots enable nefarian fight strategy action");
    add("DisableNefarianFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable nefarian fight strategy", "Native TortoiseBots disable nefarian fight strategy action");
    add("EnableVaelFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable vael fight strategy", "Native TortoiseBots enable vael fight strategy action");
    add("DisableVaelFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable vael fight strategy", "Native TortoiseBots disable vael fight strategy action");
    add("EnableFourHorsemanFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable four horseman fight strategy", "Native TortoiseBots enable four horseman fight strategy action");
    add("DisableFourHorsemanFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable four horseman fight strategy", "Native TortoiseBots disable four horseman fight strategy action");
    add("EnableGluthFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable gluth fight strategy", "Native TortoiseBots enable gluth fight strategy action");
    add("DisableGluthFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable gluth fight strategy", "Native TortoiseBots disable gluth fight strategy action");
    add("GluthChooseTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "gluth choose target", "Native TortoiseBots gluth choose target action");
    add("GluthTauntSwap", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "gluth taunt swap", "Native TortoiseBots gluth taunt swap action");
    add("EnableThaddiusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable thaddius fight strategy", "Native TortoiseBots enable thaddius fight strategy action");
    add("DisableThaddiusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable thaddius fight strategy", "Native TortoiseBots disable thaddius fight strategy action");
    add("ThaddiusAttackNearestPet", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "thaddius attack nearest pet", "Native TortoiseBots thaddius attack nearest pet action");
    add("ThaddiusMoveToPlatform", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "thaddius move to platform", "Native TortoiseBots thaddius move to platform action");
    add("ThaddiusMovePolarity", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "thaddius move polarity", "Native TortoiseBots thaddius move polarity action");
    add("EnableKelThuzadFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable kel'thuzad fight strategy", "Native TortoiseBots enable kel'thuzad fight strategy action");
    add("DisableKelThuzadFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable kel'thuzad fight strategy", "Native TortoiseBots disable kel'thuzad fight strategy action");
    add("KelThuzadChooseTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "kel'thuzad choose target", "Native TortoiseBots kel'thuzad choose target action");
    add("KelThuzadPosition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "kel'thuzad position", "Native TortoiseBots kel'thuzad position action");
    add("EnableGrobbulusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable grobbulus fight strategy", "Native TortoiseBots enable grobbulus fight strategy action");
    add("DisableGrobbulusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable grobbulus fight strategy", "Native TortoiseBots disable grobbulus fight strategy action");
    add("GrobbulusGoBehind", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "grobbulus go behind", "Native TortoiseBots grobbulus go behind action");
    add("EnableLoathebFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable loatheb fight strategy", "Native TortoiseBots enable loatheb fight strategy action");
    add("DisableLoathebFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable loatheb fight strategy", "Native TortoiseBots disable loatheb fight strategy action");
    add("LoathebChooseTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "loatheb choose target", "Native TortoiseBots loatheb choose target action");
    add("LoathebPosition", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "loatheb position", "Native TortoiseBots loatheb position action");
    add("EnableRazuviousFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable razuvious fight strategy", "Native TortoiseBots enable razuvious fight strategy action");
    add("DisableRazuviousFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable razuvious fight strategy", "Native TortoiseBots disable razuvious fight strategy action");
    add("RazuviousMindControl", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "razuvious mind control", "Native TortoiseBots razuvious mind control action");
    add("EnableSapphironFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable sapphiron fight strategy", "Native TortoiseBots enable sapphiron fight strategy action");
    add("DisableSapphironFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable sapphiron fight strategy", "Native TortoiseBots disable sapphiron fight strategy action");
    add("SapphironHide", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "sapphiron hide", "Native TortoiseBots sapphiron hide action");
    add("SapphironAvoidBlizzard", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "sapphiron avoid blizzard", "Native TortoiseBots sapphiron avoid blizzard action");
    add("EnableHeiganFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable heigan fight strategy", "Native TortoiseBots enable heigan fight strategy action");
    add("DisableHeiganFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable heigan fight strategy", "Native TortoiseBots disable heigan fight strategy action");
    add("HeiganDanceMove", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "heigan dance move", "Native TortoiseBots heigan dance move action");
    add("HeiganHoldPlatform", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "heigan hold platform", "Native TortoiseBots heigan hold platform action");
    add("EnableAnubRekhanFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable anub'rekhan fight strategy", "Native TortoiseBots enable anub'rekhan fight strategy action");
    add("DisableAnubRekhanFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable anub'rekhan fight strategy", "Native TortoiseBots disable anub'rekhan fight strategy action");
    add("AnubRekhanChooseTarget", SupportClass::RequestableNativeChoice, true, true, true, 1.0f, 300, 5, "anub'rekhan choose target", "Native TortoiseBots anub'rekhan choose target action");
    add("AnubRekhanToCenter", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "anub'rekhan to center", "Native TortoiseBots anub'rekhan to center action");
    add("MoveAwayFromVoidZone", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "move away from void zone", "Native TortoiseBots move away from void zone action");
    add("EnableSolniusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable solnius fight strategy", "Native TortoiseBots enable solnius fight strategy action");
    add("DisableSolniusFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable solnius fight strategy", "Native TortoiseBots disable solnius fight strategy action");
    add("EnableAraxxnaFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable araxxna fight strategy", "Native TortoiseBots enable araxxna fight strategy action");
    add("DisableAraxxnaFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable araxxna fight strategy", "Native TortoiseBots disable araxxna fight strategy action");
    add("EnableMoroesFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "enable moroes fight strategy", "Native TortoiseBots enable moroes fight strategy action");
    add("DisableMoroesFightStrategy", SupportClass::LocalOnly, true, true, true, 1.0f, 300, 5, "disable moroes fight strategy", "Native TortoiseBots disable moroes fight strategy action");
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
