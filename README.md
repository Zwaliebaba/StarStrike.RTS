# StarStrike.RTS

A Windows real-time strategy (RTS) game engine written in **C++23**, built on Direct3D 9
for rendering, DirectSound for audio, and DirectPlay for multiplayer. The engine spans
approximately **600 files / 523 000 lines** across three layered sub-projects.

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Building](#building)
- [Project Structure](#project-structure)
- [Architecture](#architecture)
- [Documentation](#documentation)
- [Code Standards](#code-standards)
- [Git Workflow](#git-workflow)

---

## Features

- Real-time unit control with A* pathfinding and formation movement
- Base building and structure management
- Research/technology tree progression
- Fog-of-war and line-of-sight system
- Radar/minimap display
- Peer-to-peer multiplayer via DirectPlay
- Campaign scripting via a custom Lex/Yacc language interpreter
- Save/load support (version ≥ 32 format)
- Direct3D 9 rendering pipeline (sole active backend)

---

## Requirements

| Requirement | Version |
|---|---|
| **OS** | Windows (x64 or x86) |
| **Compiler** | MSVC (Visual Studio 2022 or later) |
| **CMake** | ≥ 3.12 |
| **Build tool** | Ninja (in PATH) |
| **SDK** | DirectX 9 SDK – bundled in `DX9/` (no install required) |
| **Windows SDK** | ≥ 8.0 (for DirectXMath headers, used during math migration) |

---

## Building

```bash
# 1. Open a Developer Command Prompt for VS (sets up MSVC + Ninja paths)

# 2. Configure – choose a preset
cmake --preset x64-debug      # Debug, 64-bit (recommended for development)
cmake --preset x64-release    # Release, 64-bit
cmake --preset x86-debug      # Debug, 32-bit
cmake --preset x86-release    # Release, 32-bit

# 3. Build
cmake --build out/build/x64-debug

# 4. Run
out/build/x64-debug/StarStrike/StarStrike.exe
```

Build output lands in `out/build/<preset>/`. The `GameData/` directory must remain
alongside the executable (or be reachable via the working directory) for the game to load
assets.

### Verifying a clean build

```bash
cmake --build out/build/x64-debug 2>&1 | grep -i error
```

Zero output means a clean build.

---

## Project Structure

```
StarStrike.RTS/
├── NeuronCore/          # Layer 1: Foundation library (minimal)
├── NeuronClient/        # Layer 2: Engine/UI static library
│   ├── Framework/       #   Platform abstraction (memory, input, fonts, debug)
│   ├── ivis02/          #   Direct3D 9 rendering pipeline
│   ├── Widget/          #   UI widget system
│   ├── Gamelib/         #   Animation, hashing, file parsing
│   ├── Script/          #   Custom script interpreter
│   ├── Sound/           #   DirectSound + QMixer audio
│   └── Netplay/         #   DirectPlay multiplayer networking
├── StarStrike/          # Layer 3: Game executable (~122 .cpp files)
├── GameData/            # Runtime assets (stats, textures, audio, scripts, missions)
│   ├── Stats/           #   Unit/building definition tables
│   ├── Images/          #   Textures and sprites
│   ├── audio/           #   Sound effects and music
│   ├── script/          #   Campaign and AI scripts
│   └── WRF/             #   Mission files (.wrf)
├── DX9/                 # Bundled DirectX 9 SDK (headers + libs)
├── CMakeLists.txt       # Root CMake configuration (C++23, MSVC)
├── CMakePresets.json    # Build presets (x86/x64 × Debug/Release)
├── agents.md            # Agent/AI coding reference
├── ARCHITECTURE.md      # Detailed architecture and subsystem diagrams
├── CODE_STANDARDS.md    # Coding conventions and style guide
└── MathConv.md          # DirectXMath migration plan (active)
```

---

## Architecture

The engine uses a three-layer dependency model:

```
┌──────────────────────────────────────────────────────────────────────┐
│                    StarStrike.exe  (Game Logic)                      │
│   Init · Loop · AI · Action · Display3D · HCI · Multiplay · ...     │
├────────────┬──────────┬────────┬──────────┬────────┬───────┬─────────┤
│ Framework  │  ivis02  │ Widget │ Gamelib  │ Script │ Sound │ Netplay │
│            │  (D3D9)  │        │          │        │       │         │
│       NeuronClient  (Engine / UI Static Library)                    │
├──────────────────────────────────────────────────────────────────────┤
│                    NeuronCore  (Foundation)                          │
└──────────────────────────────────────────────────────────────────────┘
           Windows SDK  ·  DirectX 9 SDK  ·  QMixer  ·  Mplayer
```

See [`ARCHITECTURE.md`](ARCHITECTURE.md) for full subsystem diagrams and data-flow
descriptions.

---

## Documentation

| File | Purpose |
|---|---|
| [`README.md`](README.md) | This file – quick start and overview |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Detailed architecture diagrams and subsystem descriptions |
| [`CODE_STANDARDS.md`](CODE_STANDARDS.md) | Naming conventions, patterns, and style guide |
| [`agents.md`](agents.md) | Concise reference for AI coding agents |
| [`MathConv.md`](MathConv.md) | Fixed-point → DirectXMath migration plan (active work) |

---

## Code Standards

See [`CODE_STANDARDS.md`](CODE_STANDARDS.md) for the full guide. The short version:

- **Language:** C++23, compiled with MSVC. Game logic avoids exceptions and RTTI;
  use plain C-style structs and function-level encapsulation.
- **Naming:** `camelCase` for functions and variables; `UPPER_SNAKE` for enums and
  macros; `ps` prefix on pointer variables (e.g. `psDroid`, `psMatrix`).
- **Engine abstractions:** Use `Framework/` for memory, input, and surfaces. Do not
  bypass the abstraction layer with direct OS or Win32 calls.
- **Platform guards:** Wrap Windows-specific code in `#ifdef WIN32`.

---

## Git Workflow

- **Main branch:** `master` – integration target for completed features.
- **Feature branches:** `feature/<short-description>` (e.g. `feature/d3d-cleanup`).
- **Commit messages:** Short imperative sentences (`Fix fog-of-war edge case at map edge`).
- **Renderer changes:** Always run a clean build and grep for legacy symbols before
  merging (see `agents.md` → Active Migrations).
