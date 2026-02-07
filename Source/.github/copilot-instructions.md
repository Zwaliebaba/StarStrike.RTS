# Phoenix Engine - AI Coding Agent Instructions

## Project Overview

This is the **Phoenix engine** (internally codenamed "Phoenix"), an RTS game engine originally developed by Ensemble Studios. It's a large-scale C++ codebase targeting Xbox 360 and Win32, built with Visual Studio using `.vcxproj` project files.

## Architecture

### Core Module Hierarchy (Dependencies flow upward)
```
xgame (main game logic) ─┬─ xgameRender (game-specific rendering)
                         ├─ xvisual (visual/animation system)
                         ├─ xphysics (Havok physics integration)
                         ├─ xsound, xparticles, xnetwork, xinputsystem
                         └─ xsystem (file I/O, async, utilities)
                              └─ xcore (containers, memory, threading)
```

- **xcore**: Low-level utilities - custom containers (`BDynamicArray`, `BFreeList`, `BHashMap`), memory heaps, assertions, threading primitives
- **xsystem**: System services - file management (`BFileManager`, `BXFS`), profiling, logging, config
- **xgame**: Game simulation - entities, squads, AI, commands, triggers, multiplayer sync
- **xrender/xgameRender**: D3D9/Xbox 360 rendering, effects, GPU texture management
- **xvisual**: Visual/animation pipeline with Granny integration

### Key Global Singletons
Access via `extern` declarations - see [world.h](xgame/world.h), [game.h](xgame/game.h):
```cpp
extern BGame gGame;           // Main game controller
extern BWorld* gWorld;        // Current simulation world
extern BDatabase gDatabase;   // Proto definitions for objects/squads/techs
extern BFileManager gFileManager;
extern BVisualManager gVisualManager;
```

### Entity Hierarchy
```
BEntity (base) → BObject → BUnit (individual unit)
                        → BSquad (group of units)
                        → BPlatoon → BArmy
```
- Entities use `BEntityID` for references, created via `gWorld->createUnit/Squad/etc.`
- Pooled allocation with `IPoolable` interface and `DECLARE_FREELIST`/`IMPLEMENT_FREELIST` macros

## Build Configurations

Defined via preprocessor in [buildOptions.h](xcore/buildOptions.h):

| Config | Defines | Use Case |
|--------|---------|----------|
| Debug | `BUILD_DEBUG`, `DEBUG` | Full debugging, all asserts |
| Checked | `BUILD_CHECKED`, `BUILD_DEBUG` | Debug with some optimizations |
| Playtest | `BUILD_PLAYTEST` | Internal testing builds |
| Profile | `BUILD_PROFILE` | Performance profiling |
| Final | `BUILD_FINAL` | Release builds, no debug code |

Guard debug-only code with `#ifndef BUILD_FINAL`.

## Memory Management

### Heap Selection (Critical for correctness)
From [freelist.h](xcore/containers/freelist.h) and heap patterns:
```cpp
&gSimHeap       // Simulation-thread allocations (most game objects)
&gRenderHeap    // Render-thread allocations  
&gNetworkHeap   // Network subsystem
&gPrimaryHeap   // General purpose (default)
```

### Pooled Objects Pattern
```cpp
// In header:
DECLARE_FREELIST(BMyClass, 4);  // 4 = log2 of group size

// In cpp:
IMPLEMENT_FREELIST(BMyClass, 4, &gSimHeap);

// Usage:
BMyClass* p = BMyClass::getInstance();  // Acquire from pool
BMyClass::releaseInstance(p);           // Return to pool
```

## Serialization (Save/Load System)

Uses macro-based serialization in [gamefilemacros.h](xsystem/gamefilemacros.h):
```cpp
// Class must declare version:
GFDECLAREVERSION();  // In header
GFIMPLEMENTVERSION(BMyClass, 1);  // In cpp

// Save method pattern:
bool BMyClass::save(BStream* pStream, int saveType) {
    GFWRITEVERSION(pStream, BMyClass);
    GFWRITEVAR(pStream, int32, mValue);
    GFWRITESTRING(pStream, BSimString, mName, 200);
    GFWRITECLASS(pStream, saveType, mChild);
    return true;
}

// Load method pattern:
bool BMyClass::load(BStream* pStream, int saveType) {
    GFREADVERSION(pStream, BMyClass);
    GFREADVAR(pStream, int32, mValue);
    GFREADSTRING(pStream, BSimString, mName, 200);
    GFREADCLASS(pStream, saveType, mChild);
    return true;
}
```

## Coding Conventions

### Naming
- Classes: `BClassName` (B prefix)
- Globals: `gVariableName` (g prefix)
- Members: `mMemberName` (m prefix)
- Constants: `cConstantName` (c prefix)
- Pointers: `pPointerName` or `mpMemberPointer`

### Assertions
Use project-specific macros from [bassert.h](xcore/bassert.h):
```cpp
BASSERT(condition);           // Standard assert
BASSERTM(condition, "msg");   // Assert with message
BFAIL("message");             // Unconditional failure
```

### Container Types
Prefer engine containers over STL (see [containers/](xcore/containers/)):
```cpp
BDynamicSimArray<T>       // Simulation-heap dynamic array
BSmallDynamicSimArray<T>  // Small object optimized
BFreeList<T>              // Object pooling
BHashMap<K,V>             // Hash table
```

## Action System

Game actions follow this pattern in [action.h](xgame/action.h):
```cpp
class BUnitActionXxx : public BAction, IPoolable {
    DECLARE_FREELIST(BUnitActionXxx, 5);
public:
    virtual void onAcquire() override;   // Init when acquired from pool
    virtual void onRelease() override;   // Cleanup when returned
    virtual bool update(float elapsed);  // Per-frame logic
};
```

## Tools

- **PhoenixEditor**: C# WinForms map/scenario editor at [tools/PhoenixEditor/](tools/PhoenixEditor/)
- **dataBuild**: Asset pipeline tools
- Solution files: Use `xgame.sln` as main entry point

## Platform Considerations

- Code uses `#ifdef XBOX` / `#ifndef XBOX` for platform splits
- Xbox 360 uses XDK-specific types and APIs
- Win32 path uses D3D9 with DirectX SDK
