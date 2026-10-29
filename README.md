# Bot Level Brackets for Turtle WoW (Option 1 Native-Task Orchestrator)

![Bot Level Brackets Banner](preview.jpg)

[![Turtle WoW](https://img.shields.io/badge/Turtle_WoW-1.18.1%20%2F%201.12.1-orange.svg)](https://turtle-wow.org)
[![Core](https://img.shields.io/badge/Core-tortoise--wow%20(vMaNGOS)-blue.svg)](https://github.com/tortoise-wow/tortoise-wow)
[![Bot Engine](https://img.shields.io/badge/Bot%20Engine-TortoiseBots%20%7C%20PlayerBots-brightgreen.svg)](https://github.com/Sagiroth/TortoiseBots)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)]()

An intelligent, non-intrusive population orchestrator engineered for **Turtle WoW custom vMaNGOS (`tortoise-wow`)** and the **`TortoiseBots`** companion AI engine.

Operating strictly under **Option 1**: Brackets observes population, identifies unmet goals, selects suitable available bots, and requests **already implemented native tasks** through verified safe entry points (`CityVisitService`, `DoSpecificAction`). Playerbots owns all gameplay execution, movement, and routes. Core server and bot engine sources remain 100% pristine and unmodified.

---

## 🌟 Key Architecture & Option 1 Highlights

1. **Brackets Orchestrates; Unchanged Playerbots Executes:**
   The module never teaches bots new behaviors or injects custom AI strategies. Instead, it queries native services to determine where bots are needed and requests genuine existing tasks (`CityVisitService::RequestVisit`).
2. **Zero Core & Engine Edits (The Unediting Doctrine):**
   `tortoise-wow` and `TortoiseBots` are strictly read-only. All logic resides entirely within standalone module space.
3. **Zero Teleports & Zero Character Resets:**
   No forced `Player::TeleportTo` calls, no factory re-provisioning, and no artificial level resets. Native bot travel handles all physical movement organically. Level shortages are reported as **Advisory**.
4. **Honest Capability Reporting:**
   Every task is categorized by its actual native capability (`RequestableExact`, `RequestableNativeChoice`, `LocalOnly`, `ObserveOnly`, `Unavailable`). Where exact control is unsupported, the module truthfully reports shortages rather than attempting workarounds.
5. **8 Racial Capital City Hubs:**
   Full support for classic and custom Turtle WoW racial capitals:
   - **Alliance:** Stormwind, Ironforge, Darnassus, and Alah'Thalas (High Elf capital).
   - **Horde:** Orgrimmar, Undercity, Thunder Bluff, and Sparkwater Port (Goblin capital).
6. **42 Audited Native Activities:**
   Typed registry and individual configuration controls for questing, grinding, exploration, vendor service, auction house, repairs, training, and gathering.
7. **Multi-Tier Social Protections:**
   Safeguards bots friended by real players, in human guilds (online or offline), grouped with humans, in combat, or commanded as masters. Fail-closed on database errors.
8. **In-Game Administration & Telemetry:**
   Full `.blb` command suite (`status`, `cities`, `world`, `brackets`, `activities`, `ledger`, `stats`, `explain`, `reload`, `force`, `cleanup`, `debug`).

---

## 🛠️ Installation & Build Setup

### 1. Module Placement
Place or clone this module repository into your `modules` directory alongside `tortoise-wow` and `TortoiseBots`:

```bash
cd /path/to/tortoise-wow/modules/
git clone https://github.com/Ildourol/mod-twow-bot-level-brackets.git
```

### 2. Configuration Hook
The build system automatically detects `mod-twow-bot-level-brackets.cmake` and configures include paths for `TortoiseBots`.

### 3. Database Migration
Apply the character database migration for persistent real-player guild tracking:

```bash
mysql -u root -p tw_char < modules/mod-twow-bot-level-brackets/data/sql/character/2026_09_01_bot_level_brackets_guild_tracker.sql
```

*(Note: `.blb` commands are registered natively via `AllCommandScript`, requiring zero world database modifications).*

### 4. Module Configuration
Copy the configuration template into your server's module config directory:

```bash
cp modules/mod-twow-bot-level-brackets/conf/mod_twow_bot_level_brackets.conf.dist Server/etc/modules/mod_twow_bot_level_brackets.conf
```

---

## ⚙️ Configuration Quick View

```ini
[BotLevelBrackets]
# Master Enable switch
BotLevelBrackets.Enabled = 1

# Orchestration Mode (Option 1 strictly requires ExistingNativeTasks)
BotLevelBrackets.Orchestration.Mode = ExistingNativeTasks

# Scheduling Cadence & Limits
BotLevelBrackets.Orchestration.CheckSeconds = 90
BotLevelBrackets.Orchestration.MaxRequestsPerCycle = 5
BotLevelBrackets.Orchestration.MaxOutstandingPerFaction = 10

# City Orchestration
BotLevelBrackets.City.Enabled = 1
BotLevelBrackets.City.DestinationPolicy = RequireExact
BotLevelBrackets.City.QuotaMode = 2
BotLevelBrackets.City.DwellDurationMs = 180000

# Outdoor World Distribution
BotLevelBrackets.World.Enabled = 1
BotLevelBrackets.World.DestinationPolicy = RequireExact
BotLevelBrackets.World.FieldReservePct = 70

# Social Protections
BotLevelBrackets.Protection.IgnoreFriendListed = 1
BotLevelBrackets.Protection.IgnoreRealPlayerGuilds = 1
BotLevelBrackets.Protection.IgnoreArenaTeams = 1
```

---

## 🎮 In-Game Command Reference (`.blb`)

All commands require `SEC_MODERATOR` or `SEC_ADMINISTRATOR` permissions:

| Command | Description |
|:---|:---|
| `.blb status` | Global overview of module state, active engine, census totals, and active dispatches. |
| `.blb cities` (or `.blb hubs`) | Tabular display of the 8 racial capitals, residents, inbound, pending, and shortfalls. |
| `.blb world` (or `.blb regions`) | Outdoor world distribution, field reserves, and regional catalog. |
| `.blb brackets` | Level bracket distribution, actual vs desired percentages, and advisory shortages. |
| `.blb activities` | Complete inventory of all 42 audited native activities and their support classes. |
| `.blb ledger` (or `.blb pending`)| Live inspect of pending, submitted, and accepted task requests. |
| `.blb stats` | Cumulative coordinator metrics (cycles executed, requests submitted/accepted/rejected). |
| `.blb explain <guid>` | In-depth diagnostic report for an individual bot (eligibility, lease, cooldown, ledger). |
| `.blb explain city <0-7>` | Deep analysis of a specific city's population, target calculation, and shortfalls. |
| `.blb reload` | Safely reloads configuration and refreshes caches without interrupting active native tasks. |
| `.blb force` | Triggers an immediate bounded scheduling pass. |
| `.blb cleanup` | Prunes stale ledger requests and cleans expired cooldowns. |
| `.blb debug [off|summary|trace|dryrun|simulate]` | Configures diagnostic verbosity or executes an offline simulation. |
