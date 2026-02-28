#pragma once

/*
 * DdrawCompat.h
 *
 * Stub type definitions replacing <ddraw.h> for the D3D9 migration.
 * Provides enough type && vtable compatibility for existing Framework
 * code to compile without linking ddraw.lib || requiring the real header.
 *
 * All DD interface pointers are always NULL at runtime � the Framework
 * layer NULL-guards every vtable call, so no method is ever invoked.
 * The vtable stubs exist purely to satisfy the compiler.
 */

#include <windows.h>

/* If the real ddraw.h was already included, skip everything */
#ifdef DIRECTDRAW_VERSION

#ifndef RELEASE
#define RELEASE(x) if ((x) != NULL) {(void)(x)->Release(); (x) = NULL;}
#endif

#else /* No real ddraw.h � provide stub types */

/* ================================================================== */
/* Forward declarations                                                */
/* ================================================================== */

typedef struct IDirectDraw         IDirectDraw;
typedef struct IDirectDraw4        IDirectDraw4;
typedef struct IDirectDrawSurface4 IDirectDrawSurface4;
typedef struct IDirectDrawClipper  IDirectDrawClipper;
typedef struct IDirectDrawPalette  IDirectDrawPalette;

typedef IDirectDraw*         LPDIRECTDRAW;
typedef IDirectDraw4*        LPDIRECTDRAW4;
typedef IDirectDrawSurface4* LPDIRECTDRAWSURFACE4;
typedef IDirectDrawClipper*  LPDIRECTDRAWCLIPPER;
typedef IDirectDrawPalette*  LPDIRECTDRAWPALETTE;

/* ================================================================== */
/* DD data structures                                                  */
/* ================================================================== */

typedef struct _DDPIXELFORMAT_COMPAT
{
	DWORD   dwSize;
	DWORD   dwFlags;
	DWORD   dwFourCC;
	DWORD   dwRGBBitCount;
	DWORD   dwRBitMask;
	DWORD   dwGBitMask;
	DWORD   dwBBitMask;
	DWORD   dwRGBAlphaBitMask;
} DDPIXELFORMAT;

typedef struct _DDCOLORKEY_COMPAT
{
	DWORD   dwColorSpaceLowValue;
	DWORD   dwColorSpaceHighValue;
} DDCOLORKEY;

typedef struct _DDSCAPS2_COMPAT
{
	DWORD   dwCaps;
	DWORD   dwCaps2;
	DWORD   dwCaps3;
	DWORD   dwCaps4;
} DDSCAPS2;

typedef struct _DDSCAPS_COMPAT
{
	DWORD   dwCaps;
} DDSCAPS;

typedef struct _DDSURFACEDESC2_COMPAT
{
	DWORD           dwSize;
	DWORD           dwFlags;
	DWORD           dwHeight;
	DWORD           dwWidth;
	LONG            lPitch;
	DWORD           dwBackBufferCount;
	DWORD           dwRefreshRate;
	DWORD           dwAlphaBitDepth;
	DWORD           dwReserved;
	LPVOID          lpSurface;
	DDCOLORKEY      ddckCKDestOverlay;
	DDCOLORKEY      ddckCKDestBlt;
	DDCOLORKEY      ddckCKSrcOverlay;
	DDCOLORKEY      ddckCKSrcBlt;
	DDPIXELFORMAT   ddpfPixelFormat;
	DDSCAPS2        ddsCaps;
	DWORD           dwTextureStage;
} DDSURFACEDESC2;

typedef struct _DDBLTFX_COMPAT
{
	DWORD           dwSize;
	DWORD           dwDDFX;
	DWORD           dwROP;
	DWORD           dwDDROP;
	DWORD           dwRotationAngle;
	DWORD           dwZBufferOpCode;
	DWORD           dwZBufferLow;
	DWORD           dwZBufferHigh;
	DWORD           dwZBufferBaseDest;
	DWORD           dwZDestConstBitDepth;
	DWORD           dwZDestConst;
	LPDIRECTDRAWSURFACE4 lpDDSZBufferDest;
	DWORD           dwZSrcConstBitDepth;
	DWORD           dwZSrcConst;
	LPDIRECTDRAWSURFACE4 lpDDSZBufferSrc;
	DWORD           dwAlphaEdgeBlendBitDepth;
	DWORD           dwAlphaEdgeBlend;
	DWORD           dwReserved;
	DWORD           dwAlphaDestConstBitDepth;
	DWORD           dwAlphaDestConst;
	LPDIRECTDRAWSURFACE4 lpDDSAlphaDest;
	DWORD           dwAlphaSrcConstBitDepth;
	DWORD           dwAlphaSrcConst;
	LPDIRECTDRAWSURFACE4 lpDDSAlphaSrc;
	DWORD           dwFillColor;
	DWORD           dwFillDepth;
	DWORD           dwFillPixel;
	LPDIRECTDRAWSURFACE4 lpDDSPattern;
	DDCOLORKEY      ddckDestColorkey;
	DDCOLORKEY      ddckSrcColorkey;
} DDBLTFX;

