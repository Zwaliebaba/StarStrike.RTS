/***************************************************************************/
/*
 * pieTypes.h
 *
 * type defines for simple pies.
 *
 */
/***************************************************************************/

#ifndef _pieTypes_h
#define _pieTypes_h

#include "Frame.h"

/***************************************************************************/
/***************************************************************************/
/*
 *	Global Definitions
 */
/***************************************************************************/

#define PI 	  					3.141592654

/***************************************************************************/
/*
 *	Global Macros
 */
/***************************************************************************/

/***************************************************************************/
/*
 *	Global Type Definitions
 */
/***************************************************************************/
/* Integer types — aliases of Framework/Types.h types to avoid duplication */
typedef SBYTE  int8;
typedef SWORD  int16;
typedef SDWORD int32;
typedef UBYTE  uint8;
typedef UWORD  uint16;
typedef UDWORD uint32;


//
// Simple derived types
//

typedef struct {int left, top, right, bottom;} iClip;
typedef uint8 iBitmap;
typedef struct {uint8 r, g, b;} iColour;
typedef int iBool;
typedef struct {int32 x, y;} iPoint;
typedef struct {int width, height; iBitmap *bmp;} iSprite;
typedef iColour iPalette[256];
typedef struct {uint8 r, g, b, p;} iRGB8;
typedef struct {uint16 r, g, b, p;} iRGB16;
typedef struct {uint32 r, g, b, p;} iRGB32;
typedef struct {int8 x, y;} iPoint8;
typedef struct {int16 x, y;} iPoint16;
typedef struct {int32 x, y;} iPoint32;

	typedef struct {int32 x, y, z;} iVector;
	typedef struct {double x, y, z;} iVectorf;
	typedef struct {int xshift, width, height; iBitmap *bmp;
					iColour *pPal; iBool bColourKeyed; } iTexture;
	typedef struct {int32 x, y, z, u, v; uint8 g;} iVertex; 
typedef struct {FRACT x,y,z;} PIEVECTORF;
typedef struct {iVector p, r;} iView;

#endif // _pieTypes_h
