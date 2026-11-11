# Feature Porting, Parity & Adaptation Report

**Module:** `mod-twow-bot-level-brackets`  
**Upstream Reference:** `mod-player-bot-level-brackets-wotlk` (AzerothCore / WotLK 3.3.5a)  
**Target Engines:** `tortoise-wow` (Turtle WoW Custom vMaNGOS 1.12.1 / client 1.18.1.7272) & `TortoiseBots`  
**Architectural Policy:** [The Unediting Doctrine](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/AGENTS.md) (Zero core modifications)  
**Date of Audit & Implementation:** October 2026  

---

## 1. Executive Summary

This document provides a comprehensive audit and implementation report detailing the porting of all advanced features from the AzerothCore module [`mod-player-bot-level-brackets-wotlk`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-player-bot-level-brackets-wotlk) into the native Turtle WoW custom vMaNGOS module [`mod-twow-bot-level-brackets`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets).

Every feature from the upstream AzerothCore codebase was audited. No feature was left unaddressed:
* All universal gameplay and orchestration mechanics were ported **1:1**.
* All expansion-specific (Wrath of the Lich King) mechanisms were **adapted and altered** to conform strictly to Vanilla 1.12.1 and Turtle WoW 1.18.1 world geography, level limits, and engine conventions.
* Expansion-exclusive systems that do not exist in Vanilla (e.g., Death Knight class logic, Outland/Northrend maps, Dalaran sanctuary, Arena Team Manager) were **appropriately replaced** with native Vanilla equivalents (e.g., 6 racial capital hubs, level 1-60 leveling zones, Battleground/queue protection).

---

## 2. Complete Feature Audit & Implementation Matrix

