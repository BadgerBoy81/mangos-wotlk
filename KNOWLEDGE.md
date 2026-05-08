# CMaNGOS WotLK — Knowledge Base

Reference document for architecture, findings, and things we look up often.
Updated as we learn. If you find yourself searching for the same thing twice, put it here.

---

## Build & Run

### Build commands (Linux / Mint)

```bash
# From Build_linux/
cmake ../Source/mangos-wotlk \
  -DCMAKE_INSTALL_PREFIX=../Run_linux \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_POLICY_DEFAULT_CMP0167=OLD \
  -DBUILD_GAME_SERVER=ON \
  -DBUILD_LOGIN_SERVER=ON \
  -DBUILD_EXTRACTORS=OFF \
  -DBUILD_AHBOT=ON \
  -DBUILD_DEPRECATED_PLAYERBOT=ON \
  -DBUILD_PLAYERBOTS=OFF \
  -DBUILD_SCRIPTDEV=ON

# Build
cmake --build . -- -j$(nproc)

# Install (copies binaries + configs to Run_linux/)
cmake --install .
```

### Linux Mint / Debian library path note

On Debian-based Linux, CMake may resolve OpenSSL/Zlib search paths to `/usr/lib64` even when the libraries live in `/usr/lib/x86_64-linux-gnu`.
For VS Code build tasks, use explicit CMake overrides:

- `-DOPENSSL_ROOT_DIR=/usr`
- `-DOPENSSL_INCLUDE_DIR=/usr/include`
- `-DOPENSSL_CRYPTO_LIBRARY=/usr/lib/x86_64-linux-gnu/libcrypto.so`
- `-DOPENSSL_SSL_LIBRARY=/usr/lib/x86_64-linux-gnu/libssl.so`
- `-DZLIB_INCLUDE_DIR=/usr/include`
- `-DZLIB_LIBRARY_DEBUG=/usr/lib/x86_64-linux-gnu/libz.so`
- `-DZLIB_LIBRARY_RELEASE=/usr/lib/x86_64-linux-gnu/libz.so`

### Run the server

```bash
cd Run_linux/bin/
./mangosd -c ../etc/mangosd.conf    # world server
./realmd -c ../etc/realmd.conf      # auth/realm server
```

Auto-restart scripts (use GNU `screen`): `run-mangosd`, `run-realmd`

### VS Code debug source path mapping

If your debug session reports a source path under `/run/media/system/...` while your workspace is mounted under `/media/...`, add a `sourceMap` entry to `.vscode/launch.json`.
For example:

- `/run/media/system/small_storage/projects/badgerboy/WoW/WotLK` → `${workspaceFolder}/../..`
- `/run/media/system/small_storage/projects/badgerboy/WoW/WotLK/Source/mangos-wotlk` → `${workspaceFolder}`

### Database

- Host: `192.168.1.197:3306` (separate LAN machine)
- User: `mangos_wotlk_dev`
- Databases: `wotlkrealmd`, `wotlkmangos`, `wotlkcharacters`, `wotlklogs`

### Key directories

| Path (relative to WotLK/) | Purpose |
|---|---|
| `Source/mangos-wotlk/` | Main server source (git repo) |
| `Source/wotlk-db/` | Database scripts (git repo) |
| `Build_linux/` | Linux cmake build output |
| `Run_linux/bin/` | Server binaries |
| `Run_linux/etc/` | Runtime config files |

---

## CMaNGOS Architecture Overview

### The big picture

CMaNGOS is a server emulator for World of Warcraft. It receives packets from the WoW game client, processes game logic, and sends packets back. All game state lives server-side in MySQL + in-memory objects.

### Architecture layers

```
Main.cpp → Master::Run()
  └─ WorldRunnable (game loop thread)
       └─ World::Update(diff)           ← The heartbeat. Called every ~50ms.
            ├─ Update sessions           ← Per-player packet processing
            │    └─ WorldSession::Update()
            │         └─ PlayerbotMgr::UpdateAI()  ← Bot manager per account
            │              └─ PlayerbotAI::UpdateAI()  ← Per-bot AI tick
            ├─ Update maps               ← Per-zone object updates
            │    └─ Map::Update()
            │         └─ Creature/Player updates
            ├─ Update auctions, mail, events, weather...
            └─ Process CLI commands
```

### Object hierarchy

```
Object
  └─ WorldObject              (has position in the world)
       ├─ Unit                (has health, can fight)
       │    ├─ Player         (human-controlled character)
       │    └─ Creature       (NPC / mob)
       ├─ GameObject          (doors, chests, flags)
       ├─ DynamicObject       (AoE effects on ground)
       └─ Corpse
```

### Key systems

| System | Key files | What it does |
|--------|-----------|--------------|
| **World loop** | `WorldRunnable.cpp`, `World.cpp` | Drives the game tick at ~50ms intervals |
| **Sessions** | `WorldSession.cpp` | One per connected player. Handles client↔server packets (opcodes) |
| **Maps** | `Map.cpp`, `MapManager.cpp` | Each zone/instance is a Map. Updates all objects within it |
| **Spells** | `Spell.cpp`, `SpellAuras.cpp`, `SpellEffects.cpp` | Cast processing, aura application, effect resolution |
| **Movement** | `MotionGenerators/`, recastnavigation | Pathfinding via navmesh. Movement types: follow, chase, waypoint, etc. |
| **AI** | `CreatureAI.cpp`, ScriptDevAI/ | NPC behavior. Bosses have custom scripts in ScriptDevAI |
| **PlayerBot** | `PlayerBot/Base/`, `PlayerBot/AI/` | Deprecated bot system — AI-controlled Player objects |

---

## PlayerBot System

### Class hierarchy

