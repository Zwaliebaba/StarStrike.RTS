/***************************************************************************/

#include <stdio.h>

#include "Ivi.h"
#include "Rendmode.h"
#include "Tex.h"
#include "PiePalette.h"
#include "PieState.h"
#include "PieClip.h"
#include "FrameInt.h"

#include "D3drender.h"
#include "Texd3d.h"
#include "Dx6TexMan.h"

/***************************************************************************/
/* Defines */

#define	TEXEL_OFFSET_256		(1.0f/512.0f)

/***************************************************************************/
/* Macros */

#define ATTEMPT(x) if (!(x)) goto exit_with_error

/***************************************************************************/
/* global variables */

D3D9GLOBALS g_D3D9 = {0};

static BOOL				g_bAlphaKey = FALSE;
static PIE_D3D9_VERTEX	d3dVrts[pie_MAX_POLY_VERTS];
static float			g_fTextureOffset = 0.0f;
static BOOL				g_bTexelOffsetOn;

/***************************************************************************/

void
D3D9_SetDefaultRenderStates(void)
{
	IDirect3DDevice9* dev = g_D3D9.pDevice;
	if (!dev) return;

	IDirect3DDevice9_SetFVF(dev, PIE_FVF_D3D9);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_SHADEMODE,        D3DSHADE_GOURAUD);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE,         D3DCULL_NONE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_FILLMODE,         D3DFILL_SOLID);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE,          D3DZB_TRUE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE,     TRUE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ZFUNC,            D3DCMP_LESSEQUAL);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE,  FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING,         FALSE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_CLIPPING,         TRUE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_DITHERENABLE,     TRUE);
	IDirect3DDevice9_SetRenderState(dev, D3DRS_SPECULARENABLE,   TRUE);

	IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
	IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP);
	IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP);
	IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
}

/***************************************************************************/

static void
D3D9_HandleDeviceLost(void)
{
	HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(g_D3D9.pDevice);
	if (hr == D3DERR_DEVICENOTRESET)
	{
		dtm_ReleaseTextures();
		IDirect3DDevice9_Reset(g_D3D9.pDevice, &g_D3D9.presentParams);
		dtm_ReloadAllTextures();
		D3D9_SetDefaultRenderStates();
		g_D3D9.bDeviceLost = FALSE;
	}
}

/***************************************************************************/

BOOL
InitD3D( HWND hWnd, BOOL bFullscreen )
{
	HRESULT hr;

	memset( &g_D3D9, 0, sizeof(D3D9GLOBALS) );
	g_bAlphaKey = FALSE;

	g_D3D9.pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (!g_D3D9.pD3D9) return FALSE;

	/* Determine best back buffer format — prefer 32-bit, fall back to 16-bit */
	{
		D3DFORMAT fmtBB = D3DFMT_X8R8G8B8;
		D3DDISPLAYMODE currentMode;
		IDirect3D9_GetAdapterDisplayMode(g_D3D9.pD3D9, D3DADAPTER_DEFAULT, &currentMode);

		if (FAILED(IDirect3D9_CheckDeviceType(g_D3D9.pD3D9, D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL, currentMode.Format, D3DFMT_X8R8G8B8, !bFullscreen)))
		{
			if (FAILED(IDirect3D9_CheckDeviceType(g_D3D9.pD3D9, D3DADAPTER_DEFAULT,
					D3DDEVTYPE_HAL, currentMode.Format, D3DFMT_R5G6B5, !bFullscreen)))
			{
				fmtBB = currentMode.Format;
			}
			else
			{
				fmtBB = D3DFMT_R5G6B5;
			}
		}
		g_D3D9.backBufferFmt = fmtBB;
	}

	ZeroMemory(&g_D3D9.presentParams, sizeof(g_D3D9.presentParams));
	g_D3D9.presentParams.BackBufferWidth            = (UINT)pie_GetVideoBufferWidth();
	g_D3D9.presentParams.BackBufferHeight           = (UINT)pie_GetVideoBufferHeight();
	g_D3D9.presentParams.BackBufferFormat           = g_D3D9.backBufferFmt;
	g_D3D9.presentParams.BackBufferCount            = 1;
	g_D3D9.presentParams.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
	g_D3D9.presentParams.hDeviceWindow              = hWnd;
	g_D3D9.presentParams.Windowed                   = !bFullscreen;
	g_D3D9.presentParams.EnableAutoDepthStencil     = TRUE;
	g_D3D9.presentParams.AutoDepthStencilFormat     = D3DFMT_D16;
	g_D3D9.presentParams.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;

	g_D3D9.devType = D3DDEVTYPE_HAL;
	hr = IDirect3D9_CreateDevice(
		g_D3D9.pD3D9,
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&g_D3D9.presentParams,
		&g_D3D9.pDevice
	);
	if (FAILED(hr))
	{
		g_D3D9.devType = D3DDEVTYPE_REF;
		hr = IDirect3D9_CreateDevice(
			g_D3D9.pD3D9,
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_REF,
			hWnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&g_D3D9.presentParams,
			&g_D3D9.pDevice
		);
	}
	if (FAILED(hr))
	{
		IDirect3D9_Release(g_D3D9.pD3D9);
		g_D3D9.pD3D9 = NULL;
		return FALSE;
	}

	IDirect3DDevice9_GetDeviceCaps(g_D3D9.pDevice, &g_D3D9.caps);
	g_D3D9.bZBufferOn = TRUE;

	dtm_Initialise();
	D3D9_SetDefaultRenderStates();

	g_bAlphaKey = TRUE;
	IDirect3DDevice9_SetRenderState(g_D3D9.pDevice, D3DRS_ALPHAFUNC, D3DCMP_NOTEQUAL);
	IDirect3DDevice9_SetRenderState(g_D3D9.pDevice, D3DRS_ALPHAREF,  0x00000000);
	IDirect3DDevice9_SetRenderState(g_D3D9.pDevice, D3DRS_ALPHATESTENABLE, TRUE);

	return TRUE;
}

