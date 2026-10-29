# Migration Guide: Legacy Redistribution to Option 2 Opportunities

**Module:** `mod-twow-bot-level-brackets-extended`  
**From:** Legacy Bot Level Brackets (Option 1 / Direct Mutative Redistribution)  
**To:** Option 2 Registered Opportunities & Natural Progression Observation  

---

## 1. Executive Summary

Legacy versions of `mod-twow-bot-level-brackets` operated under a mutative paradigm:
* When a level bracket had a bot surplus or deficit, bots were forcefully adjusted using `BotEngineAdapter::AdjustBotLevel()` (calling `player->GiveLevel()` or `PlayerbotFactory::Randomize()`).
* Bots were forcefully dismounted and teleported to starting zones or capital cities using synthetic coordinate injections (`bot->TeleportTo(...)`).
* Bot strategies were stripped and replaced with rigid city-only strategies (`g_CityStrategies`), breaking normal AI decision cycles.

**Option 2: Registered Opportunities** eliminates all synthetic manipulation:
1. **Natural Leveling Progression**: The module observes native bot leveling (`ObserveNative`). It never resets levels, grants artificial XP, or triggers factory rollbacks.
2. **Context-Augmented Autonomous Travel**: Bots receive opportunities registered through `RegisterAiContextAugmenter()`. The bot's own AI chooses to travel via `ChooseTravelTargetAction` and `TravelMgr`.
3. **43 Native Activities**: Replaces simple static idling with 43 lore-authentic activities (trading, banking, training, resting, gathering, grinding, dungeons).
4. **Disjoint Population Accounting**: Replaces naive counts with strict disjoint set census accounting, preventing double-counting and churn oscillations.
5. **Option 3 Restricted Fallback**: Replaces routine teleportation with an emergency fallback disabled by default and restricted exclusively to capital cities.

---

## 2. Key Architectural Differences

| Feature Area | Legacy / Option 1 | Option 2 Extended |
|:---|:---|:---|
| **Level Progression** | Forced level resets (`SetLevel` / `GiveLevel`). | Natural progression (`ObserveNative`). Brackets are advisory. |
| **Movement & Travel** | Synthetic teleportation (`bot->TeleportTo`). | Native AI travel (`ChooseTravelTargetAction` / `TravelMgr`). |
| **AI Integration** | Hard strategy stripping (`RemoveStrategy`). | Registered contexts (`RegisterAiContextAugmenter`). |
| **Activities** | Monolithic idle loops. | 43 native activities with purpose bitmasks. |
| **Capital Quotas** | Simple count checks prone to overfilling. | Disjoint census + 70% field reserve bound. |
| **Relocation Fallback** | Unrestricted instant teleports. | Option 3: Disabled by default, preflight-only, 1-hr cooldown. |
| **Observability** | Basic stdout log strings. | Ring buffer, 20 categories, rate limiter, JSONL dump. |
| **Diagnostic Commands** | `.blb status`, `.blb queue`. | `.blb status`, `.blb explain bot`, `.blb explain city`, `.blb debug dump`. |

---

## 3. Configuration File Migration

If migrating from an older `mod_twow_bot_level_brackets.conf`, note the following changes:

### 3.1 Obsolete Keys Removed
The following legacy keys are no longer used by the engine and have been removed:
* `BotLevelBrackets.PopulationLimitsEnabled` (Replaced by `BotLevelBrackets.HubPopulation.Enabled`)
* `BotLevelBrackets.TeleportBotsOnReset` (Teleportation is replaced by native opportunities)
* `BotLevelBrackets.CityStrategy.Apply` (Replaced by `"blb opportunities"` AI strategy)
* `BotLevelBrackets.StartingZoneDisperse.Enabled` (Replaced by outdoor world opportunities)

### 3.2 New Configuration Parameters
Ensure your configuration includes the new Option 2 sections:
```ini
[BotLevelBrackets]
# Core switches
BotLevelBrackets.Enabled = 1
BotLevelBrackets.CheckFrequency = 300
BotLevelBrackets.OpportunityTTLSeconds = 180
BotLevelBrackets.OpportunityCooldownSeconds = 1800

# Field reserve & city ambiance
BotLevelBrackets.HubPopulation.Enabled = 1
BotLevelBrackets.HubPopulation.FieldReserveFraction = 0.70

# Option 3 restricted fallback (Emergency only)
BotLevelBrackets.Fallback.NativeRouteUnsupportedBeforeStart = 0
BotLevelBrackets.Fallback.MaxRelocationsPerHour = 5
BotLevelBrackets.Fallback.RelocationCooldownSeconds = 3600

# Diagnostics
BotLevelBrackets.Debug.Mode = Off
BotLevelBrackets.Debug.RingBufferSize = 500

# Native activities
BotLevelBrackets.Activity.AuctionBrowse.Weight = 1.0
BotLevelBrackets.Activity.TavernRest.Weight = 1.5
BotLevelBrackets.Activity.GrindMob.Weight = 3.0
...
```

---

## 4. Operational Transition Steps

1. **Clean Database Tables**:
   Legacy pending reset entries are no longer needed since levels are not forcefully mutated:
   ```sql
   DROP TABLE IF EXISTS `bot_level_brackets_pending_resets`;
   ```
   Retain `bot_level_brackets_guild_tracker` for social protection.

2. **Replace Binary & Config**:
   Recompile with `mod-twow-bot-level-brackets-extended` enabled. Copy `conf/mod_twow_bot_level_brackets.conf.dist` to `conf/mod_twow_bot_level_brackets.conf`.

3. **Verify AI Context Augmentation**:
   On server startup, verify that the module logs:
   ```text
   [BotLevelBrackets] Registered Option 2 AI Context Augmenter for TortoiseBots.
   ```

4. **Verify Status In-Game**:
   Execute `.blb status` and `.blb ledger` to verify that online bots are being discovered, disjoint census partitions are populated, and opportunities are cycling normally.
