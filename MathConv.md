# Fixed-Point 12.12 → DirectXMath Migration Plan

**Target codebase:** StarStrike.RTS
**Author:** Principal Graphics Engineer / Software Architect
**Date:** 2026-02-28
**Standard:** C++23, DirectXMath (Windows SDK, header-only)

---

## 1. Executive Summary

The rendering and math subsystem (`NeuronClient/ivis02`, `NeuronClient/Framework`) was originally written for a Sony PlayStation port, where hardware fixed-point arithmetic was mandatory. The PC build already replaced the PSX fixed-point scalar layer with `typedef float FRACT`, but the matrix and vector pipeline (`SDMATRIX`, `iVector`, `iQuat`) still uses 12.12 fixed-point integers throughout. This creates a persistent dual-precision impedance mismatch: world-space coordinates are plain integers, the transform stack stores scaled-integer basis vectors, and the D3D9 final vertex path requires `float`. Every frame, precision is silently lost through integer truncation in the `>> FP12_SHIFT` steps.

The goal is to replace the fixed-point matrix/vector/trig core with **DirectXMath**, Microsoft's SIMD-accelerated, header-only math library that ships with the Windows SDK and is the canonical companion to DXGI/D3D11/D3D12. This migration is a prerequisite for any future D3D11/D3D12 upgrade.

---

## 2. Current State Inventory

### 2.1 Type System

| Existing Type | Defined In | Description |
|---|---|---|
| `fixed` (`int32`) | `Piedef.h:158` | Signed 12.12 scalar |
| `ufixed` (`uint32`) | `Ivi.h:73` | Unsigned 12.12 scalar |
| `iVector` (`int32 x,y,z`) | `PieTypes.h:61` | 3-component integer vector; components are raw world-space integers **or** 12.12 normals depending on context |
| `iVectorf` (`double x,y,z`) | `PieTypes.h:62` | High-precision float vector (minor use) |
| `iQuat` (`int32 w,x,y,z`) | `Ivi.h:74` | Quaternion in 12.12 fixed-point |
| `PIEVECTORF` (`FRACT x,y,z`) | `PieTypes.h:66` | Float vector; FRACT = `float` |
| `SDMATRIX` (12× `SDWORD`) | `PieMatrix.h:20` | Column-layout 3×4 transform; rotation block in 12.12, translation in 12.12 |
| `FRACT` (`float`) | `Fractions.h:43` | Scalar float (formerly PSX fractional integer) |

### 2.2 Core Constants

```cpp
// Piedef.h
#define FP12_SHIFT      12          // fractional bits
#define FP12_MULTIPLIER (1 << 12)   // = 4096; 1.0 in 12.12 space

// Angle space — full revolution = 65536 units (16-bit wrap)
#define DEG_360  65536
#define DEG_1    (DEG_360 / 360)    // ≈ 182 units / degree

// Trig table (PieMatrix.cpp)
#define SC_TABLESIZE 4096           // entries per revolution
// SIN(X)  = aSinTable[(uint16)(X) >> 4]         — indexes [0..4095]
// COS(X)  = aSinTable[(uint16)(X) >> 4) + 1024] — same table, offset quarter turn
// Table values: sin(θ) * FP12_MULTIPLIER, i.e., range [-4096 .. +4096]
```

### 2.3 SDMATRIX Layout

```
Result = [x  y  z  1] × M   (row-vector left-multiply, DirectX convention)

M =  | a  b  c  0 |   row 0: basis X projected onto output axes
     | d  e  f  0 |   row 1: basis Y
     | g  h  i  0 |   row 2: basis Z
     | j  k  l  1 |   row 3: translation (world coords << FP12_SHIFT)

Identity: a=e=i=FP12_MULTIPLIER, all others=0
```

The multiplication kernel (`pie_RotProj`, `pie_ROTATE_PROJECT`, `pie_ROTATE_TRANSLATE`, `pie_VectorInverseRotate0`) always does:

```cpp
result = (v.x * M.col0 + v.y * M.col1 + v.z * M.col2 + M.translation) >> FP12_SHIFT;
// — except pie_RotProj which skips the >> and relies on z-divide for perspective.
```

### 2.4 Affected Files

**Core math (must be converted):**
- `NeuronClient/ivis02/PieMatrix.h` — SDMATRIX typedef, SIN/COS macros, transformation macros
- `NeuronClient/ivis02/PieMatrix.cpp` — matrix stack, rot X/Y/Z, pie_RotProj, pie_VectorNormalise, pie_SurfaceNormal, pie_MatCreate, pie_VectorInverseRotate0
- `NeuronClient/ivis02/PieTypes.h` — iVector, iVectorf, iQuat, PIEVECTORF
- `NeuronClient/ivis02/Piedef.h` — FP12_SHIFT, FP12_MULTIPLIER, DEG_360, angle macros
- `NeuronClient/ivis02/Ivi.h` — ufixed, iQuat, iV_RSHIFT constants
- `NeuronClient/Framework/Fractions.h` — FRACT, MAKEINT (x87 asm), MAKEFRACT, FRACTmul, etc.
- `NeuronClient/Framework/Trig.h/.cpp` — lookup-table trigSin/trigCos/trigInvSin/trigIntSqrt

