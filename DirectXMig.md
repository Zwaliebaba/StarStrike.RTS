# DirectX Renderer Standardisation – Migration Plan

## 1. Executive Summary

The rendering subsystem in `NeuronClient/ivis02/` currently supports **four** renderer
backends, selected at run-time through function-pointer tables and `switch`/`if` trees:

| Backend | Enum / Constant | Source files | Status |
|---|---|---|---|
| **Software surface** (`iV_MODE_SURFACE`) | `ENGINE_SR` | `Vsr.c` / `Vsr.h` | **Remove** |
| **DDX 640×480×256** (`iV_MODE_4101`) | `ENGINE_4101` | `V4101.c` / `V4101.h` | **Remove** |
| **3Dfx / Glide** | `ENGINE_GLIDE` | `3Dfx*.c/h` | Already excluded from build |
| **Direct3D (RGB / HAL / REF)** | `ENGINE_D3D` | `D3dmode.c/h`, `D3drender.c/h`, `Dx6TexMan.c/h`, `Texd3d.c/h` | **Keep – sole renderer** |

The goal is to **standardise on DirectX (Direct3D)** as the only renderer,
removing the Software (`Vsr`), `iV_MODE_4101` (`V4101`), and residual Glide
branches.  The PlayStation (`REND_PSX`) code is already `#ifdef PSX`-guarded
and can be left dormant — it compiles only on PSX builds.

---

## 2. Architecture Overview (Current)

```
  Game (StarStrike/)
    │
    │  war_GetRendMode() → WAR_REND_MODE enum
    │       │
    ▼       ▼
  Init.c: systemInitialise()
    │  switch(war_GetRendMode())
    │    REND_MODE_SOFTWARE → pie_Initialise(iV_MODE_4101)   ← remove
    │    REND_MODE_RGB      → pie_Initialise(REND_D3D_RGB)
    │    REND_MODE_HAL      → pie_Initialise(REND_D3D_HAL)   ← keep (default)
    │    REND_MODE_REF      → pie_Initialise(REND_D3D_REF)
    │
    ▼
  PieMode.c: pie_Initialise(mode)
    │  if (mode == REND_D3D_HAL)  → _mode_D3D_HAL()  ← keep
    │  else (SOFTWARE fallback)   → _mode_4101()      ← remove
    │
    ▼
  Rendmode.c: iV_RenderAssign(mode, surface)
    │  switch (mode)
    │    iV_MODE_SURFACE → wires _sr  function pointers   ← remove
    │    iV_MODE_4101    → wires _4101 function pointers   ← remove
    │    REND_D3D_*      → wires _dummyFunc*_D3D stubs     ← keep / clean up
    │
    ▼
  Function-pointer dispatch (iV_pLine, iV_ppBitmap, …)
```

### 2.1 Engine enum (`PieState.h`)

```c
typedef enum REND_ENGINE {
    ENGINE_UNDEFINED,
    ENGINE_16Bit,
    ENGINE_4101,    // ← remove
    ENGINE_SR,      // ← remove
    ENGINE_GLIDE,   // ← remove (already dead)
    ENGINE_D3D      // ← sole survivor
} REND_ENGINE;
```

### 2.2 War-level mode enum (`WarzoneConfig.h`)

```c
typedef enum WAR_REND_MODE {
    REND_MODE_SOFTWARE,   // ← remove
    REND_MODE_GLIDE,      // ← remove
    REND_MODE_RGB,        // keep (D3D RGB emulation)
    REND_MODE_HAL,        // keep (D3D hardware)
    REND_MODE_HAL2,       // keep
    REND_MODE_REF,        // keep (D3D reference)
} WAR_REND_MODE;
```

---

## 3. Affected Files – Full Inventory

### 3.1 Files to **delete entirely**

