# Bot Level Brackets: Operator & Configuration Guide (Option 1)

**Module:** `mod-twow-bot-level-brackets`  
**Architecture:** Option 1 — Brackets Orchestrates; Unchanged Playerbots Executes  
**Target Core:** Turtle WoW Custom vMaNGOS (`tortoise-wow` targeting client 1.18.1.7272 / 1.12.1)  
**Target Bot Engine:** `TortoiseBots` (native modular companion engine) & legacy CMaNGOS PlayerBots fallback  

---

## 1. Overview & Core Philosophy

The **Bot Level Brackets** module maintains a balanced, vibrant, and lore-friendly distribution of playerbots across the entire leveling spectrum of World of Warcraft.

Under **Option 1**:
- The native `Playerbots` engine (`TortoiseBots`) owns all gameplay tasks, route planning, mob combat, quest completion, and physical travel.
- The module acts as an external **scheduler and population observer**: identifying population deficits in racial capitals and the open world, selecting eligible bots, and requesting **already implemented native tasks** through verified public entry points (`CityVisitService::RequestVisit`).
- Zero teleports, zero character resets, zero synthetic XP, and zero custom strategy injections.
- Both core server (`tortoise-wow`) and bot engine (`TortoiseBots`) remain 100% pristine and unmodified.

---

## 2. How Option 1 Orchestration Operates

### 2.1 The Unified Scheduling Cycle

Every `BotLevelBrackets.Orchestration.CheckSeconds` (default: 90s), the module runs a unified scheduling pass:

1. **Census Snapshot**:
   Surveys online random bots and real players per faction. Counts physical residents in all 8 racial capital cities, confirmed inbound bots with accepted native travel, and pending module proposals.
2. **Quota & Shortfall Evaluation**:
   Computes feasible city targets using `QuotaAllocator`. Subtracts effective population (residents + confirmed inbound + pending reservations) from feasible target to determine actionable shortfall:
   $$\text{Shortfall} = \max(0, \text{FeasibleTarget} - (\text{Residents} + \text{Inbound} + \text{Pending}))$$
3. **Protection & Availability Filtering**:
   Only autonomous free bots are considered. Bots befriended by real players, in human guilds, grouped with real players, in active combat, or commanded by a player master are completely exempt.
4. **Verified Native Request Dispatch**:
   For cities with a shortfall, the coordinator requests a native city visit via `CityVisitService::Instance().RequestVisit(...)`.
   - If accepted: The native engine acquires an activity lease, initiates travel, and manages dwell time.
   - If refused (busy, protected, unsafe route): The module applies backoff cooldowns and backs off safely without forcing the action.
5. **Outdoor World & Field Reserve**:
   Guarantees that at least `FieldReservePct` (default: 70%) of bots remain active in the open world. Requests outdoor tasks (grinding, questing, exploration, flight discovery) through `PlayerbotAI::DoSpecificAction`.
6. **Level Bracket Progression (Advisory)**:
   Level bracket distribution across the 7 ranges (1-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60) is observed and reported. Missing supply is displayed as an advisory shortfall; the engine is never altered to artificially level or reset characters.

---

## 3. Configuration Reference

```ini
[BotLevelBrackets]
# Master switch
BotLevelBrackets.Enabled = 1

# Option 1 Mode
BotLevelBrackets.Orchestration.Mode = ExistingNativeTasks
BotLevelBrackets.Orchestration.DryRun = 0

# Cadence & Limits
BotLevelBrackets.Orchestration.CheckSeconds = 90
BotLevelBrackets.Orchestration.MaxRequestsPerCycle = 5
BotLevelBrackets.Orchestration.MaxOutstandingPerFaction = 10
BotLevelBrackets.Orchestration.MaxLooseRequestsPerCycle = 1
BotLevelBrackets.Orchestration.RequestCooldownSeconds = 1800
BotLevelBrackets.Orchestration.RejectionBackoffSeconds = 600

# City Orchestration
BotLevelBrackets.City.Enabled = 1
BotLevelBrackets.City.DestinationPolicy = RequireExact
BotLevelBrackets.City.QuotaMode = 2
BotLevelBrackets.City.DwellDurationMs = 180000

# Racial Capitals Allocation (Alliance: SW, IF, Darn, Alah; Horde: Org, UC, TB, Spark)
BotLevelBrackets.City.StormwindTarget = 7
BotLevelBrackets.City.IronforgeTarget = 5
BotLevelBrackets.City.DarnassusTarget = 4
BotLevelBrackets.City.AlahThalasTarget = 4

BotLevelBrackets.City.OrgrimmarTarget = 7
BotLevelBrackets.City.UndercityTarget = 5
BotLevelBrackets.City.ThunderBluffTarget = 4
BotLevelBrackets.City.SparkwaterTarget = 4

# Outdoor World & Field Reserve
BotLevelBrackets.World.Enabled = 1
BotLevelBrackets.World.DestinationPolicy = RequireExact
BotLevelBrackets.World.FieldReservePct = 70

# Social Protections
BotLevelBrackets.Protection.IgnoreFriendListed = 1
BotLevelBrackets.Protection.IgnoreRealPlayerGuilds = 1
BotLevelBrackets.Protection.IgnoreArenaTeams = 1
BotLevelBrackets.Protection.SocialRefreshSeconds = 300
```

---

## 4. In-Game Command Guide (`.blb`)

All commands require `SEC_MODERATOR` or `SEC_ADMINISTRATOR` permissions:

### General Inspection
* **`.blb status`**:
  Displays master status, active engine, total population, human counts, city/world orchestration states, and outstanding ledger requests.
* **`.blb cities` (or `.blb hubs`)**:
  Full tabular breakdown of all 8 racial capital cities, including physical residents, confirmed native inbound, pending module reservations, feasible targets, actionable shortfalls, and imbalance status.
* **`.blb world` (or `.blb regions`)**:
  Inspects open world field reserves and outdoor regional distribution.
* **`.blb brackets`**:
  Displays level distribution across brackets (1-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60), actual percentages, and advisory shortages.

### Activity & Ledger Telemetry
* **`.blb activities`**:
  Lists all 42 audited native activities, their support class (`RequestableExact`, `RequestableNativeChoice`, `LocalOnly`, `ObserveOnly`), enablement, weight, cooldown, and max outstanding limits.
* **`.blb ledger` (or `.blb pending`)**:
  Displays live entries in the RequestLedger (request ID, bot GUID, team, domain, destination, state, support class).
* **`.blb stats`**:
  Shows total scheduling cycles executed, total requests submitted, accepted, and rejected.

### Diagnostics & Control
* **`.blb explain <guid>`**:
  Runs a deep diagnostic check on a specific bot GUID (level, team, position, engine classification, free bot check, friend/guild protections, ready state, cooldown, active request).
* **`.blb explain city <0-7>`**:
  Detailed diagnostic analysis of a specific city's population metrics, targets, and shortfalls.
* **`.blb reload`**:
  Safely reloads configuration and refreshes protection caches without interrupting active native tasks.
* **`.blb force`**:
  Triggers an immediate bounded scheduling pass.
* **`.blb cleanup`**:
  Prunes stale ledger requests and cleans expired cooldowns.
* **`.blb debug [off|summary|trace|dryrun|simulate|clear]`**:
  Controls diagnostic verbosity, runs dry-run simulations, or clears the in-memory ring buffer.
