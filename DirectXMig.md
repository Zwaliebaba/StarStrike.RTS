# DirectX Migration Plan: Direct3D 3 → Direct3D 9 (9.0c)

**Project:** StarStrike RTS
**Author:** Principal Graphics Engineer Review
**Status:** Draft for Validation
**Target API:** Direct3D 9.0c (`d3d9.h` / `d3d9.lib`)
**Language:** C11 (no C++ migration; COM via C macros)
**Strategy:** Full rendering layer rewrite with preserved public API surface

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current State Assessment](#2-current-state-assessment)
3. [Target Architecture](#3-target-architecture)
4. [API Mapping Reference](#4-api-mapping-reference)
5. [Phase 1 — Remove DirectDraw, Establish D3D9 Device](#5-phase-1--remove-directdraw-establish-d3d9-device)
6. [Phase 2 — Texture System Migration](#6-phase-2--texture-system-migration)
7. [Phase 3 — Fixed-Point Math Conversion Layer](#7-phase-3--fixed-point-math-conversion-layer)
8. [Phase 4 — Vertex Format and Draw Call Migration](#8-phase-4--vertex-format-and-draw-call-migration)
9. [Phase 5 — State Management Migration](#9-phase-5--state-management-migration)
10. [Phase 6 — Resolution and Widescreen Support](#10-phase-6--resolution-and-widescreen-support)
11. [Phase 7 — Video Playback Repair](#11-phase-7--video-playback-repair)
12. [Phase 8 — Build System Update](#12-phase-8--build-system-update)
13. [Phase 9 — Validation and Testing](#13-phase-9--validation-and-testing)
14. [Risk Register](#14-risk-register)
15. [C-in-COM Wrapping Strategy](#15-c-in-com-wrapping-strategy)
16. [File Impact Matrix](#16-file-impact-matrix)
17. [Open Questions and Critical Missing Items](#17-open-questions-and-critical-missing-items)

---

## 1. Executive Summary

StarStrike RTS currently uses **Direct3D 3** (a component of DirectX 6, circa 1997) alongside **DirectDraw 4** for surface and display management. The rendering layer resides entirely in `NeuronClient/ivis02/` and is implemented in **pure C (C11)** using the fixed-function pipeline with no programmable shaders.

The goal of this migration is to replace the D3D3 + DirectDraw 4 rendering stack with **Direct3D 9.0c** while:

- Remaining in pure C (no C++ migration)
- Preserving the public rendering API surface visible to game-side code
- Eliminating the DirectDraw dependency entirely (D3D9 is self-contained)
- Enabling **arbitrary widescreen resolutions** (replacing the hardcoded 640×480 assumption)
- Repairing the broken **video playback** system, which depends on DirectDraw surfaces

D3D9 retains the fixed-function pipeline, so shaders are not required and the majority of render state concepts translate directly. The primary complexity lies in:
1. Replacing all DirectDraw surface management with D3D9 equivalents
2. Converting 8-bit palettized textures (unsupported in practice in D3D9) to 32-bit ARGB at load time
3. Updating the math projection pipeline to handle dynamic aspect ratios
4. Rebuilding video playback without a DirectDraw surface target

---

## 2. Current State Assessment

### 2.1 Graphics Stack Summary

| Dimension | Current Value |
|---|---|
| API | Direct3D 3 (`IDirect3D3`, `IDirect3DDevice3`) |
| Surface Management | DirectDraw 4 (`IDirectDraw4`, `IDirectDrawSurface4`) |
| Pipeline | Fixed-function (no shaders, no HLSL) |
| Vertex format | `D3DTLVERTEX` (pre-transformed + lit) |
| Math | 12.12 fixed-point with precomputed sin/cos tables |
| Textures | 8-bit palettized + 16/24/32-bit true color via DD surfaces |
| Resolutions supported | 640×480 @ 16-bit (hardcoded) |
| Language | C11 |
| COM calls | `lpVtbl->Method(ptr, ...)` style |
| Video | ESCAPE 124/130 codec via proprietary `WINSTR.LIB` → DirectDraw surface |

### 2.2 Key Source Files in `NeuronClient/ivis02/`

| File | Role | Migration Impact |
|---|---|---|
| `D3drender.h/.c` | Device init, `DISP3DGLOBALS`, draw primitives | **Full rewrite** |
| `D3dmode.h/.c` | Swap chain / surface setup, scene begin/end | **Full rewrite** |
| `Dx6TexMan.h/.c` | D3D3 texture manager (DD surfaces) | **Full rewrite** |
| `Texd3d.h/.c` | DD surface creation, palette conversion | **Full rewrite** |
| `PiePalette.h/.c` | 256-entry palette, shade tables, 16-bit conversion | **Partial rewrite** |
| `PieMatrix.h/.c` | 12.12 fixed-point matrix math, projection | **Conversion layer added** |
| `Piedraw.h/.c` | 3D shape and polygon draw dispatcher | **Moderate changes** |
| `Piefunc.h/.c` | 2D image, lines, boxes, sky, water | **Moderate changes** |
| `PieBlitFunc.h/.c` | 2D blitting | **Moderate changes** |
| `PieState.h/.c` | Render state setter functions | **API-level remap** |
| `Rendmode.h/.c` | `REND_ENGINE` enum, dispatcher | **Minor changes** |
| `PieTexture.h/.c` | 8-bit texture page download | **Partial rewrite** |
| `Piedef.h` | Vertex and polygon structures | **Vertex struct update** |

### 2.3 Known Broken Items (Pre-Migration)

- **Video playback** (`NeuronClient/Sequence/Sequence.c`): Depends on `LPDIRECTDRAWSURFACE4` as the render target; completely non-functional when DirectDraw is removed.
- `WINSTR.LIB` / `STREAMER.H`: Proprietary Eidos streaming library with no source. It renders into a CPU-side byte buffer or a DirectDraw surface. No update path exists without source.

### 2.4 Dead Code (Already Removed or Stubbed)

The following are confirmed dead and should not factor into migration effort:
- `ENGINE_4101_REMOVED`, `ENGINE_SR_REMOVED`, `ENGINE_GLIDE_REMOVED` in `REND_ENGINE`
- All `#if 0` blocks and Glide-era paths (cleaned in commit `71af3974`)
- All `WIN32` platform guards (removed in `2a7d4f08`)

---

## 3. Target Architecture

### 3.1 D3D9 Device Model (replacing D3D3 + DirectDraw)

```
Direct3DCreate9(D3D_SDK_VERSION)
    └── IDirect3D9
            └── IDirect3D9_CreateDevice(...)
                    └── IDirect3DDevice9
                            ├── D3DPRESENT_PARAMETERS  (replaces DD surface chain)
                            ├── IDirect3DSwapChain9    (implicit, managed by device)
                            ├── IDirect3DTexture9[]    (replaces TEXPAGE_D3D)
                            └── IDirect3DDevice9_Present(...)  (replaces DDraw Flip)
```

### 3.2 New Global Device Structure

Replace `DISP3DGLOBALS` (currently in `D3drender.c`) with:

```c
typedef struct D3D9GLOBALS {
    IDirect3D9*         pD3D9;           /* Created by Direct3DCreate9()       */
    IDirect3DDevice9*   pDevice;         /* The rendering device               */
    D3DPRESENT_PARAMETERS presentParams; /* Swap chain configuration           */
    D3DDEVTYPE          devType;         /* D3DDEVTYPE_HAL or D3DDEVTYPE_REF   */
    D3DCAPS9            caps;            /* Device capabilities                */
    BOOL                bZBufferOn;
    BOOL                bDeviceLost;     /* Device-lost state flag             */
    BOOL                bWindowed;
    D3DFORMAT           backBufferFmt;   /* D3DFMT_R5G6B5 or D3DFMT_X8R8G8B8 */
} D3D9GLOBALS;

extern D3D9GLOBALS g_D3D9;
```

`LPDIRECTDRAW4`, `LPDIRECTDRAWSURFACE4` (back buffer, z-buffer), `LPDIRECT3DVIEWPORT3`, `LPDIRECT3DLIGHT`, and `LPDIRECT3DMATERIAL3` are all **eliminated**.

### 3.3 New Texture Page Structure

Replace `TEXPAGE_D3D` with:

```c
typedef struct TEXPAGE_D3D9 {
    IDirect3DTexture9*  pTexture;        /* D3D9 managed texture               */
    UWORD               iWidth;
    UWORD               iHeight;
    SWORD               widthShift;      /* Width = 1 << widthShift            */
    SWORD               heightShift;
    BOOL                bColourKeyed;    /* Alpha channel used for keying      */
} TEXPAGE_D3D9;
```

### 3.4 New Pre-Transformed Vertex Type

`D3DTLVERTEX` is removed from D3D9. Define a direct equivalent in `Piedef.h`:

```c
/* D3D9 pre-transformed vertex: matches D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1 */
#define PIE_FVF_D3D9  (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1)

typedef struct PIE_D3D9_VERTEX {
    float  sx, sy, sz;   /* Screen-space x, y; z for depth buffer (0.0–1.0)  */
    float  rhw;          /* Reciprocal homogeneous W (= 1.0 for screen-space) */
    DWORD  color;        /* D3DCOLOR diffuse  (0xAARRGGBB)                    */
    DWORD  specular;     /* D3DCOLOR specular (0xAARRGGBB)                    */
    float  tu, tv;       /* Texture UV coordinates                            */
} PIE_D3D9_VERTEX;
```

This is **binary-compatible** with the existing `D3DTLVERTEX` — no vertex data needs to change, only the type name and `FVF` descriptor.

---

## 4. API Mapping Reference

The following table is the authoritative cross-reference between the D3D3/DD4 API being removed and the D3D9 replacement. Every symbol in the left column must be eliminated from the codebase.

### 4.1 Device and Initialization

| D3D3 / DirectDraw 4 | D3D9 Replacement |
|---|---|
| `DirectDrawCreate4()` | `Direct3DCreate9(D3D_SDK_VERSION)` |
| `IDirectDraw4::CreateDevice()` (indirect via D3D3) | `IDirect3D9_CreateDevice()` |
| `IDirect3D3` | `IDirect3D9` |
| `IDirect3DDevice3` | `IDirect3DDevice9` |
| `LPDIRECT3D3` | `IDirect3D9*` |
| `LPDIRECT3DDEVICE3` | `IDirect3DDevice9*` |
| `LPDIRECTDRAW4` | *(removed — D3D9 manages its own swap chain)* |
| `IDirectDraw4::SetDisplayMode()` | `D3DPRESENT_PARAMETERS.BackBufferWidth/Height` |
| `IDirectDraw4::SetCooperativeLevel()` | `D3DPRESENT_PARAMETERS.Windowed / FullScreen_RefreshRateInHz` |
| `IDirectDraw4::GetDisplayMode()` | `IDirect3DDevice9_GetDisplayMode()` |
| `IDirect3DDevice3::BeginScene()` | `IDirect3DDevice9_BeginScene()` |
| `IDirect3DDevice3::EndScene()` | `IDirect3DDevice9_EndScene()` |
| `IDirectDraw4::Flip()` | `IDirect3DDevice9_Present(NULL, NULL, NULL, NULL)` |
| `IDirect3DDevice3::Clear()` (via viewport) | `IDirect3DDevice9_Clear(0, NULL, D3DCLEAR_TARGET\|D3DCLEAR_ZBUFFER, ...)` |

### 4.2 Surfaces and Swap Chain

| D3D3 / DirectDraw 4 | D3D9 Replacement |
|---|---|
| `LPDIRECTDRAWSURFACE4` (back buffer) | Implicit in swap chain; `IDirect3DDevice9_GetBackBuffer()` if needed |
| `LPDIRECTDRAWSURFACE4` (z-buffer) | `D3DPRESENT_PARAMETERS.EnableAutoDepthStencil = TRUE` + `D3DPRESENT_PARAMETERS.AutoDepthStencilFormat = D3DFMT_D16` |
| `DDPIXELFORMAT` | `D3DFORMAT` |
| `DDSCAPS2` | *(removed)* |
| `IDirectDraw4::CreateSurface()` (for textures) | `IDirect3DDevice9_CreateTexture()` |
| `IDirectDrawSurface4::Lock()` | `IDirect3DTexture9_LockRect()` |
| `IDirectDrawSurface4::Unlock()` | `IDirect3DTexture9_UnlockRect()` |

### 4.3 Textures

| D3D3 / DirectDraw 4 | D3D9 Replacement |
|---|---|
| `LPDIRECT3DTEXTURE2` | `IDirect3DTexture9*` |
| `D3DTEXTUREHANDLE` | *(removed — use `IDirect3DTexture9*` directly)* |
| `IDirect3DDevice3::SetTexture(stage, hTex)` | `IDirect3DDevice9_SetTexture(stage, (IDirect3DBaseTexture9*)pTex)` |
| `IDirect3DDevice3::SetTextureStageState()` | `IDirect3DDevice9_SetTextureStageState()` *(same concept, new enum values)* |
| `D3DTEXTUREBLEND` enum (`MODULATEALPHA` etc.) | `D3DTSS_COLOROP` / `D3DTSS_ALPHAOP` stage state enums |
| Palette-on-DD-surface (`LPDIRECTDRAWPALETTE`) | *(removed — convert palette at texture load time to D3DFMT_A8R8G8B8)* |
| `dx6_SetBilinear()` setting `MAGFILTER`/`MINFILTER` | `IDirect3DDevice9_SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)` |

### 4.4 Viewport and Transforms

| D3D3 / DirectDraw 4 | D3D9 Replacement |
|---|---|
| `LPDIRECT3DVIEWPORT3` | `D3DVIEWPORT9` *(plain struct, set via `IDirect3DDevice9_SetViewport()`)* |
| `IDirect3DViewport3::Clear()` | `IDirect3DDevice9_Clear()` |
| `IDirect3DViewport3::SetViewport2()` | `IDirect3DDevice9_SetViewport(&vp)` |
| `IDirect3DDevice3::SetCurrentViewport()` | *(removed — viewport set directly on device)* |
| `IDirect3DDevice3::SetTransform(D3DTRANSFORMSTATE_WORLD, &mat)` | `IDirect3DDevice9_SetTransform(D3DTS_WORLD, &mat)` *(same concept)* |

### 4.5 Materials and Lights

| D3D3 / DirectDraw 4 | D3D9 Replacement |
|---|---|
| `LPDIRECT3DMATERIAL3` | *(removed — material is set as render states on device)* |
| `D3DMATERIALHANDLE` | *(removed)* |
| `IDirect3DMaterial3::SetMaterial()` | `IDirect3DDevice9_SetMaterial(&d3dMtrl)` |
| `IDirect3DDevice3::SetLightState()` | `IDirect3DDevice9_SetRenderState(D3DRS_AMBIENT, color)` etc. |
| `LPDIRECT3DLIGHT` | `IDirect3DDevice9_SetLight(index, &light)` |

### 4.6 Render States

| D3D3 State | D3D9 Equivalent |
|---|---|
| `D3DRENDERSTATE_ALPHABLENDENABLE` | `D3DRS_ALPHABLENDENABLE` |
| `D3DRENDERSTATE_SRCBLEND` | `D3DRS_SRCBLEND` |
| `D3DRENDERSTATE_DESTBLEND` | `D3DRS_DESTBLEND` |
| `D3DRENDERSTATE_COLORKEYENABLE` | `D3DRS_COLORKEYENABLE` *(removed in D3D9 — use alpha channel in texture)* |
| `D3DRENDERSTATE_ZENABLE` | `D3DRS_ZENABLE` |
| `D3DRENDERSTATE_ZWRITEENABLE` | `D3DRS_ZWRITEENABLE` |
| `D3DRENDERSTATE_ZFUNC` | `D3DRS_ZFUNC` |
| `D3DRENDERSTATE_FOGENABLE` | `D3DRS_FOGENABLE` |
| `D3DRENDERSTATE_FOGCOLOR` | `D3DRS_FOGCOLOR` |
| `D3DRENDERSTATE_SHADEMODE` | `D3DRS_SHADEMODE` |
| `D3DRENDERSTATE_CULLMODE` | `D3DRS_CULLMODE` |
| `D3DRENDERSTATE_ALPHABLENDENABLE` | `D3DRS_ALPHABLENDENABLE` |
| `D3DRENDERSTATE_ALPHATESTENABLE` | `D3DRS_ALPHATESTENABLE` |
| `D3DRENDERSTATE_ALPHAREF` | `D3DRS_ALPHAREF` |
| `D3DRENDERSTATE_ALPHAFUNC` | `D3DRS_ALPHAFUNC` |

### 4.7 Draw Calls

| D3D3 / DirectDraw 4 | D3D9 Replacement |
|---|---|
| `IDirect3DDevice3::DrawPrimitive(type, D3DFVF_TLVERTEX, ptr, count, flags)` | `IDirect3DDevice9_DrawPrimitive(type, 0, count-2)` with prior `SetFVF` + `DrawPrimitiveUP` |
| Specific: using user-pointer draw | `IDirect3DDevice9_DrawPrimitiveUP(type, primCount, pVerts, stride)` |
| `D3DPT_TRIANGLEFAN` | `D3DPT_TRIANGLEFAN` *(same)* |
| `D3DPT_LINELIST` | `D3DPT_LINELIST` *(same)* |
| `D3DPT_POINTLIST` | `D3DPT_POINTLIST` *(same)* |

> **Note on `DrawPrimitiveUP`:** D3D9 strongly prefers `IDirect3DVertexBuffer9`, but `DrawPrimitiveUP` (user pointer, no vertex buffer) is fully legal in D3D9 and is the lowest-friction migration path from D3D3's immediate-mode draw calls. Vertex buffer optimization is a post-migration performance concern.

### 4.8 Error Handling

| D3D3 / DirectDraw 4 | D3D9 |
|---|---|
| `DD_OK` | `D3D_OK` (same value, 0) |
| `DDERR_*` codes | `D3DERR_*` codes |
| `FAILED(hr)` macro | `FAILED(hr)` *(same — COM standard)* |
| Device lost: not explicitly handled | `D3DERR_DEVICELOST` / `IDirect3DDevice9_Reset()` required |

---

## 5. Phase 1 — Remove DirectDraw, Establish D3D9 Device

**Scope:** `D3drender.h/.c`, `D3dmode.h/.c`, `Rendmode.h/.c`, `CMakeLists.txt`
**Goal:** Get a D3D9 device creating successfully, a clear screen being presented, and the build compiling without DirectDraw headers.

### 5.1 New Device Initialization Flow

Replace the current `InitD3D()` / `_mode_D3D_HAL()` / `_mode_D3D_RGB()` / `_mode_D3D_REF()` hierarchy with a single `D3D9_InitDevice()` function:

```c
/* D3drender.c — new initialization */
D3D9GLOBALS g_D3D9 = {0};

BOOL D3D9_InitDevice(HWND hWnd, BOOL bFullscreen, UINT width, UINT height)
{
    g_D3D9.pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_D3D9.pD3D9) return FALSE;

    ZeroMemory(&g_D3D9.presentParams, sizeof(g_D3D9.presentParams));
    g_D3D9.presentParams.BackBufferWidth            = width;
    g_D3D9.presentParams.BackBufferHeight           = height;
    g_D3D9.presentParams.BackBufferFormat           = D3DFMT_R5G6B5;  /* 16-bit */
    g_D3D9.presentParams.BackBufferCount            = 1;
    g_D3D9.presentParams.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
    g_D3D9.presentParams.hDeviceWindow              = hWnd;
    g_D3D9.presentParams.Windowed                   = !bFullscreen;
    g_D3D9.presentParams.EnableAutoDepthStencil     = TRUE;
    g_D3D9.presentParams.AutoDepthStencilFormat     = D3DFMT_D16;
    g_D3D9.presentParams.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;

    HRESULT hr = IDirect3D9_CreateDevice(
        g_D3D9.pD3D9,
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,  /* Stay software to match D3D3 HAL behavior */
        &g_D3D9.presentParams,
        &g_D3D9.pDevice
    );
    if (FAILED(hr)) {
        /* Fallback to D3DDEVTYPE_REF */
        g_D3D9.devType = D3DDEVTYPE_REF;
        hr = IDirect3D9_CreateDevice(...);
    }
    if (FAILED(hr)) return FALSE;

    IDirect3DDevice9_GetDeviceCaps(g_D3D9.pDevice, &g_D3D9.caps);
    D3D9_SetDefaultRenderStates();
    return TRUE;
}
```

### 5.2 Frame Loop Replacement

Replace the DirectDraw `Flip()` and viewport `Clear()` calls:

```c
void D3D9_BeginScene(void)
{
    if (g_D3D9.bDeviceLost) {
        /* Attempt device recovery — see Phase 5 */
        D3D9_HandleDeviceLost();
        return;
    }
    IDirect3DDevice9_Clear(g_D3D9.pDevice, 0, NULL,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    IDirect3DDevice9_BeginScene(g_D3D9.pDevice);
}

void D3D9_EndScene(void)
{
    IDirect3DDevice9_EndScene(g_D3D9.pDevice);
    HRESULT hr = IDirect3DDevice9_Present(g_D3D9.pDevice, NULL, NULL, NULL, NULL);
    if (hr == D3DERR_DEVICELOST) {
        g_D3D9.bDeviceLost = TRUE;
    }
}
```

### 5.3 Device Lost Handling (New Requirement)

D3D3 had no device-lost concept. D3D9 introduces `D3DERR_DEVICELOST` + `IDirect3DDevice9_Reset()`. This must be handled:

```c
void D3D9_HandleDeviceLost(void)
{
    HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(g_D3D9.pDevice);
    if (hr == D3DERR_DEVICENOTRESET) {
        DTM9_ReleaseAllTextures();        /* Release all D3DPOOL_DEFAULT resources */
        IDirect3DDevice9_Reset(g_D3D9.pDevice, &g_D3D9.presentParams);
        DTM9_ReloadAllTextures();         /* Reload textures */
        D3D9_SetDefaultRenderStates();
        g_D3D9.bDeviceLost = FALSE;
    }
}
```

All `IDirect3DTexture9` objects created with `D3DPOOL_MANAGED` survive `Reset()` automatically. Use `D3DPOOL_MANAGED` for all textures to minimize reset complexity.

### 5.4 `REND_ENGINE` Update

In `Rendmode.h`, update the engine enum:

```c
typedef enum REND_ENGINE {
    ENGINE_UNDEFINED,
    ENGINE_D3D9           /* Replaces ENGINE_D3D (D3D3) */
} REND_ENGINE;
```

Remove all references to removed engine variants. The corresponding `_renderBegin_D3D()` / `_renderEnd_D3D()` function pointers in `Rendmode.c` must be retargeted to `D3D9_BeginScene()` / `D3D9_EndScene()`.

### 5.5 Remove DirectDraw Headers and Calls from All Files

Audit every `.c` / `.h` file for:
- `#include <ddraw.h>` → **remove**
- `#include <d3d.h>` (D3D3 header) → **replace with `#include <d3d9.h>`**
- Any `LPDIRECTDRAW4`, `LPDIRECTDRAWSURFACE4`, `LPDIRECTDRAWPALETTE` → **remove or replace**
- `D3DTEXTUREHANDLE`, `D3DMATERIALHANDLE` → **remove**
- `LPDIRECT3D3`, `LPDIRECT3DDEVICE3`, `LPDIRECT3DVIEWPORT3` → **replace**
- `LPDIRECT3DTEXTURE2` → **replace with `IDirect3DTexture9*`**
- `LPDIRECT3DMATERIAL3`, `LPDIRECT3DLIGHT` → **remove** (handled via device render states in D3D9)

**Deliverable:** Clean build that creates a D3D9 device, clears to black, and presents each frame.

---

## 6. Phase 2 — Texture System Migration

**Scope:** `Dx6TexMan.h/.c`, `Texd3d.h/.c`, `PiePalette.h/.c`, `PieTexture.h/.c`
**Goal:** Replace `TEXPAGE_D3D` (DD surface + D3D3 texture handle) with `TEXPAGE_D3D9` (`IDirect3DTexture9*`).

### 6.1 Palettized Texture Problem

D3D9 formally defines `D3DFMT_P8` (8-bit palettized) but hardware support is essentially non-existent on any GPU made after 2001. **Palettized textures must be converted to `D3DFMT_A8R8G8B8` at load time.**

Current path in `Texd3d.c` / `Dx6TexMan.c`:
1. Create `LPDIRECTDRAWSURFACE4` with `DDPF_PALETTEINDEXED8`
2. Attach `LPDIRECTDRAWPALETTE`
3. Obtain `LPDIRECT3DTEXTURE2` from the surface
4. Lock + fill with 8-bit indexed data

New D3D9 path:
1. Create `IDirect3DTexture9` with `D3DFMT_A8R8G8B8`, `D3DPOOL_MANAGED`
2. Lock mip level 0 (`LockRect`)
3. For each texel: look up `iColour` in the game palette → write `D3DCOLOR_ARGB(a, r, g, b)` into the 32-bit buffer
4. Apply color key: if texel matches color key, write alpha = 0; else alpha = 255
5. `UnlockRect()`

```c
/* New function signature replacing dtm_LoadTexSurface() */
BOOL DTM9_LoadTexture(iTexture* psIvisTex, SDWORD index)
{
    TEXPAGE_D3D9* pPage = &g_apTexPages[index];
    UWORD w = psIvisTex->width;
    UWORD h = psIvisTex->height;

    HRESULT hr = IDirect3DDevice9_CreateTexture(
        g_D3D9.pDevice,
        w, h,
        1,                      /* Mip levels: 1 (no mip chain for now) */
        0,                      /* Usage: 0 for managed                  */
        D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED,
        &pPage->pTexture,
        NULL
    );
    if (FAILED(hr)) return FALSE;

    D3DLOCKED_RECT lr;
    IDirect3DTexture9_LockRect(pPage->pTexture, 0, &lr, NULL, 0);

    /* Convert 8-bit palette indices → 32-bit ARGB */
    iColour* pPal = pie_GetGamePal();
    uint8*   pSrc = psIvisTex->bmp;
    uint32*  pDst = (uint32*)lr.pBits;
    for (UWORD y = 0; y < h; y++) {
        for (UWORD x = 0; x < w; x++) {
            uint8 idx = pSrc[y * w + x];
            uint8 a   = (idx == psIvisTex->colorKey) ? 0 : 255;
            pDst[y * (lr.Pitch / 4) + x] =
                D3DCOLOR_ARGB(a, pPal[idx].r, pPal[idx].g, pPal[idx].b);
        }
    }

    IDirect3DTexture9_UnlockRect(pPage->pTexture, 0);
    pPage->iWidth  = w;
    pPage->iHeight = h;
    pPage->bColourKeyed = TRUE;
    return TRUE;
}
```

### 6.2 True-Color Texture Path (16/24/32-bit)

For textures already in true-color formats, the conversion is simpler. Map existing `TEXCOLOURDEPTH` values:

| `TEXCOLOURDEPTH` | D3D9 Format |
|---|---|
| `TCD_4BIT` | Convert → `D3DFMT_A8R8G8B8` |
| `TCD_8BIT` | Convert → `D3DFMT_A8R8G8B8` *(as above)* |
| `TCD_555RGB` | `D3DFMT_X1R5G5B5` or convert to `D3DFMT_A8R8G8B8` |
| `TCD_565RGB` | `D3DFMT_R5G6B5` |
| `TCD_24BIT` | Convert → `D3DFMT_X8R8G8B8` |
| `TCD_32BIT` | `D3DFMT_A8R8G8B8` |

**Recommendation:** Standardize on `D3DFMT_A8R8G8B8` for all textures. Memory cost is acceptable for this asset scale, and it eliminates format-specific blitting code.

### 6.3 Radar Surface

`dtm_LoadRadarSurface()` currently creates a dedicated DD surface for the minimap. Replace with:
- `IDirect3DTexture9` of appropriate size, `D3DPOOL_MANAGED`
- Lock/fill each frame for the dynamic radar update, or use `D3DPOOL_DEFAULT` + `UpdateSurface` for GPU-side update

### 6.4 Texture Stage State Replacement

D3D9 eliminates the `D3DTEXTUREBLEND` enum. Replace the current blending mode selections:

| D3D3 `D3DTBLEND_*` | D3D9 `SetTextureStageState` equivalent |
|---|---|
| `MODULATEALPHA` | `COLOROP=D3DTOP_MODULATE`, `ALPHAOP=D3DTOP_MODULATE` |
| `DECAL` | `COLOROP=D3DTOP_SELECTARG1`, `ALPHAOP=D3DTOP_SELECTARG1` |
| `DECALALPHA` | `COLOROP=D3DTOP_BLENDTEXTUREALPHA`, `ALPHAOP=D3DTOP_SELECTARG2` |
| `MODULATE` | `COLOROP=D3DTOP_MODULATE`, `ALPHAOP=D3DTOP_DISABLE` |

These correspond to the `REND_MODE` / `TRANSLUCENCY_MODE` enums in `PieState.c` and must be applied by the mode-setting functions.

### 6.5 Bilinear Filtering

Replace `dx6_SetBilinear()` DD-surface approach:

```c
void D3D9_SetBilinear(BOOL bBilinearOn)
{
    D3DTEXTUREFILTERTYPE filter = bBilinearOn ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    IDirect3DDevice9_SetSamplerState(g_D3D9.pDevice, 0, D3DSAMP_MINFILTER, filter);
    IDirect3DDevice9_SetSamplerState(g_D3D9.pDevice, 0, D3DSAMP_MAGFILTER, filter);
    IDirect3DDevice9_SetSamplerState(g_D3D9.pDevice, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
}
```

### 6.6 Palette Management Module Changes

`PiePalette.c` contains two categories of code:

**Keep (game-side logic, API-agnostic):**
- `palShades[PALETTE_SIZE * PALETTE_SHADE_LEVEL]` — shade table used for vertex lighting calculations
- `transLookup[PALETTE_SIZE][PALETTE_SIZE]` — software transparency table
- `pal_GetNearestColour()` — color matching
- `pal_Init()` / `pal_ShutDown()`

**Remove (DirectDraw-specific):**
- `pal_PaletteSet()` — sets DirectDraw palette on primary surface
- `pal_Make16BitPalette()` — converts to 16-bit using `DDPIXELFORMAT` bit masks; this work now happens at texture load time in `DTM9_LoadTexture()`
- `pie_GetWinPal()` — returns `PALETTEENTRY*`; `PALETTEENTRY` is a DirectDraw type

**Deliverable:** All 32 texture pages loadable and bindable via `IDirect3DDevice9_SetTexture()`. No `LPDIRECTDRAWSURFACE4` or `LPDIRECT3DTEXTURE2` remaining.

---

## 7. Phase 3 — Fixed-Point Math Conversion Layer

**Scope:** `PieMatrix.h/.c`, `PieClip.h/.c`, `Piedef.h`
**Goal:** Preserve internal game-side fixed-point math but produce D3D9-compatible float output at the renderer boundary.

### 7.1 The Fixed-Point Boundary Problem

`PieMatrix.c` performs all 3D transformations in 12.12 fixed-point (`SDMATRIX` with `SDWORD` members). The output is `PIEVERTEX` with `sx`, `sy`, `sz` in fixed-point screen coordinates.

D3D9 takes pre-transformed vertices as **floats**. The conversion bridge must happen in the draw path, just before calling `DrawPrimitiveUP`.

### 7.2 Conversion Macro

```c
/* Convert 12.12 fixed-point to float */
#define FP12_TO_FLOAT(x)  ((float)(x) / (float)(1 << FP12_SHIFT))

/* Convert PIEVERTEX → PIE_D3D9_VERTEX for draw submission */
static inline void pievert_to_d3d9(const PIEVERTEX* pSrc, PIE_D3D9_VERTEX* pDst)
{
    pDst->sx      = (float)pSrc->sx;        /* sx/sy are already screen pixels (int) */
    pDst->sy      = (float)pSrc->sy;
    pDst->sz      = FP12_TO_FLOAT(pSrc->sz); /* z in [0.0, 1.0] range for depth buffer */
    pDst->rhw     = 1.0f;                    /* Pre-transformed: w=1 */
    pDst->color   = pSrc->light.argb;        /* PIELIGHT ARGB → D3DCOLOR */
    pDst->specular = pSrc->specular.argb;
    pDst->tu      = (float)pSrc->tu / 256.0f; /* Normalize if stored as fixed 8.8 */
    pDst->tv      = (float)pSrc->tv / 256.0f;
}
```

> **Critical:** Verify the exact scaling of `sz` in `PIEVERTEX`. Currently `sz` comes from `pie_ROTATE_PROJECT()`. Its range and format must be confirmed against the z-buffer clear value and the expected `[0.0, 1.0]` range for `D3DFMT_D16`. This is the highest-precision risk in the math boundary.

### 7.3 Perspective Projection Scaling for Arbitrary Resolution

The projection in `pie_ROTATE_PROJECT()` uses `xpshift` / `ypshift` (both `= 10`, dividing by 1024) and offsets by `xcentre` / `ycentre` (half the screen dimensions). These values are set in `_mode_D3D()` in `D3dmode.c`.

For widescreen, these must be updated dynamically:

```c
void D3D9_UpdateRendSurface(UINT width, UINT height)
{
    rendSurface.width   = (int)width;
    rendSurface.height  = (int)height;
    rendSurface.xcentre = (int)(width >> 1);
    rendSurface.ycentre = (int)(height >> 1);
    rendSurface.clip.right  = (int)width;
    rendSurface.clip.bottom = (int)height;

    /* Recompute scantable */
    for (int i = 0; i < iV_SCANTABLE_MAX; i++)
        rendSurface.scantable[i] = i * (int)width;

    /* xpshift/ypshift: currently hardcoded to 10 (÷1024).
       For widescreen, the projection scale factor should be derived
       from the field-of-view. Revisit in Phase 6. */
}
```

### 7.4 Sine/Cosine Table

The precomputed sin/cos table (`aSinTable`) in `PieMatrix.c` is independent of D3D API version. It requires **no changes**. It remains in 12.12 fixed-point and is only consumed by internal rotation operations before the float conversion boundary.

### 7.5 Matrix Stack

The 8-entry `SDMATRIX` stack (`MATRIX_MAX = 8`) in `PieMatrix.c` remains unchanged. The stack output feeds `PIEVERTEX`, which is then converted to `PIE_D3D9_VERTEX` at the draw boundary.

**Deliverable:** A `pievert_to_d3d9()` conversion layer functional in isolation, with a unit test harness that verifies known vertex positions against expected screen coordinates.

---

## 8. Phase 4 — Vertex Format and Draw Call Migration

**Scope:** `Piedraw.h/.c`, `Piefunc.h/.c`, `PieBlitFunc.h/.c`, `D3drender.h/.c`
**Goal:** Replace all `IDirect3DDevice3_DrawPrimitive()` calls with `IDirect3DDevice9_DrawPrimitiveUP()`.

### 8.1 Current Draw Path

In `D3drender.c`, drawing is done approximately as:

```c
hResult = g_sD3DGlob.psD3DDevice3->lpVtbl->DrawPrimitive(
    g_sD3DGlob.psD3DDevice3,
    D3DPT_TRIANGLEFAN,
    D3DFVF_TLVERTEX,
    (LPVOID)psVert,
    nVerts,
    CLIP_STATUS
);
```

### 8.2 New D3D9 Draw Path

```c
/* Set FVF once during device init or mode set */
IDirect3DDevice9_SetFVF(g_D3D9.pDevice, PIE_FVF_D3D9);

/* Per draw call — for triangle fans (polygons) */
void D3D9_DrawPoly(int nVerts, PIEVERTEX* pPieVerts)
{
    /* Convert fixed-point PIE vertices to D3D9 float vertices */
    static PIE_D3D9_VERTEX d3d9Verts[pie_MAX_POLY_VERTS];  /* 10 verts max */
    for (int i = 0; i < nVerts; i++)
        pievert_to_d3d9(&pPieVerts[i], &d3d9Verts[i]);

    IDirect3DDevice9_DrawPrimitiveUP(
        g_D3D9.pDevice,
        D3DPT_TRIANGLEFAN,
        (UINT)(nVerts - 2),     /* primitive count = verts - 2 for fan */
        d3d9Verts,
        sizeof(PIE_D3D9_VERTEX)
    );
}

/* Per draw call — for lines */
void D3D9_DrawLine(PIE_D3D9_VERTEX* pVerts, int nVerts)
{
    IDirect3DDevice9_DrawPrimitiveUP(
        g_D3D9.pDevice,
        D3DPT_LINELIST,
        (UINT)(nVerts / 2),
        pVerts,
        sizeof(PIE_D3D9_VERTEX)
    );
}
```

The static `d3dVrts[pie_MAX_POLY_VERTS]` array already exists in `D3dmode.c` for the D3D3 path; repurpose it with the new type.

### 8.3 2D Blitting (`PieBlitFunc.c`, `Piefunc.c`)

All 2D sprite draws (HUD, UI elements, `pie_DrawImage()`, `pie_BoxFill()`) generate pre-transformed quads. These go through the same `PIE_D3D9_VERTEX` / `DrawPrimitiveUP` path. No structural change to `PieBlitFunc.c` logic is needed — only the terminal draw call changes.

### 8.4 Sky and Water (`Piefunc.c`)

`pie_Sky()` and water rendering use the same polygon submission mechanism. They adapt automatically once `D3D9_DrawPoly()` replaces the D3D3 draw call.

### 8.5 Clipping

D3D3's `CLIP_STATUS` flag in `DrawPrimitive` enabled hardware clipping. D3D9's `DrawPrimitiveUP` always clips against the viewport by default when using pre-transformed (`D3DFVF_XYZRHW`) vertices: the hardware clips based on the rhw value. The software clipping code in `PieClip.c` remains valid as a pre-pass guard for obviously out-of-bounds polygons.

**Deliverable:** All polygons, 2D sprites, lines, and blit operations rendering correctly through the D3D9 path.

---

## 9. Phase 5 — State Management Migration

**Scope:** `PieState.h/.c`, `D3drender.h/.c`
**Goal:** Remap all existing render state setter functions to D3D9 `SetRenderState` / `SetTextureStageState` / `SetSamplerState`.

### 9.1 State Setter Function Remapping

Each public function in `PieState.c` that calls into D3D3 must be updated:

| Current Function | New D3D9 Implementation |
|---|---|
| `D3DSetAlphaBlending(BOOL)` | `SetRenderState(D3DRS_ALPHABLENDENABLE, bAlphaOn)` |
| `D3DSetTranslucencyMode(TRANSLUCENCY_MODE)` | Map enum to `D3DRS_SRCBLEND` / `D3DRS_DESTBLEND` pairs (see below) |
| `D3DSetColourKeying(BOOL)` | D3D9 has no hardware color keying. Use alpha-test instead: `SetRenderState(D3DRS_ALPHATESTENABLE, TRUE)` + `SetRenderState(D3DRS_ALPHAREF, 1)` + `SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL)`. Alpha = 0 texels are keyed out. |
| `D3DSetDepthBuffer(BOOL)` | `SetRenderState(D3DRS_ZENABLE, bOn ? D3DZB_TRUE : D3DZB_FALSE)` |
| `D3DSetDepthWrite(BOOL)` | `SetRenderState(D3DRS_ZWRITEENABLE, bWriteEnable)` |
| `D3DSetDepthCompare(D3DCMPFUNC)` | `SetRenderState(D3DRS_ZFUNC, depthCompare)` *(D3DCMPFUNC values unchanged)* |
| `D3DSetAlphaKey(BOOL)` | Alpha-test approach above; combine with texture alpha channel |
| `D3DSetTexelOffsetState(BOOL)` | Remove: D3D9 handles sub-texel accuracy automatically |

### 9.2 Translucency Mode Blend State Table

```c
void D3D9_SetTranslucencyMode(TRANSLUCENCY_MODE mode)
{
    switch (mode) {
    case TRANS_DECAL:
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
        break;
    case TRANS_DECAL_FOG:
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, TRUE);
        break;
    case TRANS_FILTER:
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    case TRANS_ALPHA:
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    case TRANS_ADDITIVE:
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND,  D3DBLEND_ONE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    }
}
```

### 9.3 Color Keying Migration — Critical Detail

**D3D9 has no hardware color key** (`D3DRENDERSTATE_COLORKEYENABLE` does not exist). The current `D3DSetColourKeying()` path must be replaced end-to-end:

1. At texture load time: whenever a texel matches the color key, write **alpha = 0** (handled in Phase 2's `DTM9_LoadTexture()`)
2. At render time: enable alpha testing (`D3DRS_ALPHATESTENABLE = TRUE`, `ALPHAREF = 1`, `ALPHAFUNC = D3DCMP_GREATEREQUAL`)
3. Calling `D3DSetColourKeying(TRUE)` now means "enable alpha test"; `D3DSetColourKeying(FALSE)` means "disable alpha test"

This is behaviorally equivalent but requires that textures are loaded with correct alpha values (Phase 2).

### 9.4 Fog

D3D9 fog API is nearly identical:

```c
void D3D9_SetFog(BOOL bEnable, D3DCOLOR fogColor, float start, float end)
{
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE,   bEnable);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGCOLOR,    fogColor);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGSTART, *(DWORD*)&start);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGEND,   *(DWORD*)&end);
}
```

### 9.5 Default Render State Initialization

Consolidate all `SetRenderState` calls currently scattered across `InitD3D()` into a single `D3D9_SetDefaultRenderStates()`:

```c
void D3D9_SetDefaultRenderStates(void)
{
    IDirect3DDevice9* dev = g_D3D9.pDevice;
    IDirect3DDevice9_SetFVF(dev, PIE_FVF_D3D9);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SHADEMODE,      D3DSHADE_GOURAUD);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE,       D3DCULL_CCW);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FILLMODE,       D3DFILL_SOLID);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE,        D3DZB_TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE,   TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZFUNC,          D3DCMP_LESSEQUAL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING,       FALSE); /* No D3D lighting */
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CLIPPING,       TRUE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
}
```

**Deliverable:** All `REND_MODE` and `TRANSLUCENCY_MODE` transitions working correctly. No D3D3 render state enums remaining.

---

## 10. Phase 6 — Resolution and Widescreen Support

**Scope:** `D3dmode.h/.c`, `PieMatrix.h/.c`, `Rendmode.h/.c`, `screen.h/.c` (framework layer)
**Goal:** Replace the hardcoded 640×480 assumption with a dynamic resolution system supporting arbitrary widths and heights, including 16:9 widescreen.

### 10.1 Remove All Hardcoded Resolution Constants

Audit every occurrence of `640`, `480`, `VIDEO_WIDTH`, `VIDEO_HEIGHT`, `REND_D3D_RGB/HAL/REF` constants, and the clipping window `640.0f × 480.0f` in `D3drender.c`.

Replace with:
```c
extern UINT g_RenderWidth;
extern UINT g_RenderHeight;
```

These are set during `D3D9_InitDevice()` and updated on resolution change.

### 10.2 Resolution Enumeration

Add a function to enumerate available full-screen display modes from D3D9:

```c
typedef struct D3D9_DISPLAYMODE {
    UINT width;
    UINT height;
    UINT refreshRate;
    D3DFORMAT format;
} D3D9_DISPLAYMODE;

UINT D3D9_EnumDisplayModes(D3D9_DISPLAYMODE* pModes, UINT maxModes)
{
    UINT count = IDirect3D9_GetAdapterModeCount(g_D3D9.pD3D9,
                     D3DADAPTER_DEFAULT, D3DFMT_R5G6B5);
    for (UINT i = 0; i < count && i < maxModes; i++) {
        D3DDISPLAYMODE mode;
        IDirect3D9_EnumAdapterModes(g_D3D9.pD3D9, D3DADAPTER_DEFAULT,
                                    D3DFMT_R5G6B5, i, &mode);
        pModes[i].width       = mode.Width;
        pModes[i].height      = mode.Height;
        pModes[i].refreshRate = mode.RefreshRateInHz;
        pModes[i].format      = mode.Format;
    }
    return (count < maxModes) ? count : maxModes;
}
```

Typical modes this will return include: 640×480, 800×600, 1024×768, 1280×720, 1280×960, 1280×1024, 1920×1080, 2560×1440.

### 10.3 Resolution Change at Runtime

D3D9 supports resolution changes via `IDirect3DDevice9_Reset()`:

```c
BOOL D3D9_ChangeResolution(UINT width, UINT height, BOOL bFullscreen)
{
    DTM9_ReleaseAllTextures();  /* D3DPOOL_DEFAULT resources must be released first */

    g_D3D9.presentParams.BackBufferWidth  = width;
    g_D3D9.presentParams.BackBufferHeight = height;
    g_D3D9.presentParams.Windowed         = !bFullscreen;

    HRESULT hr = IDirect3DDevice9_Reset(g_D3D9.pDevice, &g_D3D9.presentParams);
    if (FAILED(hr)) return FALSE;

    DTM9_ReloadAllTextures();
    D3D9_SetDefaultRenderStates();
    D3D9_UpdateRendSurface(width, height);
    D3D9_UpdateViewport(width, height);
    return TRUE;
}
```

### 10.4 Viewport Update

```c
void D3D9_UpdateViewport(UINT width, UINT height)
{
    D3DVIEWPORT9 vp;
    vp.X      = 0;
    vp.Y      = 0;
    vp.Width  = width;
    vp.Height = height;
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    IDirect3DDevice9_SetViewport(g_D3D9.pDevice, &vp);
}
```

### 10.5 Aspect Ratio and Projection Correction

The current projection uses `xpshift = ypshift = 10` (divide by 1024 = divide by ~1000). This was calibrated for 640×480. At other resolutions, the field of view changes.

**Two approaches:**

**Option A (minimal change):** Scale `xcentre` and `ycentre` only. The field of view widens with resolution, which is acceptable for an RTS where the camera is typically top-down or isometric. No change to projection math.

**Option B (correct FOV):** Introduce a perspective scale factor derived from the desired vertical FOV angle:

```c
/* target_fov_y in radians — replicate original 640x480 apparent FOV */
float aspect = (float)width / (float)height;
float scale  = ((float)height / 2.0f) / tanf(target_fov_y / 2.0f);
/* Store as integer shift or float multiplier in rendSurface */
```

**Recommendation:** Use Option A initially (lowest risk), with Option B as a follow-up if the widescreen FOV is visually unacceptable. The RTS genre is generally tolerant of wider FOV at higher resolutions.

### 10.6 2D UI Scaling

The HUD and UI system draws 2D elements in screen-space pixels. At resolutions above 640×480, these elements will appear smaller unless the UI system is updated to scale. This is **out of scope** for the graphics migration but must be flagged for a subsequent UI task.

**Deliverable:** Game renders correctly at 1280×720, 1920×1080, and 2560×1440 in both windowed and fullscreen modes.

---

## 11. Phase 7 — Video Playback Repair

**Scope:** `NeuronClient/Sequence/Sequence.h/.c`, `WINSTR.LIB`
**Goal:** Restore FMV playback without DirectDraw surfaces.

### 11.1 Root Cause of Breakage

`seq_RenderOneFrame()` and `seq_SetSequence()` take `LPDIRECTDRAWSURFACE4` as parameters. With DirectDraw removed entirely, these functions have no valid call site. The underlying `WINSTR.LIB` codec library renders video into either:
- A `LPDIRECTDRAWSURFACE4` directly (locked surface mode), or
- A CPU-side byte buffer (`seq_SetSequenceForBuffer()` / `seq_RenderOneFrameToBuffer()`)

`seq_RenderOneFrameToBuffer()` **does not require a DirectDraw surface** — it writes into `lpSF` (a plain byte buffer). This is the migration path.

### 11.2 CPU Buffer → D3D9 Texture Approach

```c
/* New video rendering flow: CPU buffer → D3D9 dynamic texture */

/* Create a D3DPOOL_DEFAULT texture for video (updated every frame) */
static IDirect3DTexture9* g_pVideoTex = NULL;
static BYTE*              g_pVideoBuffer = NULL;  /* Matches VIDEO_WIDTH × VIDEO_HEIGHT × 2 bytes (16-bit RGB) */

BOOL SEQ9_Init(UINT videoW, UINT videoH)
{
    g_pVideoBuffer = (BYTE*)malloc(videoW * videoH * 2);  /* 16-bit buffer for codec */
    HRESULT hr = IDirect3DDevice9_CreateTexture(
        g_D3D9.pDevice,
        videoW, videoH, 1,
        D3DUSAGE_DYNAMIC,       /* Updated every frame */
        D3DFMT_R5G6B5,          /* Match codec output format (16-bit RGB) */
        D3DPOOL_DEFAULT,        /* Must be DEFAULT for dynamic usage */
        &g_pVideoTex, NULL
    );
    return SUCCEEDED(hr);
}

void SEQ9_UpdateFrame(void)
{
    /* WINSTR.LIB renders into CPU buffer */
    seq_RenderOneFrameToBuffer(g_pVideoBuffer, 0, 0, VIDEO_WIDTH * VIDEO_HEIGHT);

    /* Upload to D3D9 texture */
    D3DLOCKED_RECT lr;
    IDirect3DTexture9_LockRect(g_pVideoTex, 0, &lr, NULL, D3DLOCK_DISCARD);
    BYTE* pDst = (BYTE*)lr.pBits;
    BYTE* pSrc = g_pVideoBuffer;
    for (UINT y = 0; y < VIDEO_HEIGHT; y++) {
        memcpy(pDst + y * lr.Pitch, pSrc + y * VIDEO_WIDTH * 2, VIDEO_WIDTH * 2);
    }
    IDirect3DTexture9_UnlockRect(g_pVideoTex, 0);

    /* Draw fullscreen quad */
    SEQ9_DrawFullscreenQuad(g_pVideoTex);
}
```

### 11.3 Fullscreen Video Quad

```c
void SEQ9_DrawFullscreenQuad(IDirect3DTexture9* pTex)
{
    float w = (float)g_RenderWidth;
    float h = (float)g_RenderHeight;

    PIE_D3D9_VERTEX verts[4] = {
        /* sx,     sy,   sz,  rhw,   color,       spec,        tu,   tv */
        {  0.0f,   0.0f, 0.5f, 1.0f, 0xFFFFFFFF,  0x00000000,  0.0f, 0.0f },
        {  w,      0.0f, 0.5f, 1.0f, 0xFFFFFFFF,  0x00000000,  1.0f, 0.0f },
        {  w,      h,    0.5f, 1.0f, 0xFFFFFFFF,  0x00000000,  1.0f, 1.0f },
        {  0.0f,   h,    0.5f, 1.0f, 0xFFFFFFFF,  0x00000000,  0.0f, 1.0f },
    };

    IDirect3DDevice9_SetTexture(g_D3D9.pDevice, 0, (IDirect3DBaseTexture9*)pTex);
    IDirect3DDevice9_SetRenderState(g_D3D9.pDevice, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(g_D3D9.pDevice, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_DrawPrimitiveUP(g_D3D9.pDevice, D3DPT_TRIANGLEFAN, 2, verts, sizeof(PIE_D3D9_VERTEX));
    IDirect3DDevice9_SetRenderState(g_D3D9.pDevice, D3DRS_ZENABLE, TRUE);
}
```

### 11.4 Video Pixel Format Verification

`WINSTR.LIB`'s output format must be confirmed. The existing `seq_SetSequenceForBuffer()` takes a `DDPIXELFORMAT*` parameter. With DirectDraw gone, the codec format negotiation needs to be re-examined:
- If the codec outputs `VIDEO_SOFT_WINDOW` mode (16-bit RGB 555), use `D3DFMT_X1R5G5B5`
- If the codec outputs `VIDEO_D3D_FULLSCREEN` / `VIDEO_D3D_WINDOW` (16-bit screen pixels), match the back buffer format

> **Critical gap:** `WINSTR.LIB` has no source code. If the library cannot render into a plain CPU buffer without a `LPDIRECTDRAWSURFACE4`, the entire video system requires replacement with a new codec library (e.g., Bink SDK, libvpx, or FFmpeg). This is the highest-risk item in the migration. Establish this fact early, ideally in Phase 1, before committing to the CPU-buffer approach.

### 11.5 Fallback: Replace `WINSTR.LIB` Entirely

If the CPU buffer path in `WINSTR.LIB` is non-functional, consider:
- **Bink Video SDK** (Rad Game Tools): Drop-in replacement, widely used in games of this era, has a CPU→texture path
- **FFmpeg** (LGPL): Can decode ESCAPE 124 (`CODEC_ID_ESCAPE124`) — the codec StarStrike uses — via `avcodec`. This avoids recompression of existing assets.
- **Convert FMV assets to a modern format offline**: Transcode all `.smk`/`ESCAPE` videos to VP8 (WebM) or H.264 (MP4) and use libvpx or Windows Media Foundation for playback.

**Deliverable:** Video plays at the current `VIDEO_WIDTH × VIDEO_HEIGHT` size and scales to fill the current render resolution, without any DirectDraw dependency.

---

## 12. Phase 8 — Build System Update

**Scope:** `CMakeLists.txt` (all levels), `DX9/` SDK directory
**Goal:** Replace D3D3/DirectDraw library dependencies with D3D9 equivalents.

### 12.1 Library Changes

In `StarStrike/CMakeLists.txt`:

```cmake
# REMOVE:
# ${CMAKE_SOURCE_DIR}/DX9/Lib/ddraw.lib
# ${CMAKE_SOURCE_DIR}/DX9/Lib/d3dx.lib    (D3DX for D3D3)

# ADD:
${CMAKE_SOURCE_DIR}/DX9/Lib/d3d9.lib
${CMAKE_SOURCE_DIR}/DX9/Lib/d3dx9.lib    # D3DX9 for matrix utilities (optional)

# KEEP:
${CMAKE_SOURCE_DIR}/DX9/Lib/dxguid.lib
${CMAKE_SOURCE_DIR}/DX9/Lib/dsound.lib
${CMAKE_SOURCE_DIR}/DX9/Lib/dinput.lib
${CMAKE_SOURCE_DIR}/DX9/Lib/dplayx.lib
winmm
```

### 12.2 Include Path Validation

Verify that `DX9/Include/` in the existing include path contains `d3d9.h`, `d3d9types.h`, and `d3d9caps.h`. The existing `DX9/Include/` directory already contains these files (since the path was already named `DX9`), but the actual SDK version must be confirmed to be 9.0c.

### 12.3 D3DX Dependency (Optional)

The current build links `d3dx.lib` (the D3DX utility library for D3D3). With D3D9, this becomes `d3dx9.lib`. The migration does not require D3DX9 — all matrix operations remain in the custom fixed-point `PieMatrix.c` — but `d3dx9.lib` may be useful for debugging (e.g., `D3DXGetErrorString()`). It is optional.

### 12.4 Compiler Warnings

After removing DirectDraw and D3D3 headers, the compiler will emit many "undeclared identifier" errors. The recommended approach is to resolve them phase-by-phase (not all at once), keeping the build green after each phase completes.

**Deliverable:** `cmake --build .` produces a clean compile with no D3D3/DirectDraw symbols. Only `d3d9.h` is included in renderer files.

---

## 13. Phase 9 — Validation and Testing

### 13.1 Rendering Correctness Checklist

| Test | Acceptance Criterion |
|---|---|
| Clear screen | D3D9 device initializes, presents black frame, no crash |
| Texture load | All 32 texture pages load without error; no missing texels |
| Palette conversion | 8-bit palettized textures render with correct colors; color-keyed areas are transparent |
| Polygon draw | 3D scene renders with correct depth ordering (no z-fighting regression) |
| Alpha blending | Semi-transparent units/explosions composite correctly |
| Additive blending | Particle/glow effects render brighter than background (not clamped) |
| Fog | Distant terrain fades to fog color correctly |
| 2D UI | HUD elements render at correct screen positions |
| Resolution: 640×480 | Baseline — must match pre-migration appearance exactly |
| Resolution: 1280×720 | Scene renders without distortion; UI elements still visible |
| Resolution: 1920×1080 | Fullscreen mode; no resolution negotiation failure |
| Resolution: 2560×1440 | Windowed mode works; no device creation failure |
| Windowed mode | Window resizes without crash; device reset handled |
| Alt-Tab | Device lost triggered; device reset recovers correctly |
| Video playback | FMV plays from start to finish; audio synchronized |
| Multiple sessions | No memory leak in texture page allocation across level loads |

### 13.2 Regression Test Baseline

Before any migration work begins, capture a reference recording of:
- A complete gameplay session at 640×480
- All FMV sequences
- The HUD at various game states

Use these as visual regression baselines. After each phase, compare frame captures.

### 13.3 Performance Validation

`DrawPrimitiveUP` (user-pointer path) is slower than indexed vertex buffers in D3D9 due to per-call memory copies. For an RTS game at this vintage polygon count, this is acceptable — but validate that frame time does not regress by more than 10% relative to D3D3.

If frame time regresses, the path forward is batching draw calls into `IDirect3DVertexBuffer9` with `Lock(D3DLOCK_DISCARD)` each frame — a standard D3D9 optimization that is out of scope for this migration but straightforward to add.

---

## 14. Risk Register

| ID | Risk | Severity | Probability | Mitigation |
|---|---|---|---|---|
| R-01 | `WINSTR.LIB` cannot render to CPU buffer without a DirectDraw surface | **Critical** | Medium | Confirm in Phase 1 by calling `seq_RenderOneFrameToBuffer()` before removing DirectDraw. If it fails, plan to replace with FFmpeg or Bink. |
| R-02 | `sz` component of `PIEVERTEX` is not in `[0.0, 1.0]` range for D3D9 depth buffer | **High** | Medium | Instrument `pie_ROTATE_PROJECT()` to log min/max sz values in a test level. Apply rescaling in `pievert_to_d3d9()`. |
| R-03 | `D3DRS_COLORKEYENABLE` does not exist in D3D9 — color keying behavior changes | **High** | Certain | Preload all keyed textures with alpha=0 for keyed texels in Phase 2. Test each keyed texture category explicitly. |
| R-04 | Some GPUs do not support `D3DFMT_R5G6B5` as a back buffer format | **Medium** | Low | Add `D3DFMT_X8R8G8B8` (32-bit) fallback in `D3DPRESENT_PARAMETERS`. Prefer 32-bit on modern hardware; 16-bit is a legacy path. |
| R-05 | Widescreen projection appears stretched or clipped due to aspect ratio change | **Medium** | High | Test Option A projection first; instrument with a reference grid rendering. Option B (FOV correction) is the fix if needed. |
| R-06 | `D3DPOOL_MANAGED` textures survive `Reset()` but `D3DPOOL_DEFAULT` (video texture) does not | **Medium** | Certain | The video texture (Phase 7) uses `D3DPOOL_DEFAULT`. Release and recreate it in the device-lost recovery path. |
| R-07 | 8-bit texture palette indices are not correctly mapped to game palette at runtime (wrong shade) | **Medium** | Medium | Cross-reference `pal_Init()` palette order with `pie_GetGamePal()`. Add a visual palette verification tool. |
| R-08 | Scantable (`rendSurface.scantable`) overflow at resolutions > 480 lines (`iV_SCANTABLE_MAX` limit) | **Medium** | High | Check `iV_SCANTABLE_MAX` definition in `Ivisdef.h`. If `< 1080`, increase it or eliminate the scantable (2D blitting via D3D9 does not need it). |
| R-09 | `D3DCREATE_SOFTWARE_VERTEXPROCESSING` limits shader-level vertex throughput on modern GPUs | **Low** | Certain | Acceptable for D3D9 fixed-function. Change to `D3DCREATE_HARDWARE_VERTEXPROCESSING` as a performance upgrade if shader-based rendering is added later. |
| R-10 | `tu`/`tv` texture coordinate scaling in `PIEVERTEX` uses a different normalization than D3D9 expects | **Medium** | Medium | Confirm PIEVERTEX UV units. D3D9 expects `[0.0, 1.0]`. If current UVs are `[0, 255]` or `[0, textureWidth]`, apply the appropriate divisor in `pievert_to_d3d9()`. |

---

## 15. C-in-COM Wrapping Strategy

The codebase is pure C11 and must remain so. D3D9 COM interfaces are callable from C via macros defined in `d3d9.h`.

### 15.1 How D3D9 Headers Work in C

The D3D9 headers use `#ifdef __cplusplus` guards:
- In C++: `pDevice->DrawPrimitive(...)` (vtable through smart pointer)
- In C: `IDirect3DDevice9_DrawPrimitive(pDevice, ...)` (macro expands to `pDevice->lpVtbl->DrawPrimitive(pDevice, ...)`)

These macros are **already defined** in `d3d9.h` for all interface methods. No manual wrapping is needed for standard calls.

### 15.2 Pattern for All D3D9 Calls in C

```c
/* Interface creation */
IDirect3D9* pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);

/* Method calls — C macro syntax */
HRESULT hr = IDirect3D9_CreateDevice(pD3D9, D3DADAPTER_DEFAULT, ...);
IDirect3DDevice9_SetRenderState(pDevice, D3DRS_ZENABLE, TRUE);
IDirect3DDevice9_DrawPrimitiveUP(pDevice, D3DPT_TRIANGLEFAN, 2, pVerts, stride);
IDirect3DTexture9_LockRect(pTex, 0, &lr, NULL, 0);

/* Release — all COM interfaces use IUnknown_Release */
IDirect3DDevice9_Release(pDevice);
IDirect3D9_Release(pD3D9);
```

### 15.3 GUID References

`dxguid.lib` (already linked) provides all D3D9 GUIDs. No `__uuidof()` (C++ only) is needed.

### 15.4 Error Checking Pattern

The existing `FAILED(hr)` / `SUCCEEDED(hr)` macros work identically in C. The current codebase pattern of checking return values is compatible. Adopt a consistent macro for D3D9:

```c
#define D3D9_CHECK(hr, msg) \
    do { if (FAILED(hr)) { debug_Log("D3D9 FAILED [%s]: 0x%08X", (msg), (hr)); } } while(0)
```

---

## 16. File Impact Matrix

The following table summarizes the change category for every file in `NeuronClient/ivis02/` plus the Sequence module.

| File | Change | Notes |
|---|---|---|
| `D3drender.h` | **Full rewrite** | New `D3D9GLOBALS`, new function signatures |
| `D3drender.c` | **Full rewrite** | Device init, draw path, state defaults |
| `D3dmode.h` | **Full rewrite** | Remove `SCREEN_MODE` DD dependencies |
| `D3dmode.c` | **Full rewrite** | Replace `_mode_D3D_*` functions |
| `Dx6TexMan.h` | **Full rewrite** | New `TEXPAGE_D3D9`, new function signatures |
| `Dx6TexMan.c` | **Full rewrite** | D3D9 texture creation + palette conversion |
| `Texd3d.h` | **Full rewrite** | Remove DD surface types |
| `Texd3d.c` | **Full rewrite** | Replace DD surface ops with D3D9 texture ops |
| `PiePalette.h` | **Partial** | Remove `PALETTEENTRY*`, keep game palette |
| `PiePalette.c` | **Partial** | Remove `pal_PaletteSet()`, `pal_Make16BitPalette()` |
| `PieState.h` | **Moderate** | Update setter function signatures if needed |
| `PieState.c` | **Moderate** | Remap all `SetRenderState` enums |
| `Piedraw.h` | **Minor** | No structural change |
| `Piedraw.c` | **Minor** | Replace terminal draw call only |
| `Piefunc.h` | **Minor** | No structural change |
| `Piefunc.c` | **Minor** | Replace terminal draw calls |
| `PieBlitFunc.h` | **Minor** | No structural change |
| `PieBlitFunc.c` | **Minor** | Replace terminal draw calls |
| `Piedef.h` | **Minor** | Add `PIE_D3D9_VERTEX`, add `PIE_FVF_D3D9` |
| `PieMatrix.h` | **None** | Fixed-point math unchanged |
| `PieMatrix.c` | **None** | Fixed-point math unchanged |
| `PieClip.h` | **None** | Clipping unchanged |
| `PieClip.c` | **None** | Clipping unchanged |
| `PieMode.h` | **Minor** | Mode names updated |
| `PieMode.c` | **Minor** | Calls `D3D9_InitDevice()` |
| `Rendmode.h` | **Minor** | `REND_ENGINE` enum update |
| `Rendmode.c` | **Minor** | Retarget function pointers |
| `PieTexture.h` | **Moderate** | Texture page API update |
| `PieTexture.c` | **Moderate** | Call new `DTM9_*` functions |
| `TextDraw.h/.c` | **Minor** | Text rendering through same polygon path |
| `BitImage.h/.c` | **None** | CPU-side image data; no D3D dependency |
| `Imd.h/.c` | **None** | Model loading; no D3D dependency |
| `Ivi.h/.c` | **Minor** | Init sequence calls `D3D9_InitDevice()` |
| `Ivisdef.h` | **Minor** | Check `iV_SCANTABLE_MAX` (Risk R-08) |
| `Rendfunc.c/.h` | **Minor** | Update function pointer targets |
| `Sequence/Sequence.h` | **Moderate** | Remove `LPDIRECTDRAWSURFACE4` parameters |
| `Sequence/Sequence.c` | **Moderate** | Use CPU buffer path; add D3D9 texture upload |
| `CMakeLists.txt` (StarStrike) | **Minor** | Swap `ddraw.lib`/`d3dx.lib` for `d3d9.lib`/`d3dx9.lib` |

**Files with zero graphics API dependency (not affected):**
- All of `NeuronClient/Gamelib/` (game logic)
- All of `NeuronClient/Framework/` (window management, may need HWND passed to D3D9 init)
- All of `NeuronClient/Sound/`
- All of `NeuronClient/Script/`
- All of `NeuronClient/Netplay/`
- All of `NeuronClient/Widget/` (uses renderer API surface only)
- All of `NeuronCore/`

---

## 17. Open Questions and Critical Missing Items

The following items must be resolved before or during the migration. They represent areas where the plan makes assumptions that need to be validated against the actual codebase or external dependencies.

### Q1. Can `WINSTR.LIB` render without a DirectDraw surface? (Risk R-01)
**Priority: Critical — resolve in Phase 1**

Call `seq_SetSequenceForBuffer()` and `seq_RenderOneFrameToBuffer()` with a plain `malloc()`-allocated buffer *before* removing DirectDraw. If this path is functional, the video migration in Phase 7 is straightforward. If it crashes or returns errors, the entire video subsystem needs replacement with a third-party library.

### Q2. What is the exact format and range of `sz` in `PIEVERTEX`? (Risk R-02)
**Priority: High — resolve in Phase 3**

`pie_ROTATE_PROJECT()` in `PieMatrix.c` produces the `sz` value. Its range determines the scaling factor in `pievert_to_d3d9()`. D3D9 depth buffer range is `[0.0, 1.0]`. If `sz` is already normalized, no scaling is needed. If `sz` is in fixed-point world-space depth, a scale-and-bias transform is required.

### Q3. What are the `tu`/`tv` units in `PIEVERTEX`? (Risk R-10)
**Priority: High — resolve in Phase 3**

`UWORD tu, tv` could be:
- `[0, 255]` — fraction of texture width/height in 8.8 fixed-point
- `[0, textureWidth]` — texel coordinates
- `[0.0, 1.0]` encoded as fixed-point

The correct divisor in `pievert_to_d3d9()` depends on this. Check how UV values are assigned in `Piedraw.c` or `Imd.c` when building a `PIEVERTEX`.

### Q4. What is `iV_SCANTABLE_MAX`? (Risk R-08)
**Priority: Medium — resolve in Phase 2**

Check `Ivisdef.h` for the definition of `iV_SCANTABLE_MAX`. If it is `480` or less, the scantable will overflow at 1080p. The scantable is used for line rendering and 2D blitting in the software path — with D3D9, this table may be entirely unused for the hardware rendering path. Confirm whether any D3D path code indexes it.

### Q5. Does `getdxver.cpp` (the only `.cpp` file) conflict with the C-only build?
**Priority: Low**

`getdxver.cpp` is present in the legacy project files. If it is included in the CMake build, it may cause linker issues with name mangling. Identify whether it is active in the CMake build and either exclude it or add `extern "C"` guards.

### Q6. How does the `Framework` layer pass `HWND` to the rendering system?
**Priority: Medium — resolve in Phase 1**

`D3D9_InitDevice()` requires the window handle `HWND`. Currently the D3D3 path receives this via `screenGetMode()` / DirectDraw cooperative level. Confirm the call chain from `WinMain` / `Ivi.c` through `D3dmode.c` to locate where the `HWND` is currently obtained. The D3D9 init path needs the same `HWND`.

### Q7. Are any textures animated via DirectDraw surface flipping?
**Priority: Medium**

`iTexAnim` is referenced in `iIMDPoly` and `PIED3DPOLY`. Texture animation in D3D3 era engines was sometimes implemented by swapping the active DD surface. If this is the case, the animation system needs a corresponding update to swap `IDirect3DTexture9*` pointers instead.

### Q8. Does the `Widget` system directly access any `LPDIRECTDRAWSURFACE4`?
**Priority: Low — check in Phase 1**

The `Widget` layer sits above the renderer and should only call the public renderer API in `Piefunc.c` / `PieBlitFunc.c`. Verify no widget code has a direct DD surface dependency (e.g., for screenshot or off-screen rendering).

---

## Appendix A: Migration Phase Sequence

```
Phase 1: D3D9 device + frame loop (no textures, black screen)
    │
Phase 2: Texture system (palette conversion, IDirect3DTexture9)
    │
Phase 3: Math conversion layer (pievert_to_d3d9, UV/Z investigation)
    │
Phase 4: Draw call migration (DrawPrimitiveUP, all polygon types)
    │
Phase 5: State management (render state remapping, color key)
    │
Phase 6: Resolution + widescreen (dynamic viewport, mode enumeration)
    │
Phase 7: Video playback (CPU buffer → D3D9 texture)
    │
Phase 8: Build system (library swap, clean compile)
    │
Phase 9: Validation (visual regression, resolution matrix, device lost)
```

Phases 1–5 must be executed in order (each depends on the previous). Phases 6, 7, and 8 can proceed in parallel once Phase 5 is complete.

---

## Appendix B: Key File Locations

All paths relative to repository root `C:\Users\zwali\source\repos\StarStrike.RTS\`:

- Renderer core: `NeuronClient/ivis02/`
- Sequence/Video: `NeuronClient/Sequence/`
- DirectX SDK headers: `DX9/Include/`
- DirectX SDK libraries: `DX9/Lib/`
- Build root: `CMakeLists.txt`
- Client build: `NeuronClient/CMakeLists.txt`
- Executable build: `StarStrike/CMakeLists.txt`

---

*End of DirectXMig.md*