```
PlayerbotMgr                    ← Manages all bots for one player account
  └─ PlayerbotAI                ← Core AI brain for one bot character
       └─ PlayerbotClassAI      ← Abstract interface for class-specific logic
            ├─ PlayerbotWarriorAI
            ├─ PlayerbotPaladinAI
            ├─ PlayerbotHunterAI
            ├─ PlayerbotRogueAI
            ├─ PlayerbotPriestAI
            ├─ PlayerbotShamanAI
            ├─ PlayerbotMageAI
            ├─ PlayerbotWarlockAI
            ├─ PlayerbotDruidAI
            └─ PlayerbotDeathKnightAI
```

### Bot states

| State | Meaning |
|-------|---------|
| `BOTSTATE_LOADING` | Loading during world load or zone transition |
| `BOTSTATE_NORMAL` | Idle — normal AI routines processed |
| `BOTSTATE_COMBAT` | In combat |
| `BOTSTATE_DEAD` | Dead, waiting to become ghost |
| `BOTSTATE_DEADRELEASED` | Released as ghost, waiting to revive |
| `BOTSTATE_LOOTING` | Looting after combat |
| `BOTSTATE_FLYING` | Flying (taxi/flight path) |
| `BOTSTATE_TAME` | Hunter taming |
| `BOTSTATE_DELAYED` | Delayed action (crafting etc.) |

### Combat orders

16 combat order types including: TANK, HEAL, ASSIST, PROTECT and group coordination flags like `ORDERS_TEMP_WAIT_TANKAGGRO`.

### Job roles (priority order)

Main Tank → Tank → Healer → Main DPS → DPS → Master → All

### Key files

All paths relative to `Source/mangos-wotlk/src/game/PlayerBot/`:

| File | What |
|------|------|
| `Base/PlayerbotAI.h` | Core header — enums, constants, class def (2300+ lines) |
| `Base/PlayerbotAI.cpp` | Core implementation — `UpdateAI()`, state machine, combat logic |
| `Base/PlayerbotMgr.h/.cpp` | Bot manager — login/logout bots, route commands |
| `Base/PlayerbotClassAI.h/.cpp` | Abstract class AI interface |
| `AI/Playerbot*AI.h/.cpp` | 10 class-specific AIs (spell selection, role behavior) |
| `config.h.in` | Compile-time config |
| `playerbot.conf.dist.in` | Runtime config template |

### Runtime config (playerbot.conf)

| Setting | Default | What |
|---------|---------|------|
| `PlayerbotAI.DisableBots` | 0 | Master switch |
| `PlayerbotAI.MaxNumBots` | 9 | Max bots per account |
| Collection flags | varies | Auto-collect combat/quest/loot items |
| `PlayerbotAI.SellGarbage` | 0 | Auto-sell gray items |

---

## Boss Encounters — How They Work

*(To be filled in Phase 2)*

### Pattern

Boss scripts live in `src/game/AI/ScriptDevAI/scripts/`. Each boss is a `CreatureAI` subclass with:
- `UpdateAI(uint32 diff)` — tick function with spell timers
- Phase transitions based on HP thresholds or timers
- Spell casts on timers or triggered by events

### Broggok (Blood Furnace)

*(To be filled when we study this encounter)*

- Location: Hellfire Citadel → Blood Furnace (TBC dungeon)
- Script path: `src/game/AI/ScriptDevAI/scripts/outland/hellfire_citadel/blood_furnace/` (to confirm)
- Key mechanics: waves of adds, poison bolt (AoE), bomb mechanic on random player

---

## Config Reference

### Useful mangosd.conf log filters

```
LogFilter_AIAndMovegens = 0    # AI decisions and movement
LogFilter_Combat = 0           # Combat calculations
LogFilter_SpellCast = 0        # Spell cast processing
LogFilter_Damage = 0           # Damage calculations
LogFilter_PlayerStats = 0      # Player stat changes
```

Set to `1` to **suppress** that log category. Set to `0` to see it.

### Quick log commands

```bash
# Follow server log, filter for playerbot
tail -f Server.log | grep -i playerbot

# Follow with multiple filters
tail -f Server.log | grep -iE "playerbot|combat|spell"
```

---

## CMake Build Flags

| Flag | Default | Notes |
|------|---------|-------|
| `BUILD_GAME_SERVER` | ON | World server (mangosd) |
| `BUILD_LOGIN_SERVER` | ON | Auth/realm server (realmd) |
| `BUILD_SCRIPTDEV` | ON | Boss/encounter scripts. Turn OFF for faster dev builds |
| `BUILD_DEPRECATED_PLAYERBOT` | OFF | **Our playerbot** — must be ON |
| `BUILD_PLAYERBOTS` | OFF | New playerbot mod — keep OFF |
| `BUILD_AHBOT` | OFF | Auction house bot |
| `BUILD_EXTRACTORS` | OFF | Map/DBC extractors |
| `PCH` | ON | Precompiled headers (speeds compile) |
| `DEBUG` | OFF | Full debug build |

### Mint-specific

- CMake 3.30+ requires: `-DCMAKE_POLICY_DEFAULT_CMP0167=OLD` (for Boost static linking)
- Build dependencies: `build-essential cmake git libboost-dev libssl-dev zlib1g-dev libmariadb-dev libreadline-dev libbz2-dev`
- Runtime dependency for the installed binary: `libmariadb3`

---

## Things We Keep Looking Up

*(Add entries here when you catch yourself searching for the same thing repeatedly)*

<!-- Example format:
### How to restart just the world server without rebuilding
Kill mangosd, re-run `./mangosd -c ../etc/mangosd.conf` from Run_linux/bin/

### Where are spell IDs defined?
DBCs (client data files) + `Spell.dbc` table. In code: `SpellEntry` struct.
-->
