# Option 1 Function & Subsystem Migration Matrix

**Module:** `mod-twow-bot-level-brackets`  
**Architecture:** Option 1 — Brackets Orchestrates; Unchanged Playerbots Executes  
**Date:** 29 October 2026  
**Status:** Gate A/B Complete — Monolith Decomposed into Decoupled Subsystems  

---

## 1. Executive Summary

This document details the exhaustive mapping of every legacy function, class, timer, queue, database schema, and console command from the pre-refactor implementation to its Option 1 disposition.

Under Option 1:
* **The Prime Directive is enforced:** `tortoise-wow` and `TortoiseBots` are completely unmodified.
* **Direct engine mutations are eliminated:** Synthetic teleports, direct XP grants, direct level setting, and custom AI strategy injection (`RegisterAiContextAugmenter`) are retired.
* **Orchestration replaces mutation:** The module functions strictly as a scheduling and admission orchestrator, submitting tasks to existing public native entry points (`CityVisitService::RequestVisit` and `PlayerbotAI::DoSpecificAction`) while honoring all native feasibility and need checks.

---

## 2. Legacy Class & Component Disposition Matrix

| Legacy Component / Class | Disposition | Option 1 Location / Replacement | Architectural Rationale |
|:---|:---|:---|:---|
| `BotLevelBracketsConfig` | **Replaced & Modularized** | `src/policy/PopulationPolicy.h` (`PopulationConfig`) | Centralized config parsing with strict validation, range clamps, and backwards-compatible key aliases. |
| `BotEngineAdapter` | **Preserved / Forwarded** | `src/BotEngineAdapter.h` & `src/compat/NativeTaskAdapter.h` | Thin compatibility wrapper forwarding to `NativeTaskAdapter` without modified engine assumptions. |
| `BotActivityLeaseManager` (Local duplicate) | **Retired** | Native `TortoiseBots/runtime/BotActivityLeaseManager.h` via `CityVisitService` | Option 1 delegates lease arbitration to the native engine rather than maintaining a duplicate local tracker. |
| `CensusManager` (Monolithic) | **Replaced** | `src/world/Census.h`, `src/world/Census.cpp` | Provides immutable coherent snapshots distinguishing physical residents, confirmed native inbound, and pending reservations. |
| `PopulationBalancer` | **Replaced** | `src/policy/QuotaAllocator.h`, `src/policy/QuotaAllocator.cpp` | Pure, deterministic mathematical allocator using largest-remainder apportionment with zero engine side effects. |
| `TaskCoordinator` / `SchedulingLoop` | **Replaced** | `src/scheduling/Coordinator.h`, `src/scheduling/Coordinator.cpp` | Cursor-rotated scheduling loop preventing first-GUID starvation, respecting batch dispatch limits. |
| `BotTaskLedger` | **Replaced** | `src/scheduling/RequestLedger.h`, `src/scheduling/RequestLedger.cpp` | Transient in-memory ledger with 1-task-per-bot deduplication barrier and repeat/rejection backoff cooldowns. |
| `ActivityRegistry` | **Refactored** | `src/activities/ActivityRegistry.h`, `src/activities/ActivityRegistry.cpp` | 42 audited native activities categorized into `RequestableExact`, `RequestableNativeChoice`, `LocalOnly`, `ObserveOnly`, `Unavailable`. |
| `SocialProtectionManager` | **Refactored** | `src/social/ProtectionCache.h`, `src/social/ProtectionCache.cpp` | Multi-tier safeguards protecting bots grouped with real players, friends with real players, or in player guilds. |
| `DiagnosticManager` / `TraceLogger` | **Refactored** | `src/debug/DiagnosticManager.h`, `src/debug/DiagnosticManager.cpp` | Bounded circular ring buffer (2000 events), token-bucket rate limiter (50/s), DryRun execution mode. |
| `BotBracketsCommands` | **Refactored** | `src/commands/BotBracketsCommands.h`, `src/commands/BotBracketsCommands.cpp` | Unified `.blb` CLI suite (`status`, `cities`, `world`, `brackets`, `activities`, `ledger`, `stats`, `explain`, `reload`, `force`, `cleanup`, `debug`). |
| `BotLevelBracketsWorldScript` | **Preserved** | `src/mod-twow-bot-level-brackets.cpp` | Manages world lifecycle ticks, config loading, and periodic coordinator passes. |
| `BotLevelBracketsPlayerScript` | **Preserved** | `src/mod-twow-bot-level-brackets.cpp` | Monitors player login, logout, and level-up events to maintain coherent census state. |
| `BotLevelBracketsGuildScript` | **Preserved** | `src/mod-twow-bot-level-brackets.cpp` | Monitors guild formation and membership changes to invalidate social protection caches. |

