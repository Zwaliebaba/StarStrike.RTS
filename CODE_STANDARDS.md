# StarStrike.RTS – Code Standards

## Language and Compiler

| Setting | Value |
|---|---|
| **Standard** | C++23 (stdcpp23), C++latest where specified |
| **Compiler** | MSVC (cl.exe) |
| **Platform** | Windows 10+ (Win32 desktop with WinRT APIs) |
| **Exceptions** | Enabled; use sparingly in hot paths |
| **RTTI** | Enabled; prefer `enum class` type tracking over `dynamic_cast` |
| **STL** | Preferred; use modern C++ containers, algorithms, and smart pointers |
| **C++ features** | Embrace modern C++ (concepts, ranges, std::format, coroutines where appropriate) |

All projects compile as C++ with `/std:c++23` or `/std:c++latest`.
Uses CMake `target_precompile_headers` with `pch.h` in all targets.

---

## Naming Conventions

### Classes and Types

Use `PascalCase`:

```cpp
class CoreEngine;
class FileSys;
struct NonCopyable;
class BaseException;
```

### Member Variables

Prefix with `m_` and use `camelCase`:

```cpp
class FileSys
{
  inline static std::wstring m_homeDir;
};

class BaseException : public std::exception
{
  std::string m_s;
};
```

### Functions and Local Variables

Use `camelCase`:

```cpp
void combFire(WEAPON *psWeap, BASE_OBJECT *psAttacker, BASE_OBJECT *psTarget);
int buildingCount = 0;
bool isInView = true;
float predictionTime = 0.0f;
```

### Constants and Macros

Use `UPPER_SNAKE_CASE`:

```cpp
#define MAX_PLAYERS         8
#define DROID_MAXWEAPS      1
#define DROID_MAXCOMP       (COMP_NUMCOMPONENTS - 1)

constexpr uint32_t ENGINE_VERSION = 0x00000001;
```

### Enums

Use `enum class` with explicit underlying type and `PascalCase`:

```cpp
enum class ObjectType : int8_t
{
  Invalid = -1,
  ObjDroid,
  ObjStructure,
  ObjFeature,
  ObjBullet,
  ObjTarget
};

enum class DroidType : int8_t
{
  Invalid = -1,
  Weapon,
  Sensor,
  ECM,
  Construct,
  Person,
  Cyborg,
  Transporter,
  Command,
  Repair
};
```

### File Names

Use `PascalCase.cpp` / `PascalCase.h` for most files:
- `Droid.cpp` / `DroidDef.h`
- `Display3D.cpp` / `Display3D.h`
- `FrameResource.cpp` / `FrameResource.h`

Some legacy NeuronClient subdirectories use lowercase: `ivis02/`.
Match the existing pattern in the directory you're working in.

---

## File Organisation

### Header Guards

**Prefer `#pragma once`** for new code:

```cpp
#pragma once

#include "NeuronCore.h"
// ... rest of header
```

Legacy files use `#ifndef` guards – preserve these when editing existing files:

```cpp
#ifndef INCLUDED_WORLDOBJECT_H
#define INCLUDED_WORLDOBJECT_H
// ... rest of header
#endif
```

### Include Order

```cpp
#include "YourHeader.h"        // Corresponding header in .cpp files

#include "NeuronCore.h"        // Layer includes (NeuronCore primitives)
#include "NeuronClient.h"      // Client/platform layer
#include "GameLogic.h"         // Game logic layer

#include "Frame.h"             // Framework headers
#include "Objects.h"           // Other project headers

#include <vector>              // Standard library
#include <Windows.h>           // System/SDK headers
```

- One primary concept per `.cpp`/`.h` pair.
- Keep headers minimal – forward-declare where possible, include in `.cpp`.
- All `.cpp` files **must** include `pch.h` first.

---

## Comments

Use C++ style comments for most documentation:

```cpp
// Single-line comments for brief explanations

/* Multi-line comments for longer explanations,
   file headers, or section dividers */

/// Optional: documentation comments for public APIs
/// @param unit The unit to advance
/// @return true if successful
void combFire(WEAPON *psWeap, BASE_OBJECT *psAttacker, BASE_OBJECT *psTarget);
```

