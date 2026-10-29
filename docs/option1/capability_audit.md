# Option 1 Capability Audit & Native Request Inventory

**Module:** `mod-twow-bot-level-brackets`  
**Architecture:** Option 1 — Brackets Orchestrates; Unchanged Playerbots Executes  
**Date:** 29 October 2026  
**Status:** Gate A Baseline Audit Complete  

---

## 1. Overview & Operating Boundaries

Under Option 1, all gameplay tasks are implemented and executed exclusively by unchanged native bot engines (`TortoiseBots` or legacy PlayerBots). The module strictly observes population, computes quota shortages, selects eligible bots, and requests genuine existing native tasks through audited public entry points.

### Inviolable Invariants:
1. **Core & Engine Immutability:** `tortoise-wow` and `TortoiseBots` are 100% read-only. No core patches, no header mutations.
2. **Zero Augmentation Injection:** No `RegisterAiContextAugmenter`, no custom AI strategies/actions/triggers/values registered in engine context.
3. **Zero Relocation / Mutation:** No teleports, no level mutations, no synthetic XP, no factory re-provisioning, no forced login/logout loops.
4. **Honest Capability Reporting:** Every task is classified by its real native capabilities (`RequestableExact`, `RequestableNativeChoice`, `LocalOnly`, `ObserveOnly`, `Unavailable`).

---

## 2. Dependency Pin & Baseline Environment

| Component | Target Version / Hash | Local Path | Status |
|:---|:---|:---|:---|
| **Core Engine** | Turtle WoW custom vMaNGOS (`tortoise-wow` 1.18.1.7272) | `Turtle wow modules/tortoise-wow` | Pristine / Read-Only |
| **Companion Bot Engine** | `TortoiseBots` (Modular companion engine) | `Turtle wow modules/TortoiseBots` | Pristine / Read-Only |
| **Legacy Engine Fallback** | CMaNGOS / shyalya PlayerBots | Vendored in older cores | Supported via `HAVE_LEGACY_PLAYERBOTS` |
| **Generic Core Fallback** | Native Headless Session Mode | Pure core | Supported via `HAVE_GENERIC_HEADLESS_BOTS` |

---

## 3. Native Request Inventory & Entry Point Call Traces

### 3.1 City Visit Request: `CityVisitService::RequestVisit`

* **File & Symbol:** `TortoiseBots/runtime/CityVisitService.h`:
  ```cpp
  CityVisitAdmission CityVisitService::RequestVisit(
      Player* bot,
      CityId cityId,
      uint32_t dwellDurationMs,
      std::string const& ownerId,
      uint32_t configGeneration,
      uint64_t& outRequestId
  );
  ```
* **Thread & Lifetime Contract:**
  - Invoked strictly on the **World Thread** during module world update tick (`WorldScript::OnUpdate`).
  - No concurrent invocation from background worker threads.
* **Call Chain:**
  1. `Coordinator::ProcessCityDemands()`
  2. `NativeTaskAdapter::RequestCityVisit(bot, cityId, dwellMs, generation, outReqId)`
  3. `CityVisitService::Instance().RequestVisit(...)`
  4. Native admission checks:
     - `bot->IsInWorld()`, `bot->IsAlive()`, `!bot->IsInCombat()`, `!bot->IsBeingTeleported()`
     - `!bot->GetMap()->IsDungeon()`, `!bot->GetMap()->IsRaid()`, `!bot->GetMap()->IsBattleGround()`
     - `sRandomBotFacade.IsFreeBot(bot)`
     - `!HasRealPlayerInParty(bot)`
     - Team alignment: `bot->GetTeam() == def->teamId`
     - Route feasibility: `bot->GetMapId() == def->mapId || bot->GetLevel() >= 20`
     - Lease acquisition: `BotActivityLeaseManager::Instance().TryAcquire(guidLow, BotActivity::CityVisit, totalLeaseMs)`
  5. State initialized to `CityVisitState::Pending` (or `Visiting` if already in city).
* **Return & Acknowledgment Semantics:**
  - Returns `CityVisitAdmission::Accepted`: Native lease granted, request registered with durable `requestId`.
  - Returns `CityVisitAdmission::AlreadyActive`: Bot already has pending/active visit to this city.
  - Returns `CityVisitAdmission::Busy`: Lease in use or bot engaged in non-preemptable activity.
  - Returns `CityVisitAdmission::Protected`: Bot is hired, grouped with human, or not a free pool bot.
  - Returns `CityVisitAdmission::UnsupportedCity`: City not recognized or wrong faction.
  - Returns `CityVisitAdmission::UnsafeRoute`: Level < 20 cross-continent route blocked.
