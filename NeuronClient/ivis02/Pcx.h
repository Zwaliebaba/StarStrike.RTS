#ifndef _pcx_
#define _pcx_

#include "Ivisdef.h"

extern BOOL iV_PCXLoad(char *file, iSprite *s, iColour *pal);
extern BOOL pie_PCXLoadToBuffer(char *file, iSprite *s, iColour *pal);
extern BOOL iV_PCXLoadMem(int8 *pcximge, iSprite *s, iColour *pal);
extern BOOL pie_PCXLoadMemToBuffer(int8 *pcximge, iSprite *s, iColour *pal);
//extern BOOL iV_PCXSave(char *file, iSprite *s, iColour *pal);

#endif /* _pcx_ */
