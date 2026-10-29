# Option 1 Configuration Migration & Key Reference

**Module:** `mod-twow-bot-level-brackets`  
**Architecture:** Option 1 — Brackets Orchestrates; Unchanged Playerbots Executes  
**Date:** 29 October 2026  
**Status:** Gate A Baseline Specification Complete  

---

## 1. Executive Summary

This document specifies all configuration keys for Option 1, including legacy key migration, aliasing, precedence rules, and validation ranges.

### Key Principles:
1. **Canonical New Keys Win:** If both a new key and its legacy alias are specified in the `.conf` file, the new key takes precedence with a startup/reload warning.
2. **Safe Fallbacks:** Every key has a deterministic fallback in C++ code matching the shipped template.
3. **No Unsafe Mechanisms:** Retired keys (such as `TeleportExecutionLimit`, `CityStrategies.*`) emit a clear deprecation warning and are ignored. They never trigger hidden actions.
4. **Binding Activity Toggles:** Enabling an activity allows brackets to request it if natively supported; it never forces an unsupported action.

---

## 2. Core Orchestration & Scheduling Keys

| Key | Default | Unit / Range | Meaning & Behavior |
|:---|:---:|:---:|:---|
| `BotLevelBrackets.Enabled` | `1` | `0` or `1` | Master toggle for the entire module. |
| `BotLevelBrackets.Orchestration.Mode` | `ExistingNativeTasks` | String | Must be `ExistingNativeTasks`. Option2 and Relocation modes are rejected. |
| `BotLevelBrackets.Orchestration.DryRun` | `0` | `0` or `1` | When enabled, evaluates decisions and logs `would-request` without calling native APIs. |
| `BotLevelBrackets.Orchestration.CheckSeconds` | `90` | `5` – `3600` s | Scheduling cycle cadence. (Legacy alias: `BotLevelBrackets.HubDisperse.PopulateFrequency`). |
| `BotLevelBrackets.Orchestration.MaxRequestsPerCycle` | `5` | `1` – `100` | Maximum new task requests submitted in a single cycle. (Legacy alias: `BotLevelBrackets.HubDisperse.BotsPerCycle`). |
| `BotLevelBrackets.Orchestration.MaxOutstandingPerFaction` | `10` | `1` – `200` | Maximum pending/unconfirmed requests allowed concurrently per faction. |
| `BotLevelBrackets.Orchestration.MaxLooseRequestsPerCycle` | `1` | `0` – `20` | Cap on requests where native chooses destination (`AllowNativeChoice`). |
| `BotLevelBrackets.Orchestration.RequestCooldownSeconds` | `1800` | `60` – `86400` s | Cooldown before a bot can receive another module-initiated task. |
| `BotLevelBrackets.Orchestration.RejectionBackoffSeconds` | `600` | `10` – `3600` s | Backoff applied when native refuses a request. |
| `BotLevelBrackets.Orchestration.StabilitySeconds` | `60` | `0` – `600` s | Required persistent deficit duration before issuing a request. (Legacy alias: `BotLevelBrackets.HubDisperse.StabilitySeconds`). |
| `BotLevelBrackets.Orchestration.DispatchTimeoutSeconds` | `30` | `5` – `300` s | Timeout after which unconfirmed dispatches are pruned. |
| `BotLevelBrackets.Orchestration.EarlyDepartureBackoffSeconds` | `3600` | `60` – `86400` s | Cooldown applied when a bot leaves a city prematurely. |
| `BotLevelBrackets.Orchestration.UnknownStatePolicy` | `ObserveOnly` | String | Policy when bot native state is ambiguous (`ObserveOnly` or `Reject`). |

---

## 3. City & Outdoor Domain Keys

| Key | Default | Unit / Range | Meaning & Behavior |
|:---|:---:|:---:|:---|
| `BotLevelBrackets.City.Enabled` | `1` | `0` or `1` | Enables city visit requests. (Legacy alias: `BotLevelBrackets.HubDisperse.Enabled`). |
| `BotLevelBrackets.City.DestinationPolicy` | `RequireExact` | `RequireExact` / `AllowNativeChoice` | Destination honesty: `RequireExact` uses only verified exact destinations. |
| `BotLevelBrackets.City.QuotaMode` | `2` | `1` (count) or `2` (pct) | Method for computing city quotas. (Legacy alias: `BotLevelBrackets.HubDisperse.QuotaMode`). |
| `BotLevelBrackets.City.MinLimitEnabled` | `0` | `0` or `1` | Enforces minimum headcount/pct bound. (Legacy alias: `BotLevelBrackets.HubDisperse.MinBotsLimitEnabled`). |
| `BotLevelBrackets.City.MaxLimitEnabled` | `1` | `0` or `1` | Enforces maximum headcount/pct bound. (Legacy alias: `BotLevelBrackets.HubDisperse.MaxBotsLimitEnabled`). |
| `BotLevelBrackets.City.LimitsMode` | `1` | `1` (headcount) or `2` (pct) | Units for min/max bounds. (Legacy alias: `BotLevelBrackets.HubDisperse.LimitsMode`). |
| `BotLevelBrackets.City.MinBotsPerCity` | `0` | `0` – `100` | Minimum bots per city. (Legacy alias: `BotLevelBrackets.HubDisperse.MinBotsPerHub`). |
| `BotLevelBrackets.City.MaxBotsPerCity` | `50` | `1` – `200` | Maximum bots per city. (Legacy alias: `BotLevelBrackets.HubDisperse.MaxBotsPerHub`). |
| `BotLevelBrackets.City.HysteresisBots` | `2` | `0` – `10` | Deadband threshold before acting on deficits. (Legacy alias: `BotLevelBrackets.HubDisperse.HysteresisBots`). |
| `BotLevelBrackets.City.DwellDurationMs` | `180000` | `30000` – `1800000` ms | Desired dwell time in city (3 minutes default). |
| `BotLevelBrackets.World.Enabled` | `1` | `0` or `1` | Enables outdoor activity requests. |
| `BotLevelBrackets.World.DestinationPolicy`| `RequireExact` | `RequireExact` / `AllowNativeChoice` | Exact region targeting vs native choice. |
| `BotLevelBrackets.World.FieldReservePct` | `70` | `0` – `100` % | Desired percentage of bot population reserved for the open world. |