- Comment the **why**, not the **what**. Avoid comments that restate the code.
- Keep function-level comments brief; complex logic deserves inline explanation.
- Use `//` for inline and single-line comments.
- Use `/* ... */` for file headers and multi-line blocks.

---

## Memory Management

### Allocation Macros

The codebase uses the `NEW` macro for debug tracking (`NeuronCore/Debug.h`):

```cpp
#if defined(_DEBUG)
  #define NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#else
  #define NEW new
#endif
```

Use `NEW` for raw allocations that need debug tracking:

```cpp
CoreEngine* engine = NEW CoreEngine();
```

### Legacy Allocation Macros

The legacy codebase uses `MALLOC`/`FREE` macros for debug-tracked allocation (`NeuronClient/Framework/Mem.h`):

```cpp
#if DEBUG_MALLOC
#define MALLOC(size)  memMalloc(__FILE__, __LINE__, size)
#define FREE(ptr)     memFree(__FILE__, __LINE__, ptr); ptr = NULL
#else
#define MALLOC(size)  memMallocRelease(size)
#define FREE(ptr)     memFreeRelease(ptr); ptr = NULL
#endif

// Usage:
DROID_TEMPLATE *pTemplate = (DROID_TEMPLATE *)MALLOC(sizeof(DROID_TEMPLATE));
FREE(pTemplate);
```

New code should prefer `NEW`/`delete` or smart pointers over `MALLOC`/`FREE`.

### Smart Pointers (Preferred)

**Prefer modern C++ smart pointers** for ownership:

```cpp
std::unique_ptr<Location> m_location;
std::shared_ptr<Camera> m_camera;
winrt::com_ptr<IUnknown> m_comObject;
```

- Use `std::unique_ptr<T>` for exclusive ownership.
- Use `std::shared_ptr<T>` for shared ownership.
- Use `winrt::com_ptr<T>` for COM/WinRT objects.
- Use `Neuron::ScopedHandle` for Win32 `HANDLE` values.
- Avoid raw owning pointers; raw pointers should be non-owning observers only.

### Pre-allocated Arrays

Some game objects use pre-allocated arrays with free lists for performance.
Do not heap-allocate individual objects in the game loop for these types.

---

## Platform and Architecture

### Target Platform

All code targets **Windows 10+** (x86/ x64):
- Win32 desktop application packaged as MSIX
- Uses WinRT APIs via C++/WinRT for modern Windows features
- Supports both windowed and full-screen rendering

### Platform Abstractions

Use **NeuronCore** for cross-cutting primitives:

| Instead of | Use |
|---|---|
| Direct timing APIs | `TimerCore.h` (`Neuron::Timer::Core`) |
| Raw file I/O | `FileSys.h` (`BinaryFile`, `TextFile`) |
| Debug output | `Debug.h` (`DebugTrace`, `DEBUG_ASSERT`) |
| Utility types | `NeuronHelper.h` (`ScopedHandle`, `NonCopyable`, `BaseException`) |