| File | Lines | Rationale |
|---|---|---|
| `NeuronClient/ivis02/V4101.c` | ~2 996 | Entire 4101 DDX software renderer |
| `NeuronClient/ivis02/V4101.h` | 68 | Header for above |
| `NeuronClient/ivis02/Vsr.c` | ~1 724 | Entire surface-renderer backend |
| `NeuronClient/ivis02/Vsr.h` | 56 | Header for above |
| `NeuronClient/ivis02/Amd3d.h` | AMD 3DNow! macros, only consumed by 4101 codepaths |

### 3.2 Files requiring **code edits** (NeuronClient/ivis02)

| File | Hit count | What to change |
|---|---|---|
| `Rendmode.c` | ~39 `_4101` refs, 6 `_sr` refs | Remove `iV_MODE_SURFACE` and `iV_MODE_4101` cases from `iV_RenderAssign()`; remove `cpuHas3DNow()` / `bHas3DNow`; remove `iV_MODE_4101` from `iV_VideoMemorySize()` switch |
| `Rendmode.h` | 3 refs | Remove `#include "V4101.h"`, `#include "Vsr.h"`, remove `iV_MODE_4101` and `iV_MODE_SURFACE` defines; remove `weHave3DNow()` prototype |
| `PieMode.c` | 11 refs | Remove `#include "V4101.h"`, `#include "Vsr.h"`; in `pie_Initialise()` delete SOFTWARE/4101 fallback branch; in `pie_ShutDown()` delete `ENGINE_4101` / `ENGINE_SR` / `ENGINE_GLIDE` cases; in `pie_ScreenFlip()` delete `ENGINE_4101` and `ENGINE_GLIDE` cases; in `pie_Clear()` delete `ENGINE_SR` case; in `pie_LocalRenderBegin/End()` delete `ENGINE_4101`/`ENGINE_SR` cases |
| `PieState.h` | 2 refs | Remove `ENGINE_4101`, `ENGINE_SR`, `ENGINE_GLIDE` from `REND_ENGINE` enum |
| `PieState.c` | indirect | `pie_SetRenderEngine()` – the else-branch already sets `bHardware = FALSE`; simplify but no API change needed |
| `PieBlitFunc.c` | 36 `ENGINE_4101`/`ENGINE_SR` refs + 18 `ENGINE_GLIDE` | In every `switch(pie_GetRenderEngine())`, collapse `ENGINE_4101`/`ENGINE_SR`/`ENGINE_GLIDE` cases; keep only `ENGINE_D3D` and `default` |
| `Piedraw.c` | 12 `_4101` refs, 1 `_sr`, 18 `ENGINE_GLIDE` | Remove or dead-code-eliminate `ENGINE_4101` conditionals and direct `_4101` function calls in `pie_IvisPoly()` / `pie_IvisPolyFrame()`; remove Glide branches |
| `Piefunc.c` | 5 `iV_MODE_4101` refs, 6 `ENGINE_GLIDE` | Remove software-mode branches in `pie_DrawViewingWindow()`, `pie_TransColouredTriangle()`, `pie_DownLoadBufferToScreen()`, etc. |
| `TextDraw.c` | 6 refs | Remove `ENGINE_4101` / `ENGINE_SR` cases in switch statements |
| `Rendfunc.c` | indirect | Shared helpers (`SetTransFilter`, `TransBoxFill`, `line`, `box`) remain; no direct 4101/SR symbols but verify |
| `Ivi.c` | 1 `ENGINE_GLIDE` | Remove Glide branch in `iV_Reset()` |

### 3.3 Files requiring **code edits** (StarStrike/)

