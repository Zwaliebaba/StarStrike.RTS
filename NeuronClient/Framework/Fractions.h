
// Routines to provide simple maths functions that work on both PSX & PC



// This file used to be in the deliverance src directory. But Jeremy quite correctly
// pointed out to me that it should be library based not deliverance based, and hence
// has now been moved to the lib\framework directory
//
// If you are reading this file from the deliverance source directory, please delete it now




// To multiply a FRACT by a integer just use the normal operator 
//   e.g.   FractValue2=FractValue*Interger;
//
// save is true of divide

#ifndef _FRACTIONS_
#define _FRACTIONS_

/* Check the header files have been included from frame.h if they
 * are used outside of the framework library.
 */
#if !defined(_frame_h) && !defined(FRAME_LIB_INCLUDE)
#error Framework header files MUST be included from Frame.h ONLY.
#endif

#include "Types.h"

#ifdef DEBUG

SDWORD PercentFunc(char *File,UDWORD Line,SDWORD a,SDWORD b);
SDWORD PerNumFunc(char *File,UDWORD Line,SDWORD range,SDWORD a,SDWORD b);

#define PERCENT(a,b) PercentFunc(__FILE__,__LINE__,a,b)
#define PERNUM(range,a,b) PerNumFunc(__FILE__,__LINE__,range,a,b)

#else

#define PERCENT(a,b) (((a)*100)/(b))
#define PERNUM(range,a,b) (((a)*range)/(b))

#endif


#include <math.h>

typedef float FRACT;
typedef float FRACT_D;

#define ROUND(x) ((x)>=0 ? (SDWORD)((x) + 0.5) : (SDWORD)((x) - 0.5))

#define FAST_FRACTS

#ifdef FAST_FRACTS

#define MAKEFRACT(x) ((FRACT)(x))
#define FRACTmul(x,y) ((x)*(y))
#define FRACTdiv(x,y) ((x)/(y))
#define FRACTmul_1(x,y) ((x)*(y))
#define FRACTdiv_1(x,y) ((x)/(y))
#define fSQRT(x) ((FRACT)sqrt(x))

// Jeremy ... the usual leg breaking rule aplies if you remove this again
#define iSQRT(x) ((FRACT)sqrt(x))


#define FRACTCONST(a,b) (((float)(a)) / ((float)(b)))
#define FRACTabs(a) ((float)fabs(a))

#define MAKEFRACT_D(x) ((FRACT_D)(x))
#define FRACTmul_D(x,y) ((x)*(y))
#define FRACTdiv_D(x,y) ((x)/(y))
#define fSQRT_D(x) ((FRACT)sqrt(x))

//#define MAKEINT_D(x) ((SDWORD)(x))
__inline SDWORD MAKEINT_D (float f)
{
	SDWORD i;
	__asm fld f;
	__asm fistp i;
	return i;
}

//changed definitions
//#define MAKEINT(x) ((SDWORD)(x))
//#define MAKEINT(x) (ftol(x))
__inline SDWORD MAKEINT (float f)
{
	SDWORD i;
	__asm fld f;
	__asm fistp i;
	return i;
}


//#define fastRoot(x,y) (sqrt(x * x + y * y))
#define fastRoot(x,y) ((abs(x) > abs(y)) ? (abs(x) + abs(y)/2) : (abs(y) + abs(x)/2))

//unused definitions
//#define iSQRT_D(x) ((UDWORD)sqrt((double)x))
//#define iSQRT(x) ((UDWORD)sqrt((double)x))

#else

#define MAKEFRACT(x) ((FRACT)(x))
#define FRACTmul(x,y) ((x)*(y))
#define FRACTdiv(x,y) ((x)/(y))
#define FRACTmul_1(x,y) ((x)*(y))
#define FRACTdiv_1(x,y) ((x)/(y))
#define fSQRT(x) ((FRACT)sqrt(x))

#define FRACTCONST(a,b) (((float)(a)) / ((float)(b)))
#define FRACTabs(a) ((float)fabs(a))

#define MAKEFRACT_D(x) ((FRACT_D)(x))
#define FRACTmul_D(x,y) ((x)*(y))
#define FRACTdiv_D(x,y) ((x)/(y))
#define fSQRT_D(x) ((FRACT)sqrt(x))
// Jeremy ... the usual leg breaking rule aplies if you remove this again
#define iSQRT(x) ((FRACT)sqrt(x))

#define MAKEINT(x) ((SDWORD)(x))
#define MAKEINT_D(x) ((SDWORD)(x))

#define fastRoot(x,y) (sqrt(x * x + y * y))

//#define iSQRT_D(x) ((UDWORD)sqrt((double)x))
//#define iSQRT(x) ((UDWORD)sqrt((double)x))

#endif



#endif
