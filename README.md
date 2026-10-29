# Bot Level Brackets Extended for Turtle WoW

![Bot Level Brackets Banner](preview.jpg)

[![Turtle WoW](https://img.shields.io/badge/Turtle_WoW-1.18.1%20%2F%201.12.1-orange.svg)](https://turtle-wow.org)
[![Core](https://img.shields.io/badge/Core-tortoise--wow%20(vMaNGOS)-blue.svg)](https://github.com/tortoise-wow/tortoise-wow)
[![Bot Engine](https://img.shields.io/badge/Bot%20Engine-TortoiseBots%20%7C%20PlayerBots-brightgreen.svg)](https://github.com/Sagiroth/TortoiseBots)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)]()

A next-generation, non-intrusive bot opportunity coordinator, world presence coordinator, and natural progression observer engineered specifically for **Turtle WoW custom vMaNGOS (`tortoise-wow`)** and the **`TortoiseBots`** companion AI engine.

---

## 🌟 What Does the Extended Module Do?

In standard vanilla servers running companion AI bots, playerbots often clump unnaturally at level extremes (starting zones or level 60) or roam blindly without realistic daily schedules or regional context. Previous attempts at balancing relied on synthetic teleportation, direct level resets, and intrusive core hooks.

**Bot Level Brackets Extended** introduces **Option 2 Architecture: Registered Opportunities**:
1. **Registered AI Opportunities:** Hooks into `TortoiseBots` via `RegisterAiContextAugmenter()` with fresh named contexts per bot. Bots autonomously evaluate opportunities using native `ChooseTravelTargetAction`, `TravelMgr`, and RPG target selection without artificial teleportation.
2. **43 Native Bot Activities:** Incorporates all 43 native bot activities across Capital Cities and Outdoor World domains—including AH trading, banking, tavern rest, class training, racial visits, hunting, gathering, grinding, dungeon runs, and Turtle WoW custom exploration.
3. **8 Capital City Hubs with Custom Turtle WoW Lore:** Manages dynamic ambiance across Stormwind, Ironforge, Darnassus, **Alah'Thalas (High Elf)**, Orgrimmar, Undercity, Thunder Bluff, and **Sparkwater Port (Goblin)**.
4. **Natural Progression & Level Policy:** Observes native bot progression (`ObserveNative`). Does not forcefully mutate levels or grant synthetic XP. Level brackets provide advisory census targets using largest-remainder quota allocation.
5. **Option 3 Restricted Fallback:** A safety net disabled by default (`Fallback.NativeRouteUnsupportedBeforeStart = 0`). Only activates for verified unreachable capital destinations after preflight validation, with strict landing point verification and 1-hour anti-churn throttles.
6. **Field Reserve Bound:** Guarantees that at least 70% of bots remain in the open world, capping total city resident quotas to 30% of the active bot population.
7. **Comprehensive Social Protection:** Bots grouped with humans, befriended by humans (`character_social`), in guilds with human members (online or offline), or fighting in custom arenas (Blood Ring Map 26, Sunnyglade Map 27, Gurubashi) are strictly protected.
8. **Module-Local Debug Environment:** In-memory circular ring buffer (default 500 events), 20 categories, rate limiting, JSONL dump capability, and live diagnostic commands (`.blb explain bot`, `.blb explain city`, `.blb explain region`).

---

## 🚀 Architectural Invariants

* **The Unediting Doctrine:** Zero modifications or patches to `tortoise-wow` or `TortoiseBots`. All AI contexts, triggers, actions, and state trackers exist purely within this standalone module.
* **Disjoint Set Census:** Online bots are strictly categorized into disjoint sets:
  $$\text{TotalOnline} = \text{Residents} + \text{ConfirmedInbound} + \text{PendingProposed} + \text{Unmanaged}$$
* **Hysteresis & Anti-Churn:** Opportunities are only proposed when an actionable deficit exists ($\text{Actual} + \text{Inbound} < \text{Target}$). Bots maintain a visit history ring buffer with minimum cooldowns between city and world visits.

---

## 🛠️ In-Game Administration (`.blb` Commands)

The module provides an extensive diagnostic and administration suite:

| Command | Security | Description |
|:---|:---:|:---|
| `.blb status` | Admin | Shows master state, online bot census, city resident counts, and active opportunities. |
| `.blb reload` | Admin | Atomically reloads `mod_twow_bot_level_brackets.conf` without server reboot. |
| `.blb ledger` | Admin | Displays the Opportunity Ledger summary, active intents, and intent state counts. |
| `.blb opportunities` | Admin | Lists active bot intents with destination, activity, and time-in-state. |
| `.blb activities` | Admin | Lists all 43 registered native activities, domain flags, and current weights. |
| `.blb debug mode <mode>` | Admin | Sets debug mode: `Off`, `Summary`, `Trace`, `DryRun`. |
| `.blb debug dump [count]` | Admin | Dumps recent ring buffer events to chat or server log. |
| `.blb explain bot <name>` | Admin | Explains why a specific bot was or was not selected for opportunities. |
| `.blb explain city <name>` | Admin | Displays detailed census, target quota, inbound bots, and deficit for a capital city. |
| `.blb explain region <id>` | Admin | Displays census, level envelope, and active activities for an outdoor region. |
| `.blb force cycle` | Admin | Manually triggers an immediate opportunity scheduling cycle. |

---

## ⚙️ Configuration Reference

Edit `conf/mod_twow_bot_level_brackets.conf` (copied from `conf/mod_twow_bot_level_brackets.conf.dist`):

* `BotLevelBrackets.Enabled`: Master module enable switch (`1` = active, `0` = disabled).
* `BotLevelBrackets.HubPopulation.Enabled`: Enables capital city ambiance coordination.
* `BotLevelBrackets.HubPopulation.FieldReserveFraction`: Fraction of bots reserved for open world (default `0.70`).
* `BotLevelBrackets.WorldOpportunity.Enabled`: Enables open-world regional opportunities.
* `BotLevelBrackets.Fallback.NativeRouteUnsupportedBeforeStart`: Option 3 restricted fallback (`0` = disabled, `1` = enabled).
* `BotLevelBrackets.Debug.Mode`: Diagnostic logging level (`Off`, `Summary`, `Trace`, `DryRun`).
* `BotLevelBrackets.Debug.RingBufferSize`: In-memory diagnostic ring buffer capacity (default `500`).
* `BotLevelBrackets.Activity.<Name>.Weight`: Individual weight configuration for each of the 43 native activities.

---

## 📚 Detailed Documentation

Detailed architectural and operational documentation is located in [`docs/`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets-extended/docs/):
* [**`ARCHITECTURE.md`**](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets-extended/docs/ARCHITECTURE.md): Full technical architecture, state machines, and mathematical formulas.
* [**`GUIDE.md`**](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets-extended/docs/GUIDE.md): Complete operator manual and configuration guide.
* [**`ACTIVITIES.md`**](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets-extended/docs/ACTIVITIES.md): Exhaustive index of all 43 native activities with preconditions and defaults.
* [**`DEBUG_ENVIRONMENT.md`**](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets-extended/docs/DEBUG_ENVIRONMENT.md): Diagnostic environment, categories, and JSONL log analysis.
* [**`MIGRATION_GUIDE.md`**](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets-extended/docs/MIGRATION_GUIDE.md): Step-by-step migration from legacy redistribution to Option 2.
