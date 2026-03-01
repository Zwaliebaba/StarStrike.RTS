# StarStrike.RTS – Architecture

## Overview

StarStrike.RTS is structured as three layered CMake sub-projects. Each layer depends only
on the layer below it; higher layers are never referenced by lower ones.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         StarStrike.exe  (Game Logic)                             │
│                                                                                  │
│  WinMain · Init · Loop · Game (save/load)                                       │
│  ─────────────────────────────────────────────────────────────────────────────  │
│  Simulation    │  Combat & Physics  │  Economy        │  AI & Scripting          │
│  Objects       │  Weapons           │  Power          │  AI                      │
│  Droid         │  Combat            │  Factory        │  ScriptAI                │
│  Structure     │  Projectile        │  Research       │  ScriptFuncs/CB/Obj      │
│  Feature       │                    │  Stats · Data   │  CmdDroid                │
│  ─────────────────────────────────────────────────────────────────────────────  │
│  Movement      │  Visibility        │  Rendering      │  UI & Input              │
│  Move          │  Visibility        │  Display3D      │  HCI                     │
│  Findpath      │  AdvVis            │  Bucket3d       │  FrontEnd                │
│  AStar         │  Radar             │  Effects        │  Widget (via NeuronClient)│
│  Formation     │  Sensor            │  Lighting       │  IntOrder · IntelMap     │
│  ─────────────────────────────────────────────────────────────────────────────  │
│  Multiplayer   │  Mission/Campaign  │  Config         │  Audio                   │
│  Multiplay     │  Mission · Levels  │  Config         │  Aud · AudioId           │
│  Multirecv     │  Script (custom)   │  WarzoneConfig  │                          │
│  MultiMenu     │                    │  ClParse        │                          │
├──────────────────────────────────────────────────────────────────────────────────┤
│                     NeuronClient  (Engine / UI Static Library)                   │
│                                                                                  │
│  Framework/         ivis02/             Widget/          Gamelib/                │
│  ┌──────────┐       ┌─────────────┐    ┌────────────┐   ┌─────────────┐        │
│  │ Memory   │       │ D3D9 Init   │    │ Buttons    │   │ Animation   │        │
│  │ Input    │       │ D3D9 Render │    │ Dialogs    │   │ Hash tables │        │
│  │ Surfaces │       │ PieMatrix   │    │ Lists      │   │ File parser │        │
│  │ Fonts    │       │ BSP/IMD     │    │ Events     │   │ (Lex/Yacc)  │        │
│  │ Debug    │       │ Textures    │    └────────────┘   └─────────────┘        │
│  │ Trig     │       │ FBF/PCX     │                                             │
│  │ Fractions│       └─────────────┘    Script/          Sound/     Netplay/    │
│  └──────────┘                          ┌────────────┐   ┌───────┐  ┌────────┐  │
│                                        │ Lex/Yacc   │   │DSound │  │DPlay   │  │
│                                        │ Interpreter│   │QMixer │  │Mplayer │  │
│                                        └────────────┘   └───────┘  └────────┘  │
├──────────────────────────────────────────────────────────────────────────────────┤
│                         NeuronCore  (Foundation)                                 │
│                         NeuronCore.cpp / NeuronCore.h                           │
├──────────────────────────────────────────────────────────────────────────────────┤
│           Windows SDK  ·  DirectX 9 SDK (DX9/)  ·  QMixer.lib  ·  Mplayer.lib  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Layers

### NeuronCore

The minimal foundation with no external dependencies. Provides a clean
initialisation/shutdown entry point (`NeuronCore_Init`, `NeuronCore_Shutdown`) and
baseline definitions consumed by NeuronClient.

```
NeuronCore/
├── NeuronCore.h     # Public API
└── NeuronCore.cpp   # Implementation
```

### NeuronClient

A static library compiled from seven subdirectories. All include paths are exported
as `PUBLIC` so StarStrike inherits them automatically. The library links NeuronCore
transitively.

```
NeuronClient/
├── Framework/    # OS-level abstractions
├── ivis02/       # Direct3D 9 rendering
├── Widget/       # UI component library
├── Gamelib/      # Animation, hash tables, file parsing
├── Script/       # Custom scripting language runtime
├── Sound/        # Audio management
└── Netplay/      # Multiplayer networking
```

`ivis02/` uses `file(GLOB CONFIGURE_DEPENDS ...)` – deleting a source file automatically
removes it from the build without editing CMakeLists.txt.

### StarStrike

The WIN32 executable. Links NeuronClient (static) plus Direct3D 9, DirectPlay,
DirectSound, DirectInput, QMixer, and Mplayer libraries.

---

## Subsystem Data Flow

### Game Loop

```
WinMain
  └─► Init.cpp          # One-time startup: graphics mode, DirectPlay, stats, map
        └─► Loop.cpp    # Per-frame driver
              ├─► AI.cpp / ScriptAI.cpp    # NPC decision-making
              ├─► Action.cpp               # Process queued unit orders
              ├─► Move.cpp                 # Apply velocity, collision, terrain
              ├─► Projectile.cpp           # Ballistic simulation
              ├─► Visibility.cpp           # Fog-of-war update
              ├─► Display3D.cpp            # 3D scene render (via ivis02)
              │     └─► Bucket3d.cpp       # Depth-sort objects before D3D submission
              └─► HCI.cpp                  # Mouse/keyboard → orders
```