**Game logic (callers — require update after core migration):**
- `StarStrike/Effects.h/.cpp` — PIEVECTORF velocity, iVector rotation/spin
- `StarStrike/Formation.cpp` — trigSin/trigCos calls, MAKEINT/MAKEFRACT
- `StarStrike/Findpath.cpp` — FRACTdiv, fSQRT, MAKEFRACT
- `StarStrike/Move.cpp` — FRACTCONST, BLOCK_* ratio constants
- `StarStrike/Droid.cpp` — FRACTdiv_1, FRACTmul_1, MAKEINT
- `StarStrike/Projectile.cpp` — trig function calls
- `StarStrike/Geometry.h` — angle utility macros
- `StarStrike/WarCAM.cpp`, `Radar.cpp`, `RayCast.cpp` — coordinate transforms
- `NeuronClient/ivis02/Bspimd.cpp` — IsPointOnPlane dot product

### 2.5 Problem Areas

1. **Inline x87 assembly in MAKEINT** (`Fractions.h:83-89`): uses `__asm fld f; __asm fistp i` — MSVC x64 does not support inline assembly. This is a latent build-breaker for any 64-bit target.
2. **`register` keyword** (`PieMatrix.cpp:183-184`): deprecated in C++17, removed in C++17 for auto storage class use; already causes warnings under C++23.
3. **Precision loss in pie_VectorNormalise**: Uses octahedral approximation (`size = max_component + (mid >> 2) + (min >> 2)`) instead of true Euclidean length — an artifact of avoiding `sqrt` on PSX.
4. **Dual angle systems**: `DEG_360=65536` system (PieMatrix SIN/COS table, 16-bit wrap) vs `TRIG_DEGREES=360` system (Framework Trig.cpp lookup) — callers use both; must be unified.
5. **Integer overflow risk**: In `pie_MatCreate`, terms like `(sry * srx) >> FP12_SHIFT` involve products of two 12.12 values (each up to ±4096), producing up to ±4096² ≈ ±16M before the shift — within `int32` range, but the subsequent multiplied again can overflow in deeply nested expressions.

---

## 3. DirectXMath Target Architecture

### 3.1 Why DirectXMath

- Ships with every Windows SDK ≥ 8.0 — **zero new dependencies**
- Header-only (`<DirectXMath.h>`) — no `.lib` to link
- Designed for exactly this use case: game transforms with D3D11/D3D12
- SSE2/AVX-accelerated on x86-64 automatically
- Row-major, left-multiply convention matches existing SDMATRIX layout
- `XMMATRIX` is 4×4 float; the 3×4 `SDMATRIX` expands naturally by adding a W column

### 3.2 Key DirectXMath Types

| DirectXMath | Role | Alignment |
|---|---|---|
| `XMVECTOR` | 4-float SIMD register (temporary computation) | 16-byte required |
| `XMFLOAT3` | Storage for 3-float vector (no alignment req.) | 4-byte |
| `XMFLOAT4` | Storage for 4-float vector | 4-byte |
| `XMFLOAT3X3` | Storage for 3×3 float matrix | 4-byte |
| `XMFLOAT4X4` | Storage for 4×4 float matrix | 4-byte |
| `XMMATRIX` | 4×4 SIMD matrix (temporary computation) | 16-byte required |

**Load/store pattern** (mandatory for struct members and arrays):
```cpp
XMVECTOR v = XMLoadFloat3(&myFloat3);   // storage → SIMD register
// ... do math with XMVECTOR ...
XMStoreFloat3(&myFloat3, v);            // SIMD register → storage
```

### 3.3 Angle Space Conversion

The existing system uses `DEG_360 = 65536` units per revolution.

```cpp
// Fixed-unit angle → radians
inline float AngleToRadians(int angle) {
    return static_cast<float>(angle) * (XM_2PI / 65536.0f);
}

// Radians → fixed-unit angle
inline int RadiansToAngle(float rad) {
    return static_cast<int>(rad * (65536.0f / XM_2PI));
}

// Degrees → fixed-unit angle (replaces DEG(X) macro)
inline int DegreesToAngle(int deg) {
    return (deg * 65536) / 360;
}
```

The 360-entry Framework trig table (`Trig.cpp`) uses ordinary 0–359 degree integers. Its callers (`Formation.cpp`, `Projectile.cpp`, etc.) pass the same 0-65535 units via `% TRIG_DEGREES` normalization inside `trigSin`. After migration those callers convert with `AngleToRadians` and call `XMScalarSin`/`XMScalarCos` directly.

---

## 4. Migration Phases

### Phase 0: CMake Setup

**Effort:** 1 hour
**Risk:** None

DirectXMath is header-only in the Windows SDK. Add to `NeuronClient/CMakeLists.txt`:

```cmake
# DirectXMath is header-only — no link target needed.
# The Windows SDK provides it at $(WindowsSdkDir)Include/<version>/um/
# MSVC automatically provides the SDK include path.
# For explicit portability, use vcpkg or fetch from Microsoft/DirectXMath on GitHub.

find_package(directxmath CONFIG QUIET)   # vcpkg path
if(NOT directxmath_FOUND)
    # Fall back: rely on Windows SDK (always present on Windows builds)
    message(STATUS "DirectXMath: using Windows SDK headers")
endif()
```

No changes needed to the `d3d9.lib` link line. `<DirectXMath.h>` is independent of the D3D runtime version.

---

### Phase 1: Create the Math Bridge Header

