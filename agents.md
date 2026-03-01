# StarStrike.RTS – Agent Reference

## Project Overview

StarStrike.RTS is a real-time strategy (RTS) game engine written in **C++23** targeting
Windows. It features unit control, base building, a research/tech tree, fog-of-war,
pathfinding, DirectPlay multiplayer, and save/load support. The codebase is approximately
**600 files / 523 000 lines**.

## Architecture

The project has three layers, each a CMake sub-project built in dependency order:

```
NeuronCore   →  NeuronClient  →  StarStrike (game executable)
(foundation)    (engine/UI)      (game logic)
```

See [`ARCHITECTURE.md`](ARCHITECTURE.md) for detailed diagrams and subsystem interactions.

### NeuronCore (`NeuronCore/`)

Minimal foundation: `NeuronCore.cpp` / `NeuronCore.h`. No external dependencies.

### NeuronClient (`NeuronClient/`)

Engine-level static library compiled from seven subdirectories:

| Subdirectory | Purpose |
|---|---|
| `Framework/` | Platform abstraction (memory, input, surfaces, fonts, debug) |
| `ivis02/` | Graphics rendering – Direct3D 9 (sole active backend) |
| `Widget/` | UI widget system (buttons, dialogs, lists) |
| `Gamelib/` | Utilities: animation, hashing, Lex/Yacc file parsing |
| `Script/` | Custom script interpreter for game AI/logic |
| `Sequence/` | Video/cutscene sequencing (**DISABLED** – proprietary RPL codec not viable on modern toolchains) |
| `Sound/` | Audio (DirectSound + QMixer) |
| `Netplay/` | Network multiplayer (DirectPlay) |

### StarStrike (`StarStrike/`)

Main game executable (~122 `.cpp` source files). Key files:

| File | Purpose |
|---|---|
| `Init.cpp` | System and game initialisation (2 564 lines) |
| `Action.cpp` | Command/action processing (3 254 lines) |
| `AI.cpp` | AI decision-making (842 lines) |
| `AStar.cpp` | A* pathfinding |
| `Move.cpp` | Unit movement |
| `Objects.cpp` / `ObjectDef.cpp` | Game object management |
| `Droid.cpp` | Unit (droid) behaviour |
| `Structure.cpp` | Building logic |
| `Projectile.cpp` | Bullet physics |
| `Weapons.cpp` | Weapon behaviour |
| `Research.cpp` | Tech-tree |
| `Factory.cpp` | Unit production |
| `Visibility.cpp` | Fog of war |
| `Power.cpp` | Power resource system |
| `Radar.cpp` | Radar/minimap display |
| `Game.cpp` | Save/load (version ≥ 32 format) |
| `Display.cpp` / `Display3D.cpp` | 3D rendering coordination |
| `HCI.cpp` | Human–computer interface / input |
| `FrontEnd.cpp` | Main menu |
| `Stats.cpp` | Unit/building stats and data |
| `Config.cpp` | Configuration management |
| `WarzoneConfig.cpp` | Runtime game settings |

## Technology Stack

| Component | Details |
|---|---|
| **Language** | C++23 (MSVC, no exceptions/RTTI in game logic) |
| **Build system** | CMake ≥ 3.12, Ninja generator, MSVC |
| **Graphics** | DirectX 9 (Direct3D) via `DX9/` bundled SDK |
| **Audio** | DirectSound + QMixer.lib |
| **Networking** | DirectPlay (dplayx.lib) + Mplayer.lib |
| **Input** | DirectInput |
| **Math** | 12.12 fixed-point (migrating to DirectXMath – see `MathConv.md`) |
| **Platform** | Windows only (WIN32 guards throughout) |
| **Legacy** | PSX code dormant behind `#ifdef PSX` – leave alone |

## Build System

```bash
# Configure (from repo root, Windows with MSVC + Ninja in PATH)
cmake --preset x64-debug        # or x64-release, x86-debug, x86-release

# Build
cmake --build out/build/x64-debug

# Verify zero errors before committing renderer-related changes
cmake --build out/build/x64-debug 2>&1 | grep -i error
```