typedef struct _DDDEVICEIDENTIFIER_COMPAT
{
	char            szDriver[512];
	char            szDescription[512];
	LARGE_INTEGER   liDriverVersion;
	DWORD           dwVendorId;
	DWORD           dwDeviceId;
	DWORD           dwSubSysId;
	DWORD           dwRevision;
	GUID            guidDeviceIdentifier;
} DDDEVICEIDENTIFIER;

/* ================================================================== */
/* COM interface vtable stubs                                          */
/* Only methods actually referenced in Framework code are defined.     */
/* Vtables are never instantiated � interface pointers are always NULL.*/
/* ================================================================== */

/* --- IDirectDraw (DD1) --- */
typedef struct IDirectDrawVtbl
{
	/* IUnknown */
	HRESULT (WINAPI *QueryInterface)(IDirectDraw*, REFIID, void**);
	ULONG   (WINAPI *AddRef)(IDirectDraw*);
	ULONG   (WINAPI *Release)(IDirectDraw*);
	/* IDirectDraw � unused stubs */
	void *Compact, *CreateClipper, *CreatePalette, *CreateSurface;
	void *DuplicateSurface, *EnumDisplayModes, *EnumSurfaces;
	void *FlipToGDISurface, *GetCaps, *GetDisplayMode;
	void *GetFourCCCodes, *GetGDISurface, *GetMonitorFrequency;
	void *GetScanLine, *GetVerticalBlankStatus, *Initialize;
	void *RestoreDisplayMode, *SetCooperativeLevel, *SetDisplayMode;
	void *WaitForVerticalBlank;
} IDirectDrawVtbl;

struct IDirectDraw { IDirectDrawVtbl *lpVtbl; };

/* --- IDirectDraw4 --- */
typedef struct IDirectDraw4Vtbl
{
	/* IUnknown */
	HRESULT (WINAPI *QueryInterface)(IDirectDraw4*, REFIID, void**);
	ULONG   (WINAPI *AddRef)(IDirectDraw4*);
	ULONG   (WINAPI *Release)(IDirectDraw4*);
	/* IDirectDraw */
	void *Compact;
	HRESULT (WINAPI *CreateClipper)(IDirectDraw4*, DWORD, LPDIRECTDRAWCLIPPER*, void*);
	HRESULT (WINAPI *CreatePalette)(IDirectDraw4*, DWORD, PALETTEENTRY*, LPDIRECTDRAWPALETTE*, void*);
	HRESULT (WINAPI *CreateSurface)(IDirectDraw4*, DDSURFACEDESC2*, LPDIRECTDRAWSURFACE4*, void*);
	void *DuplicateSurface, *EnumDisplayModes, *EnumSurfaces;
	void *FlipToGDISurface, *GetCaps, *GetDisplayMode;
	void *GetFourCCCodes, *GetGDISurface, *GetMonitorFrequency;
	void *GetScanLine, *GetVerticalBlankStatus, *Initialize;
	HRESULT (WINAPI *RestoreDisplayMode)(IDirectDraw4*);
	HRESULT (WINAPI *SetCooperativeLevel)(IDirectDraw4*, HWND, DWORD);
	HRESULT (WINAPI *SetDisplayMode)(IDirectDraw4*, DWORD, DWORD, DWORD, DWORD, DWORD);
	void *WaitForVerticalBlank;
	/* IDirectDraw2+ */
	void *GetAvailableVidMem;
	/* IDirectDraw4 */
	void *GetSurfaceFromDC, *RestoreAllSurfaces, *TestCooperativeLevel;
	HRESULT (WINAPI *GetDeviceIdentifier)(IDirectDraw4*, DDDEVICEIDENTIFIER*, DWORD);
} IDirectDraw4Vtbl;