/***************************************************************************/

void
ShutDownD3D( void )
{
	dtm_ReleaseTextures();

	if ( g_D3D9.pDevice )
	{
		IDirect3DDevice9_Release( g_D3D9.pDevice );
		g_D3D9.pDevice = NULL;
	}
	if ( g_D3D9.pD3D9 )
	{
		IDirect3D9_Release( g_D3D9.pD3D9 );
		g_D3D9.pD3D9 = NULL;
	}
}

/***************************************************************************/

void
D3D9_UpdateViewport(UINT width, UINT height)
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

/***************************************************************************/

BOOL
D3D9_ChangeResolution(UINT width, UINT height, BOOL bFullscreen)
{
	HRESULT hr;

	if (!g_D3D9.pDevice) return FALSE;

	dtm_ReleaseTextures();

	g_D3D9.presentParams.BackBufferWidth  = width;
	g_D3D9.presentParams.BackBufferHeight = height;
	g_D3D9.presentParams.Windowed         = !bFullscreen;

	hr = IDirect3DDevice9_Reset(g_D3D9.pDevice, &g_D3D9.presentParams);
	if (FAILED(hr)) return FALSE;

	dtm_ReloadAllTextures();
	D3D9_SetDefaultRenderStates();

	pie_SetVideoBufferWidth(width);
	pie_SetVideoBufferHeight(height);

	D3D9_UpdateViewport(width, height);

	return TRUE;
}

/***************************************************************************/

