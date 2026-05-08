# Copilot Instructions — CMaNGOS WotLK Project

## Project context

This workspace contains files for a private CMaNGOS WoW server project (Classic, TBC, WotLK expansions). The active work is on the **WotLK** expansion, specifically the **deprecated PlayerBot system**.

The developer is an experienced TypeScript/Go engineer learning C++ through this codebase. Explanations should bridge from modern language concepts where helpful. Do not over-explain basic programming — focus on C++ specifics and CMaNGOS patterns.

## First thing every conversation

At the start of each new conversation, read these files to understand current status and accumulated knowledge:

1. `PLAN.md` — living project plan with phase tracking, current status, and session log
2. `KNOWLEDGE.md` — architecture reference, build commands, key file paths, and accumulated findings
3. `PLAYERBOTS.md` — PlayerBot-specific notes on bot states and behavior

## Key facts

- **Server type:** CMaNGOS WotLK (C++20, CMake build system)
- **PlayerBot:** Using `BUILD_DEPRECATED_PLAYERBOT=ON` (not the newer mod)
- **OS:** Bazzite (Fedora-based Linux)
- **IDE:** VS Code with clangd
- **Build type:** RelWithDebInfo
- **Database:** MariaDB on separate LAN machine (192.168.1.197)
- **Source repo:** This workspace root (git, personal fork)
- **Build output:** `../../Build_linux/` (relative to workspace root)
- **Runtime:** `../../Run_linux/` (relative to workspace root)

## Code conventions

- Follow existing CMaNGOS code style (no `.clang-format` exists — match surrounding code)
- PlayerBot files are under `src/game/PlayerBot/` (Base/ for core, AI/ for class-specific)
- When modifying PlayerBot code, keep changes isolated to minimize upstream merge conflicts
- The deprecated playerbot is intentionally chosen to avoid merge conflicts — do not suggest migrating to the new system

## When helping with C++

- This is C++20 but the codebase uses older patterns (raw pointers, C-style casts in places). Don't modernize code outside the scope of the current task.
- Explain CMaNGOS-specific patterns (update loops, packet handlers, AI hooks) when encountered for the first time — add explanations to KNOWLEDGE.md for future reference.
- When suggesting code changes, always mention which file(s) and roughly where (function name or nearby code).

## Session workflow

- After completing work, update `PLAN.md` status and session log
- Add new findings to `KNOWLEDGE.md`
- Keep things concise — the developer has very limited time
