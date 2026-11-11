# Portability & Migration Guide: Turtle WoW Cores & Bot Engines

> **Module**: `mod-twow-bot-level-brackets`  
> **Target Platforms**: `tortoise-wow` (Canonical Turtle WoW custom vMaNGOS) & `tortoise-wow-mantech-turtle` (Legacy)  
> **Bot Ecosystems**: `TortoiseBots` (Modern Modular) & CMaNGOS PlayerBots (Vendored Monolithic)

---

## 1. Engine & Core Architectural Divergence

Before this portability refactoring, `mod-twow-bot-level-brackets` was hard-coded against an older, monolithic development fork (`tortoise-wow-mantech-turtle`), where PlayerBots was vendored directly inside the core source tree at `src/modules/PlayerBots`.

When moving to the canonical upstream core (`tortoise-wow`) and modern `TortoiseBots`, critical architectural differences emerged:

### Architectural Comparison Matrix

| Component | Upstream Canonical (`tortoise-wow` + `TortoiseBots`) | Legacy Fork (`mantech-turtle` / `BUILD_PLAYERBOTS`) |
| :--- | :--- | :--- |
| **Core Source Tree** | Clean, strict vMaNGOS. Contains **NO** `src/modules/PlayerBots`. | PlayerBots vendored in `src/modules/PlayerBots/`. |
| **Module Loading** | Modular C++ via `modules/<module>/` and `ConfigureModules.cmake`. | Compiled conditionally via `-DBUILD_PLAYERBOTS=ON`. |
| **Bot Session Transport** | Generic `SessionTransport::Headless` (`session->IsHeadless()`). | Monolithic custom hooks and session wrappers. |
| **Core Immutability** | Strict Unediting Doctrine (0 changes to core headers or sources). | Core often patched with `if (IsBot())` or `GetBot()`. |
| **Bot AI Lifetime** | Owned exclusively by `TortoiseBots::BotManager` and `PlayerbotAIStorage`. | Owned by `sRandomPlayerbotMgr` and raw AI pointers. |
| **Bot Releveling** | `PlayerbotFactory::MakeComplete()` with incremental seed. | `PlayerbotFactory::Randomize(bool, bool)`. |
| **Class Formatter** | `ChatHelper::formatClass(uint8)` in `ai/playerbot/ChatHelper.h`. | Vendored `playerbot/ChatHelper.h`. |
| **Network Packets** | Packets suppressed for headless sessions (`!session->IsHeadless()`). | Unfiltered packet dispatch to null sessions. |

---

## 2. CMake Auto-Discovery Engine

The module's `mod-twow-bot-level-brackets.cmake` implements automated environment detection during CMake's `POST_TARGETS` phase:

```cmake
# 1. Search for TortoiseBots in modules or sibling repositories
set(TORTOISEBOTS_FOUND FALSE)
set(TORTOISEBOTS_SEARCH_PATHS
  "${CMAKE_SOURCE_DIR}/modules/TortoiseBots"
  "${CMAKE_SOURCE_DIR}/../TortoiseBots"
  "${CMAKE_CURRENT_LIST_DIR}/../TortoiseBots"
)
foreach(SEARCH_PATH ${TORTOISEBOTS_SEARCH_PATHS})
  if(EXISTS "${SEARCH_PATH}/runtime/BotManager.h")
    set(TORTOISEBOTS_ROOT "${SEARCH_PATH}")
    set(TORTOISEBOTS_FOUND TRUE)
    break()
  endif()
endforeach()

# 2. Search for Legacy Vendored PlayerBots
set(LEGACY_PLAYERBOTS_FOUND FALSE)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/modules/PlayerBots/playerbot/RandomPlayerbotMgr.h")
  set(LEGACY_PLAYERBOTS_ROOT "${CMAKE_SOURCE_DIR}/src/modules/PlayerBots")
  set(LEGACY_PLAYERBOTS_FOUND TRUE)
endif()
```

### Compiler Definitions Injected

1. **When `TortoiseBots` is discovered**:
   * `-DHAVE_TORTOISEBOTS=1`
   * Target includes: `${TORTOISEBOTS_ROOT}`, `${TORTOISEBOTS_ROOT}/ai`, `${TORTOISEBOTS_ROOT}/ai/playerbot`, `${TORTOISEBOTS_ROOT}/runtime`, etc.
2. **When Legacy `PlayerBots` is discovered**:
   * `-DHAVE_LEGACY_PLAYERBOTS=1`
   * Target includes: `${LEGACY_PLAYERBOTS_ROOT}`, `${LEGACY_PLAYERBOTS_ROOT}/playerbot`, etc.
