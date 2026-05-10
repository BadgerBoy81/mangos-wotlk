# CMaNGOS WotLK — Project Plan

Living document. Updated after each working session.

## Background

Private CMaNGOS WotLK server project. Focus: learn the C++ codebase, understand and refactor the deprecated PlayerBot system, extend it to handle boss encounters with role-aware bots. Running on Linux Mint, previously Bazzite and Windows/Visual Studio.

The server is built and running on Linux. Database is on a separate LAN machine (192.168.1.197).

---

## Current Status

**Active phase:** Phase 3 — Deep Dive into Deprecated PlayerBot
**Last session:** 2026-05-10 — Completed deep mapping of the deprecated PlayerBot system, including update loop, command routing, combat flow, login/session flow, and class AI behavior.

---

## Phase 0: Ways of Working ✅

> Establish a system so we can pick this up after weeks/months away.

- [x] Created `PLAN.md` (this file) — living project plan with status tracking
- [x] Created `KNOWLEDGE.md` — reference doc for architecture, findings, and things we look up often
- [x] `PLAYERBOTS.md` exists — bot-state notes and PlayerBot-specific knowledge base
- [ ] `PlayerbotUpdateAI.drawio` — update iteratively as understanding grows
- **Convention:** After each session, update status here and add findings to KNOWLEDGE.md

---

## Phase 1: Developer Experience Setup

> Comfortable edit → build → run → inspect loop on Linux Mint.

### Steps

- [x] **1.1 Workspace restructuring**
  - Multi-root workspace: `cmangos-wotlk.code-workspace` in WotLK/
  - Primary root: `Source/mangos-wotlk` (C++ source, git)
  - Secondary: `Run_linux/etc` (server configs)
  - Secondary: `DB_Scripts` (SQL)
  - Living docs (PLAN.md, KNOWLEDGE.md, PLAYERBOTS.md) moved here, gitignored
  - copilot-instructions.md in `.github/`

