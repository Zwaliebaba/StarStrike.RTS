#pragma once

/***************************************************************************/
/*
 * pieState.h
 *
 * render State controlr all pumpkin image library functions.
 *
 */
/***************************************************************************/



/***************************************************************************/

#include "Frame.h"
#include "Piedef.h"

/***************************************************************************/
/*
 *	Global Definitions
 */
/***************************************************************************/


/***************************************************************************/
/*
 *	Global Variables
 */
/***************************************************************************/


/***************************************************************************/
/*
 *	Global ProtoTypes
 */
/***************************************************************************/
extern BOOL pie_Download8bitTexturePage(void* bitmap,UWORD Width,UWORD Height);//assumes 256*256 page
extern BOOL pie_Reload8bitTexturePage(void* bitmap,UWORD Width,UWORD Height, SDWORD index);
extern UDWORD pie_GetLastPageDownloaded(void);
extern int pie_AddBMPtoTexPages( 	iSprite* s, char* filename, int type,
					BOOL bColourKeyed, BOOL bResource);
