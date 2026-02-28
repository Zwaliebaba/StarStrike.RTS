#pragma once

/*
 * dderror.h
 *
 * Convert direct draw error codes to an error string.
 *
 */

#pragma warning (disable : 4201 4214 4115 4514)
#include <windows.h>
#pragma warning (default : 4201 4214 4115)

#include "Types.h"

/* Turn a DD, D3D, || D3DRM error code into a char */
extern char *DDErrorToString(HRESULT error);

