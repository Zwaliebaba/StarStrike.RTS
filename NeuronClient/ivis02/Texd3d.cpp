/***************************************************************************/

#include <stdio.h>

#include "Frame.h"
#include "Tex.h"
#include "Rendmode.h"

#include "D3drender.h"
#include "Texd3d.h"

/***************************************************************************/
/*
 *	Source
 */
/***************************************************************************/

/***************************************************************************/
/*
 * NearestPowerOf2
 *
 * Calculates next power of 2 up from current value
 * (used because D3D textures need to be power of 2 wide && high).
 */
/***************************************************************************/

UWORD
NearestPowerOf2( UDWORD i)
{
	SWORD lShift = 0;

	while(i > (UDWORD)(1 << lShift))
	{
		lShift ++;
	}
	ASSERT (((lShift < 11),"NearestPowerOf2: value %i out of bounds\n", i));
	return (UWORD)(1 << lShift);
}

UWORD
NearestPowerOf2withShift( UDWORD i, SWORD *shift)
{
	SWORD lShift = 0;

	while(i > (UDWORD)(1 << lShift))
	{
		lShift ++;
	}
	*shift = lShift;
	ASSERT (((lShift < 11),"NearestPowerOf2: value %i out of bounds\n", i));
	return (UWORD)(1 << lShift);
}

/***************************************************************************/

