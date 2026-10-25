# Population balancing audit fixes

This change hardens the standalone Bot Level Brackets module for Turtle WoW custom vMaNGOS. It changes no core or TortoiseBots source, adds no host hooks, and requires no new database migration. Existing city `*Pct` keys and the mode 1 count / mode 2 percentage interpretation are retained.

## Behavior changes

| Audit finding | Implemented behavior | Regression evidence |
| :--- | :--- | :--- |
| Level 60 dispersal returns to the same capital | Outdoor faction destinations replace capital destinations. Hub evacuation rejects managed hub overlap, including overlapping custom starter destinations. | Evacuate both primary capitals and an overlapping High Elf starter/capital; final position is outside hubs. |
| Routing changes when bracket layout changes | Physical level bands select destinations independently of configurable bracket indices. | A level 45 bot with a single custom bracket still reaches the level 40–49 destination. |
| Queued teleports outlive eligibility | Execution repeats bot/random identity, configured social/name/guild/master exclusions and movement safety checks. Configuration reload invalidates transient intents. | Friend, guild and name exclusions added after enqueue prevent relocation; generation mismatch and expired intents are cancelled. |
| Work drains in a single burst | One shared attempt budget per 2-second executor tick, oldest entries first with GUID tie-breaks, fresh work before retries, and bounded delayed retries. | A budget of 1 executes only 1 of 3 entries per tick; a permanently failing entry stops after 3 attempts. |
| Reset persistence loses offline work | Offline and logging-out entries wait; logout does not delete saved reset rows. TTL includes offline time; online targets and protections are revalidated. | An offline queued reset survives processing and logout, then applies after login. |
| Later distribution passes ignore earlier moves | Counts project valid pending resets into destination brackets; those bots are not queued again as source candidates. | Repeated distribution passes leave an already sufficient queue unchanged. |
| Protected residents disappear from city census | All online random residents count; only eligible candidates may move. The percentage denominator includes all online random faction bots, including excluded/protected bots. | A protected resident fills a city target without recruiting an extra bot; pending inbound is counted once. |
| Repeated scans and fixed city priority | One world census groups candidates by hub and faction. City priority rotates every hub pass. | With a budget of 1, consecutive passes populate different eligible cities. |
| City detection broadly resets AI | Configured non-combat strategy changes carry a snapshot owned by the module. Entry applies once; exit restores only changes that still match the applied state. | One entry does not reapply; unrelated outdoor behavior and unrelated/custom strategies survive restoration. |
| Commands mutate module state from command threads | In-game requests carry GUID + copied text to a bounded queue drained by the world update; authorization is repeated on execution. Console stays inline. | Force has no effect until drain; a downgraded requester cannot force a cycle; reload can enable the disabled module. |
| Legacy accessor recursively calls itself | The adapter qualifies the global legacy `::GetBotAI` accessor. | Legacy behavior binary calls the real adapter entry point successfully. |
| Generic mode exposes engine-only types | Callers use an engine-neutral non-combat strategy interface; generic mode has no `BotState` dependency. | Actual module and adapter compile and execute with a generic host double that deliberately declares no `BotState`. |
| Code and template frequencies differ | Both use hub 90s, starter 120s and wrong-map 180s. | Literal configuration fallback/template parity check. |
| Diagnostics use different targets/census | Status uses the exact bracket allocation helper; hub diagnostics share the scheduler census and show raw/final targets, residents, movable bots, pending movements and effective population. | Same production census/target helpers used by the behavior tests and commands. |

## Operator settings

```ini
BotLevelBrackets.Teleport.ExecutionLimit = 25
BotLevelBrackets.Teleport.MaxAttempts = 3
BotLevelBrackets.HubDisperse.PopulationLimitsEnabled = 1
BotLevelBrackets.HubDisperse.MinBotsPerHub = 0
BotLevelBrackets.HubDisperse.MaxBotsPerHub = 50
```

The execution limit is a shared budget, separate from each scheduler's existing `BotsPerCycle`. A failed attempt counts against the budget. Unsafe entries wait without consuming an attempt. Failure retries wait `2 * attempts` seconds; dropping a failed entry does not stop a future scheduler pass from selecting that bot again.

