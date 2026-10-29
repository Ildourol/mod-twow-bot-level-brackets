# Diagnostic Debug Environment Specification

**Module:** `mod-twow-bot-level-brackets-extended`  
**Observability Suite:** Circular Ring Buffer, 20 Discrete Categories, Token-Bucket Rate Limiter, JSONL Telemetry

---

## 1. Overview & Non-Intrusive Design

The **Bot Level Brackets Extended** debug architecture is engineered for live production environments. It guarantees zero impact on tick latency during normal operation while providing comprehensive diagnostic introspection when troubleshooting bot distributions, candidate selection, or fallback events.

### 1.1 Invariants
* **Zero Allocations in Hot Paths**: Log entries are formatted directly into fixed-size stack buffers before being copied into the circular ring buffer.
* **Bounded In-Memory Footprint**: The ring buffer operates with a fixed maximum capacity (default: 500 events, configurable via `BotLevelBrackets.Debug.RingBufferSize`), overwriting oldest events when full.
* **Token Bucket Rate Limiting**: Emits at most $N$ logs per second (default: 10/sec) to the server log (`sLog.outString`), suppressing spam during burst cycles.

---

## 2. Operating Modes (`BotLevelBrackets.Debug.Mode`)

The debug mode can be set in the configuration file or toggled live via `.blb debug mode <mode>`:

| Mode | Overhead | Behavior & Output |
|:---|:---:|:---|
| **`Off`** (Default) | Negligible | Ring buffer captures high-severity events only (warnings/errors). No general logging output. |
| **`Summary`** | Minimal | Emits high-level cycle metrics every 300 seconds (total online bots, city deficits, proposed count, arrival rate). |
| **`Trace`** | Moderate | Logs fine-grained candidate evaluations, intent transitions (`PROPOSED` -> `PLANNING` -> `TRAVELING`), and AI trigger activations. |
| **`DryRun`** | Non-disruptive | Runs the complete census and candidate selection pipeline, logging what would be scheduled without creating active intents in the ledger. |

---

## 3. The 20 Diagnostic Categories

All log entries are tagged with one of 20 discrete categories, enabling granular filtering and analysis:

| Index | Category Key | Description |
|:---:|:---|:---|
| 0 | `General` | General module lifecycle events (startup, shutdown, reload). |
| 1 | `Coordinator` | Master distribution cycle initiation and execution. |
| 2 | `Candidate` | Bot candidate scoring, filtering, and selection. |
| 3 | `Ledger` | Opportunity intent creation, transition, and release. |
| 4 | `Census` | Disjoint population counting and field reserve checks. |
| 5 | `CityAmbiance` | Capital city resident quotas, arrivals, and departures. |
| 6 | `WorldOpportunity`| Outdoor regional opportunities and area envelope matching. |
| 7 | `Activity` | Native activity selection lottery and weights. |
| 8 | `Fallback` | Option 3 emergency relocation fallback checks and execution. |
| 9 | `AiContext` | Context augmentation registration and Strategy/Action execution. |
| 10 | `Trigger` | AI trigger evaluations (`CityOpportunityTrigger`, `WorldOpportunityTrigger`). |
| 11 | `Action` | AI action execution (`CityOpportunityAction`, `WorldOpportunityAction`). |
| 12 | `Social` | Real-player friend list and guild member safety exclusions. |
| 13 | `PvPSafety` | Arena (Blood Ring, Gurubashi) and BG queue protection. |
| 14 | `Travel` | Native travel target creation and flight plan validation. |
| 15 | `Landing` | Destination coordinate checks and landing zone verification. |
| 16 | `Hysteresis` | Actionable deficit damping and churn prevention. |
| 17 | `Cooldown` | Destination and activity visit cooldown enforcement. |
| 18 | `Config` | Configuration staging, generation tracking, and validation. |
| 19 | `Command` | In-game `.blb` administrative CLI dispatch. |

---

## 4. In-Game Diagnostic Inspection (`.blb`)

Operators can query the debug environment directly in-game:

### 4.1 `.blb debug dump [count]`
Dumps the most recent $N$ events from the circular ring buffer. Output is formatted with precise timestamps, category tags, and context messages:

```text
[BLB DEBUG DUMP] (Last 3 entries):
[10:14:02.124] [Census] Online=120, Residents=28, Inbound=6, FieldReserve=86 (71.6%)
[10:14:02.128] [Coordinator] Stormwind deficit=4, proposing 2 candidates
[10:14:02.132] [Ledger] Intent #1042 PROPOSED for bot 'Arthas' -> Stormwind (ClassTrainer)
```

### 4.2 `.blb explain bot <name>`
Provides a step-by-step trace of candidate evaluation for a specific bot:
```text
[BLB EXPLAIN BOT] Bot 'Grommash' (Lvl 24 Orc Warrior):
- Social Protected: No (No human friends, guild members all bots)
- Master Controlled: No
- Combat/Flight/Dead: False (Alive, Idle in Crossroads)
- Active Intent: None
- Recent Visits: Orgrimmar (cooldown expires in 412s)
- Eligibility: Eligible for Outdoor World Opportunities (Ashenvale, Stonetalon)
```

### 4.3 `.blb explain city <name>`
Displays full quota telemetry for a capital city:
```text
[BLB EXPLAIN CITY] Stormwind:
- Faction: Alliance | Map: 0 | Area: 1519
- Quota: 15 | Residents: 11 | Confirmed Inbound: 2
- Actionable Deficit: 2
- Top Needs: WeaponTrainer (0), BankDeposit (1)
```

---

## 5. Standardized JSONL Telemetry

When dumped to the server log file or exported via automated tools, entries conform to standardized JSON Lines (JSONL):

```json
{"timestamp": 1729845242, "category": "Ledger", "level": "INFO", "bot": "Thoradin", "guid": 14022, "intentId": 1045, "domain": "City", "destination": "Ironforge", "activity": "BankDeposit", "state": "PROPOSED"}
```

This format allows seamless parsing by external monitoring tools, Elasticsearch, Grafana Loki, or workspace audit scripts.
