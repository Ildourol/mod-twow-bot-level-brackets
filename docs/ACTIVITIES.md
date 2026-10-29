# Native Bot Activity Catalog (Section 26 Specification)

**Module:** `mod-twow-bot-level-brackets-extended`  
**Total Registered Activities:** 43  
**Domain Division:** Capital City (17), Outdoor World (26)

---

## 1. Overview

In **Bot Level Brackets Extended**, bots are not passive background props or simple grinders. They engage in a rich spectrum of 43 lore-authentic native activities. Each activity defines:
* **Unique Activity ID & Key**: Used for configuration matching (`BotLevelBrackets.Activity.<Key>.Weight`).
* **Domain Bitmask**: `DOMAIN_CITY` (1), `DOMAIN_WORLD` (2), or combined.
* **Native Purpose Bitmask**: Maps directly to TortoiseBots travel purposes (`Grind`, `Quest`, `Gather`, `Rpg`, etc.).
* **Default Weight**: Baseline selection probability during coordinator candidate evaluation.
* **Default Duration**: Typical time-in-state before the opportunity naturally completes and releases.
* **Default Cooldown**: Cooldown applied to the bot before the same activity can be reassigned.

---

## 2. Capital City Domain Activities (17 Activities)

These activities are assigned exclusively within the 8 capital cities (Stormwind, Ironforge, Darnassus, Alah'Thalas, Orgrimmar, Undercity, Thunder Bluff, Sparkwater Port):

| ID | Activity Key | Display Name | Native Purpose | Default Weight | Duration | Cooldown |
|:---:|:---|:---|:---:|:---:|:---:|:---:|
| 1 | `AuctionBrowse` | Auction House Browsing | Trade (1 << 4) | 1.0 | 300s | 1800s |
| 2 | `AuctionBid` | Auction House Bidding | Trade (1 << 4) | 0.8 | 180s | 1800s |
| 3 | `BankDeposit` | Bank Vault Storage | Bank (1 << 5) | 1.0 | 120s | 1200s |
| 4 | `BankWithdraw` | Bank Vault Retrieval | Bank (1 << 5) | 1.0 | 120s | 1200s |
| 5 | `TavernRest` | Inn & Tavern Rest | Tavern (1 << 6) | 1.5 | 600s | 3600s |
| 6 | `TavernDrink` | Tavern Socializing | Tavern (1 << 6) | 1.2 | 300s | 1800s |
| 7 | `ClassTrainer` | Class Skill Training | Trainer (1 << 7) | 2.0 | 240s | 1800s |
| 8 | `ProfessionTrainer` | Trade Skill Training | Trainer (1 << 7) | 1.5 | 240s | 1800s |
| 9 | `WeaponTrainer` | Weapon Skill Training | Trainer (1 << 7) | 1.0 | 240s | 3600s |
| 10 | `FirstAidTrainer` | First Aid Training | Trainer (1 << 7) | 1.0 | 180s | 3600s |
| 11 | `VendorSellJunk` | Merchant Junk Selling | Vendor (1 << 8) | 2.0 | 180s | 900s |
| 12 | `VendorBuyFood` | Merchant Rations Restock | Vendor (1 << 8) | 1.5 | 180s | 1200s |
| 13 | `VendorRepair` | Armorer Equipment Repair | Repair (1 << 9) | 2.5 | 180s | 900s |
| 14 | `CityPatrol` | Capital Street Strolling | Explore (1 << 1) | 1.0 | 450s | 1800s |
| 15 | `CityVisitGuild` | Guild Master Visitation | Rpg (1 << 0) | 0.8 | 240s | 3600s |
| 16 | `CityVisitRacial` | Racial Leader Audience | Rpg (1 << 0) | 0.7 | 300s | 7200s |
| 17 | `CityRestIdle` | City Plaza Idle Chat | Tavern (1 << 6) | 1.0 | 360s | 1200s |

---

## 3. Outdoor World Domain Activities (26 Activities)

These activities are assigned across the 38 outdoor regions and wilderness zones:

| ID | Activity Key | Display Name | Native Purpose | Default Weight | Duration | Cooldown |
|:---:|:---|:---|:---:|:---:|:---:|:---:|
| 18 | `GrindMob` | Open World Mob Grinding | Grind (1 << 12) | 3.0 | 900s | 600s |
| 19 | `GrindBeast` | Beast Hunting | Grind (1 << 12) | 2.5 | 900s | 600s |
| 20 | `GrindHumanoid` | Humanoid Camp Raiding | Grind (1 << 12) | 2.5 | 900s | 600s |
| 21 | `GrindUndead` | Scourge Extermination | Grind (1 << 12) | 2.0 | 900s | 600s |
| 22 | `GrindElemental` | Elemental Hunting | Grind (1 << 12) | 1.5 | 900s | 600s |
| 23 | `GrindDemon` | Demon Purging | Grind (1 << 12) | 1.2 | 900s | 600s |
| 24 | `GrindDragonkin` | Dragonkin Slaying | Grind (1 << 12) | 1.0 | 900s | 600s |
| 25 | `GatherHerb` | Herbalism Foraging | Gather (1 << 10) | 1.5 | 1200s | 900s |
| 26 | `GatherMine` | Mining Node Prospecting | Gather (1 << 10) | 1.5 | 1200s | 900s |
| 27 | `GatherSkin` | Leatherworking Skinning | Gather (1 << 10) | 1.5 | 1200s | 900s |
| 28 | `GatherFish` | Open Water Angling | Gather (1 << 10) | 1.0 | 900s | 1800s |
| 29 | `QuestTurnIn` | Quest Reward Claiming | Quest (1 << 11) | 2.5 | 300s | 600s |
| 30 | `QuestAccept` | Quest Acquisition | Quest (1 << 11) | 2.5 | 300s | 600s |
| 31 | `QuestComplete` | Quest Objective Execution | Quest (1 << 11) | 3.0 | 1200s | 600s |
| 32 | `ExploreRoad` | Main Highway Scouting | Explore (1 << 1) | 1.2 | 600s | 1200s |
| 33 | `ExploreRuins` | Ancient Ruins Survey | Explore (1 << 1) | 1.0 | 750s | 1800s |
| 34 | `ExploreCoast` | Shoreline & Reef Trek | Explore (1 << 1) | 1.0 | 750s | 1800s |
| 35 | `ExploreMountain` | Mountain Ridge Pathing | Explore (1 << 1) | 0.8 | 900s | 2400s |
| 36 | `ExploreCamp` | Outpost Discovery | Explore (1 << 1) | 1.2 | 600s | 1200s |
| 37 | `DungeonRun` | Dungeon Group Incursion | Dungeon (1 << 13) | 1.0 | 2400s | 3600s |
| 38 | `DungeonPrep` | Instance Supply Prep | Vendor (1 << 8) | 1.2 | 300s | 1800s |
| 39 | `WorldPvPPatrol` | Contested Territory Patrol | PvP (1 << 14) | 1.5 | 900s | 1200s |
| 40 | `WorldPvPDefend` | Outpost Defense Guard | PvP (1 << 14) | 1.2 | 900s | 1200s |
| 41 | `RestCamp` | Wilderness Campfire Rest | Tavern (1 << 6) | 1.0 | 450s | 1800s |
| 42 | `ExploreTurtleZone`| Custom Turtle Zone Tour | Explore (1 << 1) | 1.5 | 1200s | 2400s |
| 43 | `RacialValleyVisit`| Starter Valley Pilgrimage | Rpg (1 << 0) | 1.0 | 600s | 7200s |

---

## 4. Runtime Configuration & Tuning

Every activity can have its weight customized in `mod_twow_bot_level_brackets.conf` using the template:

```ini
BotLevelBrackets.Activity.<ActivityKey>.Weight = <float>
```

For example, to emphasize profession gathering over mob grinding:
```ini
BotLevelBrackets.Activity.GrindMob.Weight = 1.0
BotLevelBrackets.Activity.GatherHerb.Weight = 3.5
BotLevelBrackets.Activity.GatherMine.Weight = 3.5
```

Setting a weight to `0.0` completely disables that specific activity from the selection lottery.
