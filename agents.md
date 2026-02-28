# StarStrike.RTS – Agent Reference

## Project Overview

StarStrike.RTS is a real-time strategy (RTS) game engine written entirely in **C (C11)** targeting Windows. It features unit control, base building, a research/tech tree, fog-of-war, pathfinding, multiplayer via DirectPlay, and save/load support. The codebase is approximately **600 files / 523 000 lines of C**.

## Architecture

The project has three layers, each a CMake sub-project built in dependency order:

```
NeuronCore   →  NeuronClient  →  StarStrike (game executable)
(foundation)    (engine/UI)      (game logic)
```

### NeuronCore (`NeuronCore/`)
Minimal foundation: `NeuronCore.c` / `NeuronCore.h`.

### NeuronClient (`NeuronClient/`)
Engine-level libraries:

| Subdirectory | Purpose |
|---|---|
| `Framework/` | Platform abstraction (memory, input, surfaces, fonts, debug) |
| `ivis02/` | Graphics rendering – Direct3D 9 (sole active backend) |
| `Widget/` | UI widget system (buttons, dialogs, lists) |
| `Gamelib/` | Utilities: animation, hashing, file parsing |
| `Script/` | Custom script interpreter for game AI/logic |
| `Sequence/` | Video/cutscene sequencing |
| `Sound/` | Audio (DirectSound + QMixer) |
| `Netplay/` | Network multiplayer (DirectPlay) |

### StarStrike (`StarStrike/`)
Main game executable (~122 C source files). Key files:

| File | Purpose |
|---|---|
| `Init.c` | System and game initialisation (2 564 lines) |
| `Action.c` | Command/action processing (3 254 lines) |
| `AI.c` | AI decision-making (842 lines) |
| `AStar.c` | A* pathfinding |
| `Move.c` | Unit movement |
| `Objects.c` / `ObjectDef.c` | Game object management |
| `Droid.c` | Unit (droid) behaviour |
| `Structure.c` | Building logic |
| `Projectile.c` | Bullet physics |
| `Weapons.c` | Weapon behaviour |
| `Research.c` | Tech-tree |
| `Factory.c` | Unit production |
| `Visibility.c` | Fog of war |
| `Power.c` | Power resource system |
| `Radar.c` | Radar/minimap display |
| `Game.c` | Save/load (version ≥ 32 format) |
| `Display.c` / `Display3D.c` | 3D rendering coordination |
| `HCI.c` | Human–computer interface / input |
| `FrontEnd.c` | Main menu |
| `Stats.c` | Unit/building stats and data |
| `Config.c` | Configuration management |
| `WarzoneConfig.c` | Runtime game settings |

## Technology Stack

| Component | Details |
|---|---|
| **Language** | C11 (no C++) |
| **Build system** | CMake ≥ 3.12, Ninja generator, MSVC |
| **Graphics** | DirectX 9 (Direct3D) via `DX9/` bundled SDK |
| **Audio** | DirectSound + QMixer.lib |
| **Networking** | DirectPlay (dplayx.lib) + Mplayer.lib |
| **Input** | DirectInput |
| **Platform** | Windows only (WIN32 guards throughout) |
| **Legacy** | PSX code dormant behind `#ifdef PSX` – leave alone |

## Build System

```bash
# Configure (from repo root, Windows with MSVC + Ninja)
cmake --preset <preset-name>   # see CMakePresets.json for available presets

# Build
cmake --build build/ --config Release

# Verify zero errors before committing renderer-related changes
cmake --build build/ 2>&1 | grep -i error
```

Source files in `NeuronClient/ivis02/` are collected with `file(GLOB …)` in
`NeuronClient/CMakeLists.txt` — **deleting a file is enough to remove it from
the build**; no CMake edit is needed.

## Coding Conventions

- **Pure C11** – no C++ constructs, no `//` comments in older files (use `/* */`).
- Naming follows the existing file's style: `camelCase` for functions/variables, `UPPER_SNAKE` for enums and macros.
- Avoid introducing new platform abstractions; use the existing `Framework/` layer.
- Do not add C++ headers or link C++ runtime.
- Guard Windows-specific code with `#ifdef WIN32` (already universal here, but keep it explicit for future portability).

## Active Migration: DirectX-Only Renderer

> **See `DirectXMig.md` for the full plan.**

The render subsystem is being standardised on Direct3D. Three legacy backends are being removed:

| Backend | Constant | Status |
|---|---|---|
| Software surface | `ENGINE_SR` / `iV_MODE_SURFACE` | **Remove** |
| DDX 640×480×256 | `ENGINE_4101` / `iV_MODE_4101` | **Remove** |
| 3Dfx / Glide | `ENGINE_GLIDE` | Already excluded from build; sweep remaining symbols |
| **Direct3D** | `ENGINE_D3D` | **Keep – sole renderer** |

### What to grep for (all must reach zero hits outside comments)

```
ENGINE_4101  ENGINE_SR  ENGINE_GLIDE
iV_MODE_4101  iV_MODE_SURFACE
REND_MODE_SOFTWARE  REND_MODE_GLIDE
_4101  _sr  weHave3DNow  cpuHas3DNow
```

### Files targeted for deletion

- `NeuronClient/ivis02/V4101.c` (~2 996 lines)
- `NeuronClient/ivis02/V4101.h`
- `NeuronClient/ivis02/Vsr.c` (~1 724 lines)
- `NeuronClient/ivis02/Vsr.h`
- `NeuronClient/ivis02/Amd3d.h`

### Known risk: 2D overlay blitting

`IntDisplay.c` and `MapDisplay.c` temporarily switch to software mode for radar/minimap blitting via `iV_RenderAssign(iV_MODE_4101, …)`. When removing these calls, verify the Direct3D path handles the same blits correctly before deleting.

## Game Data

Asset files live in `GameData/` and are loaded at runtime – do not modify binary asset formats without updating the corresponding loader in `Stats.c` / `Data.c`.

| Directory | Contents |
|---|---|
| `GameData/Stats/` | Unit/building stat tables |
| `GameData/script/` | Game scripts (Lua-like custom language) |
| `GameData/audio/` | Sound effects and music |
| `GameData/Images/` | Textures and sprites |
| `GameData/savegame/` | Save-game slot data |

## Git Workflow

- Main integration branch: `master`
- Feature work branches follow `feature/<description>` convention.
- Commit messages are short imperative sentences (see `git log --oneline`).
- This repo's remote is at `http://local_proxy@127.0.0.1:50433/git/Zwaliebaba/StarStrike.RTS`.