| File | Hit count | What to change |
|---|---|---|
| `Init.c` | 2 | Remove `REND_MODE_SOFTWARE` / `default` case in `systemInitialise()` (lines ~1212-1220); make `REND_MODE_HAL` the default |
| `ClParse.c` | 4 | Remove `-software` command-line handler; under `COVERMOUNT`, default to `REND_MODE_HAL` instead of `REND_MODE_SOFTWARE` |
| `Config.c` | 1 | In `loadRenderMode()`, if stored mode is `REND_MODE_SOFTWARE`, migrate to `REND_MODE_HAL` |
| `WarzoneConfig.h` | 1 | Remove `REND_MODE_SOFTWARE` and `REND_MODE_GLIDE` from `WAR_REND_MODE` enum |
| `WarzoneConfig.c` | 2 | In `war_SetDefaultStates()`, change default from `REND_MODE_SOFTWARE` to `REND_MODE_HAL`; in `war_SetRendMode()`, remove `REND_MODE_SOFTWARE` special-casing |
| `FrontEnd.c` | 3 | Remove `FRONTEND_SOFTWARE` menu option and its `REND_MODE_SOFTWARE` display string; remove Glide menu option |
| `Effects.c` | 8 | Remove all `if(pie_GetRenderEngine() == ENGINE_4101)` guards – let the D3D path always run |
| `IntDisplay.c` | 6 (+ 2 commented) | Remove `iV_RenderAssign(iV_MODE_4101, …)` calls; these force software-mode for specific 2D blits – replace with current renderer re-assign |
| `MapDisplay.c` | 2 | Same pattern – remove `iV_RenderAssign(iV_MODE_4101, …)` calls |
| `Component.c` | 2 | Remove `ENGINE_4101` guard branches |
| `Display3D.c` | 2 | Remove `ENGINE_4101` guard branches |
| `SeqDisp.c` | 1 | Verify; may reference `iV_MODE_4101` – remove if present |

---

## 4. Step-by-Step Migration Plan

### Phase 1 – Preparation & safety net

| # | Task | Details |
|---|---|---|
| 1.1 | **Create a Git branch** | `feature/directx-only-renderer` off `main` |
| 1.2 | **Ensure current build succeeds** | `cmake --build` with Ninja, confirm zero errors |
| 1.3 | **Grep baseline** | Record all hits for `ENGINE_4101`, `ENGINE_SR`, `ENGINE_GLIDE`, `_4101`, `_sr`, `iV_MODE_4101`, `iV_MODE_SURFACE`, `REND_MODE_SOFTWARE`, `REND_MODE_GLIDE` |

### Phase 2 – Remove enum values & defines

| # | File | Action |
|---|---|---|
| 2.1 | `PieState.h` | Remove `ENGINE_16Bit`, `ENGINE_4101`, `ENGINE_SR`, `ENGINE_GLIDE` from `REND_ENGINE` |
| 2.2 | `Rendmode.h` | Remove `#define iV_MODE_4101`, `#define iV_MODE_SURFACE`, `#define REND_16BIT`; remove `#include "V4101.h"`, `#include "Vsr.h"`; remove `weHave3DNow()` prototype |
| 2.3 | `WarzoneConfig.h` | Remove `REND_MODE_SOFTWARE`, `REND_MODE_GLIDE` from `WAR_REND_MODE` enum |

### Phase 3 – Update initialisation path

| # | File | Action |
|---|---|---|
| 3.1 | `WarzoneConfig.c` | `war_SetDefaultStates()` → default to `REND_MODE_HAL`; `war_SetRendMode()` → remove SOFTWARE-specific fog/translucent disabling |
| 3.2 | `Init.c` | `systemInitialise()` → remove `REND_MODE_SOFTWARE` / `default` case; make `REND_MODE_HAL` the `default` |
| 3.3 | `ClParse.c` | Remove `-software` handler; in `COVERMOUNT` blocks, default to `REND_MODE_HAL` |
| 3.4 | `Config.c` | `loadRenderMode()` → if persisted value is now-deleted `REND_MODE_SOFTWARE` (0), remap to `REND_MODE_HAL` |
| 3.5 | `FrontEnd.c` | Remove the software-mode menu item (`FRONTEND_SOFTWARE`) and its display code; remove Glide menu entry |

### Phase 4 – Gut the render-assign dispatch