void
BeginSceneD3D( void )
{
	HRESULT	hResult;

	if ( g_D3D9.bDeviceLost )
	{
		D3D9_HandleDeviceLost();
		return;
	}

	IDirect3DDevice9_Clear(g_D3D9.pDevice, 0, NULL,
		D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
		D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

	hResult = IDirect3DDevice9_BeginScene(g_D3D9.pDevice);
	if ( hResult != D3D_OK )
	{
		DBERROR( ("BeginSceneD3D: BeginScene failed: 0x%08X", hResult) );
	}
}

/***************************************************************************/

void
EndSceneD3D( void )
{
	HRESULT	hResult;

	IDirect3DDevice9_EndScene(g_D3D9.pDevice);

	hResult = IDirect3DDevice9_Present(g_D3D9.pDevice, NULL, NULL, NULL, NULL);
	if (hResult == D3DERR_DEVICELOST)
	{
		g_D3D9.bDeviceLost = TRUE;
	}
}

/***************************************************************************/
/*
 * lowest level poly draw
 */
/***************************************************************************/

void D3DDrawPoly( int nVerts, PIE_D3D9_VERTEX * psVert)
{
	HRESULT	hResult;

	IDirect3DDevice9_SetFVF(g_D3D9.pDevice, PIE_FVF_D3D9);

	if (nVerts >= 3)
	{
		hResult = IDirect3DDevice9_DrawPrimitiveUP(
					g_D3D9.pDevice,
					D3DPT_TRIANGLEFAN,
					(UINT)(nVerts - 2),
					(LPVOID)psVert,
					sizeof(PIE_D3D9_VERTEX) );
	}
	else if (nVerts == 2)
	{
		hResult = IDirect3DDevice9_DrawPrimitiveUP(
					g_D3D9.pDevice,
					D3DPT_LINELIST,
					1,
					(LPVOID)psVert,
					sizeof(PIE_D3D9_VERTEX) );
	}
	else
	{
		hResult = IDirect3DDevice9_DrawPrimitiveUP(
					g_D3D9.pDevice,
					D3DPT_POINTLIST,
					(UINT)nVerts,
					(LPVOID)psVert,
					sizeof(PIE_D3D9_VERTEX) );
	}

	(void)hResult;
}

/***************************************************************************/
/*
 * PIEVERTEX poly draw
 */
/***************************************************************************/

void
D3D_PIEPolygon( SDWORD numVerts, PIEVERTEX* pVrts)
{
	SDWORD    i;

	for(i = 0; i < numVerts; i++)
	{
		d3dVrts[i].sx = (float)pVrts[i].sx;
		d3dVrts[i].sy = (float)pVrts[i].sy;
		if (d3dVrts[i].sy > (float)LONG_TEST)
		{
			return;
		}
		d3dVrts[i].sz = (float)pVrts[i].sz * (float)INV_MAX_Z;
		d3dVrts[i].rhw = (float)1.0 / pVrts[i].sz;
		d3dVrts[i].tu = (float)pVrts[i].tu * (float)INV_TEX_SIZE + g_fTextureOffset;
		d3dVrts[i].tv = (float)pVrts[i].tv * (float)INV_TEX_SIZE + g_fTextureOffset;
		d3dVrts[i].color = pVrts[i].light.argb;
		d3dVrts[i].specular = pVrts[i].specular.argb;
	}

	D3DDrawPoly( numVerts, d3dVrts);
}

/***************************************************************************/

void
D3DSetTexelOffsetState( BOOL bOffsetOn )
{
	g_bTexelOffsetOn = bOffsetOn;
}

/***************************************************************************/

void
D3DSetTranslucencyMode( TRANSLUCENCY_MODE transMode )
{
	IDirect3DDevice9*	dev = g_D3D9.pDevice;
	static BOOL			bFirst = TRUE, bBlendEnableLast;
	BOOL				bBlendEnable;
	static D3DBLEND		srcBlendLast  = D3DBLEND_ZERO, destBlendLast = D3DBLEND_ZERO;
	D3DBLEND			srcBlend, destBlend;
	static DWORD		dwAlphaOpLast   = D3DTOP_DISABLE,
						dwAlphaArg1Last = (DWORD)-1,
						dwAlphaArg2Last = (DWORD)-1;
	DWORD				dwAlphaOp       = D3DTOP_DISABLE,
						dwAlphaArg1     = (DWORD)-1,
						dwAlphaArg2     = (DWORD)-1;

	switch (transMode)
	{
		case TRANS_ALPHA:
			srcBlend     = D3DBLEND_SRCALPHA;
			destBlend    = D3DBLEND_INVSRCALPHA;
			bBlendEnable = TRUE;
			dwAlphaOp   = D3DTOP_MODULATE;
			dwAlphaArg1 = D3DTA_TEXTURE;
			dwAlphaArg2 = D3DTA_DIFFUSE;
			break;
		case TRANS_ADDITIVE:
			srcBlend     = D3DBLEND_ONE;
			destBlend    = D3DBLEND_ONE;
			bBlendEnable = TRUE;
			dwAlphaOp    = D3DTOP_SELECTARG1;
			dwAlphaArg1  = D3DTA_DIFFUSE;
			break;
		case TRANS_FILTER:
			srcBlend     = D3DBLEND_SRCALPHA;
			destBlend    = D3DBLEND_SRCCOLOR;
			bBlendEnable = TRUE;
			dwAlphaOp    = D3DTOP_SELECTARG1;
			dwAlphaArg1  = D3DTA_DIFFUSE;
			break;
		default:
		case TRANS_DECAL:
			srcBlend     = D3DBLEND_ONE;
			destBlend    = D3DBLEND_ZERO;
			bBlendEnable = FALSE;
			dwAlphaOp    = D3DTOP_SELECTARG1;
			dwAlphaArg1  = D3DTA_TEXTURE;
			break;
	}

	if ( bFirst || (srcBlend != srcBlendLast) )
		IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, srcBlend);
	if ( bFirst || (destBlend != destBlendLast) )
		IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, destBlend);
	if ( bFirst || (bBlendEnable != bBlendEnableLast) )
		IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, bBlendEnable);
	if ( bFirst || (dwAlphaOp != dwAlphaOpLast) )
		IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, dwAlphaOp);
	if ( (bFirst || (dwAlphaArg1 != dwAlphaArg1Last)) && (dwAlphaArg1 != (DWORD)-1) )
		IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, dwAlphaArg1);
	if ( (bFirst || (dwAlphaArg2 != dwAlphaArg2Last)) && (dwAlphaArg2 != (DWORD)-1) )
		IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG2, dwAlphaArg2);

	if ( bFirst ) bFirst = FALSE;
	srcBlendLast    = srcBlend;
	destBlendLast   = destBlend;
	bBlendEnableLast = bBlendEnable;
	dwAlphaOpLast   = dwAlphaOp;
	dwAlphaArg1Last = dwAlphaArg1;
	dwAlphaArg2Last = dwAlphaArg2;
}