**New file:** `NeuronClient/Framework/Math.h`
**Effort:** 2–3 hours
**Risk:** Low — additive only, no callers yet

This header provides:
1. The `#include <DirectXMath.h>` entry point
2. `using namespace DirectX;` scoped to math translation units (never in a public header — use fully-qualified names in headers)
3. Inline conversion helpers between legacy types and DirectXMath types
4. A thin `StrikeMatrix` alias around `XMFLOAT4X4` for the transition period

```cpp
// NeuronClient/Framework/Math.h
#pragma once
#include <DirectXMath.h>
#include "Types.h"   // SDWORD, UDWORD, etc.

using namespace DirectX;

// ─── Angle space ──────────────────────────────────────────────────────────────
// Legacy angle units: full circle = 65536 (DEG_360)
static constexpr float kAngleUnitsPerCircle = 65536.0f;

[[nodiscard]] inline float AngleToRadians(SDWORD angle) noexcept {
    return static_cast<float>(angle) * (XM_2PI / kAngleUnitsPerCircle);
}
[[nodiscard]] inline SDWORD RadiansToAngle(float rad) noexcept {
    return static_cast<SDWORD>(rad * (kAngleUnitsPerCircle / XM_2PI));
}
[[nodiscard]] inline SDWORD DegreesToAngle(int deg) noexcept {
    return (deg * static_cast<SDWORD>(kAngleUnitsPerCircle)) / 360;
}

// ─── 12.12 fixed-point scalars ────────────────────────────────────────────────
static constexpr float kFP12Scale = 4096.0f;   // == FP12_MULTIPLIER

[[nodiscard]] inline float FP12ToFloat(SDWORD v) noexcept {
    return static_cast<float>(v) / kFP12Scale;
}
[[nodiscard]] inline SDWORD FloatToFP12(float v) noexcept {
    return static_cast<SDWORD>(v * kFP12Scale);
}

// ─── iVector ↔ XMFLOAT3 ───────────────────────────────────────────────────────
// iVector components are raw world-space integers (NOT fixed-point scalars).
struct iVector;  // forward-declared; defined in PieTypes.h

[[nodiscard]] inline XMFLOAT3 IVectorToFloat3(const iVector& v) noexcept {
    return { static_cast<float>(v.x),
             static_cast<float>(v.y),
             static_cast<float>(v.z) };
}
[[nodiscard]] inline iVector Float3ToIVector(const XMFLOAT3& f) noexcept {
    return { static_cast<SDWORD>(f.x),
             static_cast<SDWORD>(f.y),
             static_cast<SDWORD>(f.z) };
}

// iVector storing a 12.12 normalised vector → unit XMFLOAT3
[[nodiscard]] inline XMFLOAT3 IVectorNormToFloat3(const iVector& v) noexcept {
    return { FP12ToFloat(v.x), FP12ToFloat(v.y), FP12ToFloat(v.z) };
}

// ─── SDMATRIX ↔ XMMATRIX ──────────────────────────────────────────────────────
// SDMATRIX is a 3×4 row-major matrix stored as 12.12 fixed-point integers.
// Expand to XMMATRIX (4×4) by dividing by kFP12Scale and adding W row/column.
struct SDMATRIX;  // forward-declared; defined in PieMatrix.h

[[nodiscard]] inline XMMATRIX SDMatrixToXMMatrix(const SDMATRIX& m) noexcept;
[[nodiscard]] inline SDMATRIX XMMatrixToSDMatrix(FXMMATRIX m) noexcept;
// Implementations are in Math.cpp (include PieMatrix.h there).
```

> **Convention note:** `XMMATRIX` and `XMVECTOR` must live on the stack or in aligned allocations. Never embed them directly in heap-allocated structs without `__declspec(align(16))` or `alignas(16)`. Use `XMFLOAT4X4` / `XMFLOAT3` for storage.

---

### Phase 2: Replace SDMATRIX with XMFLOAT4X4

**Files:** `PieMatrix.h`, `PieMatrix.cpp`
**Effort:** 1 day
**Risk:** Medium — central transform stack; keep old code under `#ifdef LEGACY_FIXEDPOINT` until tests pass

#### 2a. New matrix type

In `PieMatrix.h`, replace:
```cpp
typedef struct {SDWORD a, b, c,  d, e, f,  g, h, i,  j, k, l;} SDMATRIX;
```
with:
```cpp
// New: row-major 4×4 float matrix (XMFLOAT4X4 is 16 floats, no SIMD alignment req.)
using StrikeMatrix = XMFLOAT4X4;
// Keep SDMATRIX as a deprecated alias during the transition
using SDMATRIX [[deprecated("migrate to StrikeMatrix / XMFLOAT4X4")]] = XMFLOAT4X4;
```

The 3×4 → 4×4 expansion:
```
Old SDMATRIX             New XMFLOAT4X4 (_ij notation: row i, col j)
─────────────────────    ─────────────────────────────────────────────────
a/FP12  b/FP12  c/FP12  → _11  _12  _13  _14=0
d/FP12  e/FP12  f/FP12  → _21  _22  _23  _24=0
g/FP12  h/FP12  i/FP12  → _31  _32  _33  _34=0
j/FP12  k/FP12  l/FP12  → _41  _42  _43  _44=1
```
Translation was stored pre-scaled: `j = worldX << FP12_SHIFT`, so dividing by `kFP12Scale` recovers the original world integer as a float.

