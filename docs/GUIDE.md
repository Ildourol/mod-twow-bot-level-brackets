# Bot Level Brackets Extended: Operator & Configuration Guide

**Module:** `mod-twow-bot-level-brackets-extended`  
**Target Core:** Turtle WoW Custom vMaNGOS (`tortoise-wow` targeting client 1.18.1.7272 / 1.12.1)  
**Target Bot Engine:** `TortoiseBots` (native modular companion engine)  
**Architecture:** Option 2: Registered Opportunities via `RegisterAiContextAugmenter`  

---

## 1. Overview & Core Philosophy

The **Bot Level Brackets Extended** module coordinates dynamic world presence, capital city ambiance, and natural leveling bracket observation for autonomous playerbots.

Rather than forcing unnatural level resets or synthetic teleports, the Extended module implements **Option 2: Registered Opportunities**:
* **Autonomous Scheduling:** The `OpportunityCoordinator` proposes opportunities to bots whose current level and status match destination deficits.
* **Native AI Travel Execution:** Bots receive custom AI contexts (`BracketsStrategyContext`, `BracketsActionContext`) registered directly with `TortoiseBots`. Bots navigate natively using `ChooseTravelTargetAction` and `TravelMgr`.
* **Lore-Friendly Capital Ambiance:** Populates classic capitals and Turtle WoW custom capitals (**Alah'Thalas** for High Elves, **Sparkwater Port** for Goblins) based on configurable resident quotas.
* **Field Reserve Guarantee:** Protects the open world by reserving at least 70% of bots for leveling zones.
* **Option 3 Restricted Fallback:** Emergency relocation fallback is strictly disabled by default (`Fallback.NativeRouteUnsupportedBeforeStart = 0`). Only activates for verified unreachable capital destinations with safe landing coordinates.

---

## 2. In-Game Administration CLI (`.blb` Commands)

The module registers administrative commands under the `.blb` and `.botbrackets` namespaces (Security Level 3 / Admin).

### 2.1 Inspection & Status
* **`.blb status`**:
  Prints overall module health, active debug mode, total online bots, city resident census, and active opportunity counts.
* **`.blb ledger`**:
  Displays a summary of the `OpportunityLedger`, including active intents, state distribution (`PROPOSED`, `PLANNING`, `TRAVELING`, `ARRIVED`), and terminal counters.
* **`.blb opportunities`**:
  Lists all currently active bot opportunity intents, displaying bot name, domain, destination, assigned activity, and elapsed duration.
* **`.blb activities`**:
  Dumps the full catalog of 43 registered native activities, showing domain bitmasks, native purpose flags, and active weights.

### 2.2 Deep Diagnostic Explanations
* **`.blb explain bot <bot_name>`**:
  Inspects a specific bot and explains why it was selected or rejected for opportunities (checks social protection, guild membership, current state, visit cooldowns, and active intent).
* **`.blb explain city <city_name>`**:
  Displays detailed census information for a capital city (Stormwind, Ironforge, Darnassus, Alah'Thalas, Orgrimmar, Undercity, Thunder Bluff, Sparkwater Port), showing actual residents, confirmed inbound, target quota, and calculated deficit.
* **`.blb explain region <region_id>`**:
  Displays detailed census for an outdoor leveling region, showing active bot count, safe level envelopes, and enabled activities.

### 2.3 Debug & Maintenance Controls
* **`.blb debug mode <Off|Summary|Trace|DryRun>`**:
  Changes the debug logging mode on the fly without server restart.
* **`.blb debug dump [count]`**:
  Dumps the most recent entries from the diagnostic circular ring buffer (default 50 entries, up to buffer capacity).
* **`.blb force cycle`**:
  Manually triggers an immediate evaluation cycle in the `OpportunityCoordinator`.
* **`.blb reload`**:
  Atomically reloads configuration parameters from `mod_twow_bot_level_brackets.conf`.

---

## 3. Configuration Reference (`mod_twow_bot_level_brackets.conf`)

### 3.1 Master Switches & Timers
```ini
[BotLevelBrackets]
# Master enable switch (1 = enabled, 0 = disabled)
BotLevelBrackets.Enabled = 1

# Master distribution & opportunity scheduling cycle frequency in seconds (default: 300)
BotLevelBrackets.CheckFrequency = 300

# Periodic maintenance tick frequency in milliseconds (default: 5000)
BotLevelBrackets.MaintenanceTickMs = 5000

# Time-To-Live in seconds for proposed opportunities before expiration (default: 180)
BotLevelBrackets.OpportunityTTLSeconds = 180

# Minimum cooldown in seconds before a bot can return to the same destination (default: 1800)
BotLevelBrackets.OpportunityCooldownSeconds = 180
```

### 3.2 Capital City Ambiance & Field Reserve
```ini
# Enable capital city ambiance population
BotLevelBrackets.HubPopulation.Enabled = 1

# Minimum fraction of bots strictly reserved for the outdoor world (default: 0.70)
BotLevelBrackets.HubPopulation.FieldReserveFraction = 0.70

# Capital city resident target quotas (Alliance)
BotLevelBrackets.HubPopulation.Alliance.Stormwind = 15
BotLevelBrackets.HubPopulation.Alliance.Ironforge = 12
BotLevelBrackets.HubPopulation.Alliance.Darnassus = 8
BotLevelBrackets.HubPopulation.Alliance.AlahThalas = 8

# Capital city resident target quotas (Horde)
BotLevelBrackets.HubPopulation.Horde.Orgrimmar = 15
BotLevelBrackets.HubPopulation.Horde.Undercity = 12
BotLevelBrackets.HubPopulation.Horde.ThunderBluff = 8
BotLevelBrackets.HubPopulation.Horde.Sparkwater = 8
```

### 3.3 Option 3 Restricted Relocation Fallback
```ini
# Option 3 restricted fallback (0 = disabled, 1 = enabled)
# Emergency fallback for verified unreachable capital destinations
BotLevelBrackets.Fallback.NativeRouteUnsupportedBeforeStart = 0

# Maximum emergency relocations permitted per hour (default: 5)
BotLevelBrackets.Fallback.MaxRelocationsPerHour = 5

# Anti-churn cooldown in seconds per bot after fallback relocation (default: 3600)
BotLevelBrackets.Fallback.RelocationCooldownSeconds = 3600

# Early departure detection threshold in seconds (default: 300)
BotLevelBrackets.Fallback.EarlyDepartureThresholdSeconds = 300

# Penalized backoff duration in seconds for bots leaving prematurely (default: 7200)
BotLevelBrackets.Fallback.EarlyDepartureBackoffSeconds = 7200
```

### 3.4 Diagnostic Logging & Ring Buffer
```ini
# Debug mode: Off, Summary, Trace, DryRun (default: Off)
BotLevelBrackets.Debug.Mode = Off

# Size of in-memory circular ring buffer for events (default: 500)
BotLevelBrackets.Debug.RingBufferSize = 500

# Token bucket rate limiter: maximum log events per second (default: 10)
BotLevelBrackets.Debug.RateLimitPerSecond = 10

# Maximum events exported per dump command (default: 100)
BotLevelBrackets.Debug.DumpExportLimit = 100
```

### 3.5 Native Activity Weights
Each of the 43 native activities can be tuned individually:
```ini
BotLevelBrackets.Activity.AuctionBrowse.Weight = 1.0
BotLevelBrackets.Activity.BankDeposit.Weight = 1.0
BotLevelBrackets.Activity.TavernRest.Weight = 1.5
BotLevelBrackets.Activity.ClassTrainer.Weight = 2.0
BotLevelBrackets.Activity.GrindMob.Weight = 3.0
BotLevelBrackets.Activity.GatherHerb.Weight = 1.5
BotLevelBrackets.Activity.DungeonRun.Weight = 1.0
BotLevelBrackets.Activity.ExploreTurtleZone.Weight = 1.5
```
*(See `docs/ACTIVITIES.md` for the full reference of all 43 activities)*