---

## 4. Level & Population Policy Keys (Advisory)

| Key | Default | Unit / Range | Meaning & Behavior |
|:---|:---:|:---:|:---|
| `BotLevelBrackets.LevelPolicy.Mode` | `ObserveNative` | String | Advisory mode. Level distributions are observed and reported. |
| `BotLevelBrackets.Population.Mode` | `ObserveNative` | String | Advisory mode. Total headcount is observed without login loops. |
| `BotLevelBrackets.LevelPolicy.NumRanges` | `7` | `1` – `10` | Number of configured level brackets. |
| `BotLevelBrackets.LevelPolicy.UseDynamic` | `0` | `0` or `1` | Scales bracket targets based on active human player levels. |
| `BotLevelBrackets.LevelPolicy.HumanWeight` | `1.0` | `0.0` – `10.0` | Weighting factor for human level distribution. |
| `BotLevelBrackets.LevelPolicy.SyncFactions` | `0` | `0` or `1` | Synchronizes Alliance and Horde target percentages. |

---

## 5. Social & Identity Protection Keys

| Key | Default | Meaning & Behavior |
|:---|:---:|:---|
| `BotLevelBrackets.Protection.IgnoreFriendListed` | `1` | Excludes bots appearing on real players' friend lists. |
| `BotLevelBrackets.Protection.IgnoreRealPlayerGuilds`| `1` | Excludes bots belonging to guilds containing real players. |
| `BotLevelBrackets.Protection.IgnoreArenaTeams` | `1` | Excludes bots belonging to arena teams. |
| `BotLevelBrackets.Protection.SocialRefreshSeconds`| `300` | Cadence for refreshing friends and guild caches from character DB. |
| `BotLevelBrackets.Protection.ExcludedBotNames` | `""` | Comma-separated list of bot names immune from module requests. |

---

## 6. Activity Registry Keys Schema

All 42 individual activities follow the unified schema:
```ini
BotLevelBrackets.Activities.Enabled = 1
BotLevelBrackets.Activities.RequireSupported = 0

BotLevelBrackets.Activities.<ActivityID>.Enabled = 1
BotLevelBrackets.Activities.<ActivityID>.CityEnabled = 1
BotLevelBrackets.Activities.<ActivityID>.WorldEnabled = 1
BotLevelBrackets.Activities.<ActivityID>.Weight = 1.0
BotLevelBrackets.Activities.<ActivityID>.CooldownSeconds = 300
BotLevelBrackets.Activities.<ActivityID>.MaxOutstandingPerFaction = 2
```

---

## 7. Retired Keys & Deprecation Matrix

| Retired Key | Former Meaning | Option-1 Handling |
|:---|:---|:---|
| `BotLevelBrackets.TeleportOnLevelChange` | Teleported bot after level mutation | **Ignored** (warning logged; zero teleports in Option 1). |
| `BotLevelBrackets.TeleportExecutionLimit`| Rate limit on teleport worker | **Ignored** (warning logged). |
| `BotLevelBrackets.TeleportMaxAttempts` | Retries for failed teleports | **Ignored** (warning logged). |
| `BotLevelBrackets.PendingQueueTTL` | Expiry timer for teleport queue | **Ignored** (warning logged). |
| `BotLevelBrackets.FlaggedProcessLimit` | Batch limit for level resets | **Ignored** (warning logged; level mutation retired). |
| `BotLevelBrackets.MaxPendingQueueSize` | Queue limit for resets | **Ignored** (warning logged). |
| `BotLevelBrackets.CityStrategies.*` | Injected custom strategies | **Ignored** (warning logged; zero strategy injection). |
| `BotLevelBrackets.ManageCityStrategies` | Stripped/restored strategies | **Ignored** (warning logged). |
| `BotLevelBrackets.WorldTeleportResetStrategies` | Reset strategies on teleport | **Ignored** (warning logged). |
| `BotLevelBrackets.StartingZoneDisperseFrequency` | Forced starter zone evacuation | **Ignored** (warning logged; native scatter owns evacuation). |
| `BotLevelBrackets.WrongMapDisperseFrequency` | Forced wrong-map recovery | **Ignored** (warning logged; native recovery handles maps). |