Available presets (see `CMakePresets.json`):

| Preset | Architecture | Config |
|---|---|---|
| `x64-debug` | 64-bit | Debug |
| `x64-release` | 64-bit | Release |
| `x86-debug` | 32-bit | Debug |
| `x86-release` | 32-bit | Release |

Source files in `NeuronClient/ivis02/` are collected with `file(GLOB …)` in
`NeuronClient/CMakeLists.txt` — **deleting a file is enough to remove it from
the build**; no CMake edit is needed.

## Coding Conventions

See [`CODE_STANDARDS.md`](CODE_STANDARDS.md) for the full style guide. Quick reference:

- **C++23** compiled via MSVC – game logic avoids exceptions and RTTI; engine code may
  use standard library headers where appropriate.
- Naming follows the existing file's style: `camelCase` for functions/variables,
  `UPPER_SNAKE` for enums and macros.
- Pointer variables are prefixed `ps` (e.g. `psMatrix`, `psDroid`).
- Avoid introducing new platform abstractions; use the existing `Framework/` layer.
- Guard Windows-specific code with `#ifdef WIN32`.

## Active Migrations

### 1. DirectX-Only Renderer

The render subsystem is being standardised on Direct3D. Three legacy backends are being
removed:

| Backend | Constant | Status |
|---|---|---|
| Software surface | `ENGINE_SR` / `iV_MODE_SURFACE` | **Remove** |
| DDX 640×480×256 | `ENGINE_4101` / `iV_MODE_4101` | **Remove** |
| 3Dfx / Glide | `ENGINE_GLIDE` | Already excluded from build; sweep remaining symbols |
| **Direct3D** | `ENGINE_D3D` | **Keep – sole renderer** |

Grep targets (all must reach zero hits outside comments):

```
ENGINE_4101  ENGINE_SR  ENGINE_GLIDE
iV_MODE_4101  iV_MODE_SURFACE
REND_MODE_SOFTWARE  REND_MODE_GLIDE
_4101  _sr  weHave3DNow  cpuHas3DNow
```

Files targeted for deletion:

- `NeuronClient/ivis02/V4101.cpp` (~2 996 lines)
- `NeuronClient/ivis02/V4101.h`
- `NeuronClient/ivis02/Vsr.cpp` (~1 724 lines)
- `NeuronClient/ivis02/Vsr.h`
- `NeuronClient/ivis02/Amd3d.h`

**Known risk:** `IntDisplay.cpp` and `MapDisplay.cpp` temporarily switch to software mode
for radar/minimap blitting via `iV_RenderAssign(iV_MODE_4101, …)`. Verify the Direct3D
path handles the same blits correctly before deleting.

### 2. DirectXMath Migration

The fixed-point 12.12 math subsystem is being replaced with DirectXMath (Windows SDK
header-only, SIMD-accelerated). See **[`MathConv.md`](MathConv.md)** for the full
seven-phase migration plan.

## Game Data

Asset files live in `GameData/` and are loaded at runtime – do not modify binary asset
formats without updating the corresponding loader in `Stats.cpp` / `Data.cpp`.

| Directory | Contents |
|---|---|
| `GameData/Stats/` | Unit/building stat tables (.txt) |
| `GameData/script/` | Game scripts (custom Lex/Yacc language) |
| `GameData/audio/` | Sound effects and music |
| `GameData/Images/` | Textures and sprites |
| `GameData/WRF/` | Mission files (.wrf – Warzone Rescue Format) |
| `GameData/savegame/` | Save-game slot data |

## Git Workflow

- Main integration branch: `master`
- Feature work branches follow `feature/<description>` convention.
- Commit messages are short imperative sentences (see `git log --oneline`).
- Remote: `http://local_proxy@127.0.0.1:50433/git/Zwaliebaba/StarStrike.RTS`