#### 2b. Identity matrix constant

```cpp
// PieMatrix.cpp — replaces _MATRIX_ID
static const XMFLOAT4X4 kIdentityMatrix = {
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1
};
```

#### 2c. Matrix stack

```cpp
static constexpr int MATRIX_MAX = 8;
static StrikeMatrix  aMatrixStack[MATRIX_MAX];
StrikeMatrix        *psMatrix = &aMatrixStack[0];
// psMatrix still usable as a pointer; no API surface change for callers yet.
```

#### 2d. pie_MatReset / pie_MatBegin / pie_MatEnd

These only copy stack entries — no math changes required. Replace:
```cpp
*psMatrix = _MATRIX_ID;
// →
*psMatrix = kIdentityMatrix;
```
The rest stays identical.

#### 2e. Rotation functions

Replace `pie_MatRotY` (and X, Z) with DirectXMath:

```cpp
void pie_MatRotY(int y) {
    if (y == 0) return;
    const float rad = AngleToRadians(y);
    XMMATRIX current = XMLoadFloat4x4(psMatrix);
    XMMATRIX rot     = XMMatrixRotationY(rad);
    // Pre-multiply: new = rot × current  (object-space rotation)
    XMStoreFloat4x4(psMatrix, XMMatrixMultiply(rot, current));
}

void pie_MatRotX(int x) {
    if (x == 0) return;
    XMMATRIX current = XMLoadFloat4x4(psMatrix);
    XMMATRIX rot     = XMMatrixRotationX(AngleToRadians(x));
    XMStoreFloat4x4(psMatrix, XMMatrixMultiply(rot, current));
}

void pie_MatRotZ(int z) {
    if (z == 0) return;
    XMMATRIX current = XMLoadFloat4x4(psMatrix);
    XMMATRIX rot     = XMMatrixRotationZ(AngleToRadians(z));
    XMStoreFloat4x4(psMatrix, XMMatrixMultiply(rot, current));
}
```

> **Handedness note:** Verify that `XMMatrixRotationY` produces the same rotation sense as the original SIN/COS table. DirectXMath uses a **left-handed** coordinate system by default. The original code appears left-handed (Y-up, Z-into-screen for the D3D9 projection). If the world uses a right-handed convention, use `XMMatrixRotationRollPitchYaw` variants or negate the angle. This must be confirmed with a visual regression test.

#### 2f. pie_MatCreate (Euler → matrix)

```cpp
void pie_MatCreate(iVector *r, StrikeMatrix *m) {
    // r->x, r->y, r->z are in legacy angle units
    const float rx = AngleToRadians(r->x);
    const float ry = AngleToRadians(r->y);
    const float rz = AngleToRadians(r->z);
    // DirectXMath: Roll=Z, Pitch=X, Yaw=Y  (verify against original column tests)
    XMMATRIX mat = XMMatrixRotationRollPitchYaw(rx, ry, rz);
    XMStoreFloat4x4(m, mat);
}
```

#### 2g. Translation macros

Replace:
```cpp
#define pie_INVTRANSX(X)    psMatrix->j = (X) << FP12_SHIFT
#define pie_MATTRANS(X,Y,Z) { psMatrix->j = (X)<<FP12_SHIFT; ... }
```
with inline functions:
```cpp
inline void pie_SetTranslation(float x, float y, float z) noexcept {
    psMatrix->_41 = x;
    psMatrix->_42 = y;
    psMatrix->_43 = z;
}
// Integer world-coord overload (most callers pass SDWORD)
inline void pie_SetTranslation(SDWORD x, SDWORD y, SDWORD z) noexcept {
    psMatrix->_41 = static_cast<float>(x);
    psMatrix->_42 = static_cast<float>(y);
    psMatrix->_43 = static_cast<float>(z);
}
```
The `pie_TRANSLATE` macro (incremental translate along current axes) maps to:
```cpp
inline void pie_Translate(float dx, float dy, float dz) noexcept {
    XMMATRIX m = XMLoadFloat4x4(psMatrix);
    XMMATRIX t = XMMatrixTranslation(dx, dy, dz);
    XMStoreFloat4x4(psMatrix, XMMatrixMultiply(t, m));
}
```

#### 2h. pie_RotProj

The current `pie_RotProj` performs a software perspective projection. Keep it for now but convert the internal arithmetic:

```cpp
int32 pie_RotProj(iVector *v3d, iPoint *v2d) {
    XMVECTOR pos    = XMVectorSet(
        static_cast<float>(v3d->x),
        static_cast<float>(v3d->y),
        static_cast<float>(v3d->z), 1.0f);
    XMMATRIX m      = XMLoadFloat4x4(psMatrix);
    XMVECTOR tpos   = XMVector4Transform(pos, m);

    XMFLOAT4 r;
    XMStoreFloat4(&r, tpos);
    const float x = r.x, y = r.y, z = r.z;

    // Perspective divide (same logic as before — no change to render semantics yet)
    const int32 zfx = static_cast<int32>(z) >> psRendSurface->xpshift;
    const int32 zfy = static_cast<int32>(z) >> psRendSurface->ypshift;
    const int32 zz  = static_cast<int32>(z) >> STRETCHED_Z_SHIFT;

    if (zfx <= 0 || zfy <= 0 || zz < MIN_STRETCHED_Z) {
        v2d->x = LONG_WAY;
        v2d->y = LONG_WAY;
    } else {
        v2d->x = psRendSurface->xcentre + static_cast<int32>(x) / zfx;
        v2d->y = psRendSurface->ycentre - static_cast<int32>(y) / zfy;
    }
    return zz;
}
```