* **Cancellation:**
  - `CityVisitService::Instance().CancelVisit(botGuid, CityVisitCancelReason::OwnerRequested, ownerId)` releases the lease and restores idle state safely.
* **Support Class:** `RequestableExact` (for the 8 canonical capital cities).

---

### 3.2 Native Action Dispatch: `PlayerbotAI::DoSpecificAction`

* **File & Symbol:** `TortoiseBots/ai/playerbot/PlayerbotAI.h`:
  ```cpp
  virtual bool PlayerbotAI::DoSpecificAction(
      const std::string& name,
      Event event = Event(),
      bool silent = false
  );
  ```
* **Thread & Lifetime Contract:**
  - Invoked strictly on the **World Thread**.
* **Call Chain & Gate Verification:**
  1. `Coordinator::DispatchNativeAction(bot, actionName)`
  2. `PlayerbotAI::DoSpecificAction(name, Event(), true)`
  3. Loops through engines: `ActionResult res = engine->ExecuteAction(name, event)`
  4. `Engine::ExecuteAction(name, event)` executes:
     - `ClearActionFailures(action, event)`
     - `bool isUseful = action->isUseful()`: **Native Need Gate** (checks inventory, funds, quest availability, durability, etc.).
     - If `!isUseful`, returns `ACTION_RESULT_USELESS`.
     - `bool isPossible = action->isPossible()`: **Native Feasibility Gate** (checks distance, target reachability, reagents, spells).
     - If `!isPossible`, returns `ACTION_RESULT_IMPOSSIBLE`.
     - `ListenAndExecute(action, event)`: Invokes native execution.
* **Return Semantics:**
  - Returns `true` ONLY on `ACTION_RESULT_OK`.
  - Returns `false` on `ACTION_RESULT_IMPOSSIBLE` or `ACTION_RESULT_USELESS`.
* **Support Class:** Depends on individual action:
  - Vendor/Repair/Trainer/AH: `RequestableNativeChoice` (native selects target vendor/trainer/AH).
  - Local interaction (Gossip, Emote, RestSit): `LocalOnly`.

---

### 3.3 Outdoor Travel Observation: `TravelTarget` & `TravelMgr`

* **File & Symbol:** `TortoiseBots/ai/playerbot/TravelMgr.h`:
  ```cpp
  TravelTarget* tt = ai->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
  TravelStatus status = tt->GetStatus();
  TravelState state = tt->GetTravelState();
  ```
* **Read-Only Inspection:**
  - Reads active destination without mutating travel fields, waypoints, or future-purpose strings.
  - Detects if bot is inbound to a region or working on quest/grind/gather.
* **Support Class:** `ObserveOnly`.

---

## 4. Capability Classes Summary

| Capability Class | Definition | Allowed Coordinator Action |
|:---|:---|:---|
| **`RequestableExact`** | Safe existing native API accepts & guarantees exact target destination | Request at idle boundary within quota budgets |
| **`RequestableNativeChoice`** | Safe native API accepts task, but native logic selects exact destination | Request under `AllowNativeChoice` policy; record actual destination on arrival |
| **`LocalOnly`** | Task valid only at bot's current physical position | Request only when local preconditions hold; zero movement forced |
| **`ObserveOnly`** | Native owns behavior; no safe public external request API | Count/report in census; never initiate or fake |
| **`Unavailable`** | Missing, unsafe, or retired mechanism | Log limitation; keep strictly disabled |

---

## 5. Audited Engine Seams & Missing APIs

1. **Exact Outdoor Region Parameter:**
   - TortoiseBots `ChooseTravelTargetAction` selects quest/grind/gather targets based on bot level and native travel distance, without an external region filter parameter.
   - **Disposition:** Outdoor requests operate in `RequestableNativeChoice` or `ObserveOnly`. Under `BotLevelBrackets.World.DestinationPolicy = RequireExact`, outdoor region-specific requests report `NoExactDestinationControl` rather than forcing location.
2. **Direct Level Distribution Enforcement:**
   - Native ladder (`RandomBotService` / login ladder) owns which level bots log in.
   - **Disposition:** Level bracket distribution goals are classified as **Advisory**. Shortages in actual level supply are displayed truthfully in `.blb brackets` and `.blb status` without attempting character resets or synthetic XP boosts.
3. **Total Online Headcount Mutation:**
   - Bot logins are governed by native `aiplayerbot.conf` and `RandomBotService`.
   - **Disposition:** Population mode is `ObserveNative`. The module observes online headcount without driving independent login/logout loops.