- [x] **1.2 Generate `compile_commands.json`** *(critical for IntelliSense)*
  - Re-run cmake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` in `Build_linux/`
  - Symlink `Build_linux/compile_commands.json` → `Source/mangos-wotlk/compile_commands.json`

- [x] **1.3 VS Code extensions**
  - **clangd** — C++ IntelliSense via compile_commands.json
  - **CMake Tools** — build/configure integration
  - **CodeLLDB** — Linux C++ debugger

- [x] **1.4 Build tasks** (`.vscode/tasks.json`)
  - Build: `cmake --build <Build_linux> -- -j$(nproc)`
  - Install: `cmake --install <Build_linux>`
  - Compound: Build & Install
  - Ctrl+Shift+B → build

- [x] **1.5 Launch/Debug config** (`.vscode/launch.json`)
  - Launch `Run_linux/bin/mangosd` under LLDB
  - Working directory: `Run_linux/bin/`
  - Args: `-c ../etc/mangosd.conf`
  - Current build type `RelWithDebInfo` works (debug symbols + near-release speed)

- [ ] **1.6 Log management**
  - Learn `mangosd.conf` log filters (`LogFilter_AIAndMovegens`, `LogFilter_Combat`, etc.)
  - Quick filter: `tail -f Server.log | grep -i playerbot`
  - Later: consider PlayerBot-specific log channel (code change)

### Done when

- [x] Go-to-definition on `PlayerbotAI` works in VS Code
- [x] Build from VS Code completes successfully
- [x] Can hit a breakpoint in `PlayerbotAI::UpdateAI()` or `Player::Update()`
- [x] `compile_commands.json` picked up by clangd

---

## Phase 2: Codebase High-Level Understanding

> Mental model of CMaNGOS architecture — know where to look for anything.

### Steps

- [ ] **2.1 Study architecture layers** (document findings in KNOWLEDGE.md)
  1. Entry point → World loop → Map system → Object hierarchy
  2. Player session & packet handling
  3. Spell system
  4. AI system (creatures + bots)
  5. Movement & pathfinding
  6. ScriptDev2 (boss scripts)
  7. Database & DBCs

- [x] **2.2 Trace how PlayerBot hooks into the world loop**
  - `World::Update()` → `Map::Update()` → `Player::Update()` → `PlayerbotAI::UpdateAI()`

- [ ] **2.3 Study how boss encounters work**
  - ScriptDevAI pattern: `CreatureAI` subclass, `UpdateAI()`, spell timers, phases
  - Find and read the Broggok encounter script

### Done when

- [ ] Can explain the path from `Main.cpp` to a playerbot AI tick
- [ ] Can explain how a spell cast flows through the system
- [ ] Can locate and read the Broggok encounter script
- [ ] Architecture documented in KNOWLEDGE.md

---

## Phase 3: Deep Dive into Deprecated PlayerBot

> Complete, documented understanding of the bot system.

### Steps

- [x] **3.1 Map full class hierarchy**
  - `PlayerbotMgr` → `PlayerbotAI` → `PlayerbotClassAI` → 10 class AIs

- [x] **3.2 Trace UpdateAI flow in detail**
  - State machine: LOADING → NORMAL → COMBAT → DEAD → LOOTING → etc.
  - Per-state behavior, combat orders, class AI delegation

- [x] **3.3 Map communication system**
  - Chat commands → `PlayerbotMgr` → bots
  - `HandleBotOutgoingPacket` — how world events reach bots

- [x] **3.4 Map combat system**
  - Combat orders (TANK/HEAL/ASSIST/PROTECT), threat management
  - Spell selection per class, group coordination

- [x] **3.5 Document pain points and refactoring targets**
  - Code smells, hardcoded values, unclear flow
  - Note specific file/line references

### Done when

- [x] Can draw full bot lifecycle (load → idle → combat → loot → idle)
- [x] Can trace "healer casts heal on tank taking damage" through the code
- [x] Pain points documented with file/line references
- [ ] `PlayerbotUpdateAI.drawio` updated with accurate flow

---

## Phase 4: PlayerBot Refactoring

> Readable, maintainable code — same behavior, clearer structure.

### Steps

- [ ] **4.1 Define scope** — same external interface, same cmake flag, internal clarity only
- [ ] **4.2 Refactoring targets** *(filled after Phase 3)*
  - Break up `UpdateAI()` into state-specific handlers
  - Extract combat decision logic
  - Standardize class AI patterns
  - Add structured logging with PlayerBot tag
- [ ] **4.3 Execute** — one refactoring per commit, test after each

### Done when

- [ ] Bots behave identically to pre-refactor
- [ ] Each method's purpose explainable in one sentence
- [ ] No merge conflicts with upstream

---

## Phase 5: Boss Fight Milestone — Blood Furnace: Broggok

> Party of 1 human + 4 bots clears Broggok with bots handling mechanics.

### Steps

- [ ] **5.1 Understand encounter** — Poison Bolt (AoE), waves of adds, bomb mechanic (DoT requiring movement). Verify spell IDs from encounter script.
- [ ] **5.2 Define role behaviors**
  - Tank: hold boss/adds, position
  - Healer: heal through AoE, extra on bomb target
  - DPS: focus target, interrupt
  - All: react to bomb — move away from group (pathfinding)
- [ ] **5.3 Implement encounter awareness** — detect debuffs, role-specific responses
- [ ] **5.4 Iterate** — start with tank+healer on trash, add bomb reaction, test

### Done when

- [ ] Full Broggok clear with 1 human + 4 bots
- [ ] Tank holds aggro, healer keeps party alive, DPS contributes
- [ ] Bomb mechanic handled (bot moves, healer compensates)
- [ ] No bot deaths from preventable mechanics

---

## Decisions

| Decision | Rationale |
|----------|-----------|
| Use deprecated playerbot | Avoids merge conflicts with upstream; already familiar |
| Workspace root = `mangos-wotlk` | Best for C++ IntelliSense, git, and navigation |
| Build type = `RelWithDebInfo` | Debug symbols + near-release speed |
| Start with log-based debugging | Log filtering first, LLDB for deep inspection |
| Boss target = Broggok | Clear mechanics (bomb, adds, positional) suitable for role-based bot AI |
| Refactor before extend | Understand and clean first, then add features |

## Open Items

- **Upstream merge cadence** — monthly or quarterly (deprecated playerbot = minimal conflicts)
- **Testing approach** — manual in-game for now; consider test harness for combat logic in Phase 4+
- **Build optimization** — PCH is ON, ccache detected; if build times are painful, investigate further

---

## Session Log

| Date | Summary |
|------|---------|
| 2026-04-19 | Initial planning session. Created PLAN.md, KNOWLEDGE.md. Explored codebase structure, PlayerBot files, Linux build setup. Defined 6-phase plan with Broggok milestone. Completed Phase 0. Started Phase 1: created multi-root workspace, moved docs and copilot-instructions into mangos-wotlk root. |
| 2026-05-04 | Migrated from Bazzite to Linux Mint. Verified `Run_linux/bin/mangosd` exists. Identified missing runtime library `libmariadb3` and updated docs for Mint package names. |
| 2026-05-08 | Validated VS Code workflow: `clangd` IntelliSense, `CMake Tools` configure/build, and `CodeLLDB` breakpoint debugging all working. |
| 2026-05-06 | Generated `compile_commands.json`, created `.vscode/tasks.json` and `.vscode/launch.json`, and linked compile commands into the workspace. |
| 2026-05-10 | Completed PlayerBot knowledge mapping through `PlayerbotAI::UpdateAI()`, bot login/command flow, combat/loot state transitions, and class AI behavior. |
| 2026-05-10 | Mapped `PlayerbotAI` update flow, state machine behavior, combat/loot loops, and class-specific idle/combat hooks. |