> **Future work (not in scope here):** Replace the hand-rolled perspective projection with a proper D3D-compatible view-projection matrix (`XMMatrixLookAtLH` + `XMMatrixPerspectiveFovLH`), feed transformed vertices directly to the D3D vertex buffer, and remove `psRendSurface->xpshift` from the hot path.

#### 2i. pie_ROTATE_PROJECT macro

Replace the macro with an inline function to avoid expression-statement syntax issues with XMVECTOR/XMMATRIX under C++23:

```cpp
inline void pie_RotateProject(
    float x, float y, float z,
    int32& xs, int32& ys) noexcept
{
    XMVECTOR v = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR t = XMVector4Transform(v, XMLoadFloat4x4(psMatrix));
    XMFLOAT4 r; XMStoreFloat4(&r, t);

    const int32 zfx = static_cast<int32>(r.z) >> psRendSurface->xpshift;
    const int32 zfy = static_cast<int32>(r.z) >> psRendSurface->ypshift;
    if (zfx > 0 && zfy > 0) {
        xs = psRendSurface->xcentre + static_cast<int32>(r.x) / zfx;
        ys = psRendSurface->ycentre - static_cast<int32>(r.y) / zfy;
    } else {
        xs = ys = 1 << 15;
    }
}
```

#### 2j. pie_VectorNormalise

Replace the octahedral approximation with proper float math:

```cpp
void pie_VectorNormalise(iVector *v) {
    XMVECTOR vec = XMVectorSet(
        static_cast<float>(v->x),
        static_cast<float>(v->y),
        static_cast<float>(v->z), 0.0f);
    vec = XMVector3Normalize(vec);
    XMFLOAT3 f; XMStoreFloat3(&f, vec);
    // Store back as 12.12 fixed-point (callers that use the result in
    // fixed-point cross products still expect this scale — see Phase 4)
    v->x = FloatToFP12(f.x);
    v->y = FloatToFP12(f.y);
    v->z = FloatToFP12(f.z);
}
```

After the iVector type itself is migrated to `XMFLOAT3` (Phase 3), this function becomes trivial.

#### 2k. pie_SurfaceNormal

```cpp
void pie_SurfaceNormal(iVector *p1, iVector *p2, iVector *p3, iVector *v) {
    XMVECTOR v1 = XMVectorSet(
        static_cast<float>(p3->x - p1->x),
        static_cast<float>(p3->y - p1->y),
        static_cast<float>(p3->z - p1->z), 0.0f);
    XMVECTOR v2 = XMVectorSet(
        static_cast<float>(p2->x - p1->x),
        static_cast<float>(p2->y - p1->y),
        static_cast<float>(p2->z - p1->z), 0.0f);
    XMVECTOR n = XMVector3Normalize(XMVector3Cross(v1, v2));
    XMFLOAT3 f; XMStoreFloat3(&f, n);
    // Return as 12.12 for existing callers
    v->x = FloatToFP12(f.x);
    v->y = FloatToFP12(f.y);
    v->z = FloatToFP12(f.z);
}
```

#### 2l. pie_VectorInverseRotate0

```cpp
void pie_VectorInverseRotate0(iVector *v1, iVector *v2) {
    XMVECTOR pos = XMVectorSet(
        FP12ToFloat(v1->x),
        FP12ToFloat(v1->y),
        FP12ToFloat(v1->z), 0.0f);
    // Inverse rotate = multiply by transpose of rotation block
    XMMATRIX m       = XMLoadFloat4x4(psMatrix);
    XMMATRIX mRotInv = XMMatrixTranspose(m);   // valid for pure rotation matrices
    XMVECTOR result  = XMVector3TransformNormal(pos, mRotInv);
    XMFLOAT3 f; XMStoreFloat3(&f, result);
    // Return as 12.12
    v2->x = FloatToFP12(f.x);
    v2->y = FloatToFP12(f.y);
    v2->z = FloatToFP12(f.z);
}
```

---

### Phase 3: Migrate iVector and iQuat to Float Types

**Effort:** 2–3 days
**Risk:** High — `iVector` is used pervasively; do this module-by-module

#### 3a. New type definitions in PieTypes.h

```cpp
// Keep iVector as a plain integer struct for game-logic coordinates
// (map grid, tile indices) — rename the 3D math role to use XMFLOAT3.
// Use a type alias with a deprecation attribute during the transition.

using Vec3  = XMFLOAT3;   // 3D float vector for math/rendering
using Vec3d = XMFLOAT4;   // Homogeneous 3D float vector

// iVector stays for pure integer use (grid coords, pixel coords, etc.)
// Add explicit converters in Math.h.
```

For this codebase the safest approach is **not** to immediately rename `iVector`. Instead:
1. Add `Vec3` alongside it.
2. Convert each module's math-path to `Vec3`.
3. Leave integer-coordinate uses as `iVector`.

#### 3b. iQuat migration