struct IDirectDraw4 { IDirectDraw4Vtbl *lpVtbl; };

/* --- IDirectDrawSurface4 --- */
typedef struct IDirectDrawSurface4Vtbl
{
	/* IUnknown */
	HRESULT (WINAPI *QueryInterface)(IDirectDrawSurface4*, REFIID, void**);
	ULONG   (WINAPI *AddRef)(IDirectDrawSurface4*);
	ULONG   (WINAPI *Release)(IDirectDrawSurface4*);
	/* IDirectDrawSurface */
	void *AddAttachedSurface;
	void *AddOverlayDirtyRect;
	HRESULT (WINAPI *Blt)(IDirectDrawSurface4*, RECT*, IDirectDrawSurface4*, RECT*, DWORD, DDBLTFX*);
	void *BltBatch, *BltFast;
	void *DeleteAttachedSurface, *EnumAttachedSurfaces, *EnumOverlayZOrders;
	HRESULT (WINAPI *Flip)(IDirectDrawSurface4*, IDirectDrawSurface4*, DWORD);
	HRESULT (WINAPI *GetAttachedSurface)(IDirectDrawSurface4*, DDSCAPS2*, LPDIRECTDRAWSURFACE4*);
	void *GetBltStatus;
	HRESULT (WINAPI *GetCaps)(IDirectDrawSurface4*, DDSCAPS2*);
	void *GetClipper;
	HRESULT (WINAPI *GetColorKey)(IDirectDrawSurface4*, DWORD, DDCOLORKEY*);
	HRESULT (WINAPI *GetDC)(IDirectDrawSurface4*, HDC*);
	void *GetFlipStatus;
	void *GetOverlayPosition;
	void *GetPalette;
	HRESULT (WINAPI *GetPixelFormat)(IDirectDrawSurface4*, DDPIXELFORMAT*);
	HRESULT (WINAPI *GetSurfaceDesc)(IDirectDrawSurface4*, DDSURFACEDESC2*);
	void *Initialize, *IsLost;
	HRESULT (WINAPI *Lock)(IDirectDrawSurface4*, RECT*, DDSURFACEDESC2*, DWORD, HANDLE);
	HRESULT (WINAPI *ReleaseDC)(IDirectDrawSurface4*, HDC);
	void *Restore;
	HRESULT (WINAPI *SetClipper)(IDirectDrawSurface4*, LPDIRECTDRAWCLIPPER);
	HRESULT (WINAPI *SetColorKey)(IDirectDrawSurface4*, DWORD, DDCOLORKEY*);
	HRESULT (WINAPI *SetPalette)(IDirectDrawSurface4*, LPDIRECTDRAWPALETTE);
	HRESULT (WINAPI *Unlock)(IDirectDrawSurface4*, LPVOID);
	void *UpdateOverlay, *UpdateOverlayDisplay, *UpdateOverlayZOrder;
	/* IDirectDrawSurface2+ */
	void *GetDDInterface, *PageLock, *PageUnlock;
	/* IDirectDrawSurface3+ */
	void *SetSurfaceDesc;
	/* IDirectDrawSurface4 */
	void *SetPrivateData, *GetPrivateData, *FreePrivateData;
	void *GetUniquenessValue, *ChangeUniquenessValue;
} IDirectDrawSurface4Vtbl;

struct IDirectDrawSurface4 { IDirectDrawSurface4Vtbl *lpVtbl; };

/* --- IDirectDrawClipper --- */
typedef struct IDirectDrawClipperVtbl
{
	/* IUnknown */
	HRESULT (WINAPI *QueryInterface)(IDirectDrawClipper*, REFIID, void**);
	ULONG   (WINAPI *AddRef)(IDirectDrawClipper*);
	ULONG   (WINAPI *Release)(IDirectDrawClipper*);
	/* IDirectDrawClipper */
	void *GetClipList, *GetHWnd, *Initialize, *IsClipListChanged;
	void *SetClipList;
	HRESULT (WINAPI *SetHWnd)(IDirectDrawClipper*, DWORD, HWND);
} IDirectDrawClipperVtbl;

struct IDirectDrawClipper { IDirectDrawClipperVtbl *lpVtbl; };