/***************************************************************************/

void D3DSetAlphaKey( BOOL bAlphaOn )       { g_bAlphaKey = bAlphaOn; }
BOOL D3DGetAlphaKey( void )                { return g_bAlphaKey; }

/***************************************************************************/

void
D3DSetColourKeying( BOOL bKeyingOn )
{
	IDirect3DDevice9_SetRenderState(g_D3D9.pDevice,
					D3DRS_ALPHATESTENABLE, bKeyingOn);
}

/***************************************************************************/

void D3DSetDepthBuffer( BOOL bDepthBufferOn )
{
	IDirect3DDevice9_SetRenderState(g_D3D9.pDevice,
				D3DRS_ZENABLE, bDepthBufferOn ? D3DZB_TRUE : D3DZB_FALSE);
}

void D3DSetDepthWrite( BOOL bWriteEnable )
{
	IDirect3DDevice9_SetRenderState(g_D3D9.pDevice,
				D3DRS_ZWRITEENABLE, bWriteEnable);
}

void D3DSetDepthCompare( D3DCMPFUNC depthCompare )
{
	IDirect3DDevice9_SetRenderState(g_D3D9.pDevice,
					D3DRS_ZFUNC, depthCompare);
}

/***************************************************************************/

void
D3DSetClipWindow(SDWORD xMin, SDWORD yMin, SDWORD xMax, SDWORD yMax)
{
	D3DVIEWPORT9 vp;
	vp.X      = (DWORD)xMin;
	vp.Y      = (DWORD)yMin;
	vp.Width  = (DWORD)(xMax - xMin);
	vp.Height = (DWORD)(yMax - yMin);
	vp.MinZ   = 0.0f;
	vp.MaxZ   = 1.0f;
	IDirect3DDevice9_SetViewport(g_D3D9.pDevice, &vp);
}

/***************************************************************************/

void
D3DReInit( void )
{
	if (g_D3D9.pDevice)
	{
		dtm_ReleaseTextures();
		IDirect3DDevice9_Reset(g_D3D9.pDevice, &g_D3D9.presentParams);
		dtm_Initialise();
		dtm_ReloadAllTextures();
		D3D9_SetDefaultRenderStates();
		D3DSetColourKeying( TRUE );
		dx6_SetBilinear( pie_GetBilinear() );
	}
}

/***************************************************************************/

void D3DTestCooperativeLevel( BOOL bGotFocus )
{
	HRESULT	hRes;
	static BOOL	bReInit = FALSE;

	if (!g_D3D9.pDevice) return;

	hRes = IDirect3DDevice9_TestCooperativeLevel(g_D3D9.pDevice);
	if ( hRes == D3DERR_DEVICELOST )
	{
		bReInit = TRUE;
	}
	else if ( hRes == D3DERR_DEVICENOTRESET && bReInit )
	{
		D3DReInit();
		bReInit = FALSE;
	}
	else if ( hRes == D3D_OK && bReInit )
	{
		bReInit = FALSE;
	}
}

/***************************************************************************/
