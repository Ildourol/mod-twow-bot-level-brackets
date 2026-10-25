# Bot Level Brackets for Turtle WoW

![Bot Level Brackets Banner](preview.jpg)

[![Turtle WoW](https://img.shields.io/badge/Turtle_WoW-1.18.1%20%2F%201.12.1-orange.svg)](https://turtle-wow.org)
[![Core](https://img.shields.io/badge/Core-tortoise--wow%20(vMaNGOS)-blue.svg)](https://github.com/tortoise-wow/tortoise-wow)
[![Bot Engine](https://img.shields.io/badge/Bot%20Engine-TortoiseBots%20%7C%20PlayerBots-brightgreen.svg)](https://github.com/Sagiroth/TortoiseBots)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)]()

A dynamic, intelligent, and lore-friendly bot level balancing and world presence module engineered for **Turtle WoW custom vMaNGOS (`tortoise-wow`)** and the **`TortoiseBots`** companion AI engine.

---

## 🌟 What Does This Module Do?

In standard World of Warcraft servers running companion bot engines, playerbots naturally clump at extreme ends—either stagnating at starting levels or clustering at level 60. This leaves the leveling world feeling deserted, disrupts dungeon finding, and empties mid-level zones.

**Bot Level Brackets** solves this completely:
1. **Automated Population Balancing:** Continuously surveys online random bots and reallocates bots from over-populated level brackets to under-populated ones according to customizable target quotas.
2. **Dynamic Real-Player Following:** Automatically detects where real human players are leveling and dynamically shifts bot distributions toward those brackets, ensuring leveling players always have active companions and opponents.
3. **Turtle WoW Custom Race & City Integration:** Fully supports Turtle WoW's custom playable races (**High Elves** and **Goblins**), populating regional capitals (**Alah'Thalas** and **Sparkwater Port**) alongside the 6 classic capitals, and routing young bots to native starting valleys (**Thalassian Highlands** and **Blackstone Island**).
4. **Blood Ring Arena & PvP Protection:** Protects bots engaged in the custom **Blood Ring Arena** (Map 26), **Gurubashi Arena**, **Sunnyglade Valley** (Map 27), and active Battleground queues from sudden level changes or disruptive teleports.
5. **Real-Player Group & Guild Protection:** Bots befriended by real players, in guilds with human members (online or offline), grouped with humans, or commanded as masters are strictly exempt from redistribution.
6. **World Presence & Capital Ambiance:** Populates major capital cities with natural bot ambiance and disperses over-leveled or stranded bots back to appropriate open-world questing hubs.

---

## 🚀 Key Features

* **Multi-Engine Portability:** Compile-time auto-discovery seamlessly adapts to modern modular **`TortoiseBots`**, legacy vendored PlayerBots (`sRandomPlayerbotMgr`), or core headless bot sessions.
* **8 Racial Capital City Hubs:** Ambiance quota management for Stormwind, Ironforge, Darnassus, Alah'Thalas (High Elf), Orgrimmar, Undercity, Thunder Bluff, and Sparkwater Port (Goblin).
* **Native Racial Starter Routing:** Level 1–9 bots are intelligently transported to their native racial starting valleys rather than generic dumping grounds.
* **Persistent Recovery Across Reboots:** Pending level adjustments are stored in MySQL/MariaDB (`bot_level_brackets_pending_resets`), ensuring saved resets wait through logout/restart until the bot returns, expires or becomes ineligible.
* **Safe State Verification:** Bots in combat, casting spells, flying on taxis, trading, looting, dead, or logging out are never interrupted.
* **Churn Dampening:** Threshold dampening suppresses redistribution oscillations in near-balanced brackets.
* **In-Game Administration Suite:** Complete `.blb` and `.botbrackets` CLI with live status reporting, on-the-fly config reloading, queue telemetry, and cumulative stats.

---

## 🛠️ Installation

### 1. Module Placement
Clone or place this repository into your `modules` directory alongside `tortoise-wow` and `TortoiseBots`:

```bash
cd /path/to/tortoise-wow/modules/
git clone https://github.com/Ildourol/mod-twow-bot-level-brackets.git
```

### 2. Build Configuration
Run CMake to configure the server (CMake automatically discovers `TortoiseBots` and enables native integration):

```bash
cd /path/to/tortoise-wow/build/
cmake .. -DMODULES=static
cmake --build . --config Release
```

### 3. Database Schema Import
Apply the non-intrusive character database migrations to your character DB (`tw_char`):

```bash
mysql -u root -p tw_char < modules/mod-twow-bot-level-brackets/data/sql/character/2026_09_01_bot_level_brackets_guild_tracker.sql
mysql -u root -p tw_char < modules/mod-twow-bot-level-brackets/data/sql/character/2026_10_22_bot_level_brackets_pending_resets.sql
```

*(Note: Turtle WoW / vMaNGOS implements commands natively in C++ via `AllCommandScript`, so no world database tables or SQL imports are required for `.blb` commands).*

### 4. Configuration
Copy the configuration template to your server's module configuration directory:

```bash
cp modules/mod-twow-bot-level-brackets/conf/mod_twow_bot_level_brackets.conf.dist Server/etc/modules/mod_twow_bot_level_brackets.conf
```

---

## ⚙️ Configuration Quick View

Edit `mod_twow_bot_level_brackets.conf` to customize brackets and timing:

```ini
[BotLevelBrackets]
# Enable the module
BotLevelBrackets.Enabled = 1

# Check frequency in seconds (default: 5 minutes)
BotLevelBrackets.CheckFrequency = 300

# Level Bracket Definitions (Alliance & Horde)
BotLevelBrackets.NumRanges = 7
BotLevelBrackets.Alliance.Range1.Lower = 1
BotLevelBrackets.Alliance.Range1.Upper = 9
BotLevelBrackets.Alliance.Range1.Pct = 16
BotLevelBrackets.Horde.Range1.Lower = 1
BotLevelBrackets.Horde.Range1.Upper = 9
BotLevelBrackets.Horde.Range1.Pct = 16
# Configure Range2 through Range7 with the numbered keys in the full template.

# Dynamic Real-Player Following (0 = disabled, 1 = enabled)
BotLevelBrackets.Dynamic.UseDynamicDistribution = 0
BotLevelBrackets.Dynamic.RealPlayerWeight = 1.0

# Protection Rules
BotLevelBrackets.IgnoreFriendListed = 1
BotLevelBrackets.IgnoreGuildBotsWithRealPlayers = 1
BotLevelBrackets.IgnoreArenaTeamBots = 1

# Teleportation & World Presences
BotLevelBrackets.TeleportOnLevelChange = 1
BotLevelBrackets.Teleport.ExecutionLimit = 25
BotLevelBrackets.Teleport.MaxAttempts = 3
BotLevelBrackets.HubDisperse.Enabled = 1
BotLevelBrackets.HubDisperse.QuotaMode = 2
BotLevelBrackets.HubDisperse.MinBotsLimitEnabled = 0
BotLevelBrackets.HubDisperse.MaxBotsLimitEnabled = 1
BotLevelBrackets.HubDisperse.LimitsMode = 1
BotLevelBrackets.HubDisperse.MinBotsPerHub = 0
BotLevelBrackets.HubDisperse.MaxBotsPerHub = 50
BotLevelBrackets.HubDisperse.HysteresisBots = 2
BotLevelBrackets.HubDisperse.StabilitySeconds = 60
BotLevelBrackets.HubDisperse.CooldownSeconds = 180

# QuotaMode 1: city Pct settings are absolute bot headcounts (0-500).
# QuotaMode 2: city Pct settings are faction percentages (0-100).
# LimitsMode 1: Min/Max bounds are absolute bot counts (e.g. Min 10 bots, Max 50 bots).
# LimitsMode 2: Min/Max bounds are percentages of online faction bots (e.g. Min 5%, Max 25%).
# MinBotsLimitEnabled / MaxBotsLimitEnabled independently toggle the floor and ceiling.
# (Legacy upgrade notice: If using legacy 'PopulationLimitsEnabled', it automatically falls
# back to controlling both toggles until updated).
# Example: StormwindPct 20, Min 10, Max 50, QuotaMode 1, LimitsMode 1 -> targets 20 bots.

# Capital City Ambiance & Open-World Strategy Management
BotLevelBrackets.HubDisperse.ManageCityStrategies = 1
BotLevelBrackets.HubDisperse.CityStrategies = "+rpg,+rpg quest,+rpg vendor,+rpg explore,+rpg maintenance,+rpg craft,+emote,+sit,+custom::say"
BotLevelBrackets.HubDisperse.CityStripStrategies = "-grind,-wander,-loot,-tfish"
BotLevelBrackets.WorldTeleport.ResetStrategies = 1
```

📖 **For an exhaustive, setting-by-setting reference and tuning examples, read the [Operator & Configuration Guide](docs/GUIDE.md).**

---

## 🎮 In-Game Commands

Game Masters and Administrators can manage the module using `.blb <command>` or `.botbrackets <command>`:

| Command | Security | Description |
| :--- | :---: | :--- |
| `.blb status` | Moderator (1) | Shows active engine name, running status, hub quotas, safeguard settings, and detailed tabular bracket allocations. |
| `.blb reload` | Administrator (3) | Hot-reloads configuration files, resets stability timers, and recomputes target quotas without server restart. |
| `.blb force` | Administrator (3) | Immediately triggers a full distribution cycle across all online bots. |
| `.blb pending` | Moderator (1) | Displays current pending reset and teleport queue depths, TTL parameters, and entry ages. |
| `.blb cleanup` | Administrator (3) | Purges guilds from the persistent tracker that no longer contain human players. |
| `.blb stats` | Moderator (1) | Displays cumulative counters since boot: cycles run, bots adjusted, teleports, skips, and cooldown skips. |
| `.blb hubs` | Moderator (1) | Displays real-time capital hub diagnostics: targets, effective counts, imbalance states, stability durations, and action status. |

---

## 📚 Documentation

* 📖 **[Operator & Configuration Guide](docs/GUIDE.md)**: In-depth manual covering architecture, mathematical allocation formulas, safe state matrix, every configuration knob, and troubleshooting.

---

## 🛡️ License

This project is licensed under the **GNU General Public License v2 (GPL-2.0)**.



## Audit hardening and regression checks

Hub evacuation now leaves managed cities, including at level 60. Queued actions recheck protections, saved resets survive offline periods, and in-game admin commands execute on the world thread. City strategy changes have ownership tracking and are restored without broad AI engine resets. Population diagnostics share the scheduler's census, and Min/Max still bound each city's configured target in both quota modes.

Read [Audit fixes and validation](docs/AUDIT_FIXES.md) for operator details, compatibility references and remaining live-server checks.

Run the deterministic checks with Python 3 and g++ (C++17):

```bash
python3 tests/test_hub_population.py
python3 tests/test_module_behavior.py
python3 tests/test_config_defaults.py
```

The module behavior suite compiles the actual module and adapter with host doubles in native TortoiseBots, legacy PlayerBots and generic headless modes. It does not replace a build against the real server or in-game validation.