---

## 3. Function-Level Disposition Matrix

### 3.1 Census & World State Tracking

| Legacy Function | Disposition | Option 1 Implementation | Notes |
|:---|:---|:---|:---|
| `UpdateCensus()` | **Migrated** | `CensusManager::TakeSnapshot()` | Generates immutable, point-in-time census snapshot across all capital cities and world zones. |
| `GetCityResidentCount(CityId)` | **Migrated** | `CensusSnapshot::GetPhysicalResidents(HubId)` | Counts only physically present bots within city boundaries. |
| `GetCityInboundCount(CityId)` | **Migrated** | `CensusSnapshot::GetConfirmedInbound(HubId)` | Inspects native travel state / `CityVisitService` inbound reservations. |
| `GetPendingReservationCount(CityId)`| **Added** | `CensusSnapshot::GetPendingReservations(HubId)` | Reflects transient requests in ledger awaiting native engine pickup. |
| `IsBotInCity(Player*, CityId)` | **Migrated** | `ReadOnlyCatalog::ResolveZoneToHub(zoneId)` | Pure DBC/catalog zone resolution without modifying bot state. |

### 3.2 Policy & Quota Allocation

| Legacy Function | Disposition | Option 1 Implementation | Notes |
|:---|:---|:---|:---|
| `CalculateCityTarget()` | **Migrated** | `QuotaAllocator::CalculateHubQuotas()` | Largest-remainder apportionment using configured target percentages and human population weighting. |
| `CalculateShortage()` | **Migrated** | `QuotaAllocator::CalculateShortage()` | Compares effective population (`residents + inbound + reservations`) against targets with hysteresis deadband. |
| `EnforcePopulationCaps()` | **Migrated** | `QuotaAllocator::ClampToBounds()` | Respects `MinHeadcount`, `MaxHeadcount`, and `FieldReservePercent`. |
| `SelectDestinationCity()` | **Migrated** | `PopulationPolicy::SelectDestination()` | Enforces destination policies: `RequireExact` (strict racial/geographical matching) vs `AllowNativeChoice`. |

### 3.3 Scheduling & Request Dispatch

| Legacy Function | Disposition | Option 1 Implementation | Notes |
|:---|:---|:---|:---|
| `ProcessSchedulingLoop()` | **Migrated** | `Coordinator::RunSchedulingPass()` | Runs single periodic pass, tracks elapsed runtime, respects per-tick dispatch limits (`MaxDispatchesPerTick`). |
| `SelectCandidateBots()` | **Migrated** | `Coordinator::CollectEligibleCandidates()` | Filters bots by level bracket, combat state, dead state, taxi flight, and social protection. |
| `RotateCursor()` | **Added** | `Coordinator::m_lastProcessedGuid` | Round-robin cursor rotation ensuring no first-GUID scheduling bias or starvation. |
| `TeleportBotToCity()` | **RETIRED** | *None (Prohibited)* | Direct teleports are an architectural violation under Option 1. |
| `ForceSetBotLevel()` | **RETIRED** | *None (Prohibited)* | Direct level mutations are an architectural violation under Option 1. |
| `InjectStrategy()` | **RETIRED** | *None (Prohibited)* | Strategy injection (`RegisterAiContextAugmenter`) violates Option 1 boundaries. |
| `DispatchTask()` | **Migrated** | `NativeTaskAdapter::SubmitTaskRequest()` | Submits through `CityVisitService::RequestVisit` or `PlayerbotAI::DoSpecificAction`. |

