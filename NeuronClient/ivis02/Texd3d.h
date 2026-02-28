/***************************************************************************/

#ifndef _TEX3D_H_
#define _TEX3D_H_

/***************************************************************************/

#include <d3d9.h>

typedef enum TEXCOLOURDEPTH
{
	TCD_4BIT,			// 16 colour palette
	TCD_8BIT,			// 256 colour palette
	TCD_555RGB,			// 16 bit true colour using 5 bits for R,G and B
	TCD_565RGB,			// 16 bit true colour using 6 bits for G, but 5 for R and B
	TCD_24BIT,			// 24 bit true colour
	TCD_32BIT			// 32 bit true colour
}
TEXCOLOURDEPTH;

typedef struct TEXPAGE_D3D
{
	IDirect3DTexture9*		pTexture;		/* D3D9 managed texture		*/
	UWORD					iWidth;
	UWORD					iHeight;
	SWORD					widthShift;
	SWORD					heightShift;
	BOOL					bColourKeyed;
}
TEXPAGE_D3D;

/***************************************************************************/
extern TEXPAGE_D3D			*psRadarTexpage;
/***************************************************************************/

UWORD	NearestPowerOf2( UDWORD i );
UWORD	NearestPowerOf2withShift( UDWORD i, SWORD *shift );

/***************************************************************************/

#endif	/* _TEX3D_H_ */

/***************************************************************************/