Use **NeuronClient** for platform layer:
- **Framework/**: OS/platform abstraction (memory, resources, input, windowing)
- **ivis02/**: Rendering via DirectDraw/Direct3D 9
- **Gamelib/**: Animation, hash tables, parser, game timing
- **Widget/**: UI widget system
- **Sound/**: DirectSound + QMixer audio
- **Script/**: Scripting system
- **Netplay/**: Multiplayer via DirectPlay

Do not add new Win32 or DirectX calls directly in `StarStrike` game files.
Route platform needs through `NeuronCore` or `NeuronClient`.

---

## Coding Patterns

### Classes and Structs

Use `class` for entities with behavior; `struct` for plain data:

```cpp
// Behavior-rich types use class
class CoreEngine
{
public:
  static void Startup();
  static void Shutdown();
};

class FileSys
{
public:
  static void SetHomeDirectory(const std::wstring& _path);
  [[nodiscard]] static std::wstring GetHomeDirectory();

protected:
  inline static std::wstring m_homeDir;
};

// Plain data uses struct (members public by default)
struct WEAPON
{
  UDWORD  nStat;
  UDWORD  hitPoints;
  UDWORD  ammo;
  UDWORD  lastFired;
  UDWORD  recoilValue;
};
```

Inheritance is used in NeuronCore (e.g. `BaseException : std::exception`).
Legacy code uses C-style `typedef struct` with macro-based "inheritance" (`BASE_ELEMENTS`).

### Error Handling

Prefer return codes, assertions, and debug output:

```cpp
if (psDroid == NULL)
{
  Fatal("droidUpdate: null droid pointer");
  return FALSE;
}

ASSERT_TEXT(psStructure != NULL, "structureUpdate: invalid structure pointer");
DEBUG_ASSERT(pTemplate != nullptr);
```

- Use `DebugTrace` (NeuronCore/Debug.h) for debug output.
- Use `DEBUG_ASSERT` for invariants (compiles out in release).
- Validate arguments even in release builds when failure is possible.
- Exceptions are enabled but used sparingly; prefer error codes in hot paths.

### Modern C++ Features

Embrace modern C++ where appropriate:

```cpp
// Range-based for loops
for (const auto& [key, value] : m_map)
{
  // ...
}

// std::format with DebugTrace (NeuronCore/Debug.h)
DebugTrace("Droid {}: pos=({}, {}, {})\n",
  psDroid->id, psDroid->x, psDroid->y, psDroid->z);

// Auto for complex types
auto it = m_entities.find(id);
```

Do not avoid modern C++ for the sake of tradition; the codebase is C++23.

### Loop Iteration Patterns

For legacy linked lists (droids, structures, features):

```cpp
// Typical linked-list iteration in StarStrike
for (DROID *psDroid = apsDroidLists[player]; psDroid; psDroid = psDroid->psNext)
{
  // process droid
}
```

For STL containers, prefer range-based for or algorithms.
---

## Project Structure

### CMake Layout

Top-level `CMakeLists.txt` (project `StarStrike`, CMake 3.12+, Ninja generator):

| Target | Type | Purpose |
|---|---|---|
| **StarStrike** | Win32 Executable | Main game client |
| **NeuronClient** | Static Library | Platform layer (rendering, input, audio, UI, networking) |
| **NeuronCore** | Static Library | Foundation (math, timing, file I/O, utilities, C++/WinRT) |

### Project Dependencies

```
StarStrike.exe
  ├─ NeuronClient (static lib)
  │   └─ NeuronCore (static lib)
  ├─ DirectX 9 SDK (DX9/Lib)
  ├─ QMixer.lib, Mplayer.lib
  └─ winmm
```

### File Organization

- `StarStrike/` – Main game executable (droids, structures, combat, AI, missions, UI)
- `NeuronClient/` – Platform layer, with subdirectories:
  - `Framework/` – OS abstraction (memory, resources, input, windowing, debug)
  - `Gamelib/` – Animation, hash tables, parser, game timing
  - `Widget/` – UI widget system
  - `ivis02/` – Rendering / image library (DirectDraw, Direct3D 9)
  - `Sound/` – Audio (DirectSound, QMixer)
  - `Script/` – Scripting engine
  - `Netplay/` – Multiplayer (DirectPlay)
- `NeuronCore/` – Foundation utilities, timing, file I/O, debug, C++/WinRT
- `DX9/` – DirectX 9 SDK headers and libraries

---

## Build Conventions

### Building the Project

```powershell
# Configure (first time, or after CMakeLists.txt changes)
cmake --preset x86-debug

# Build (Debug x86)
cmake --build out/build/x86-debug

# Build (Release x86)
cmake --preset x86-release
cmake --build out/build/x86-release
```

### Compiler Settings

All projects use (set in top-level `CMakeLists.txt`):
- `CMAKE_CXX_STANDARD 23` – C++23 standard
- `CMAKE_CXX_STANDARD_REQUIRED ON` – Standard is mandatory
- `CMAKE_CXX_EXTENSIONS OFF` – No compiler extensions
- **CMake precompiled headers** – `target_precompile_headers` with `pch.h` in each target

Debug builds:
- `/MDd` – Multi-threaded Debug DLL runtime
- `_DEBUG` preprocessor define

Release builds:
- `/MD` – Multi-threaded DLL runtime
- `NDEBUG` preprocessor define

### Adding New Files

**Source files (.cpp/.h)**:
- Add files to the appropriate directory; CMake `file(GLOB ... CONFIGURE_DEPENDS)` picks them up automatically
- Re-run CMake configure if the glob doesn't detect new files
- Ensure `.cpp` files include `pch.h` first

### Warnings and Errors

- **Zero warnings policy** for new code.
- Existing files may have warnings; do not introduce new ones.
- Use `#pragma warning(disable:...)` sparingly and document why.
- Prefer fixing the warning over disabling it.

---

## Renderer Architecture

### Graphics Stack

The renderer uses **DirectDraw and Direct3D 9** (`NeuronClient/ivis02/`):

- **DirectDraw** – `Screen.*`, `Surface.*` for 2D backbuffer and surface management
- **Direct3D 9** – `D3drender.*` for 3D hardware-accelerated rendering
- **Pie System** – `Piedef.*`, `PieState.*`, `PieDraw.*` for model rendering
- **Texture Management** – `Texd3d.*`, `Dx6TexMan.*` for texture loading and binding
- **BSP/IMD** – `Bspimd.*`, `Imd.*` for model format loading

### Rendering Flow

1. `gameLoop()` in `Loop.cpp` drives the main frame
2. `draw3DScene()` in `Display3D.cpp` orchestrates 3D rendering
3. Terrain, structures, droids, features, effects rendered in layers
4. 2D HUD and widgets overlaid via `Widget/` and `ivis02/TextDraw.*`

### Renderer Rules

- **All 3D draw calls** go through the `ivis02/` pie rendering layer.
- **Do not** call Direct3D 9 directly from `StarStrike/` game files.
- **Texture loading** uses the pie texture management system.
- **Performance**: Minimize state changes; batch draw calls where possible.

### Rendering Patterns

```cpp
// Typical rendering of a droid
void renderDroid(DROID *psDroid)
{
  iIMDShape *pIMD = psDroid->sDisplay.imd;

  pie_MatBegin();
  pie_TRANSLATE(psDroid->x, psDroid->y, psDroid->z);
  pie_MatRotY(psDroid->direction);

  pie_Draw3DShape(pIMD, 0, psDroid->player, WZCOL_WHITE, 0, 0);

  pie_MatEnd();
}
```

### Debug Rendering

Use `#ifdef _DEBUG` for debug visualizations:

```cpp
#ifdef _DEBUG
  DebugTrace("Rendering droid %d at (%d, %d, %d)\n", psDroid->id, psDroid->x, psDroid->y, psDroid->z);
#endif
```

---

## Simulation and Timing

### Game Loop Structure

The main loop (`Loop.cpp`) runs through game states:
- `GAMECODE_CONTINUE` – Normal frame processing
- `GAMECODE_NEWLEVEL` – Level transition
- `GAMECODE_RESTARTGAME` / `GAMECODE_QUITGAME` – Session control

`gameLoop()` calls `frameUpdate()` (NeuronClient/Framework) for Windows message processing,
then advances game systems and renders.

### Time Slicing

Simulation uses game ticks managed by `NeuronClient/Gamelib/GTime.h`:

```cpp
#define GAME_TICKS_PER_SEC  1000
#define DEFAULT_RECOIL_TIME (GAME_TICKS_PER_SEC / 4)
```

- Game timing uses `gameTime` / `gameTime2` globals for deterministic simulation.
- Time-sensitive systems (movement, combat, AI) are driven by game ticks.
- `frameUpdate()` handles Windows messages and frame rate tracking.

### Timing Rules

- **Do not block** inside the game loop; Windows message pump must run.
- **Use `Neuron::Timer::Core`** (NeuronCore/TimerCore.h) for high-resolution timing.
- **Avoid frame-rate dependent logic**; tie to game ticks.

### Performance

Enable the profiler with the `PROFILER_ENABLED` define (set automatically in debug builds):

```cpp
#ifdef PROFILER_ENABLED
  // Profiling code here
#endif
```

---

## Networking

### Architecture

- **Client**: `NeuronClient/Netplay/` handles multiplayer via DirectPlay.
- **Protocol**: DirectPlay sessions with custom message types defined in `Multiplay.h`.
  - `NET_DROID`, `NET_BUILD`, `NET_TEXTMSG`, etc. for game state synchronisation.
  - `GAMESTRUCT` / `PLAYERSTRUCT` in `Netplay.h` for session management.

### Threading Model

- **Main loop**: Network messages processed each frame via `recvMessage()`.
- **DirectPlay**: Handles session enumeration, joining, and message delivery.

### Networking Rules

- Keep message types aligned in `Multiplay.h`; update send and receive sides together.
- Do not add blocking network calls in the game loop.
- Use `DBPRINTF` / `DebugTrace` to log network events during development.
- Maximum message size is `MaxMsgSize` (8000 bytes), max players `MaxNumberOfPlayers` (8).

---

## Debugging and Telemetry

### Debug Output

```cpp
// Modern (NeuronCore/Debug.h) — preferred for new code
DebugTrace("Starting droid advance\n");
DebugTrace("Droid {}: pos=({}, {}, {})\n",
  psDroid->id, psDroid->x, psDroid->y, psDroid->z);

DebugTrace("Starting entity advance\n");
DebugTrace(std::format("Entity {}: pos=({}, {}, {})\n", m_id, m_pos.x, m_pos.y, m_pos.z));
```

Output appears in the Visual Studio Output window or debugger console.

### Assertions

```cpp
// Modern (NeuronCore/Debug.h)
DEBUG_ASSERT(psDroid != nullptr);
ASSERT_TEXT(numWeaps <= DROID_MAXWEAPS, "Too many weapons: {}", numWeaps);
```

Assertions compile out in release builds; validate arguments separately for release.

### In-Game Profiler

Enable with `PROFILER_ENABLED` define (automatically set in debug builds via `NeuronCore.h`).

---

## Testing and Validation

### Test Projects

No dedicated test projects exist yet. Testing is manual.

### Testing Strategy

- Validate manually via gameplay testing.
- Use `DebugTrace` and `DBPRINTF` to trace runtime behaviour.
- Favor small, observable changes; test incrementally.

---

## Version Control and Commits

### Git Commit Style

Use imperative mood, present tense:

```
Add cyborg repair droid type
Fix droid pathfinding at gateway boundaries
Refactor structure damage calculation
Update CMakeLists.txt for precompiled headers
```

- Keep subject line under 72 characters.
- One logical change per commit.
- Reference relevant files or subsystems in the body when non-obvious.

### Branch Strategy

- `main` – Stable, deployable code.
- Feature branches for new work; merge via pull request.
- Keep branches short-lived; merge frequently.

---

## Additional Guidelines

### When Adding Features

1. Check `NeuronCore` for existing utilities before writing new helpers.
2. Place game logic (droids, structures, combat) in `StarStrike/`, not in `NeuronClient/`.
3. Keep network message types aligned in `Multiplay.h`; update send and receive sides.
4. Route platform needs through `NeuronCore` or `NeuronClient/Framework`.

### When Debugging

1. Use `DebugTrace` or `DBPRINTF` liberally during development.
2. Enable `PROFILER_ENABLED` to identify performance issues.
3. Build frequently to catch warnings and errors early.
4. Validate with gameplay testing; no automated tests yet.

### When Refactoring

1. Preserve existing behavior; validate with gameplay testing.
2. Update comments and documentation.
3. Keep commits focused; one logical change per commit.
4. Ensure zero new warnings introduced.

---

## Summary Checklist

When writing new code, ensure:

- [ ] Uses PascalCase for classes, camelCase for functions/variables, m_ prefix for members.
- [ ] Includes `pch.h` first in `.cpp` files.
- [ ] Uses `#pragma once` for new headers (or preserves existing `#ifndef` guards).
- [ ] Prefers smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw owning pointers.
- [ ] Uses `NEW` macro for raw allocations, `MALLOC`/`FREE` only in legacy code paths.
- [ ] Routes platform needs through `NeuronCore` or `NeuronClient`, not direct Win32/DirectX in `StarStrike/`.
- [ ] Uses `DebugTrace` and `DEBUG_ASSERT` for debugging (or legacy `DBPRINTF`/`ASSERT` in existing code).
- [ ] Maintains zero new compiler warnings.
- [ ] Follows existing patterns in the module you're editing.
- [ ] Commits with clear, imperative-mood messages.

---

*Keep these standards up-to-date as the codebase evolves. Update this document when introducing new conventions or deprecating old ones.*