| # | File | Action |
|---|---|---|
| 4.1 | `Rendmode.c` | In `iV_RenderAssign()`: remove entire `case iV_MODE_SURFACE:` block; remove entire `case iV_MODE_4101:` block; keep `case REND_D3D_RGB/HAL/REF:` only. Remove `cpuHas3DNow()`, `bHas3DNow`, `weHave3DNow()`. Remove `iV_MODE_4101` from `iV_VideoMemorySize()` switch. Remove `#include "V4101.h"`, `#include "Vsr.h"` |
| 4.2 | `PieMode.c` | In `pie_Initialise()`: remove the `else // REND_MODE_SOFTWARE` fallback that calls `_mode_4101()`; assert or error on unknown mode. In `pie_ShutDown()`: remove `ENGINE_4101`, `ENGINE_SR`, `ENGINE_GLIDE` cases. In `pie_ScreenFlip()`: remove `ENGINE_4101` and `ENGINE_GLIDE` cases. In `pie_Clear()`: remove `ENGINE_SR` case. In `pie_LocalRenderBegin/End()`: remove `ENGINE_4101`/`ENGINE_SR` cases. Remove `#include "V4101.h"`, `#include "Vsr.h"` |

### Phase 5 – Clean up per-file engine branches

For every file listed in §3.2 and §3.3 above, apply the same pattern:

1. Find every `if (pie_GetRenderEngine() == ENGINE_4101)` or `switch` case.
2. If the branch **disables/skips** a feature for software mode → delete the guard, keep the D3D-path code.
3. If the branch **calls a `_4101`/`_sr` function** → delete the branch; ensure the D3D path runs unconditionally.
4. If the branch tests `rendSurface.usr == iV_MODE_4101` (e.g. `IntDisplay.c`, `MapDisplay.c`, `Piefunc.c`) → delete the condition / entire block; replace `iV_RenderAssign(iV_MODE_4101, &rendSurface)` with the current renderer's mode.

Detailed per-file:

| File | Specific actions |
|---|---|
| `PieBlitFunc.c` | 36 occurrences of `ENGINE_4101`/`ENGINE_SR` in `switch` cases — collapse these with their `ENGINE_GLIDE` siblings; keep `ENGINE_D3D` behaviour only |
| `Piedraw.c` | Remove 4 `ENGINE_4101` conditionals; remove 4 direct `_4101` function calls in `pie_IvisPoly`/`pie_IvisPolyFrame`; remove 18 `ENGINE_GLIDE` branches |
| `Piefunc.c` | Remove 5 `iV_MODE_4101` branches; remove 6 `ENGINE_GLIDE` branches |
| `TextDraw.c` | Remove 3 pairs of `ENGINE_4101`/`ENGINE_SR` switch-case entries |
| `Ivi.c` | Remove `ENGINE_GLIDE` branch in `iV_Reset()` |
| `Effects.c` | Remove 8 `ENGINE_4101` guards |
| `IntDisplay.c` | Remove 4 active `iV_RenderAssign(iV_MODE_4101, …)` calls |
| `MapDisplay.c` | Remove 2 `iV_RenderAssign(iV_MODE_4101, …)` calls |
| `Component.c` | Remove 2 `ENGINE_4101` guards |
| `Display3D.c` | Remove 2 `ENGINE_4101` guards |
| `SeqDisp.c` | Verify and remove any residual reference |

### Phase 6 – Delete dead source files

| # | File to delete |
|---|---|
| 6.1 | `NeuronClient/ivis02/V4101.c` (~2 996 lines) |
| 6.2 | `NeuronClient/ivis02/V4101.h` (68 lines) |
| 6.3 | `NeuronClient/ivis02/Vsr.c` (~1 724 lines) |
| 6.4 | `NeuronClient/ivis02/Vsr.h` (56 lines) |
| 6.5 | `NeuronClient/ivis02/Amd3d.h` (AMD 3DNow! — only used by 4101 path and `cpuHas3DNow`) |

These are picked up automatically by the `file(GLOB … "ivis02/*.c" "ivis02/*.h")`
in `NeuronClient/CMakeLists.txt`, so deleting the files is sufficient — no CMake
edit is required.

