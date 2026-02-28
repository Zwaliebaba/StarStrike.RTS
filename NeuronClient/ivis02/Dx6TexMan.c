/***************************************************************************/
/*
 * Dx6TexMan.c
 *
 * D3D9 texture management system.
 * Replaces the DirectDraw/D3D3 texture manager.
 *
 */
/***************************************************************************/

#include "Frame.h"
#include "Piedef.h"
#include "PieState.h"
#include "PiePalette.h"
#include "D3drender.h"
#include "Dx6TexMan.h"
#include "Tex.h"


/***************************************************************************/
/*
 *	Local Definitions
 */
/***************************************************************************/

#define MAX_TEX_PAGES 32
#define RADAR_TEXPAGE_D3D 31
#define BIG_TEXTURE_SIZE 256
#define RADAR_SIZE 128

/***************************************************************************/
/*
 *	Local Variables
 */
/***************************************************************************/

static TEXPAGE_D3D	aTexPages[MAX_TEX_PAGES];
static TEXPAGE_D3D	radarTexPage;
TEXPAGE_D3D			*psRadarTexpage = &radarTexPage;

/***************************************************************************/
/*
 *	Source
 */
/***************************************************************************/

BOOL dtm_Initialise(void)
{
	SDWORD i;

	for (i = 0; i < MAX_TEX_PAGES; i++)
	{
		aTexPages[i].pTexture = NULL;
	}
	radarTexPage.pTexture = NULL;

	return TRUE;
}

/***************************************************************************/

BOOL dtm_ReleaseTextures(void)
{
	SDWORD i;

	for (i = 0; i < MAX_TEX_PAGES; i++)
	{
		if (aTexPages[i].pTexture != NULL)
		{
			IDirect3DTexture9_Release(aTexPages[i].pTexture);
			aTexPages[i].pTexture = NULL;
		}
	}

	if (radarTexPage.pTexture != NULL)
	{
		IDirect3DTexture9_Release(radarTexPage.pTexture);
		radarTexPage.pTexture = NULL;
	}

	return TRUE;
}

/***************************************************************************/

BOOL dtm_RestoreTextures(void)
{
	/* D3DPOOL_MANAGED textures survive device Reset() automatically */
	return TRUE;
}

/***************************************************************************/

BOOL dtm_ReLoadTexture(SDWORD i)
{
	if (i < 0 || i >= RADAR_TEXPAGE_D3D)
	{
		return FALSE;
	}
	if (_TEX_PAGE[i].tex.bmp == NULL)
	{
		return FALSE;
	}
	return dtm_LoadTexSurface(&_TEX_PAGE[i].tex, i);
}

/***************************************************************************/

BOOL dtm_ReloadAllTextures(void)
{
	SDWORD i;

	for (i = 0; i < MAX_TEX_PAGES; i++)
	{
		if ((_TEX_PAGE[i].tex.bmp != NULL) && (i != RADAR_TEXPAGE_D3D))
		{
			dtm_LoadTexSurface(&_TEX_PAGE[i].tex, i);
		}
	}
	return TRUE;
}

/***************************************************************************/
/*
 * dtm_LoadTexSurface
 *
 * Load an 8-bit palettized texture into a D3D9 A8R8G8B8 managed texture.
 * Converts palette indices to 32-bit ARGB at load time.
 * Color key index 0 is written with alpha = 0 (transparent).
 */
/***************************************************************************/

BOOL dtm_LoadTexSurface(iTexture *psIvisTex, SDWORD index)
{
	TEXPAGE_D3D		*pPage;
	iColour			*pPal;
	UBYTE			*pSrc;
	DWORD			*pDst;
	D3DLOCKED_RECT	lr;
	HRESULT			hr;
	UWORD			w, h, x, y;

	if (index < 0 || index >= MAX_TEX_PAGES)
	{
		return FALSE;
	}

	pPage = &aTexPages[index];
	w = (UWORD)psIvisTex->width;
	h = (UWORD)psIvisTex->height;

	/* Release old texture if reloading */
	if (pPage->pTexture != NULL)
	{
		IDirect3DTexture9_Release(pPage->pTexture);
		pPage->pTexture = NULL;
	}

	/* Create D3D9 managed texture in A8R8G8B8 format */
	hr = IDirect3DDevice9_CreateTexture(
		g_D3D9.pDevice,
		(UINT)NearestPowerOf2withShift(w, &pPage->widthShift),
		(UINT)NearestPowerOf2withShift(h, &pPage->heightShift),
		1,					/* mip levels */
		0,					/* usage: 0 for managed */
		D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED,
		&pPage->pTexture,
		NULL
	);
	if (FAILED(hr))
	{
		DBERROR(("dtm_LoadTexSurface: CreateTexture failed: 0x%08X", hr));
		return FALSE;
	}

	pPage->iWidth  = NearestPowerOf2(w);
	pPage->iHeight = NearestPowerOf2(h);
	pPage->bColourKeyed = psIvisTex->bColourKeyed;

	/* Lock && convert 8-bit palette indices to 32-bit ARGB */
	hr = IDirect3DTexture9_LockRect(pPage->pTexture, 0, &lr, NULL, 0);
	if (FAILED(hr))
	{
		DBERROR(("dtm_LoadTexSurface: LockRect failed: 0x%08X", hr));
		return FALSE;
	}

	pPal = pie_GetGamePal();
	pSrc = psIvisTex->bmp;

	for (y = 0; y < h; y++)
	{
		pDst = (DWORD*)((BYTE*)lr.pBits + y * lr.Pitch);
		for (x = 0; x < w; x++)
		{
			UBYTE idx = pSrc[y * w + x];
			UBYTE a   = (idx == 0) ? 0 : 255;  /* colour key on index 0 */
			pDst[x] = D3DCOLOR_ARGB(a, pPal[idx].r, pPal[idx].g, pPal[idx].b);
		}
	}

	IDirect3DTexture9_UnlockRect(pPage->pTexture, 0);

	return TRUE;
}