/* --- IDirectDrawPalette --- */
typedef struct IDirectDrawPaletteVtbl
{
	/* IUnknown */
	HRESULT (WINAPI *QueryInterface)(IDirectDrawPalette*, REFIID, void**);
	ULONG   (WINAPI *AddRef)(IDirectDrawPalette*);
	ULONG   (WINAPI *Release)(IDirectDrawPalette*);
	/* IDirectDrawPalette */
	void *GetCaps, *GetEntries, *Initialize;
	HRESULT (WINAPI *SetEntries)(IDirectDrawPalette*, DWORD, DWORD, DWORD, PALETTEENTRY*);
} IDirectDrawPaletteVtbl;

struct IDirectDrawPalette { IDirectDrawPaletteVtbl *lpVtbl; };

/* ================================================================== */
/* DD constants                                                        */
/* ================================================================== */

#ifndef DD_OK
#define DD_OK                       0
#endif

/* DDPF flags */
#define DDPF_RGB                    0x00000040
#define DDPF_PALETTEINDEXED8        0x00000020

/* DDSCAPS flags */
#define DDSCAPS_PRIMARYSURFACE      0x00000200
#define DDSCAPS_OFFSCREENPLAIN      0x00000040
#define DDSCAPS_SYSTEMMEMORY        0x00000800
#define DDSCAPS_VIDEOMEMORY         0x00004000
#define DDSCAPS_3DDEVICE            0x00002000
#define DDSCAPS_COMPLEX             0x00000008
#define DDSCAPS_FLIP                0x00000010
#define DDSCAPS_BACKBUFFER          0x00000004
#define DDSCAPS_LOCALVIDMEM         0x10000000
#define DDSCAPS_NONLOCALVIDMEM      0x20000000

/* DDSD flags */
#define DDSD_CAPS                   0x00000001
#define DDSD_HEIGHT                 0x00000002
#define DDSD_WIDTH                  0x00000004
#define DDSD_PIXELFORMAT            0x00001000
#define DDSD_BACKBUFFERCOUNT        0x00000020

/* DDBLT flags */
#define DDBLT_COLORFILL             0x00000400
#define DDBLT_WAIT                  0x01000000
#define DDBLT_ASYNC                 0x00000200
#define DDBLT_KEYSRCOVERRIDE        0x00002000

/* DDPCAPS flags */
#define DDPCAPS_1BIT                0x00000100
#define DDPCAPS_2BIT                0x00000200
#define DDPCAPS_4BIT                0x00000001
#define DDPCAPS_8BIT                0x00000004
#define DDPCAPS_ALLOW256            0x00000040

/* DDSCL flags */
#define DDSCL_EXCLUSIVE             0x00000010
#define DDSCL_FULLSCREEN            0x00000001
#define DDSCL_NORMAL                0x00000008

/* DDFLIP flags */
#define DDFLIP_WAIT                 0x00000001

/* DD error codes */
#define DDERR_WASSTILLDRAWING       ((HRESULT)0x8876001EL)
#define DDERR_UNSUPPORTED           ((HRESULT)0x80004001L)

/* DDLOCK flags */
#define DDLOCK_WAIT                 0x00000001

/* DD callback return values */
#define DDENUMRET_OK                1
#define DDENUMRET_CANCEL            0

/* DDGDI flags */
#define DDGDI_GETHOSTIDENTIFIER     0x00000001

/* DDENUM flags */
#define DDENUM_ATTACHEDSECONDARYDEVICES  0x00000001
#define DDENUM_DETACHEDSECONDARYDEVICES  0x00000002
#define DDENUM_NONDISPLAYDEVICES         0x00000004

/* DDCKEY flags */
#define DDCKEY_SRCBLT               0x00000008

/* Callback typedefs */
typedef BOOL (WINAPI *LPDDENUMCALLBACKEXA)(GUID*, LPSTR, LPSTR, LPVOID, HMONITOR);
typedef HRESULT (WINAPI *LPDIRECTDRAWENUMERATEEXA)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);

/* ================================================================== */
/* COM Release macro — calls Release via vtable                       */
/* At runtime, pointers are always NULL so Release is never called.    */
/* ================================================================== */

#ifndef RELEASE
#define RELEASE(x) if ((x) != NULL) {(void)(x)->Release(); (x) = NULL;}
#endif

#endif /* DIRECTDRAW_VERSION */