### Phase 7 – Simplify `PieState.c` engine helper

After removal, `pie_SetRenderEngine()` can be simplified:

```c
void pie_SetRenderEngine(REND_ENGINE rendEngine)
{
    rendStates.rendEngine = rendEngine;
    rendStates.bHardware = (rendEngine == ENGINE_D3D);
}
```

`pie_Hardware()` now always returns `TRUE` when the engine is running.

### Phase 8 – Build & verify

| # | Task |
|---|---|
| 8.1 | `cmake --build` — fix any remaining references the grep missed |
| 8.2 | Full-text search for `_4101`, `_sr`, `ENGINE_SR`, `ENGINE_4101`, `ENGINE_GLIDE`, `REND_MODE_SOFTWARE`, `REND_MODE_GLIDE`, `iV_MODE_4101`, `iV_MODE_SURFACE`, `weHave3DNow`, `cpuHas3DNow` — confirm zero hits outside of comments |
| 8.3 | Smoke-test with `-D3D` command-line flag — confirm initialisation & rendering |

---

## 5. Unused Functions Eligible for Cleanup

These functions exist **only** to serve the removed backends and can be deleted
once the migration is complete.

### 5.1 V4101.c / V4101.h — all symbols (~50+ functions)

All exported from `V4101.h`:

- `_mode_4101`, `_close_4101`, `_vsync_4101`, `_bank_off_4101`, `_bank_on_4101`
- `_palette_4101`, `_clear_4101`
- Primitives: `_hline_4101`, `_vline_4101`, `_line_4101`, `_aaline_4101`, `_pixel_4101`
- Shapes: `_circle_4101`, `_circlef_4101`, `_boxf_4101`, `_box_4101`
- Triangles: `_ftriangle_4101`, `_tgtriangle_4101`, `_gtriangle_4101`, `_ttriangle_4101`, `_tttriangle_4101`, `_triangle_4101`, `_tstriangle_4101`, `_ttstriangle_4101`
- Polygons: `_fpolygon_4101`, `_gpolygon_4101`, `_tpolygon_4101`, `_tgpolygon_4101`, `_tspolygon_4101`, `_ttpolygon_4101`, `_ttwpolygon_4101`, `_polygon_4101`, `_ttspolygon_4101`
- Quads: `_fquad_4101`, `_gquad_4101`, `_tquad_4101`, `_ttquad_4101`, `_quad_4101`
- Bitmaps: `_bitmap_4101`, `_tbitmapcolour_4101`, `_bitmapcolour_4101`, `_rbitmap_4101`, `_rbitmapr90_4101`, `_rbitmapr180_4101`, `_rbitmapr270_4101`, `_gbitmap_4101`, `_tbitmap_4101`, `_sbitmap_4101`, `_bitmapr90_4101`, `_bitmapr180_4101`, `_bitmapr270_4101`
- `iV_StrobeLine`

### 5.2 Vsr.c / Vsr.h — all symbols (~40+ functions)

- `_mode_sr`, `_close_sr`, `_vsync_sr`, `_bank_off_sr`, `_bank_on_sr`
- `_palette_sr`, `_palette_setup_sr`, `_clear_sr`
- Primitives: `_hline_sr`, `_vline_sr`, `_line_sr`, `_aaline_sr`, `_pixel_sr`
- Shapes: `_circle_sr`, `_circlef_sr`, `_boxf_sr`, `_box_sr`
- Triangles: `_ftriangle_sr`, `_gtriangle_sr`, `_ttriangle_sr`, `_tttriangle_sr`, `_triangle_sr`
- Polygons: `_fpolygon_sr`, `_gpolygon_sr`, `_tpolygon_sr`, `_ttpolygon_sr`, `_polygon_sr`
- Quads: `_fquad_sr`, `_gquad_sr`, `_tquad_sr`, `_ttquad_sr`, `_quad_sr`
- Bitmaps: `_bitmap_sr`, `_bitmapcolour_sr`, `_tbitmapcolour_sr`, `_rbitmap_sr`, `_rbitmapr90_sr`, `_rbitmapr180_sr`, `_rbitmapr270_sr`, `_gbitmap_sr`, `_tbitmap_sr`, `_sbitmap_sr`, `_bitmapr90_sr`, `_bitmapr180_sr`, `_bitmapr270_sr`

