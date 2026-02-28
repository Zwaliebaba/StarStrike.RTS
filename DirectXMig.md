# DirectX Migration Plan: Direct3D 3 → Direct3D 9 (9.0c)

**Project:** StarStrike RTS
**Author:** Principal Graphics Engineer Review
**Status:** In Progress — Renderer Layer (ivis02) Complete, Framework Layer Pending
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
18. [Implementation Status](#18-implementation-status)

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

| ID | Risk | Severity | Probability | Status | Mitigation |
|---|---|---|---|---|---|
| R-01 | `WINSTR.LIB` cannot render to CPU buffer without a DirectDraw surface | **Critical** | Medium | 🔶 **Open** | Requires runtime test — Sequence files not in main tree. See Q1. |
| R-02 | `sz` component of `PIEVERTEX` is not in `[0.0, 1.0]` range for D3D9 depth buffer | **High** | Medium | ✅ **Mitigated** | `sz` is stretched int `[256, 32000]`, already normalized via `* INV_MAX_Z` (= 1/32000) in `D3drender.c:279`. See Q2. |
| R-03 | `D3DRS_COLORKEYENABLE` does not exist in D3D9 — color keying behavior changes | **High** | Certain | ✅ **Mitigated** | Alpha-test implemented in `D3DSetColourKeying()`. Textures load with alpha=0 for keyed texels. |
| R-04 | Some GPUs do not support `D3DFMT_R5G6B5` as a back buffer format | **Medium** | Low | 🔶 **Open** | `D3DFMT_X8R8G8B8` fallback should be added in `InitD3D()`. |
| R-05 | Widescreen projection appears stretched or clipped due to aspect ratio change | **Medium** | High | ⏳ **Deferred** | Phase 6 not started. Option A (scale xcentre/ycentre only) is the initial approach. |
| R-06 | `D3DPOOL_MANAGED` textures survive `Reset()` but `D3DPOOL_DEFAULT` (video texture) does not | **Medium** | Certain | ⏳ **Deferred** | Applies only when Phase 7 video texture is implemented. |
| R-07 | 8-bit texture palette indices are not correctly mapped to game palette at runtime (wrong shade) | **Medium** | Medium | 🔶 **Open** | Requires runtime visual verification. |
| R-08 | Scantable (`rendSurface.scantable`) overflow at resolutions > 1024 lines | **Medium** | High | ✅ **Confirmed** | `iV_SCANTABLE_MAX = 1024` (from `Ivisdef.h:27`). Will overflow at 1080p. Used by `Rendfunc.c`, `TextDraw.c` (software paths). Must increase to `2160` or eliminate. See Q4. |
| R-09 | `D3DCREATE_SOFTWARE_VERTEXPROCESSING` limits shader-level vertex throughput on modern GPUs | **Low** | Certain | ✅ **Accepted** | Acceptable for D3D9 fixed-function. Upgrade to `HARDWARE_VERTEXPROCESSING` later. |
| R-10 | `tu`/`tv` texture coordinate scaling in `PIEVERTEX` uses a different normalization than D3D9 expects | **Medium** | Medium | ✅ **Mitigated** | UVs are integer texel coords `[0, 256]`, normalized via `* INV_TEX_SIZE` (= 1/256) in `D3drender.c:281-282`. See Q3. |

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

The following items were identified as unknowns during the planning phase. Each has now been investigated against the actual codebase. Resolved items are marked ✅; items still requiring runtime validation are marked 🔶.

### Q1. Can `WINSTR.LIB` render without a DirectDraw surface? (Risk R-01)
**Priority: Critical**
**Status:** 🔶 **Cannot be resolved statically — requires runtime test**

`Sequence.c` / `Sequence.h` are not present in the main working tree (only in a `.claude/worktrees/` branch). The `WINSTR.LIB` binary has no source. The question of whether `seq_RenderOneFrameToBuffer()` works without a live `LPDIRECTDRAWSURFACE4` can only be answered by calling it at runtime with a plain `malloc()` buffer.

**Action:** When Sequence files are restored to the main tree, execute the test described in Phase 7 §11.1 before committing to the CPU-buffer approach.

### Q2. What is the exact format and range of `sz` in `PIEVERTEX`? (Risk R-02)
**Priority: High**
**Status:** ✅ **Resolved — `sz` is a stretched integer Z in range `[256, ~32000]`, normalized by `* INV_MAX_Z` to `[0.0, 1.0]`**

The Z pipeline is:

1. **World-space rotation** in `Piedraw.c:490` produces `rz` in 12.12 fixed-point (units: world distance × 4096).
2. **Stretch** at `Piedraw.c:494`: `pPixels->d3dz = D3DVAL((rz >> STRETCHED_Z_SHIFT))` — right-shift by 10 compresses the fixed-point to a stretched integer. Typical range: `[MIN_STRETCHED_Z=256, ~32000]`.
3. **PIEVERTEX assignment** at `Piedraw.c:553`: `pieVrts[n].sz = MAKEINT(scrPoints[*index].d3dz)` — stores the stretched integer in `PIEVERTEX.sz` (`SDWORD`).
4. **D3D9 normalization** at `D3drender.c:279`: `d3dVrts[i].sz = (float)pVrts[i].sz * (float)INV_MAX_Z` where `INV_MAX_Z = 1/32000 = 0.00003125`. This produces `[0.0, ~1.0]` — correct for the D3D9 depth buffer.
5. **RHW** at `D3drender.c:280`: `d3dVrts[i].rhw = 1.0f / pVrts[i].sz` — reciprocal of stretched Z.
6. **2D interface elements** use `INTERFACE_DEPTH = MAX_Z - 1 = 31999`, placing them at the far end of the depth buffer (behind 3D geometry).

**Key constants** (from `Piedef.h`):
- `STRETCHED_Z_SHIFT = 10`
- `MAX_Z = 32000.0f`
- `INV_MAX_Z = 0.00003125f` (= 1/32000)
- `MIN_STRETCHED_Z = 256` (near clip; values below this are culled)
- `INTERFACE_DEPTH = 31999.0f` (far plane for UI)

**Conclusion:** The existing conversion `sz * INV_MAX_Z` is already correct for D3D9. The `pievert_to_d3d9()` function described in Phase 3 §7.2 should use `pDst->sz = (float)pSrc->sz * INV_MAX_Z` — which is exactly what `D3D_PIEPolygon()` in `D3drender.c:279` already does. **Risk R-02 is mitigated.**

### Q3. What are the `tu`/`tv` units in `PIEVERTEX`? (Risk R-10)
**Priority: High**
**Status:** ✅ **Resolved — `tu`/`tv` are integer texel coordinates in `[0, 256]`, normalized by `* INV_TEX_SIZE` to `[0.0, 1.0]`**

- `PIEVERTEX.tu` and `PIEVERTEX.tv` are `UWORD` (unsigned 16-bit) holding integer texel coordinates. Values range from `0` to `256` (the texture atlas is 256×256 texels).
- `INV_TEX_SIZE = 0.00390625f` (= 1/256) defined in `Piedef.h:65`.
- Normalization happens at the draw boundary in `D3drender.c:281-282`:
  ```c
  d3dVrts[i].tu = (float)pVrts[i].tu * (float)INV_TEX_SIZE + g_fTextureOffset;
  d3dVrts[i].tv = (float)pVrts[i].tv * (float)INV_TEX_SIZE + g_fTextureOffset;
  ```
  `g_fTextureOffset` is a sub-texel bias (typically `0.0f` or a small epsilon).
- The same pattern is used in `Piedraw.c:1432-1433` and `Piefunc.c:365-366`.
- Texture animation (`iTexAnim`) works by adding integer offsets to `tu`/`tv` to index different frames within the 256×256 tile sheet (`Piedraw.c:1129-1134`). This is pure UV arithmetic — **no DD surface flipping** is involved.

**Conclusion:** The `pievert_to_d3d9()` conversion should use `pDst->tu = (float)pSrc->tu * INV_TEX_SIZE`. This is already implemented. **Risk R-10 is mitigated.**

### Q4. What is `iV_SCANTABLE_MAX`? (Risk R-08)
**Priority: Medium**
**Status:** ✅ **Resolved — `iV_SCANTABLE_MAX = 1024`. Will overflow at 1080p.**

From `Ivisdef.h:27`:
```c
#define iV_SCANTABLE_MAX    1024
```

The scantable is declared in the `iSurface` struct (`Ivisdef.h:73`):
```c
int32 scantable[iV_SCANTABLE_MAX];  // currently uses 4k per structure (!)
```

**Usage analysis** — the scantable is indexed by Y-coordinate in:
- `Rendfunc.c:230,697` — software line/box rendering (indexed by `i` = scanline Y)
- `TextDraw.c:862,888,1091` — software text rendering (indexed by `y + h`)
- `D3dmode.c:48` — initialization: `rendSurface.scantable[i] = i * WIDTH_D3D`
- `Rendmode.c:184` — initialization: `s->scantable[i] = i * width`

At 1080p, `y` values will reach up to `1079`, which exceeds `iV_SCANTABLE_MAX = 1024` → **buffer overrun**.

**Mitigation options:**
1. Increase `iV_SCANTABLE_MAX` to `2160` (supports up to 4K) — simple but adds 8.4 KB per `iSurface` instance.
2. Replace scantable lookups with `y * width` multiplication at usage sites — eliminates the array entirely.
3. Confirm whether `Rendfunc.c` and `TextDraw.c` software paths are actually reachable when `pie_Hardware() == TRUE` (ENGINE_D3D). If they are dead code in the D3D path, the overflow is theoretical.

**Action required in Phase 6** when supporting resolutions above 1024 lines.

### Q5. Does `getdxver.cpp` (the only `.cpp` file) conflict with the C-only build?
**Priority: Low**
**Status:** ✅ **Resolved — no conflict. File is not compiled.**

`getdxver.cpp` exists at `NeuronClient/ivis02/getdxver.cpp` but is **not referenced in any `CMakeLists.txt`**. It is a legacy Microsoft sample (copyright 1995-1997) that detects DirectX version at runtime. It includes `<ddraw.h>`, `<dinput.h>`, and `<d3drm.h>`.

Since it is not in the build, it causes no compilation or linker issues. It can be deleted as dead code or left in place — it is harmless.

### Q6. How does the `Framework` layer pass `HWND` to the rendering system?
**Priority: Medium**
**Status:** ✅ **Resolved — call chain is `D3dmode.c` → `screenGetHWnd()` → `Screen.c:hWndMain`**

The HWND flow is:
1. `D3dmode.c:52` calls `InitD3D(screenGetHWnd(), (g_ScreenMode == SCREEN_FULLSCREEN))`.
2. `screenGetHWnd()` is defined in `Screen.c:952` and returns `hWndMain`.
3. `hWndMain` is set in `Screen.c:995` by `screenInitialise()` which receives the window handle from `WinMain`.
4. `InitD3D()` in `D3drender.c:91` receives the HWND and passes it to `D3DPRESENT_PARAMETERS.hDeviceWindow` at line 107.

**This call chain is already functional in the D3D9 migration.** No change needed — `screenGetHWnd()` is API-agnostic (returns a plain `HWND` from a global variable). It does not depend on DirectDraw and will survive the Framework `Screen.c` migration.

### Q7. Are any textures animated via DirectDraw surface flipping?
**Priority: Medium**
**Status:** ✅ **Resolved — No. Texture animation is UV-based (tile sheet), not surface flipping.**

`iTexAnim` in `Piedraw.c` implements animation by offsetting `tu`/`tv` coordinates to select different frames within a 256×256 texture tile sheet:

```c
// Piedraw.c:1121-1129
framesPerLine = 256 / poly->pTexAnim->textureWidth;
vFrame = 0;
while (frame >= framesPerLine) {
    frame -= framesPerLine;
    vFrame += poly->pTexAnim->textureHeight;
}
uFrame = frame * poly->pTexAnim->textureWidth;
// Then: poly->pVrts[j].tu += uFrame; poly->pVrts[j].tv += vFrame;
```

This is pure integer UV arithmetic. **No DD surface swapping, no texture pointer changes.** The animation system is completely API-agnostic and requires no migration work.

### Q8. Does the `Widget` system directly access any `LPDIRECTDRAWSURFACE4`?
**Priority: Low**
**Status:** ✅ **Resolved — No. Zero DirectDraw references in Widget code.**

A grep of all `.c` and `.h` files in `NeuronClient/Widget/` for `LPDIRECTDRAW`, `LPDIRECTDRAWSURFACE`, `ddraw.h`, `screenGetSurface`, and `screenGetBackBuffer` returns **zero matches**. The Widget layer communicates exclusively through the public renderer API (`Piefunc.c` / `PieBlitFunc.c`). No migration work is needed for Widgets.

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

## 18. Implementation Status

*Last updated after Framework layer migration and `ddraw.lib` removal — all changes compile cleanly.*

### 18.1 Phase Completion Summary

| Phase | Description | Status | Notes |
|---|---|---|---|
| **Phase 1** | D3D9 device + frame loop | ✅ **Complete** | `D3drender.c/h` fully rewritten. `D3D9GLOBALS`, `BeginSceneD3D`/`EndSceneD3D` with device-lost handling. |
| **Phase 2** | Texture system | ✅ **Complete** | `Dx6TexMan.c` uses `IDirect3DDevice9_CreateTexture` + `LockRect`. `Texd3d.h` defines `TEXPAGE_D3D` with `IDirect3DTexture9*`. Radar surface migrated. |
| **Phase 3** | Fixed-point math conversion | ✅ **Implicit** | `D3DVAL` compat macro added to `Piedef.h`. `PIE_D3D9_VERTEX` type used at draw boundary. Full `pievert_to_d3d9()` function deferred — vertex conversion is inline in draw paths. |
| **Phase 4** | Vertex format + draw calls | ✅ **Complete** | `PIE_D3D9_VERTEX` and `PIE_FVF_D3D9` defined in `Piedef.h`. All draw paths in `D3drender.c`, `Piedraw.c`, `Piefunc.c` use `IDirect3DDevice9_DrawPrimitiveUP`. |
| **Phase 5** | State management | ✅ **Complete** | `D3D9_SetDefaultRenderStates()` in `D3drender.c`. Translucency mode mapping via `D3DRS_SRCBLEND`/`D3DRS_DESTBLEND`. Alpha-test replaces color keying. Fog via `D3DRS_FOG*`. Viewport via `D3DVIEWPORT9`. |
| **Phase 6** | Resolution + widescreen | ⏳ **Not started** | Framework migration complete — no longer blocked. Can proceed with dynamic resolution support. |
| **Phase 7** | Video playback | ⏳ **Not started** | `NeuronClient/Sequence/` files not present in main working tree. |
| **Phase 8** | Build system | ✅ **Complete** | `d3d9.lib` linked. `ddraw.lib` **removed**. `d3dx.lib` removed. `<d3d.h>` and `<d3drm.h>` removed from all files. |
| **Phase 9** | Validation + testing | ⏳ **Not started** | Framework migration complete — runtime testing can begin. |

### 18.2 Renderer Layer (`NeuronClient/ivis02/`) — File Status

**Zero D3D3/DirectDraw symbols remain in the renderer layer** (confirmed via grep — no `LPDIRECTDRAW*`, `LPDIRECT3D3`, `LPDIRECT3DDEVICE3`, `LPDIRECT3DTEXTURE2`, `DDPIXELFORMAT`, `DDSURFACEDESC`, `D3DTLVERTEX`, `D3DRENDERSTATE_*`, `D3DTBLEND_*`, `#include <ddraw.h>`, or `#include <d3d.h>` in any `.c`/`.h` file under `ivis02/`).

| File | Planned Change | Actual Status | Details |
|---|---|---|---|
| `D3drender.h` | Full rewrite | ✅ **Done** | `D3D9GLOBALS` struct, all function signatures use D3D9 types. Includes `<d3d9.h>`. |
| `D3drender.c` | Full rewrite | ✅ **Done** | `D3D9_InitDevice`, `BeginSceneD3D`/`EndSceneD3D` with Present + device-lost. `D3D9_SetDefaultRenderStates`. `D3DDrawPoly`/`D3D_PIEPolygon` via `DrawPrimitiveUP`. Translucency mode mapping. Fog, depth, alpha-test state setters. Viewport + resolution change via `Reset`. |
| `D3dmode.h` | Full rewrite | ✅ **Done** | DD surface dependencies removed. |
| `D3dmode.c` | Full rewrite | ✅ **Done** | `_mode_D3D_*` functions replaced. |
| `Dx6TexMan.h` | Full rewrite | ✅ **Done** | Clean API — `dtm_LoadTexSurface`, `dtm_LoadRadarSurface`, `dx6_SetBilinear`. |
| `Dx6TexMan.c` | Full rewrite | ✅ **Done** | `IDirect3DDevice9_CreateTexture` with `D3DPOOL_MANAGED`. Palette-to-ARGB conversion at load time. Radar texture. Bilinear via `SetSamplerState`. |
| `Texd3d.h` | Full rewrite | ✅ **Done** | `TEXPAGE_D3D` uses `IDirect3DTexture9*`. Includes `<d3d9.h>`. |
| `Texd3d.c` | Full rewrite | ✅ **Done** | DD surface ops replaced with D3D9 texture ops. |
| `Piedef.h` | Minor | ✅ **Done** | `PIE_D3D9_VERTEX`, `PIE_FVF_D3D9`, `D3DVAL` compat macro added. `PIED3DPOLY.pVrts` uses `PIE_D3D9_VERTEX*`. |
| `PiePalette.h` | Partial | ✅ **Done** | `pie_GetWinPal()` removed. `PALETTEENTRY*` remains (Windows GDI type, not DD). |
| `PiePalette.c` | Partial | ✅ **Done** | `pie_GetWinPal()` removed. `pal_Make16BitPalette()` rewritten — hardcoded R5G6B5 format, `DDPIXELFORMAT` eliminated. `palette16Bit[]` still populated for `TextDraw.c` text rendering. |
| `PieState.h` | Moderate | ✅ **Done** | `pie_SetDirectDrawDeviceName`/`pie_GetDirectDrawDeviceName` removed. Dead engine enum values kept for branch compilation. |
| `PieState.c` | Moderate | ✅ **Done** | `DDrawDriverName[256]` and associated functions removed. Render state setters use D3D9 enums via `D3drender.c`. |
| `Piedraw.c` | Minor | ✅ **Done** | Uses `PIE_D3D9_VERTEX` and D3D9 draw path. |
| `Piefunc.h` | Minor | ✅ **Done** | `pie_RenderImageToSurface` signature updated — `LPDIRECTDRAWSURFACE4` removed. |
| `Piefunc.c` | Minor | ✅ **Done** | `pie_RenderImageToSurface` stubbed (dead path). Uses `PIE_D3D9_VERTEX`. |
| `PieBlitFunc.h` | Minor | ✅ **Done** | No DD types. |
| `PieBlitFunc.c` | Minor | ✅ **Done** | `bufferTo16Bit()` uses hardcoded R5G6B5. |
| `TextDraw.h` | Minor | ✅ **Done** | `pie_DrawTextToSurface` signature updated — `LPDIRECTDRAWSURFACE4` removed. |
| `TextDraw.c` | Minor | ✅ **Done** | `#include <ddraw.h>` removed. `pie_DrawTextToSurface` stubbed (no callers). |
| `PieMode.c` | Minor | ✅ **Done** | Calls `_close_D3D`, `_renderBegin_D3D`, `_renderEnd_D3D` which target D3D9 functions. |
| `Rendmode.h` | Minor | ✅ **Done** | `REND_D3D_RGB/HAL/REF` constants kept for legacy branch compilation. |
| `PieMatrix.h/.c` | None | ✅ **No change needed** | Fixed-point math unchanged. |
| `PieClip.h/.c` | None | ✅ **No change needed** | Clipping unchanged. |
| `PieTexture.h/.c` | Moderate | ✅ **Done** | Calls `DTM9_*` functions. |
| `BitImage.h/.c` | None | ✅ **No change needed** | CPU-side image data. |
| `Imd.h/.c` | None | ✅ **No change needed** | Model loading. |

### 18.3 Framework Layer (`NeuronClient/Framework/`) — Migration Complete

The Framework layer has been fully decoupled from DirectDraw at the linker level. DD init is stubbed, all DD vtable calls are NULL-guarded, and `ddraw.lib` has been removed from the build. `<ddraw.h>` remains included in 4 Framework files for type definitions only (no linker dependency).

| File | Status | Details |
|---|---|---|
| `Screen.c` | ✅ **Migrated** | `screenInitialise()` stubs DD init — sets `psDD`/`psFront`/`psBack` to NULL, hardcodes R5G6B5 pixel format. `screenFlip()` maintains flip sync state only — D3D9 Present handled by renderer. All DD vtable calls (`Lock`/`Unlock`/`Blt`/`Flip`) NULL-guarded with early return. `screenShutDown()` cleans up sync objects. |
| `Screen.h` | ✅ **Functional** | `screenGetDDObject()` returns NULL. `screenGetSurface()` returns NULL. `screenGetBackBufferPixelFormat()` returns hardcoded R5G6B5 struct. All callers guarded. |
| `Font.c` | ✅ **Guarded** | `fontPrint()` and `fontPrintChar()` return early when `psBack == NULL`. |
| `Cursor.c` | ✅ **Guarded** | Cursor thread skips DD blitting when `psFront == NULL` (sleeps instead). |
| `Surface.c` | ✅ **Guarded** | `surfCreate()`, `surfRecreate()`, `surfLoadFrom8Bit()`, `surfLoadFromSurface()`, `DDSetColorKey()` all return early when DD objects are NULL. |
| `Dderror.c` | ✅ **Stubbed** | `DDErrorToString()` replaced with generic HRESULT-to-hex stub. `<ddraw.h>`, `<d3d.h>`, `<d3drm.h>` removed. |
| `Image.c` | ✅ **Cleaned** | `#include <ddraw.h>` removed (unused). |
| `Input.c` | ✅ **Cleaned** | `#include <ddraw.h>` removed (unused). |
| `Frame.h` | 🔶 **Type dependency** | Includes `"ddraw.h"` for type definitions used by `Screen.h`. |

**Remaining `<ddraw.h>` includes (type definitions only — no linker dependency):**
- `Screen.c`, `Screen.h`, `Cursor.c`, `Frame.h` — 4 files total

**Game-side callers updated:**

| Caller | Function | Status |
|---|---|---|
| `StarStrike/Disp2D.c` (3 calls) | `screenGetSurface()` | ✅ NULL-guarded — returns early when `psBack == NULL` |
| `StarStrike/MultiInt.c` (1 call) | `screenGetBackBufferPixelFormat()` | ✅ Works — returns hardcoded R5G6B5 struct (`dwRGBBitCount == 16`) |
| `StarStrike/SeqDisp.c` (11 calls) | `screenGetSurface()`, `screenGetBackBufferPixelFormat()` | 🔶 Video playback — deferred to Phase 7. NULL surface causes graceful failure. |

### 18.4 Build System Status

| Item | Status | Detail |
|---|---|---|
| `d3d9.lib` | ✅ Linked | In `StarStrike/CMakeLists.txt` |
| `ddraw.lib` | ✅ **Removed** | No linker dependency on DirectDraw |
| `d3dx.lib` | ✅ Removed | Was D3DX for D3D3 |
| `d3dx9.lib` | ⏳ Not added | Optional — not needed for current migration |
| `dxguid.lib` | ✅ Linked | Provides D3D9 GUIDs |
| `dplayx.lib` | ✅ Linked | DirectPlay — separate from DD/D3D migration |
| `<d3d9.h>` | ✅ Used | In `D3drender.h`, `Texd3d.h`, `Piedef.h` |
| `<ddraw.h>` | ✅ Removed from renderer | Remains in 4 Framework files for type definitions only |
| `<d3d.h>` | ✅ **Removed** | Zero includes remaining |
| `<d3drm.h>` | ✅ **Removed** | Zero includes remaining |
| Build result | ✅ **Clean** | `cmake --build .` succeeds with zero errors, zero `ddraw.lib` linker dependency |

### 18.5 Dead Code Cleaned Up

| Item | Location | Action Taken |
|---|---|---|
| `pie_GetWinPal()` | `PiePalette.c` / `PiePalette.h` | ✅ **Removed** — function and declaration deleted |
| `pie_SetDirectDrawDeviceName()` | `PieState.c` / `PieState.h` | ✅ **Removed** — function, declaration, and `DDrawDriverName[256]` deleted |
| `pie_GetDirectDrawDeviceName()` | `PieState.c` / `PieState.h` | ✅ **Removed** — function and declaration deleted |
| `DDErrorToString()` full switch | `Dderror.c` | ✅ **Stubbed** — 330-line switch replaced with generic hex formatter |
| `#include <ddraw.h>` in `Image.c` | `Image.c` | ✅ **Removed** — no DD types used |
| `#include <ddraw.h>` in `Input.c` | `Input.c` | ✅ **Removed** — no DD types used |
| `#include <d3d.h>` in `Dderror.c` | `Dderror.c` | ✅ **Removed** |
| `#include <d3drm.h>` in `Dderror.c` | `Dderror.c` | ✅ **Removed** |

**Remaining dead code (low priority — compiles harmlessly):**

| Item | Location | Reason Kept |
|---|---|---|
| `pie_DrawTextToSurface()` | `TextDraw.c` | Stubbed — no callers. Harmless. |
| `pie_RenderImageToSurface()` | `Piefunc.c` | Stubbed — no callers. Harmless. |
| `pie_D3DRenderForFlip()` | `PieBlitFunc.c` | Always no-ops. Harmless. |
| `ENGINE_4101_REMOVED` / `ENGINE_SR_REMOVED` / `ENGINE_GLIDE_REMOVED` | `PieState.h` | ~60 game-side `if (ENGINE_GLIDE)` branches reference these. Dead at runtime but required for compilation. |
| `REND_D3D_RGB` / `REND_D3D_HAL` / `REND_D3D_REF` | `Rendmode.h` | Legacy mode constants — kept for branch compilation. |
| `psWinPal` global | `PiePalette.c` | Used by `pal_AddNewPalette()` for `screenSetPalette()` (itself a no-op). |

### 18.6 Remaining Work — Next Steps

**Priority 1 — Resolution + widescreen (Phase 6, now unblocked):**
- Framework migration is complete — Phase 6 can proceed
- Update `rendSurface` for dynamic resolution (xcentre/ycentre scaling)
- Increase `iV_SCANTABLE_MAX` from 1024 to 2160 (Risk R-08)
- Add D3D9 display mode enumeration
- Add `D3DFMT_X8R8G8B8` fallback for back buffer format (Risk R-04)

**Priority 2 — Video playback (Phase 7):**
- Locate or recreate `Sequence.c`/`Sequence.h` in main working tree
- Test `WINSTR.LIB` CPU buffer path (Risk R-01)
- Implement D3D9 dynamic texture upload for video frames
- Update `SeqDisp.c` callers of `screenGetSurface()` (11 call sites)

**Priority 3 — Validation (Phase 9):**
- Runtime test at 640×480 baseline
- Device-lost recovery test (Alt-Tab)
- Resolution matrix testing
- Visual regression comparison

**Priority 4 — Cosmetic cleanup (optional):**
- Rename `REND_ENGINE::ENGINE_D3D` → `ENGINE_D3D9` across all files
- Rename `TEXPAGE_D3D` → `TEXPAGE_D3D9`
- Remove ~60 dead `if (ENGINE_GLIDE)` branches in game-side code
- Replace `<ddraw.h>` in Framework files with stub header (eliminate SDK dependency)

---

*End of DirectXMig.md*
