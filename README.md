# Turtle WoW Module: Bot Level Brackets

The **Bot Level Brackets** module for Turtle WoW (VMaNGOS 1.12.1) ensures an even, configurable spread of playerbots across defined level ranges (brackets).

It periodically evaluates online random bot populations and automatically re-levels bots from overpopulated brackets to underpopulated brackets, while respecting player groups, friend lists, and guilds with real players.

---

## Features

- **Faction-Specific Brackets**: Fully configurable level boundaries and desired percentage distributions for Alliance and Horde.
- **Dynamic Player-Weighted Distribution**: Automatically shifts bot level proportions to follow real player level clusters.
- **Guild & Friend List Protection**: Excludes bots that are friends with real players or in guilds with human players (both online and offline via persistent database tracking).
- **Group Safety**: Bots currently in groups with real players, in combat, in battlegrounds, in flight, or being teleported are never interrupted.
- **In-Game Commands**: Built-in `.blb reload`, `.blb status`, and `.blb cleanup` commands.

---

## Installation

1. Copy or clone this directory into `tortoise-wow/modules/mod-twow-bot-level-brackets`:
   ```sh
   cd /path/to/tortoise-wow/modules
   git clone <repo-url> mod-twow-bot-level-brackets
   ```
2. Re-run CMake to generate the build files:
   ```sh
   cd /path/to/tortoise-wow/build
   cmake .. -DMODULES=static
   ```
3. Compile the server:
   ```sh
   cmake --build . --config Release
   ```
4. Copy `conf/mod_twow_bot_level_brackets.conf.dist` to your server config directory as `mod_twow_bot_level_brackets.conf` and adjust settings as needed.
5. Apply `data/sql/character/2026_09_01_bot_level_brackets_guild_tracker.sql` to your character database.

---

## Configuration

Edit `mod_twow_bot_level_brackets.conf` to configure bracket ranges, check frequencies, and exclusion rules.