`iQuat` (`int32 w,x,y,z`) is only defined in `Ivi.h` and does not appear to be actively used in any game logic (the PSX quaternion interpolation path was removed). Mark it deprecated and provide the DirectXMath quaternion representation via `XMVECTOR` (loaded from `XMFLOAT4`).

```cpp
// Ivi.h
struct [[deprecated("use XMFLOAT4 + XMQuaternion* functions")]] iQuat {
    int32 w, x, y, z;
};
```

---

### Phase 4: Replace Trig Lookup Tables

**Files:** `Trig.h`, `Trig.cpp`, `PieMatrix.h` (SIN/COS macros)
**Effort:** 1 day
**Risk:** Low — callers are straightforward

#### 4a. Replace SIN/COS macros

In `PieMatrix.h`, delete:
```cpp
#define SIN(X)  aSinTable[(uint16)(X) >> 4]
#define COS(X)  aSinTable[((uint16)(X) >> 4) + 1024]
```
Replace with:
```cpp
// Returns sin/cos in float — callers that previously used the 12.12
// integer result must be updated to float arithmetic.
[[nodiscard]] inline float SIN(int angle) noexcept {
    return XMScalarSin(AngleToRadians(angle));
}
[[nodiscard]] inline float COS(int angle) noexcept {
    return XMScalarCos(AngleToRadians(angle));
}
```

`XMScalarSin`/`XMScalarCos` are scalar (non-SIMD) wrappers in DirectXMath with the same accuracy as `sinf`/`cosf`, and are inline — no overhead.

#### 4b. Replace trigSin / trigCos (Trig.cpp)

The Framework trig layer uses a 360-entry table with `TRIG_DEGREES=360`:

```cpp
// Trig.h — new declarations
[[nodiscard]] inline FRACT trigSin(SDWORD angle) noexcept {
    return XMScalarSin(static_cast<float>(angle % 360) * (XM_2PI / 360.0f));
}
[[nodiscard]] inline FRACT trigCos(SDWORD angle) noexcept {
    return XMScalarCos(static_cast<float>(angle % 360) * (XM_2PI / 360.0f));
}
[[nodiscard]] inline FRACT trigInvSin(FRACT val) noexcept {
    return XMScalarASin(val);
}
[[nodiscard]] inline FRACT trigInvCos(FRACT val) noexcept {
    return XMScalarACos(val);
}
[[nodiscard]] inline FRACT trigIntSqrt(UDWORD val) noexcept {
    return sqrtf(static_cast<float>(val));
}
```

`trigInitialise` / `trigShutDown` become no-ops (allocating and populating the lookup tables is no longer needed).

Delete `Trig.cpp`. Make all functions header-only inline.

#### 4c. Delete aSinTable

In `PieMatrix.cpp`, delete:
```cpp
int aSinTable[SC_TABLESIZE + (SC_TABLESIZE/4)];
```
and the initialization loop in `pie_MatInit`. The declaration `extern SDWORD aSinTable[]` in `PieMatrix.h` is also removed.

---

### Phase 5: Replace Fractions.h MAKEINT inline assembly

**File:** `Fractions.h`
**Effort:** 30 minutes
**Risk:** None

The `__asm fld f; __asm fistp i` form is invalid in MSVC x64. Replace:

```cpp
// Old (x86 only, x64 broken):
__inline SDWORD MAKEINT(float f) {
    SDWORD i;
    __asm fld f;
    __asm fistp i;
    return i;
}

// New (C++23, all targets, same "round to nearest" semantics):
[[nodiscard]] inline SDWORD MAKEINT(float f) noexcept {
    return static_cast<SDWORD>(std::lroundf(f));
}
[[nodiscard]] inline SDWORD MAKEINT_D(float f) noexcept {
    return static_cast<SDWORD>(std::lroundf(f));
}
```

`std::lroundf` uses round-half-away-from-zero semantics (same as x87 `fistp` with the default rounding mode). Add `#include <cmath>` to replace `#include <math.h>`.

Also remove the `register` keyword in `PieMatrix.cpp`:
```cpp
// Old:  register t;  register cra, sra;
// New:  int t;       int cra, sra;  // or let the compiler decide
```

---

### Phase 6: Update Game Logic Callers

**Effort:** 2–3 days
**Risk:** Medium — many files, but changes are mechanical

#### 6a. Effects (StarStrike/Effects.h, Effects.cpp)

`EFFECT::position` and `EFFECT::velocity` are `PIEVECTORF` (`FRACT x,y,z` = `float x,y,z`). These are already float — no type change needed. The `rotation` and `spin` fields are `iVector` (used as integer Euler angles in legacy units). Keep them as `iVector` (integer angle units) and convert to radians at the point of use in `pie_MatRotY` etc., which now accept legacy angles natively.

Verify `GRAVITON_GRAVITY = (FRACT)(-800)` — this is a world-unit acceleration per update, not a 12.12 value. No change needed.

#### 6b. Formation (StarStrike/Formation.cpp)

```cpp
// Old:
xoffset = MAKEINT(FRACTmul(trigSin(dir), MAKEFRACT(dist)));
// New (after Phase 4 trig replacements — trigSin now returns float directly):
xoffset = static_cast<SDWORD>(trigSin(dir) * static_cast<float>(dist));
```
`MAKEFRACT`, `FRACTmul`, and `MAKEINT` collapse into direct arithmetic once `FRACT == float` and the trig functions return `float`. The macros already expand to `((float)(x) * (float)(y))` — they just add noise. Callers should be simplified to direct expressions.