### Rendering Pipeline

```
Display3D.cpp
  └─► ivis02/D3drender.cpp        # Submit vertex buffers to D3D9
        ├─► ivis02/Bspimd.cpp     # BSP+IMD 3D mesh transform & clip
        │     └─► ivis02/PieMatrix.cpp  # 12.12 fixed-point transforms (→ DirectXMath)
        └─► ivis02/Fbf.cpp        # Texture page (FBF) atlas management
```

> **Active migration:** The 12.12 fixed-point math layer (`PieMatrix.cpp`,
> `PieTypes.h`, `Fractions.h`) is being replaced with DirectXMath. See
> [`MathConv.md`](MathConv.md) for the phased migration plan.

### Pathfinding

```
HCI.cpp  (player issues move order)
  └─► Action.cpp  (enqueue DACTION_MOVE)
        └─► Findpath.cpp  (request path)
              ├─► AStar.cpp         # Core A* grid search
              ├─► GatewayRoute.cpp  # High-level gateway graph search
              └─► OptimisePath.cpp  # Post-process path for smoothing
                    └─► Move.cpp    # Step-by-step execution each frame
```

### Multiplayer Synchronisation

```
Multiplay.cpp
  ├─► Multirecv.cpp   # Receive and apply remote packets
  ├─► Multigifts.cpp  # Resource trading
  ├─► Multijoin.cpp   # Lobby and session join
  └─► NeuronClient/Netplay/  # DirectPlay session management (Mplayer.lib)
```

### Scripting

```
GameData/script/*.script
  └─► NeuronClient/Script/   # Lex/Yacc parser + interpreter
        ├─► ScriptFuncs.cpp  # Bound C++ functions callable from scripts
        ├─► ScriptCB.cpp     # Event callbacks (unit killed, research complete, …)
        └─► ScriptObj.cpp    # Game object bindings exposed to scripts
```

---

## Key Data Structures

| Structure | Defined in | Role |
|---|---|---|
| `BASE_OBJECT` | `Objects.h` | Base type for all game entities |
| `DROID` | `DroidDef.h` | Unit (inherits BASE_OBJECT) |
| `STRUCTURE` | `StructureDef.h` | Building (inherits BASE_OBJECT) |
| `FEATURE` | `FeatureDef.h` | Terrain feature (inherits BASE_OBJECT) |
| `PROJECTILE` | `Projectile.h` | Bullet/missile in flight |
| `SDMATRIX` | `ivis02/PieMatrix.h` | 3×4 transform, rotation in 12.12 fixed-point |
| `iVector` | `ivis02/PieTypes.h` | 3-component integer vector |
| `STATS_TEMPLATE` | `StatsDef.h` | Unit design blueprint |

Objects are managed in pre-allocated arrays with free lists – no heap allocation per
entity at runtime.

---

## Asset Pipeline

```
GameData/Stats/*.txt          ──► Stats.cpp / Data.cpp   (loaded once at startup)
GameData/script/*.script      ──► Script/ interpreter    (parsed at mission start)
GameData/Images/texpages/     ──► ivis02/Fbf.cpp         (texture atlas upload to D3D)
GameData/components/*.imd     ──► ivis02/Bspimd.cpp      (3D mesh load)
GameData/WRF/Cam1/*.wrf       ──► Levels.cpp / Mission.cpp (mission data)
GameData/audio/*.wav          ──► Sound/Aud.cpp           (DirectSound buffers)
```

---

## Active Migrations

### 1. DirectX-Only Renderer

Three legacy backends are being removed; Direct3D 9 is the sole renderer.

```
ivis02/
├── V4101.cpp  [REMOVE]  640×480×256 legacy mode
├── Vsr.cpp    [REMOVE]  Software rasterizer
├── Amd3d.h    [REMOVE]  AMD 3DNow SIMD hints (obsolete)
├── Dglide.h   [REMOVED] 3Dfx Glide (already excluded from build)
└── D3drender.cpp  [KEEP]  Active D3D9 backend
```

### 2. Fixed-Point → DirectXMath

The 12.12 fixed-point math in `ivis02/PieMatrix.cpp` and `Framework/Fractions.h` is
being replaced with the Windows SDK `<DirectXMath.h>` header. The migration is phased
over seven steps; see [`MathConv.md`](MathConv.md).

---

## Platform Notes

- **Windows only.** All platform-specific code is guarded with `#ifdef WIN32`.
- **PSX legacy.** Dormant PlayStation code remains behind `#ifdef PSX` – do not delete
  or modify these blocks.
- **x86 and x64.** Both architectures are supported via CMake presets. Inline x87
  assembly (`MAKEINT` in `Fractions.h`) is incompatible with x64 and will be removed
  during the DirectXMath migration.
- **`Sequence/` disabled.** The cutscene subsystem uses a proprietary RPL codec that
  does not build on modern toolchains; it is excluded from the active build.