3. **When no PlayerBots engine is detected**:
   * `-DHAVE_GENERIC_HEADLESS_BOTS=1`
   * Pure core compilation using native `Player` and `WorldSession` capabilities with zero external dependencies.

---

## 3. C++ API Translation & Adapter Guide

All engine interactions pass through [`src/BotEngineAdapter.h`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Twow/Turtle%20wow%20modules/mod-twow-bot-level-brackets/src/BotEngineAdapter.h). When developing or reviewing bot level bracket features, consult this translation table:

| Action / Query | `TortoiseBots` Implementation | Legacy PlayerBots Implementation | Generic Headless Implementation |
| :--- | :--- | :--- | :--- |
| **Is Bot?** | `session->IsHeadless() \|\| BotManager::Instance().IsBot(guid)` | `GetBotAI(player) != nullptr` | `session->IsHeadless()` |
| **Is Random Bot?** | `BotManager::Instance().IsRandomBot(guid)` | `sRandomPlayerbotMgr.IsRandomBot(player)` | `session->IsHeadless()` |
| **Has Real Master?** | `ai->HasRealPlayerMaster() \|\| !rec->masterGuid.IsEmpty()` | `ai->HasRealPlayerMaster()` | Non-headless member in party |
| **Min/Max Level** | `sPlayerbotAIConfig.randomBotMinLevel` | `sPlayerbotAIConfig.randomBotMinLevel` | `1` to `CONFIG_UINT32_MAX_PLAYER_LEVEL` |
| **Is Levels Disabled?** | `sPlayerbotAIConfig.disableRandomLevels` | `sPlayerbotAIConfig.disableRandomLevels` | `false` |
| **Account In Pool?** | `sPlayerbotAIConfig.IsInRandomAccountList(acc)` | `sPlayerbotAIConfig.IsInRandomAccountList(acc)` | `false` |
| **Relevel Bot** | `GiveLevel`/`SetLevel` + `factory.MakeComplete()` | `factory.Randomize(false, false)` | `GiveLevel`/`SetLevel` + `InitStatsForLevel(false)` |
| **Save Character** | `bot->SaveToDB()` | `bot->SaveToDB()` | `bot->SaveToDB()` |

---

## 4. WotLK to Vanilla / Turtle WoW Seam Fixes

During the audit, four critical WotLK/AzerothCore porting remnants were identified and remediated:

1. **`CLASS_DEATH_KNIGHT` Elimination**:
   * *Problem*: In AzerothCore/WotLK, Death Knights require special level 55-60 clamping. In Vanilla 1.12.1 and Turtle WoW 1.18.1, Death Knights do not exist (`CLASS_DEATH_KNIGHT` is commented out in `SharedDefines.h`).
   * *Fix*: Completely removed DK branches from `AdjustBotToRange` and `GetOrFlagPlayerBracket`.
2. **Method Casing (`getClass()` ➡️ `GetClass()`)**:
   * *Problem*: `Player::getClass()` is an AzerothCore method; vMaNGOS implements `Player::GetClass()`.
   * *Fix*: Standardized to `GetClass()`.
3. **Logout Stun Check (`IsStunnedByLogout()`)**:
   * *Problem*: `IsStunnedByLogout()` does not exist in vMaNGOS.
   * *Fix*: Replaced with native session query: `bot->GetSession()->isLogingOut()`.
4. **Spell Casting Check (`IsNonMeleeSpellCasted()`)**:
   * *Problem*: `IsNonMeleeSpellCasted()` does not exist in vMaNGOS `Unit.h`.
   * *Fix*: Replaced with `bot->GetCurrentSpell(CURRENT_GENERIC_SPELL) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL)`.
5. **Database Memory Management**:
   * *Problem*: `CharacterDatabase.Query(...)` returns a raw `QueryResult*` in vMaNGOS.
   * *Fix*: Standardized to `QueryResult* result = CharacterDatabase.Query(...)` followed by mandatory `delete result;`.

---

## 5. Deployment & Verification Checklist

To verify that the module builds and functions cleanly across environments:

1. **Run Static Module Compliance Audit**:
   ```powershell
   python tools/audit_module_port.py mod-twow-bot-level-brackets
   ```
2. **Verify Core Immutability**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools/verify_core_immutability.ps1
   ```
3. **Verify Configuration Encoding**:
   Ensure `conf/mod_twow_bot_level_brackets.conf.dist` is encoded in UTF-8 without BOM.
4. **Apply SQL Migration**:
   Import `data/sql/character/2026_09_01_bot_level_brackets_guild_tracker.sql` into the character database (`tw_char`).