#### 6c. Findpath (StarStrike/Findpath.cpp)

```cpp
// Old:
fraction = FRACTdiv(MAKEFRACT(timeSoFar), MAKEFRACT(MoveData->arrivalTime));
length   = fSQRT(FRACTmul(xDif, xDif) + FRACTmul(yDif, yDif));
// New:
const float fraction = static_cast<float>(timeSoFar) / static_cast<float>(MoveData->arrivalTime);
const float length   = sqrtf(static_cast<float>(xDif * xDif + yDif * yDif));
// Or using DirectXMath:
const XMVECTOR d = XMVectorSet(static_cast<float>(xDif), static_cast<float>(yDif), 0, 0);
const float length = XMVectorGetX(XMVector2Length(d));
```

#### 6d. Bspimd (NeuronClient/ivis02/Bspimd.cpp)

`IsPointOnPlane` already converts to float (via `MAKEFRACT`). After the migration it becomes:
```cpp
inline int IsPointOnPlane(PSPLANE *psPlane, iVector *vP) {
    XMVECTOR point  = XMVectorSet(
        static_cast<float>(vP->x - psPlane->vP.x),
        static_cast<float>(vP->y - psPlane->vP.y),
        static_cast<float>(vP->z - psPlane->vP.z), 0.0f);
    XMVECTOR normal = XMVectorSet(psPlane->a, psPlane->b, psPlane->c, 0.0f);
    const float dot = XMVectorGetX(XMVector3Dot(point, normal));
    if (fabsf(dot) < 0.01f) return IN_PLANE;
    return (dot < 0.0f) ? OPPOSITE_SIDE : SAME_SIDE;
}
```

#### 6e. fastRoot macro

```cpp
// Old: ((abs(x) > abs(y)) ? (abs(x) + abs(y)/2) : (abs(y) + abs(x)/2))
// Replace with:
inline float fastRoot(float x, float y) noexcept {
    return XMVectorGetX(XMVector2Length(XMVectorSet(x, y, 0, 0)));
}
// Or simply: sqrtf(x*x + y*y) — modern FPU is fast enough.
```

---

### Phase 7: Remove Legacy Fixed-Point Residuals

After Phases 1–6 are validated:

1. **Delete `aSinTable`** and its `extern` declaration.
2. **Delete `trigInitialise`/`trigShutDown`** implementations from `Trig.cpp`. Remove `Trig.cpp` entirely; `Trig.h` becomes header-only.
3. **Delete `FP12_SHIFT`, `FP12_MULTIPLIER`** from `Piedef.h` (or leave as documentation comments with `[[deprecated]]` constexpr replacements).
4. **Delete `pie_MatInit` sin-table loop** — only keep `pie_MatReset()` call.
5. **Delete the `FAST_FRACTS` #ifdef block** and its PSX-era `#else` branch.
6. **Delete `iQuat`** if confirmed unused.
7. **Delete `iVectorf`** (`double x,y,z`) — replace any users with `XMFLOAT3`.

---

## 5. Testing Strategy

### 5.1 Unit Tests (new, add to build)

Create `NeuronClient/Tests/MathTests.cpp`:

```cpp
// Test 1: Angle conversion round-trip
assert(RadiansToAngle(AngleToRadians(16384)) == 16384);   // DEG_360/4

// Test 2: SDMatrix → XMMATRIX → SDMatrix identity round-trip
SDMATRIX identity = kIdentityMatrix;
XMMATRIX xm = XMLoadFloat4x4(&identity);
SDMATRIX back; XMStoreFloat4x4(&back, xm);
assert(memcmp(&identity, &back, sizeof(SDMATRIX)) == 0);

// Test 3: Rotation sanity — rotate (1,0,0) 90° around Y → (0,0,1) in left-handed
XMVECTOR v = XMVectorSet(1,0,0,0);
XMMATRIX r = XMMatrixRotationY(XM_PIDIV2);
XMVECTOR vr = XMVector3TransformNormal(v, r);
// expect vr ≈ (0, 0, 1) — verify handedness

// Test 4: VectorNormalise produces unit vector
iVector v4{3, 4, 0};
pie_VectorNormalise(&v4);
float len = sqrtf(FP12ToFloat(v4.x)*FP12ToFloat(v4.x) + ...);
assert(fabsf(len - 1.0f) < 0.001f);
```

### 5.2 Visual Regression

Run the game with a known scenario (formation movement, unit rotation, camera pan). Compare screenshots before and after each phase using pixel-diff tooling. Focus on:
- Model orientation during `pie_MatRotY` calls
- Surface normal shading continuity
- Projection (no visible swim or scale shift)
- Formation angles

### 5.3 Numeric Regression for Game Logic

Log-compare the outputs of `trigSin(angle)`, `trigCos(angle)` for angles 0, 90, 180, 270 (in legacy units) before and after `Trig.cpp` deletion. The table-based version had quantization error; the new float version is more accurate — this is intentional and correct.

---

## 6. CMake Changes Summary