| Feature | Upstream Status (WotLK) | Turtle WoW vMaNGOS Status | Technical Implementation / Adaptation Details |
| :--- | :--- | :--- | :--- |
| **Capital Hub Ambiance & Dispersal** | Implemented (Dalaran + 6 capitals) | **Implemented & Adapted** | 6 Vanilla racial capitals with polar radial offsets; disperses excess/wrong-level bots; populates under-quota hubs. |
| **Starting-Zone Dispersal** | Implemented (8 starter areas) | **Implemented & Adapted** | Disperses over-leveled bots (> Lvl 9) from the 8 Turtle WoW starting valleys: 6 classic (Northshire, Coldridge, Shadowglen, Valley of Trials, Red Cloud Mesa, Deathknell) + 2 custom (Alah'Thalas High Elf, Tel'Abim Goblin). |
| **Wrong-Map Dispersal** | Implemented (Maps 0, 1, 530, 571) | **Implemented & Adapted** | Enforces open-world continental maps (Eastern Kingdoms 0, Kalimdor 1, Deeprun Tram 369); preserves dungeon/raid instances. |
| **Teleport to Level Zones** | Implemented (9 ranges, 1-80) | **Implemented & Adapted** | Dismounts and teleports bots to safe questing hubs across 7 Vanilla brackets (1-60). Supports arbitrary $N$ brackets. |
| **Pending Reset Queue TTL** | Implemented (`PendingQueueTTL`) | **Implemented 1:1** | Expired queue entries older than TTL seconds are dropped from memory and DB to avoid stale bottlenecks. |
| **Pending Queue Size Cap** | Implemented (`MaxPendingQueueSize`) | **Implemented 1:1** | Rejects new enqueues when queue depth reaches cap, preventing unbounded RAM consumption. |
| **Pending Queue Architecture** | Implemented (`db-characters` SQL) | **Pure RAM-Only with 20s Startup Self-Healing** | High-performance thread-safe in-memory `std::unordered_map` with zero disk I/O, zero database contention, and automated 20s startup census recovery. |
| **Bracket Churn Dampener** | Implemented (`MinBracketChangeThreshold`) | **Implemented 1:1** | Surplus bots below threshold are not moved, preventing constant churn in near-balanced brackets. |
| **Social List Timer Decoupling** | Implemented (`SocialListRefreshFrequency`) | **Implemented 1:1** | Dedicated timer runs independently of heavy distribution cycles. |
| **Arena & Battleground Protection** | Implemented (`IgnoreArenaTeamBots`) | **Fully Implemented & Adapted** | Native Turtle WoW Arena integration: guards bots in Blood Ring Arena (Map 26), Gurubashi Arena (`InGurubashiArena`), Battlegrounds (AV, WSG, AB, Sunnyglade Valley Map 27), and arena/BG queues (`InBattleGroundQueue`). |
| **Cumulative Statistics** | Implemented (`stats` counters) | **Implemented 1:1** | `g_StatsCyclesRun`, `g_StatsBotsAdjusted`, `g_StatsBotsTeleported`, `g_StatsBotsSkipped` tracked via atomics. |
| **Dual-Lifecycle Guild Tracking** | Implemented (`GuildScript` + `PlayerScript`) | **Implemented 1:1** | Native `GuildScript::OnAddMember` / `OnRemoveMember` and `PlayerScript::OnLogin` track human presence in guilds. |
| **Protected Duel Zones** | Implemented (Capital cities) | **Expanded & Adapted** | Guards all 8 racial capitals: Stormwind (1519), Orgrimmar (1637), Ironforge (1537), Undercity (1497), Darnassus (1657), Thunder Bluff (1638), Alah'Thalas (2040/5225), Sparkwater Port (4021), plus Gurubashi Arena (2177, 1741) and Blood Ring Arena (Map 26). |
| **Racial Starter Zone Routing** | N/A (Standard WotLK) | **Added (Turtle WoW Custom)** | Level 1–9 bots routed to native racial starting zones including Thalassian Highlands (High Elf) and Blackstone Island (Goblin). |
| **In-Game Administration CLI** | Implemented (`CommandScript`) | **Implemented & Adapted** | Handled via pre-dispatch [`AllCommandScript`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/tortoise-wow/src/game/ScriptObjects.h#L700-L730) for `.blb` and `.botbrackets` (`reload`, `status`, `force`, `pending`, `cleanup`, `stats`). |
| **Death Knight Level 55 Clamp** | Implemented (`CLASS_DEATH_KNIGHT`) | **Excluded (Incompatible)** | Death Knights do not exist in Vanilla 1.12.1 / Turtle WoW 1.18.1; removed dead branch. |
| **WotLK Maps & Dalaran** | Implemented (Maps 530, 571) | **Excluded & Replaced** | Outland and Northrend maps replaced by classic Eastern Kingdoms (0) and Kalimdor (1). |

---

## 3. Detailed Breakdown: Features Added & Implemented 1:1

### 1. Capital City Hub Ambiance & Dispersal (`HubPopulate`)
* **Configuration:**
  * `BotLevelBrackets.HubDisperse.Enabled` (Default: `1`)
  * `BotLevelBrackets.HubDisperse.BotsPerCycle` (Default: `50`)
  * `BotLevelBrackets.HubDisperse.MaxBotsPerHub` (Default: `50`)
  * `BotLevelBrackets.HubPopulate.Frequency` (Default: `30` seconds)
* **Mechanics:**
  * **Part A (Dispersal):** Iterates over capital city hubs. Any bot exceeding the hub's ambiance quota (`MaxBotsPerHub`) or possessing a level outside the hub's valid level window is enqueued for teleportation back to its designated leveling zone.
  * **Part B (Population):** If a capital city hub has fewer bots than `MaxBotsPerHub`, idle, safe bots wandering in the open world are pulled into the city and spawned with randomized radial polar coordinates:
    $$\Delta x = c_x + \cos(\theta) \cdot r, \quad \Delta y = c_y + \sin(\theta) \cdot r$$

### 2. Starting-Zone Dispersal (`StartingZoneDisperse`)
* **Configuration:**
  * `BotLevelBrackets.StartingZoneDisperse.Frequency` (Default: `15` seconds)
* **Mechanics:**
  * Prevents low-level starting zones from becoming congested with high-level bots.
  * Every 15 seconds, scans starter sanctuaries. Any bot with $\text{Level} > 9$ is automatically enqueued for teleportation to its faction's leveling zone. Covers all 8 starting valleys (Northshire, Coldridge, Shadowglen, Alah'Thalas High Elf, Valley of Trials, Red Cloud Mesa, Deathknell, Tel'Abim Goblin).

### 3. Wrong-Map Dispersal (`WrongMapDisperse`)
* **Configuration:**
  * `BotLevelBrackets.WrongMapDisperse.Frequency` (Default: `60` seconds)
* **Mechanics:**
  * Detects bots stranded on non-continental, test, or invalid maps while excluding active dungeon/raid instances (`!bot->GetMap()->IsDungeon()`).
  * Enqueues stranded bots to return to their appropriate open-world leveling zone.

### 4. Teleportation on Level Adjustment
* **Configuration:**
  * `BotLevelBrackets.TeleportOnLevelChange` (Default: `1`)
* **Mechanics:**
  * When a bot is adjusted into a new bracket, it is not left behind in an inappropriate level zone.
  * A `PendingTeleportEntry` is created and processed by the dedicated teleport ticker (running every 2 seconds). The bot is safely dismounted (`bot->Unmount()`) and moved via `bot->TeleportTo(...)`.

### 5. Pending Queue Robustness: TTL & Max Cap
* **Configuration:**
  * `BotLevelBrackets.MaxPendingQueueSize` (Default: `0` - unlimited)
  * `BotLevelBrackets.PendingQueueTTL` (Default: `0` - disabled)
* **Mechanics:**
  * `MaxPendingQueueSize`: Rejects new enqueues when queue depth reaches the cap, preventing unbounded RAM consumption during mass combat or dungeon runs.
  * `PendingQueueTTL`: Automatically drops entries that remain unprocessed longer than the TTL threshold (e.g., bots stuck in long combat encounters).

### 6. High-Performance RAM-Only Queue Architecture & Startup Self-Healing
* **Memory Model:** Pure thread-safe in-memory `std::unordered_map<uint32_t, PendingResetEntry>` guarded by `std::mutex`.
* **Zero Database Contention:**
  * Eliminates all synchronous disk writes and SQL lock contention on `tw_char`.
  * No manual SQL migration needed by server administrators.
* **Automated Startup Census Recovery:**
  * 20 seconds after server boot (`m_distributionTimer >= 20000`), a comprehensive census pass evaluates all online bots and immediately repopulates pending queues for any bot needing adjustment.
  * Completely immune to stale or orphaned character GUID records across offline character deletions.

### 7. Minimum Bracket Change Threshold Churn Dampener
* **Configuration:**
  * `BotLevelBrackets.MinBracketChangeThreshold` (Default: `0` - disabled)
* **Mechanics:**
  * Prevents redistribution oscillations where brackets with a surplus of 1 bot constantly trade bots back and forth.
  * Bots are only moved from a bracket if:
    $$\text{ActualCount} - \text{DesiredCount} > \text{MinBracketChangeThreshold}$$

### 8. Social List Timer Decoupling
* **Configuration:**
  * `BotLevelBrackets.SocialListRefreshFrequency` (Default: `300` seconds)
* **Mechanics:**
  * The human friends list cache (`character_social`) is refreshed on an independent timer, decoupled from the main bot distribution cycle.

### 9. Cumulative Statistics Tracking
* **Counters:**
  * `g_StatsCyclesRun`: Total distribution cycles completed.
  * `g_StatsBotsAdjusted`: Total bots successfully level-adjusted.
  * `g_StatsBotsTeleported`: Total bots teleported to zones or hubs.
  * `g_StatsBotsSkipped`: Total bots skipped due to unsafe conditions (combat, flight, trading, casting).
* **CLI Query:** Accessible live in-game via `.blb stats` or `.botbrackets stats`.

---

## 4. Features Adapted & Altered for Turtle WoW Custom vMaNGOS

### 1. Level Limits & Brackets (Vanilla 1-60 vs WotLK 1-80)
* **WotLK Upstream:** Configured for levels 1 to 80 across 9 default brackets.
* **Turtle WoW Adaptation:**
  * Scaled to level 1-60 across 7 standard brackets:
    * **Range 1:** 1–9 (16%)
    * **Range 2:** 10–19 (14%)
    * **Range 3:** 20–29 (14%)
    * **Range 4:** 30–39 (14%)
    * **Range 5:** 40–49 (14%)
    * **Range 6:** 50–59 (14%)
    * **Range 7:** 60–60 (14%)
  * Dynamically supports any user-configured number of ranges ($N$ ranges).

### 2. Leveling Zone Coordinates
* **WotLK Upstream:** Included Outland bases (Hellfire, Zangarmarsh) and Northrend bases (Borean Tundra).
* **Turtle WoW Adaptation:**
  * **Alliance Leveling Hubs:**
    1. Levels 1–9: Goldshire, Elwynn Forest (Map 0: `-9464.0, 62.0, 56.0`)
    2. Levels 10–19: Sentinel Hill, Westfall (Map 0: `-10515.0, 1070.0, 55.0`)
    3. Levels 20–29: Lakeshire, Redridge Mountains (Map 0: `-9250.0, -2160.0, 64.0`)
    4. Levels 30–39: Rebel Camp, Northern Stranglethorn (Map 0: `-11340.0, -200.0, 75.0`)
    5. Levels 40–49: Feathermoon Stronghold, Feralas (Map 1: `-4380.0, 2060.0, 49.0`)
    6. Levels 50–59: Chillwind Camp, Western Plaguelands (Map 0: `940.0, -1450.0, 63.0`)
    7. Level 60: Stormwind City (Map 0: `-8810.0, 640.0, 94.0`)
  * **Horde Leveling Hubs:**
    1. Levels 1–9: Valley of Trials, Durotar (Map 1: `-618.0, -4251.0, 38.0`)
    2. Levels 10–19: Razor Hill, Durotar (Map 1: `310.0, -4680.0, 16.0`)
    3. Levels 20–29: Crossroads, The Barrens (Map 1: `-440.0, -2650.0, 96.0`)
    4. Levels 30–39: Tarren Mill, Hillsbrad Foothills (Map 0: `-24.0, -870.0, 55.0`)
    5. Levels 40–49: Camp Mojache, Feralas (Map 1: `-4400.0, 190.0, 25.0`)
    6. Levels 50–59: Un'Goro Crater Entrance, Kalimdor (Map 1: `-7180.0, -1070.0, -188.0`)
    7. Level 60: Orgrimmar (Map 1: `1570.0, -4400.0, 8.0`)

### 3. Capital City Hubs (Ambiance Sanctuaries)
* **WotLK Upstream:** Relied heavily on Dalaran (Map 571) as the primary sanctuary for levels 68–80.
* **Turtle WoW Adaptation:**
  * Uses all 8 canonical and custom racial capitals of Turtle WoW:
    * **Alliance:** Stormwind City (Map 0), Ironforge (Map 0), Darnassus (Map 1), and Alah'Thalas (Map 0, High Elf Capital).
    * **Horde:** Orgrimmar (Map 1), Undercity (Map 0), Thunder Bluff (Map 1), and Sparkwater Port (Map 1, Goblin Port/Capital).

### 4. Turtle WoW Arena & PvP Integration
* **WotLK Upstream:** Queried `sArenaTeamMgr->GetArenaTeamById(...)` for 2v2/3v3/5v5 team charters.
* **Turtle WoW Adaptation:**
  * Turtle WoW custom vMaNGOS implements the custom **Blood Ring Arena** (`BATTLEGROUND_BR = 4` on Map 26) and the **Gurubashi Arena** (`InGurubashiArena`), plus Sunnyglade Valley (`BATTLEGROUND_SV = 5` on Map 27).
  * `BotInArena(bot)` and `BotInArenaTeam(bot)` check `bot->InArena()`, `bot->InGurubashiArena(false)`, `bot->InBattleGround()`, `bot->InBattleGroundQueue()`, and maps 26/27.
  * Bots in arenas or battleground queues are safely excluded from level bracket reassignment and teleportation.

### 5. Dismount Method
* **WotLK Upstream:** Called `bot->Dismount()`.
* **Turtle WoW Adaptation:**
  * In `tortoise-wow` (`Unit.h` / `Player.h`), the dismount method is:
    ```cpp
    if (bot->IsMounted())
        bot->Unmount();
    ```
  * Abstracted cleanly inside [`BotEngineAdapter::TeleportBot`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets/src/BotEngineAdapter.h#L286-L298).

### 5. Chat & Console Administration
* **WotLK Upstream:** Registered commands via AzerothCore's `Acore::ChatCommands::ChatCommandTable`.
* **Turtle WoW Adaptation:**
  * In `tortoise-wow`, ChatCommand tables bind member function pointers to `ChatHandler`. Module-level pre-dispatch command interception is cleanly handled via [`AllCommandScript::CanExecuteCommand`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/tortoise-wow/src/game/ScriptObjects.h#L700-L730).
  * Supports both `.blb` and `.botbrackets` (or `.botlevelbrackets`) aliases.
  * Formatted with vMaNGOS `printf` style (`PSendSysMessage("%s", ...)`) rather than C++20 `fmt` style.

---

## 5. Features Incompatible with Vanilla (and Exclusion Rationale)

1. **Death Knight Hero Class Exception:**
   * *Upstream Behavior:* Checked `if (bot->getClass() == CLASS_DEATH_KNIGHT)` and clamped levels $\ge 55$.
   * *Reason for Exclusion:* Death Knights do not exist in Vanilla 1.12.1 or Turtle WoW 1.18.1. Retaining this check would introduce dead branches and unnecessary condition checks.
2. **Outland (Map 530) & Northrend (Map 571) Schedulers:**
   * *Upstream Behavior:* Dedicated WrongMap rules for maps 530 and 571.
   * *Reason for Exclusion:* These maps do not exist in the Vanilla client DBC and world database. Replaced with continental checks for maps 0 and 1.
3. **Expansion Starter Areas (Sunstrider Isle & Ammen Vale):**
   * *Upstream Behavior:* Starting-zone dispersal for Blood Elf and Draenei zones on Map 530.
   * *Reason for Exclusion:* Not applicable to classic Vanilla starter zones.
4. **Arena Team Protection (`sArenaTeamMgr`):**
   * *Upstream Behavior:* Iterated over `MAX_ARENA_SLOT` and queried `sArenaTeamMgr->GetArenaTeamById(...)`.
   * *Reason for Adaptation:* While standard 1.12.1 lacked arenas, Turtle WoW custom vMaNGOS features native arenas (Blood Ring Map 26, Gurubashi Arena). The option `BotLevelBrackets.IgnoreArenaTeamBots` is retained and enhanced to protect bots participating in Blood Ring (`bot->InArena()`), Gurubashi (`bot->InGurubashiArena()`), battlegrounds (`bot->InBattleGround()`), or queued for PvP (`bot->InBattleGroundQueue()`).

---

## 6. Architecture & Memory Safety Verification

1. **The Unediting Doctrine (Immutability):**
   * Neither `tortoise-wow` nor `TortoiseBots` was modified in any way.
   * Verified via `powershell tools/verify_core_immutability.ps1`: **100% Clean**.
2. **Database Memory Safety:**
   * Every `QueryResult*` returned from `CharacterDatabase.Query` is explicitly guarded and freed with `delete result;`, eliminating SQL query memory leaks.
3. **UTF-8 Encoding Without BOM:**
   * Verified that `conf/mod_twow_bot_level_brackets.conf.dist` is encoded in pure UTF-8 without BOM (`HasBOM == False`), guaranteeing ACE configuration stream compatibility.
4. **Static Linter Compliance:**
   * Verified via `python tools/audit_module_port.py mod-twow-bot-level-brackets`: **100% Passed**.