### 5.3 Rendmode.c — dead helpers

- `cpuHas3DNow()` — inline x86 asm; 3DNow! detection only relevant to software path
- `weHave3DNow()` — accessor for above
- `bHas3DNow` — static variable

### 5.4 Piedraw.c — software-only drawing routines

- `pie_IvisPoly()` / `pie_IvisPolyFrame()` — low-level ivis-style polygon draw used **only** when `ENGINE_4101`. Verify no D3D path calls these before removing.

### 5.5 FrontEnd.c — UI dead code

- `FRONTEND_SOFTWARE` widget / menu item definition
- `REND_MODE_SOFTWARE` display-string formatting in `displayTitleBitmap()`

### 5.6 Amd3d.h — entire file

AMD 3DNow! inline assembly macros. Only consumed by the `cpuHas3DNow()` detection
and potentially 4101 inner loops. Safe to delete.

---

## 6. Risk Assessment & Mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| `IntDisplay.c` / `MapDisplay.c` rely on 4101 for 2D overlay blitting | Medium | These files call `iV_RenderAssign(iV_MODE_4101, …)` to temporarily switch to software blitting for radar/minimap. After removal, verify D3D can perform the same blits via its existing `pie_D3DSetupRenderForFlip` / texture upload path. If not, implement a D3D-native 2D blit. |
| `Piedraw.c` calls `_tttriangle_4101` etc. directly (not via function pointers) | Medium | Lines 1815–1836 call 4101 functions from inside `pie_IvisPoly`. Confirm these paths are only reached when `ENGINE_4101`; if so, they die with `pie_IvisPoly`. |
| Persisted `renderMode=0` (`REND_MODE_SOFTWARE`) in user registry/config | Low | `Config.c` must map old value 0 → `REND_MODE_HAL` on load |
| Command-line scripts using `-software` | Low | Remove flag; document in release notes |
| 3Dfx/Glide code already excluded but symbols still litter switch statements | Low | Clean up simultaneously for consistency |

---

## 7. Estimated Scope

| Category | Lines removed (approx.) | Lines edited (approx.) |
|---|---|---|
| Deleted files (V4101 + Vsr + Amd3d) | **~4 850** | 0 |
| Rendmode.c (switch cases + 3DNow) | ~200 | ~20 |
| PieMode.c (init + shutdown + flip) | ~60 | ~15 |
| PieBlitFunc.c (switch cleanup) | ~120 | ~30 |
| Piedraw.c (conditionals + direct calls) | ~80 | ~20 |
| Piefunc.c | ~30 | ~10 |
| TextDraw.c | ~20 | ~5 |
| StarStrike game files (8 files) | ~60 | ~30 |
| Enums & headers | ~15 | ~10 |
| **Total** | **~5 435** | **~140** |

---

## 8. Verification Checklist

- [ ] `cmake --build` succeeds with zero errors (Ninja generator)
- [ ] No remaining references to `ENGINE_4101`, `ENGINE_SR`, `ENGINE_GLIDE`
- [ ] No remaining references to `iV_MODE_4101`, `iV_MODE_SURFACE`, `REND_MODE_SOFTWARE`, `REND_MODE_GLIDE`
- [ ] No remaining `#include "V4101.h"` or `#include "Vsr.h"`
- [ ] `V4101.c`, `V4101.h`, `Vsr.c`, `Vsr.h`, `Amd3d.h` no longer present
- [ ] Application launches with `-D3D` flag and renders correctly
- [ ] Old persisted `renderMode=0` config is gracefully migrated
- [ ] Radar / minimap overlay renders correctly (was using 4101 blitting path)
- [ ] Front-end options menu no longer shows "Software" or "3DFX" options
