/*
 * dderror.c
 *
 * Convert Direct Draw error numbers to strings.
 * NOTE: DD/D3D3 error codes are no longer used (D3D9 migration).
 * This function is retained for compilation only.
 */

#pragma warning (disable : 4201 4214 4115 4514)
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#pragma warning (default : 4201 4214 4115)

/* Allow frame header files to be singly included */
#define FRAME_LIB_INCLUDE

#include "Dderror.h"
#include "Types.h"

/*
 * DDErrorToString
 * 
 * Stub — DD/D3D3 error codes are no longer generated.
 * Returns a generic string for any HRESULT.
 */
char *DDErrorToString(HRESULT error)
{
    static char buf[64];
    if (error == 0) return "No error.\0";
    sprintf(buf, "DD/D3D error 0x%08X", (unsigned int)error);
    return buf;
}