Min/Max bound the calculated target, not observed population. For Stormwind 7% and 3,000 online Alliance random bots, the raw target is 210 and the bounded target is 50. Protected residents still count but never become movable just because the city is above target. Hysteresis, stability, cooldown and bot availability remain relevant.

`WorldTeleport.ResetStrategies` now controls restoration of module-owned non-combat changes. It does not wipe all AI engines. If another system visibly changes a touched strategy, restoration skips that changed state; this does not claim to detect an explicit order that leaves exactly the same state. Snapshots are transient, cleared on logout/level reprovisioning, and are not persisted as bot preferences. Updated city strategy configuration applies on new entries.

`.blb reload` uses a separate core `Config` instance to load the main file and module files before applying this module's values. A load failure reported by the core loader leaves active module settings untouched; syntax/error handling follows that core API. This does not reload the TortoiseBots AI configuration or change settings in other modules. Reload invalidates queued teleport intents and resets hub stability timers; saved reset entries remain subject to target validation.

Useful commands:

- `.blb hubs`: raw configuration, raw and bounded targets, actual residents, safe candidates, queued inbound/outbound and effective population.
- `.blb stats`: failures/cancellations, strategy ownership counts and measured last/maximum hub-cycle duration in microseconds.
- `.blb pending`: saved reset/teleport queue sizes and ages.

## Framework boundaries and provenance

The integration follows the current core's `WorldScript`, `PlayerScript`, `GuildScript`, `AllCommandScript`, `ObjectGuid`, `Config` and character-database interfaces. All added AI ownership and command execution state is used on the world thread; off-thread command/lifecycle entry points hand off copied values under locks. Bot identity remains in the adapter; no bot-specific core session field or core hook is introduced.

References inspected, not vendored:

- [tortoise-wow core](https://github.com/tortoise-wow/tortoise-wow/tree/4a9b6d564b685aaf63eb056f1f0e6d6adc9e46b4): `src/game/ScriptObjects.h`, `src/game/Chat/Chat.cpp`, `src/shared/Config/Config.h` and `Config.cpp`.
- [TortoiseBots architecture and host boundaries](https://github.com/Ildourol/TortoiseBots/tree/64ffe51a33a4283bb48c183e5f61ccf8d7b97293): architecture invariants, `host/BotChatAdapter.cpp`, AI storage and adapter APIs. The GUID handoff follows this architectural pattern; this change does not vendor or copy the bot command subsystem.

## Validation and its limits

Run:

```bash
python3 tests/test_config_defaults.py
python3 tests/test_hub_population.py
python3 tests/test_module_behavior.py
python3 tests/test_module_behavior.py --sanitize
```

The quota test compiles production helpers. The module behavior suite compiles the actual complete module translation unit and adapter in all 3 conditional paths with deterministic host/AI/database doubles, using C++17 and `-Wall -Wextra -Werror`. Sanitizer mode adds AddressSanitizer and UndefinedBehaviorSanitizer. Leak detection defaults off because managed sandboxes may deny its `/proc` tracing; set `ASAN_OPTIONS=detect_leaks=1` to opt in on a suitable host. These are simulated behavior tests, not a claim of real server integration or SQL execution.

A real core/module build and live game validation require the server build dependencies, matching AI checkout, world data and a running character database. They are not supplied by these tests. Before deployment:

1. Build against the intended custom core with native TortoiseBots enabled; check the generic/module-off configurations separately. A real legacy build requires its own supported legacy checkout.
2. Observe level 60 evacuation from Stormwind and Orgrimmar and confirm destinations are walkable with the installed map data. Verify custom race and custom bracket routing.
3. Queue a bot, then protect it through group/master/guild/friend state; verify no forbidden relocation.
4. Restart with saved resets while bots reconnect slowly; verify rows remain and work resumes without duplicate provisioning.
5. Exercise `.blb` commands in game and console, including reload while disabled, malformed configuration, authorization changes and logout with queued commands.
6. Compare `.blb hubs` counts against actual residents and pending moves. Watch `.blb stats` for retries and cycle timings under the intended population.

No benchmark speedup or live-game behavior is claimed from the host-double tests alone.