/***************************************************************************/

void dtm_SetTexturePage(SDWORD i)
{
	if (!g_D3D9.pDevice) return;

	if ((i >= 0) && (i < MAX_TEX_PAGES) && (aTexPages[i].pTexture != NULL))
	{
		IDirect3DDevice9_SetTexture(g_D3D9.pDevice, 0,
			(IDirect3DBaseTexture9*)aTexPages[i].pTexture);
	}
	else
	{
		IDirect3DDevice9_SetTexture(g_D3D9.pDevice, 0, NULL);
	}
}

/***************************************************************************/

BOOL dtm_LoadRadarSurface(BYTE* radarBuffer)
{
	iColour		*pPal;
	DWORD		*pDst;
	D3DLOCKED_RECT	lr;
	HRESULT		hr;
	UWORD		x, y;

	/* Release old radar texture if reloading */
	if (radarTexPage.pTexture != NULL)
	{
		IDirect3DTexture9_Release(radarTexPage.pTexture);
		radarTexPage.pTexture = NULL;
	}

	/* Create radar texture */
	hr = IDirect3DDevice9_CreateTexture(
		g_D3D9.pDevice,
		BIG_TEXTURE_SIZE, BIG_TEXTURE_SIZE,
		1, 0,
		D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED,
		&radarTexPage.pTexture,
		NULL
	);
	if (FAILED(hr))
	{
		DBERROR(("dtm_LoadRadarSurface: CreateTexture failed: 0x%08X", hr));
		return FALSE;
	}

	radarTexPage.iWidth  = BIG_TEXTURE_SIZE;
	radarTexPage.iHeight = BIG_TEXTURE_SIZE;
	NearestPowerOf2withShift(BIG_TEXTURE_SIZE, &radarTexPage.widthShift);
	NearestPowerOf2withShift(BIG_TEXTURE_SIZE, &radarTexPage.heightShift);
	radarTexPage.bColourKeyed = FALSE;

	/* Also store as the radar texpage slot */
	if (aTexPages[RADAR_TEXPAGE_D3D].pTexture != NULL)
	{
		IDirect3DTexture9_Release(aTexPages[RADAR_TEXPAGE_D3D].pTexture);
	}
	aTexPages[RADAR_TEXPAGE_D3D].pTexture = radarTexPage.pTexture;
	IDirect3DTexture9_AddRef(radarTexPage.pTexture);
	aTexPages[RADAR_TEXPAGE_D3D].iWidth  = radarTexPage.iWidth;
	aTexPages[RADAR_TEXPAGE_D3D].iHeight = radarTexPage.iHeight;
	aTexPages[RADAR_TEXPAGE_D3D].widthShift  = radarTexPage.widthShift;
	aTexPages[RADAR_TEXPAGE_D3D].heightShift = radarTexPage.heightShift;
	aTexPages[RADAR_TEXPAGE_D3D].bColourKeyed = FALSE;

	/* Lock && convert 8-bit radar data to ARGB */
	hr = IDirect3DTexture9_LockRect(radarTexPage.pTexture, 0, &lr, NULL, 0);
	if (FAILED(hr))
	{
		DBERROR(("dtm_LoadRadarSurface: LockRect failed: 0x%08X", hr));
		return FALSE;
	}

	pPal = pie_GetGamePal();
	for (y = 0; y < RADAR_SIZE; y++)
	{
		pDst = (DWORD*)((BYTE*)lr.pBits + y * lr.Pitch);
		for (x = 0; x < RADAR_SIZE; x++)
		{
			UBYTE idx = radarBuffer[y * RADAR_SIZE + x];
			pDst[x] = D3DCOLOR_ARGB(255, pPal[idx].r, pPal[idx].g, pPal[idx].b);
		}
	}

	IDirect3DTexture9_UnlockRect(radarTexPage.pTexture, 0);

	return TRUE;
}

/***************************************************************************/

SDWORD dtm_GetRadarTexImageSize(void)
{
	return RADAR_SIZE;
}

/***************************************************************************/

void dx6_SetBilinear(BOOL bBilinearOn)
{
	if (!g_D3D9.pDevice) return;

	if (bBilinearOn)
	{
		IDirect3DDevice9_SetSamplerState(g_D3D9.pDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice9_SetSamplerState(g_D3D9.pDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}
	else
	{
		IDirect3DDevice9_SetSamplerState(g_D3D9.pDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		IDirect3DDevice9_SetSamplerState(g_D3D9.pDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	}
}

/***************************************************************************/