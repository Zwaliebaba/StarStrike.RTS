# StarStrike.RTS – Code Standards

## Language and Compiler

| Setting | Value |
|---|---|
| **Standard** | C++23 |
| **Compiler** | MSVC (cl.exe) |
| **Exceptions** | Disabled in game logic; avoid `try`/`catch` in StarStrike layer |
| **RTTI** | Avoid `dynamic_cast` and `typeid` in game logic |
| **STL** | Use in utility/engine code; avoid per-frame STL allocations in the game loop |
| **C++ features** | Prefer C-compatible constructs in hot paths; use modern C++ at module boundaries |

The project compiles as C++ (not C) so all C++ keywords are reserved. Older source
files were originally written for C89/C11; do not downgrade their dialect.

---

## Naming Conventions

### Functions and Variables

Use `camelCase`:

```cpp
void updateDroidPosition(DROID* psDroid);
int  buildingCount = 0;
bool renderFrameReady;
```

### Constants, Enums, and Macros

Use `UPPER_SNAKE_CASE`:

```cpp
#define FP12_SHIFT   12
#define MAX_PLAYERS  8

enum DROID_TYPE {
    DROID_WEAPON,
    DROID_SENSOR,
    DROID_CONSTRUCT,
};
```

### Pointer Variables

Prefix with `ps` (pointer to struct):

```cpp
DROID*     psDroid;
STRUCTURE* psStructure;
SDMATRIX*  psMatrix;
```

### File Names

Match the existing per-module convention: `PascalCase.cpp` / `PascalCase.h` for engine
files (e.g. `PieMatrix.cpp`), `PascalCase.cpp` / `PascalCase.h` for game files
(e.g. `Display3D.cpp`). Do not introduce snake_case or lowercase filenames.

---

## File Organisation

```
// Header guard (all .h files)
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

// Includes
#include "NeuronCore.h"    // Own layer first
#include "Framework/Mem.h" // Engine layer second
                           // System/SDK headers last

// Type definitions and structs
typedef struct { ... } MY_STRUCT;

// Function declarations
void myFunction(MY_STRUCT* ps);

#endif // MODULE_NAME_H
```

- One primary concept per `.cpp`/`.h` pair.
- Keep headers minimal – forward-declare where possible, include in `.cpp`.
- Do not use `#pragma once`; use `#ifndef` guards to match the existing codebase.

---

## Comments

```cpp
/* Block comments for file-level, section, and multi-line explanations. */

// Single-line comments are acceptable in C++23 source.

/*
 * Function: updateVisibility
 * Purpose:  Recompute fog-of-war for all droids owned by player.
 * Returns:  void
 */
void updateVisibility(int playerIndex);
```

- Comment the *why*, not the *what*. Avoid comments that restate the code.
- Keep function-level comments at the declaration in the header file.
- Legacy files use only `/* */` style; do not change existing comment style when
  making targeted edits.

---

## Memory Management

The engine uses its own allocator via `Framework/Mem.h`. Do not use `new`/`delete` or
`malloc`/`free` directly in game logic:

```cpp
/* Allocate */
DROID* psDroid = (DROID*)MALLOC(sizeof(DROID));

/* Free */
FREE(psDroid);
```

- Game entities (Droid, Structure, Projectile, Feature) live in **pre-allocated arrays**
  with free lists. Do not heap-allocate individual objects in the game loop.
- Resource lifetimes follow a clear init/shutdown pattern – call the module's
  `xxxInitialise()` before use and `xxxShutDown()` on exit.

---

## Platform Guards

Wrap anything Windows-specific:

```cpp
#ifdef WIN32
    // DirectX or Win32 API call
#endif

#ifdef PSX
    // Legacy PlayStation code – do not modify or delete
#endif
```

All code in this repo targets Windows; however, keep `#ifdef WIN32` guards in place for
explicit intent and future-portability clarity.

---

## Engine Abstraction Layer

Use `Framework/` abstractions instead of direct OS or SDK calls:

| Instead of | Use |
|---|---|
| `malloc` / `free` | `MALLOC` / `FREE` (Framework/Mem.h) |
| Win32 `ReadFile` / `WriteFile` | Framework file I/O wrappers |
| Direct `DirectInput` polling | `Framework/Input.h` abstraction |
| Direct Win32 surface operations | `Framework/Frame.h` / `Image.h` |

Do not add new Win32 or DirectX calls directly in StarStrike game-logic files.
Route new platform needs through `Framework/` or `ivis02/`.

---

## Coding Patterns

### Structs Over Classes

Prefer plain structs with free functions over C++ classes:

```cpp
/* Prefer this */
typedef struct { int x, y; } POINT2D;
void initPoint(POINT2D* ps, int x, int y);

/* Avoid this in game logic */
class Point2D {
    int x_, y_;
public:
    Point2D(int x, int y) : x_(x), y_(y) {}
};
```

### Error Handling

Use return codes and `ASSERT`/debug output rather than exceptions:

```cpp
if (!psStructure) {
    DBERROR(("structureUpdate: null pointer\n"));
    return false;
}
```

### Loop Iteration Over Object Arrays

Iterate pre-allocated arrays using the module's own iterator pattern, not range-based
for over STL containers, for core simulation loops:

```cpp
for (int i = 0; i < MAX_DROIDS; i++) {
    DROID* psDroid = &apsDroidLists[playerIndex][i];
    if (!psDroid->active) continue;
    // process psDroid
}
```

### Fixed-Point Math (Legacy)

Until the DirectXMath migration is complete (see `MathConv.md`), fixed-point helpers
are in `Framework/Fractions.h` and `ivis02/PieMatrix.h`:

```cpp
fixed fVal = MAKEFRACT(1.5f);   /* float → 12.12 fixed */
int   iVal = MAKEINT(fVal);     /* 12.12 fixed → int   */
```

Do not introduce new uses of `MAKEFRACT`/`MAKEINT` in new code – use `float` directly
and let the math migration consolidate the boundary.

---

## Build Conventions

- Source files added to `NeuronClient/ivis02/` are automatically picked up by
  `file(GLOB CONFIGURE_DEPENDS ...)` – no CMakeLists.txt edit required.
- Files added to other directories must be explicitly listed in the corresponding
  `CMakeLists.txt`.
- Always build with zero warnings treated as errors on new code (`/W4 /WX` for MSVC).
- Run `cmake --build 2>&1 | grep -i error` to confirm a clean build before pushing.

---

## Renderer Rules

- **Direct3D 9 is the only active backend.** Do not add code paths for
  `ENGINE_SR`, `ENGINE_4101`, or `ENGINE_GLIDE`.
- All new 3D draw calls go through `ivis02/D3drender.cpp` vertex buffer submission.
- 2D blitting (radar, minimap) must use the Direct3D path; do not reintroduce
  software-mode blitting calls.
- Texture data flows through the FBF texture-page system (`ivis02/Fbf.cpp`);
  do not load raw images directly into D3D surfaces in game code.

---

## Git Commit Style

```
Fix fog-of-war occlusion at tile boundary
Add commander-droid formation support
Remove ENGINE_4101 from IntDisplay
```

- Imperative mood, present tense.
- One logical change per commit.
- Keep subject line under 72 characters.
- Reference relevant files or subsystems in the body when the change is non-obvious.
