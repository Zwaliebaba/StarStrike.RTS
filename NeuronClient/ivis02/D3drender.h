/***************************************************************************/

#ifndef _D3DRENDER_H_
#define _D3DRENDER_H_

/***************************************************************************/

#include <d3d9.h>
#include "Frame.h"
#include "Texd3d.h"
#include "PieState.h"

/***************************************************************************/

#define RAMP_SIZE_D3D		64

#define D3D_TRI_COLOURKEY	0x0001
#define D3D_TRI_NOCULL		0x0002
#define D3D_TRI_ALPHABLEND	0x0004

typedef struct D3D9GLOBALS {
	IDirect3D9*         pD3D9;           /* Created by Direct3DCreate9()       */
	IDirect3DDevice9*   pDevice;         /* The rendering device               */
	D3DPRESENT_PARAMETERS presentParams; /* Swap chain configuration           */
	D3DDEVTYPE          devType;         /* D3DDEVTYPE_HAL or D3DDEVTYPE_REF   */
	D3DCAPS9            caps;            /* Device capabilities                */
	BOOL                bZBufferOn;
	BOOL                bDeviceLost;     /* Device-lost state flag             */
	BOOL                bWindowed;
	D3DFORMAT           backBufferFmt;   /* D3DFMT_R5G6B5 or D3DFMT_X8R8G8B8  */
} D3D9GLOBALS;

extern D3D9GLOBALS g_D3D9;

/***************************************************************************/

extern BOOL	InitD3D( HWND hWnd, BOOL bFullscreen );
extern void	ShutDownD3D( void );
extern void	BeginSceneD3D( void );
extern void	EndSceneD3D( void );
extern void D3D_PIEPolygon( SDWORD numVerts, PIEVERTEX* pVrts);
extern void D3DDrawPoly( int nVerts, PIE_D3D9_VERTEX * psVert);
extern void D3DSetAlphaBlending( BOOL bAlphaOn );
extern void D3DSetTranslucencyMode( TRANSLUCENCY_MODE transMode );

extern void	D3DSetColourKeying( BOOL bKeyingOn );
extern void D3DSetDepthBuffer( BOOL bDepthBufferOn );
extern void D3DSetDepthWrite( BOOL bWriteEnable );
extern void D3DSetDepthCompare( D3DCMPFUNC depthCompare );

extern void	D3DSetAlphaKey( BOOL bAlphaOn );
extern BOOL	D3DGetAlphaKey( void );

extern void	D3DSetTexelOffsetState( BOOL bOffsetOn );

extern void	D3DReInit( void );
extern void D3DTestCooperativeLevel( BOOL bGotFocus );

extern void D3DSetClipWindow(SDWORD xMin, SDWORD yMin, SDWORD xMax, SDWORD yMax);

extern void D3D9_SetDefaultRenderStates(void);

/***************************************************************************/

#endif	/* _D3DRENDER_H_ */

/***************************************************************************/