### 3.4 Request Ledger & Deduplication

| Legacy Function | Disposition | Option 1 Implementation | Notes |
|:---|:---|:---|:---|
| `RecordTaskSubmission()` | **Migrated** | `RequestLedger::RecordRequest()` | Tracks GUID, activity ID, target hub, timestamp, and state (`Pending`, `Acknowledged`, `Executing`, `Completed`, `Failed`). |
| `IsBotBusy()` | **Migrated** | `RequestLedger::HasActiveRequest()` | Strict 1-task-per-bot barrier prevents overlapping or competing requests. |
| `PruneExpiredRequests()` | **Migrated** | `RequestLedger::PruneExpired()` | Cleans up requests exceeding timeout thresholds (default: 300s). |
| `HandleNativeRejection()` | **Added** | `RequestLedger::RecordRejection()` | Applies exponential backoff cooldown to prevent request thrashing. |

### 3.5 Social Protection

| Legacy Function | Disposition | Option 1 Implementation | Notes |
|:---|:---|:---|:---|
| `CheckPlayerGroup()` | **Migrated** | `ProtectionCache::IsProtectedFromOrchestration()` | Protects bots currently in a group with real human players. |
| `CheckPlayerFriend()` | **Migrated** | `ProtectionCache::IsFriendWithHuman()` | Protects bots on a real human player's friend list. |
| `CheckPlayerGuild()` | **Migrated** | `ProtectionCache::IsInHumanGuild()` | Protects bots belonging to guilds containing real human players. |
| `CheckExcludedNames()` | **Migrated** | `ProtectionCache::IsNameExcluded()` | Case-insensitive regex/string lookup for manually exempt bots. |

### 3.6 Diagnostics & CLI Commands

| Legacy Function | Disposition | Option 1 Implementation | Notes |
|:---|:---|:---|:---|
| `LogDebug()` | **Migrated** | `DiagnosticManager::Log()` | Category-filtered, token-bucket rate-limited diagnostic logging. |
| `RecordEvent()` | **Migrated** | `DiagnosticManager::RecordEvent()` | Circular ring buffer (2000 events) queryable via CLI and test harness. |
| `CommandStatus()` | **Migrated** | `BotBracketsCommands::HandleStatus()` | Displays active mode, hub populations, ledger size, and recent throughput. |
| `CommandCities()` | **Migrated** | `BotBracketsCommands::HandleCities()` | Displays detailed per-city quotas, resident counts, and shortfalls. |
| `CommandExplain()` | **Added** | `BotBracketsCommands::HandleExplain()` | Generates real-time diagnostic breakdown explaining why a bot was or was not orchestrated. |
| `CommandDebug()` | **Migrated** | `BotBracketsCommands::HandleDebug()` | Toggles debug categories and DryRun mode at runtime. |

---

## 4. Timers, Queues, and Concurrency Model

* **Thread Safety Model:**
  * All scheduling, census queries, and request dispatches occur strictly on the core World tick thread (`World::Update`).
  * No separate background threads invoke core or Playerbots APIs.
  * Mutexes are restricted to transient caches and diagnostic ring buffers where thread isolation is needed.
* **Timers:**
  * `CoordinatorTimer`: Configurable interval (`BotLevelBrackets.Scheduler.IntervalMs`, default: 5000ms).
  * `CensusTimer`: Configurable interval (`BotLevelBrackets.Census.IntervalMs`, default: 2500ms).
  * `LedgerPruneTimer`: Configurable interval (default: 10000ms).
  * `ProtectionCacheTimer`: Invalidated by events and refreshed periodically (default: 30000ms).

---

## 5. Database Schema & Migration Invariants

* **Module Schema Independence:**
  * No custom database tables required for core runtime operation.
  * All transient states (census, ledger, quotas) are stored in volatile module memory and reconstructed on server restart.
  * No alterations or migrations are made to `mangos`, `characters`, `realmd`, or bot engine tables.
  * Zero SQL migrations are required or applied to core schemas.