```cmake
# CMakeLists.txt (root) — already C++23, no change needed.

# NeuronClient/CMakeLists.txt
# Add to include directories (Windows SDK path is automatic with MSVC):
# target_include_directories(NeuronClient PUBLIC
#     ${CMAKE_SOURCE_DIR}/DX9/Include   # existing — keep
# )
# DirectXMath: no lib link needed, no new include path needed (in Windows SDK).
# If using vcpkg for portability:
# find_package(directxmath CONFIG REQUIRED)
# target_link_libraries(NeuronClient PRIVATE Microsoft::DirectXMath)
```

---

## 7. File Change Summary

| File | Action |
|---|---|
| `NeuronClient/Framework/Math.h` | **CREATE** — bridge header, angle/FP conversions |
| `NeuronClient/Framework/Fractions.h` | **MODIFY** — replace x87 asm MAKEINT; simplify macros |
| `NeuronClient/Framework/Trig.h` | **MODIFY** — make all functions inline, remove table declarations |
| `NeuronClient/Framework/Trig.cpp` | **DELETE** — entire file becomes dead code |
| `NeuronClient/ivis02/PieMatrix.h` | **MODIFY** — replace SDMATRIX, SIN/COS macros, transformation macros |
| `NeuronClient/ivis02/PieMatrix.cpp` | **MODIFY** — all functions; delete aSinTable, table init loop |
| `NeuronClient/ivis02/PieTypes.h` | **MODIFY** — add Vec3 alias, deprecate iVectorf/iQuat |
| `NeuronClient/ivis02/Piedef.h` | **MODIFY** — deprecate FP12_SHIFT, FP12_MULTIPLIER |
| `NeuronClient/ivis02/Ivi.h` | **MODIFY** — deprecate iQuat, ufixed |
| `NeuronClient/ivis02/Bspimd.cpp` | **MODIFY** — IsPointOnPlane to DirectXMath dot |
| `StarStrike/Effects.h/.cpp` | **MINOR MODIFY** — verify PIEVECTORF/iVector usage |
| `StarStrike/Formation.cpp` | **MODIFY** — simplify MAKEINT/MAKEFRACT/trig chains |
| `StarStrike/Findpath.cpp` | **MODIFY** — simplify FRACT arithmetic |
| `StarStrike/Droid.cpp` | **MODIFY** — simplify FRACTdiv/FRACTmul |
| `StarStrike/Move.cpp` | **MINOR MODIFY** — FRACTCONST → float literals |
| `StarStrike/Projectile.cpp` | **MODIFY** — trig call updates |
| `NeuronClient/CMakeLists.txt` | **MINOR MODIFY** — optional DirectXMath vcpkg hook |

---

## 8. Risks and Mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| Coordinate system handedness mismatch (left vs right) | High | Add rotation sanity unit test in Phase 2 before touching callers |
| Floating-point non-determinism breaking multiplayer sync | Medium | RTS games often use deterministic int-tick simulation; flag any gameplay path that used fixed-point for synchronisation and keep them integer until a determinism audit is done |
| `XMMATRIX` alignment violation if used in heap structs | Medium | Always use `XMFLOAT4X4` for storage; only use `XMMATRIX` on the stack in functions |
| Behavioural change in `pie_VectorNormalise` (octahedral → Euclidean) | Low | The Euclidean result is more correct; visual regression test covers this |
| Merge conflicts with parallel feature work | Low | Perform this migration on a dedicated branch; rebase frequently |
| `register` keyword removal causing unexpected codegen | None | It was ignored by compilers anyway since C++17 |

---

## 9. Rollback Strategy

Each phase is guarded by a CMake option and a `#define`:

```cmake
option(LEGACY_FIXEDPOINT "Use original 12.12 fixed-point math" OFF)
```

```cpp
// PieMatrix.cpp
#ifdef LEGACY_FIXEDPOINT
   // Original implementation
#else
   // DirectXMath implementation
#endif
```

Keep the legacy code block intact through Phases 0–6 with the `#ifdef`. Remove it only in Phase 7 after visual regression passes. This allows a one-line rollback at any point during development.

---

## 10. Estimated Effort

| Phase | Description | Estimate |
|---|---|---|
| 0 | CMake setup | 1 h |
| 1 | Math bridge header | 3 h |
| 2 | SDMATRIX → XMFLOAT4X4, all PieMatrix.cpp | 1 day |
| 3 | iVector/iQuat type aliases | 4 h |
| 4 | Trig table replacement | 1 day |
| 5 | MAKEINT asm fix, `register` removal | 1 h |
| 6 | Game logic callers | 2 days |
| 7 | Dead code removal | 2 h |
| Testing | Unit + visual regression | 1 day |
| **Total** | | **~6 working days** |

---

## 11. Post-Migration Opportunities

Once the fixed-point layer is gone and the codebase speaks `XMFLOAT3`/`XMMATRIX`:

1. **D3D11/D3D12 upgrade** — the vertex pipeline can be wired directly to `XMMATRIX` constant buffers with minimal adaptation.
2. **Proper view-projection camera** — replace the `psRendSurface->xpshift` perspective hack with `XMMatrixPerspectiveFovLH` + `XMMatrixLookAtLH`.
3. **SIMD-accelerate batch transforms** — unit positions during a frame update can be processed with `XMVector3TransformCoordStream`.
4. **Quaternion animation blending** — `iQuat` removal opens the path to `XMQuaternionSlerp` for smooth model rotation.
5. **Remove DX9 SDK vendored headers** — once all DX9 usage is replaced, the `DX9/Include` tree can be deleted from the repository.
