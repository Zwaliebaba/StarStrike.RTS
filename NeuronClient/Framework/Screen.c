/*
 * Screen.c
 *
 * Basic double buffered display using direct draw.
 *
 */

#include <stdio.h>

#pragma warning (disable : 4201 4214 4115 4514)
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include "DdrawCompat.h"
#pragma warning (default : 4201 4214 4115)

#include "Frame.h"
#include "FrameInt.h"

/* Control Whether the back buffer is in system memory for full screen */
#define FULL_SCREEN_SYSTEM	TRUE

/* The Current screen size and bit depth */
UDWORD		screenWidth = 0;
UDWORD		screenHeight = 0;
UDWORD		screenDepth = 0;

/* The current screen mode (full screen/windowed) */
SCREEN_MODE		screenMode = SCREEN_WINDOWED;

/* Which mode (of operation) the library is running in */
DISPLAY_MODES	displayMode;

/* The handle for the main application window */
HANDLE		hWndMain = NULL;

/* The Direct Draw objects */
#define MAX_DDDEVICES 6
static	LPDIRECTDRAW		psDD1 = NULL;
LPDIRECTDRAW4		psDD = NULL;
GUID aDDDeviceGUID[MAX_DDDEVICES];
DDDEVICEIDENTIFIER aDDDeviceInfo[MAX_DDDEVICES];
DDDEVICEIDENTIFIER aDDHostInfo[MAX_DDDEVICES];
static numDevices = 0;
/* The Front and back buffers */
LPDIRECTDRAWSURFACE4	psFront = NULL;
/* The back buffer is not static to give a back door to display routines so
 * they can get at the back buffer directly.
 * Mainly did this to link in Sam's 3D engine.
 */
LPDIRECTDRAWSURFACE4	psBack = NULL;

/* The palette for palettised modes */
static LPDIRECTDRAWPALETTE	psPalette = NULL;

/* The actual palette entries for the display palette */
#define PAL_MAX				256
static PALETTEENTRY			asPalEntries[PAL_MAX];

/* The palette entries of the display palette converted to the current
 * windows true colour format - used in 8BITFUDGE mode
 */
static UDWORD				aWinPalette[PAL_MAX];

/* The bit depth at which it is assumed the mode is palettised */
#define PALETTISED_BITDEPTH   8

/* The number of bits in one colour gun of the windows PALETTEENTRY struct */
#define PALETTEENTRY_BITS 8

/* The window's clipper object */
static LPDIRECTDRAWCLIPPER	psClipper = NULL;

/* The Pixel format of the front buffer */
DDPIXELFORMAT		sFrontBufferPixelFormat;

/* The Pixel format of the back buffer */
DDPIXELFORMAT		sBackBufferPixelFormat;

/* Window's Pixel format */
DDPIXELFORMAT		sWinPixelFormat;

/* The size of the windows display mode */
//static UDWORD		winDispWidth, winDispHeight;

// The current flip state
FLIP_STATE	screenFlipState;

// The critical section for the screen flipping
CRITICAL_SECTION sScreenFlipCritical;

// The semaphore for the screen flipping
HANDLE	hScreenFlipSemaphore;

//backDrop
#define BACKDROP_WIDTH	640
#define BACKDROP_HEIGHT	480
UWORD*  pBackDropData = NULL;
BOOL    bBackDrop = FALSE;
BOOL    bUpload = FALSE;

//fog
DWORD	fogColour = 0;


typedef struct _win_pack
{
	SDWORD		rShift,rPalShift;
	SDWORD		gShift,gPalShift;
	SDWORD		bShift,bPalShift;
} WIN_PACK;
static WIN_PACK		sWinPack;

/* The current colour for line drawing */
static UDWORD				lineColour=0;

/* The current colour for text drawing */
static UDWORD				textColour=0;

/* The current fill colour */
static UDWORD				fillColour=0;

/* flag forcing buffers into video memory */
static BOOL					g_bVidMem;

static UDWORD	backDropWidth = BACKDROP_WIDTH;
static UDWORD	backDropHeight = BACKDROP_HEIGHT;


// ------------------------------------------------------------------
// We can't create a direct draw surface because Direct draw isn't
// initialised when Glide is running, so we have to use a DIB section
BITMAPINFO	systemAreaForGlide;
HBITMAP		systemBitmap;
void		*pSystemBitmap;


void	printTextToGlideArea( void )
{
HDC	glideHDC;

	glideHDC = GetDC(systemBitmap);

}


BOOL	makeSystemAreaForGlide(UDWORD width, UDWORD height)
{
UDWORD	*mask;
HDC		hdc;
	// Setup our surface - assumes always 16 bit
	systemAreaForGlide.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	systemAreaForGlide.bmiHeader.biWidth = width;
	systemAreaForGlide.bmiHeader.biHeight = height;
	systemAreaForGlide.bmiHeader.biPlanes = 1;
	systemAreaForGlide.bmiHeader.biBitCount = 16; // always for Glide for Warzone
	systemAreaForGlide.bmiHeader.biCompression = BI_BITFIELDS;
	systemAreaForGlide.bmiHeader.biSizeImage = 0;
	systemAreaForGlide.bmiHeader.biXPelsPerMeter = 0;
	systemAreaForGlide.bmiHeader.biYPelsPerMeter = 0;
	systemAreaForGlide.bmiHeader.biClrUsed = 0;
	systemAreaForGlide.bmiHeader.biClrImportant = 0;

	// Set up the pixel format masks
	mask = (UDWORD*)systemAreaForGlide.bmiColors;
	mask[0] = 0x7c00;
	mask[1] = 0x03e0;
	mask[2] = 0x001f;

	hdc = GetDC( (HWND)hWndMain);
	if(hdc = NULL)
	{
		DBERROR(("Can't get device context on the main window"));
		return(FALSE);
	}

	systemBitmap = CreateDIBSection(hdc,&systemAreaForGlide,DIB_RGB_COLORS,&pSystemBitmap,NULL,0);

	if(systemBitmap == NULL)
	{
		DBERROR(("Can't create the DIB section"));
		return(FALSE);
	}
	printTextToGlideArea();
	return(TRUE);
}

// End of new stuff for the system font printing on a Glide Surface
// ----------------------------------------------------------------


/*********************************************************/
/*********************************************************/
/*********************************************************/
/*********************************************************/
/*********************************************************/
// DX6 code

//-----------------------------------------------------------------------------
// Name: DDEnumCallbackEx()
// Desc: This callback gets the information for each device enumerated
//-----------------------------------------------------------------------------
BOOL WINAPI 
DDEnumCallbackEx(GUID *pGUID, LPSTR pDescription, LPSTR pName,
				 LPVOID pContext, HMONITOR hm)
{
	/* DD enumeration removed — D3D9 handles device selection */
	(void)pGUID; (void)pDescription; (void)pName; (void)pContext; (void)hm;
	return DDENUMRET_CANCEL;
}




//-----------------------------------------------------------------------------
// Name: DDEnumCallback()
// Desc: Old style callback retained for backwards compatibility
//-----------------------------------------------------------------------------
BOOL WINAPI 
DDEnumCallback(GUID *pGUID, LPSTR pDescription, LPSTR pName, LPVOID context)
{
    return (DDEnumCallbackEx(pGUID, pDescription, pName, context, NULL));
}




//-----------------------------------------------------------------------------
// Name: frame_DDEnumerate()
// Desc: Entry point to the program. Initializes everything and calls
//       DirectDrawEnumerateEx() to get all of the device info.
//-----------------------------------------------------------------------------
BOOL frameDDEnumerate(void)
{
	/* DD enumeration removed — D3D9 handles device selection */
	numDevices = 0;
	return TRUE;
}

SDWORD frameGetNumDDDevices(void)
{
	return numDevices;
}

char* frameGetDDDeviceName(SDWORD deviceId)
{
	return aDDDeviceInfo[numDevices].szDriver;
}

/*********************************************************/
/*********************************************************/
/*********************************************************/
/*********************************************************/
/*********************************************************/
/*********************************************************/

/* Get the pixel format for the Windowed display.
 * Only used at startup.
 */
static BOOL getWindowsPixelFormat(void)
{
	HRESULT				ddrval;
	DDSURFACEDESC2		ddsd;		// Direct Draw surface description
	UDWORD		currMask;

	ASSERT((psDD != NULL,
		"getWindowsPixelFormat: NULL Direct Draw object"));
	ASSERT(((psFront == NULL) &&
			(psBack == NULL) &&
			(psClipper == NULL),
		"getWindowsPixelFormat: DD objects have not been released"));

	/* Set the cooperative level - windowed */
	ddrval = psDD->lpVtbl->SetCooperativeLevel(
				psDD,
				hWndMain,
				DDSCL_NORMAL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Set cooperative level failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Get the Primary Surface. */
	memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	ddrval = psDD->lpVtbl->CreateSurface(
					psDD,
					&ddsd,
					&psFront,
					NULL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Create Primary Surface failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	//copy the palette
	memset(&sFrontBufferPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sFrontBufferPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddrval = psFront->lpVtbl->GetPixelFormat(
				psFront,
				&sFrontBufferPixelFormat);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get pixel format:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Get the size of the windows display */
	ddrval = psFront->lpVtbl->GetSurfaceDesc(psFront, &ddsd);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get primary surface description:\n%s",
			DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Get the pixel format of the front buffer */
	memset(&sWinPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sWinPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddrval = psFront->lpVtbl->GetPixelFormat(
				psFront,
				&sWinPixelFormat);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get pixel format:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Release the primary surface */
	RELEASE(psFront);

	/* Now calculate the colour packing if windows is in a true colour mode */
	if (sWinPixelFormat.dwRGBBitCount >= 16)
	{
		currMask = sWinPixelFormat.dwRBitMask;
		for(sWinPack.rShift=0; !(currMask & 1); currMask >>= 1, sWinPack.rShift++);
		for(sWinPack.rPalShift=0; (currMask & 1); currMask >>=1, sWinPack.rPalShift++);
		sWinPack.rPalShift = PALETTEENTRY_BITS - sWinPack.rPalShift;

		currMask = sWinPixelFormat.dwGBitMask;
		for(sWinPack.gShift=0; !(currMask & 1); currMask >>= 1, sWinPack.gShift++);
		for(sWinPack.gPalShift=0; (currMask & 1); currMask >>= 1, sWinPack.gPalShift++);
		sWinPack.gPalShift = PALETTEENTRY_BITS - sWinPack.gPalShift;

		currMask = sWinPixelFormat.dwBBitMask;
		for(sWinPack.bShift=0; !(currMask & 1); currMask >>= 1, sWinPack.bShift++);
		for(sWinPack.bPalShift=0; (currMask & 1); currMask >>= 1, sWinPack.bPalShift++);
		sWinPack.bPalShift = PALETTEENTRY_BITS - sWinPack.bPalShift;
	}

	return TRUE;
}


/* Convert the display palette to a set of true colour entries of
 * the same pixel format as the windows display
 */
static void updateWindowsPalette(UDWORD first, UDWORD count)
{
	UDWORD		i;

	if (sWinPixelFormat.dwRGBBitCount >= 16)
	{
		for(i=0; i<count; i++)
		{
			aWinPalette[i + first] =
				(asPalEntries[i+first].peRed >> sWinPack.rPalShift) << sWinPack.rShift;
			aWinPalette[i + first] |=
				(asPalEntries[i+first].peGreen >> sWinPack.gPalShift) << sWinPack.gShift;
			aWinPalette[i + first] |=
				(asPalEntries[i+first].peBlue >> sWinPack.bPalShift) << sWinPack.bShift;
		}
	}
}

/* Create the direct draw objects for a windowed display */
static BOOL createDDWindowed( void )
{
	HRESULT				ddrval;
	DDSURFACEDESC2		ddsd;		// Direct Draw surface description
	RECT				sWinSize;

	ASSERT((psDD != NULL,
		"createDDWindowed: NULL Direct Draw object"));
	ASSERT(((psFront == NULL) &&
			(psBack == NULL) &&
			(psClipper == NULL),
		"createDDWindowed: DD objects have not been released"));

	/* Set the cooperative level - windowed */
	ddrval = psDD->lpVtbl->SetCooperativeLevel(
				psDD,
				hWndMain,
				DDSCL_NORMAL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Set cooperative level failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Create the Primary Surface.
	 * Can't create a flipping structure as the surface is for a window.
	 * Instead we'll do a blt of the back buffer to the front buffer.
	 */
	memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	ddrval = psDD->lpVtbl->CreateSurface(
					psDD,
					&ddsd,
					&psFront,
					NULL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Create Primary Surface failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	//copy the palette
	memset(&sFrontBufferPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sFrontBufferPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddrval = psFront->lpVtbl->GetPixelFormat(
				psFront,
				&sFrontBufferPixelFormat);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get pixel format:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Get the pixel format of the front buffer */
	memset(&sWinPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sWinPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddrval = psFront->lpVtbl->GetPixelFormat(
				psFront,
				&sWinPixelFormat);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get pixel format:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}


	// Check the available bit depth and see what modes we can run in
	if ((screenDepth == PALETTISED_BITDEPTH) &&
		(sWinPixelFormat.dwRGBBitCount >= 16))
	{
		/*
		 * windowed 16+ bit front buffer 8 bit back buffer
		 */
		displayMode = MODE_8BITFUDGE;
	}
	else if (sWinPixelFormat.dwRGBBitCount != screenDepth)
	{
		DBERROR(("Windows bit depth is not set to the required format.\n"
				 "Application switching to full screen mode."));
		displayMode = MODE_FULLSCREEN;
		RELEASE(psFront);
		return FALSE;
	}
	else
	{
		//windowed 16 bit front buffer 16 bit back buffer
		displayMode = MODE_BOTH;
	}

	/* Create the back buffer */
	if (displayMode == MODE_8BITFUDGE)
	{
		memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
		ddsd.dwSize = sizeof(DDSURFACEDESC2);
		ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
		ddsd.dwWidth = screenWidth;
		ddsd.dwHeight = screenHeight;
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 8;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY |
							  DDSCAPS_3DDEVICE;
		ddrval = psDD->lpVtbl->CreateSurface(
					psDD,
					&ddsd,
					&psBack,
					NULL);
		if (ddrval != DD_OK)
		{
			DBERROR(("Create Back surface failed:\n%s", DDErrorToString(ddrval)));
			return FALSE;
		}
	}
	else
	{
		memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
		ddsd.dwSize = sizeof(DDSURFACEDESC2);
		ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
		ddsd.dwWidth = screenWidth;
		ddsd.dwHeight = screenHeight;
		ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
		ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
		/* Force the back buffer into system memory for a debug build */
#ifdef DEBUG
		if ( !g_bVidMem )
		{
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		}
#endif
		ddrval = psDD->lpVtbl->CreateSurface(
					psDD,
					&ddsd,
					&psBack,
					NULL);
		if (ddrval != DD_OK)
		{
			DBERROR(("Create Back surface failed:\n%s", DDErrorToString(ddrval)));
			return FALSE;
		}
	}
	
	/* Create Clippers for the front surface */
	ddrval = psDD->lpVtbl->CreateClipper(
					psDD,
					0,
					&psClipper,
					NULL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Failed to create clipper:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}
	ddrval = psClipper->lpVtbl->SetHWnd(
					psClipper,
					0,
					hWndMain);
	if (ddrval != DD_OK)
	{
		DBERROR(("SetHWnd failed for clipper:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}
	ddrval = psFront->lpVtbl->SetClipper(
					psFront,
					psClipper);
	if (ddrval != DD_OK)
	{
		DBERROR(("Failed to set clipper for front buffer:\n%s",
				 DDErrorToString(ddrval)));
		return FALSE;
	}

	/* If we are in a palettised mode, create a palette */
	if (screenDepth == PALETTISED_BITDEPTH)
	{
		/* Create the palette from the stored palette entries */
		ddrval = psDD->lpVtbl->CreatePalette(
						psDD,
						DDPCAPS_8BIT,
						asPalEntries,
						&psPalette,
						NULL);
		if (ddrval != DD_OK)
		{
			DBERROR(("Failed to create palette:\n%s", DDErrorToString(ddrval)));
			return FALSE;
		}

		/* Assign the palette to the front buffer unless we're 8bit fudging */
		if (displayMode != MODE_8BITFUDGE)
		{
			ddrval = psFront->lpVtbl->SetPalette(psFront, psPalette);
			if (ddrval != DD_OK)
			{
				DBERROR(("Couldn't set palette for front buffer:\n%s",
							DDErrorToString(ddrval)));
			}
		}

		/* Assign the palette to the back buffer */
		ddrval = psBack->lpVtbl->SetPalette(psBack, psPalette);
		if (ddrval != DD_OK)
		{
			DBERROR(("Couldn't set palette for back buffer:\n%s",
						DDErrorToString(ddrval)));
		}
	}

	/* Reset the style of the window to have title bars, etc. */
	(void)SetWindowLong(hWndMain, GWL_STYLE, WIN_STYLE);
	(void)SetWindowLong(hWndMain, GWL_EXSTYLE, WIN_EXSTYLE);

	/* Get the actual size of window we want (including the size of
	   title bars etc.) */
	(void)SetRect(&sWinSize, 0, 0, screenWidth, screenHeight);
	(void)AdjustWindowRectEx(&sWinSize,
					   WIN_STYLE,
                       FALSE,
                       WIN_EXSTYLE);

	/* The rectangle returned has values for the window edges relative to
	   the display area origin, i.e. left and top are negative - so we have
	   to adjust */
	sWinSize.right -= sWinSize.left;
	sWinSize.left = 0;
	sWinSize.bottom -= sWinSize.top;
	sWinSize.top = 0;
	
	/* Set the window size.
	 * Ripped this out of the D3D example code - not too sure why we
	 * have to do it twice.  I've no wish to become a windows programmer
	 * so if it works why worry :-)
	 */
	(void)SetWindowPos(hWndMain, NULL,
				 0, 0,
				 sWinSize.right, sWinSize.bottom,
				 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	(void)SetWindowPos(hWndMain, HWND_NOTOPMOST,
				 0, 0, 0, 0,
				 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS);

	return TRUE;
}


/* Release the DD objects for a windowed display */
static BOOL releaseDDWindowed(void)
{
	RELEASE(psPalette);
	RELEASE(psClipper);
	RELEASE(psFront);
	RELEASE(psBack);

	return TRUE;
}


/* Create the DD objects for a full screen display */
static BOOL createDDFullScreen( void )
{
	HRESULT				ddrval;
	DDSURFACEDESC2		ddsd;		// Direct Draw surface description
#if !FULL_SCREEN_SYSTEM
	DDSCAPS				ddscaps;
#endif

	ASSERT((psDD != NULL,
		"createDDFullScreen: No Direct Draw Object"));
	ASSERT(((psFront == NULL) &&
			(psBack == NULL) &&
			(psClipper == NULL),
		"createDDFullScreen: DD objects have not been released"));

	/* Make the app window completely undecorated so GDI is
	 * effectively shut out.
	 */
	(void)SetWindowLong(hWndMain, GWL_STYLE, 0);
	(void)SetWindowLong(hWndMain, GWL_EXSTYLE, 0);

	/* Set the cooperative level - exclusive */
	ddrval = psDD->lpVtbl->SetCooperativeLevel(
				psDD,
				hWndMain,
				DDSCL_EXCLUSIVE |
				DDSCL_FULLSCREEN);
	if (ddrval != DD_OK)
	{
		DBERROR(("Set cooperative level failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Set the display mode */
	ddrval = psDD->lpVtbl->SetDisplayMode(
				psDD,
				screenWidth, screenHeight, screenDepth,
				0,0);		// Set these so the DD1 version is used
	if (ddrval != DD_OK)
	{
		DBERROR(("Set display mode failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

#if FULL_SCREEN_SYSTEM
	/* Create the Primary Surface. */
	memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	ddrval = psDD->lpVtbl->CreateSurface(
					psDD,
					&ddsd,
					&psFront,
					NULL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Create Primary Surface failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	//copy the palette
	memset(&sFrontBufferPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sFrontBufferPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddrval = psFront->lpVtbl->GetPixelFormat(
				psFront,
				&sFrontBufferPixelFormat);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get pixel format:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Create the back buffer */
	memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	ddsd.dwWidth = screenWidth;
	ddsd.dwHeight = screenHeight;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
	/* Force the back buffer into system memory unless flag set */
	if ( !g_bVidMem )
	{
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
	}
	ddrval = psDD->lpVtbl->CreateSurface(
				psDD,
				&ddsd,
				&psBack,
				NULL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Create Back surface failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}
#else
	/* Create the Primary Surface. */
	memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;

	ddsd.dwBackBufferCount = 1;	// Use 2 for triple buffering, 1 for double

	ddrval = psDD->lpVtbl->CreateSurface(
					psDD,
					&ddsd,
					&psFront,
					NULL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Create Primary Surface failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	//copy the palette
	memset(&sFrontBufferPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sFrontBufferPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddrval = psFront->lpVtbl->GetPixelFormat(
				psFront,
				&sFrontBufferPixelFormat);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get pixel format:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Get the back buffer */
	memset(&ddscaps, 0, sizeof(DDSCAPS));
	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	ddrval = psFront->lpVtbl->GetAttachedSurface(
				psFront,
				&ddscaps,
				&psBack);
	if (ddrval != DD_OK)
	{
		DBERROR(("Get Back surface failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}
#endif

	/* Get the pixel format of the front buffer */
	memset(&sWinPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sWinPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddrval = psFront->lpVtbl->GetPixelFormat(
				psFront,
				&sWinPixelFormat);
	if (ddrval != DD_OK)
	{
		DBERROR(("Couldn't get pixel format:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}

	/* If we are in a palettised mode, create a palette */
	if (screenDepth == PALETTISED_BITDEPTH)
	{
		/* Create the palette from the stored palette entries */
		ddrval = psDD->lpVtbl->CreatePalette(
						psDD,
						DDPCAPS_8BIT,
						asPalEntries,
						&psPalette,
						NULL);
		if (ddrval != DD_OK)
		{
			DBERROR(("Failed to create palette:\n%s", DDErrorToString(ddrval)));
			return FALSE;
		}

		/* Assign the palette to the front buffer */
		ddrval = psFront->lpVtbl->SetPalette(psFront, psPalette);
		if (ddrval != DD_OK)
		{
			DBERROR(("Couldn't set palette for front buffer:\n%s",
						DDErrorToString(ddrval)));
		}

		/* Assign the palette to the back buffer */
		ddrval = psFront->lpVtbl->SetPalette(psBack, psPalette);
		if (ddrval != DD_OK)
		{
			DBERROR(("Couldn't set palette for back buffer:\n%s",
						DDErrorToString(ddrval)));
		}
	}

	return TRUE;
}


/* Release the DD objects for a full screen display */
static BOOL releaseDDFullScreen(void)
{
	HRESULT		ddrval;

	if (psDD == NULL)
	{
		return TRUE;
	}

	ASSERT((screenMode == SCREEN_FULLSCREEN,
		"releaseDDFullScreen: Attempt to release when not in full screen mode"));
	ASSERT((psClipper == NULL,
		"releaseDDFullScreen: Clipper object not released"));

	/* Clear up exclusive mode */
	screenFlipToGDI();
	ddrval = psDD->lpVtbl->RestoreDisplayMode(psDD);
	if (ddrval != DD_OK)
	{
		DBERROR(("RestoreDisplayMode failed:\n%s",
					DDErrorToString(ddrval)));
		return FALSE;
	}

	/* Set the cooperative level - windowed */
	ddrval = psDD->lpVtbl->SetCooperativeLevel(
				psDD,
				hWndMain,
				DDSCL_NORMAL);
	if (ddrval != DD_OK)
	{
		DBERROR(("Set cooperative level failed:\n%s", DDErrorToString(ddrval)));
		return FALSE;
	}

	RELEASE(psPalette);
	RELEASE(psBack);
	RELEASE(psFront);

	return TRUE;
}


/* get screen window handle */
HWND screenGetHWnd( void )
{
	return hWndMain;
}

BOOL screenInitialiseGlide(UDWORD	width, UDWORD height, HANDLE hWindow)
{
	/* Store the screen information */
	screenWidth = width;
	screenHeight = height;
	screenMode = SCREEN_FULLSCREEN;

//	makeSystemAreaForGlide(width,height);

//	hWndMain = hWindow;
//	(void)SetWindowLong(hWndMain, GWL_STYLE, 0);
//	(void)SetWindowLong(hWndMain, GWL_EXSTYLE, 0);
	return(TRUE);
}

/* Initialise the double buffered display */
BOOL screenInitialise(UDWORD		width,			// Display width
					  UDWORD		height,			// Display height
					  UDWORD		bitDepth,		// Display bit depth
					  BOOL			fullScreen,		// Whether to start windowed
													// or full screen.
					  BOOL			bVidMem,		// Whether to put surfaces in
													// video memory
					  BOOL			bDDraw,			// Whether to create ddraw surfaces												// video memory
					  HANDLE		hWindow)		// The main windows handle
{
	HRESULT				ddrval;
	DDSURFACEDESC2		sSurfDesc;
	BOOL				modeAvailable = FALSE;
	UDWORD				i,j,k, index;
//	DDSURFACEDESC		ddsd;		// Direct Draw surface description

	/* Store the screen information */
	screenWidth = width;
	screenHeight = height;
	screenDepth = bitDepth;
//	screenRequested = fullScreen;

	hWndMain = hWindow;

	/* store vidmem flag */
	g_bVidMem = bVidMem;


	/* DD object creation removed — D3D9 handles device init in D3drender.c */
	frameDDEnumerate();
	psDD1 = NULL;
	psDD = NULL;

	/* Hardcode pixel format to R5G6B5 (16-bit) for compatibility —
	 * the D3D9 back buffer format is set in InitD3D() */
	memset(&sFrontBufferPixelFormat, 0, sizeof(DDPIXELFORMAT));
	sFrontBufferPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	sFrontBufferPixelFormat.dwFlags = DDPF_RGB;
	sFrontBufferPixelFormat.dwRGBBitCount = 16;
	sFrontBufferPixelFormat.dwRBitMask = 0xF800;
	sFrontBufferPixelFormat.dwGBitMask = 0x07E0;
	sFrontBufferPixelFormat.dwBBitMask = 0x001F;
	memcpy(&sBackBufferPixelFormat, &sFrontBufferPixelFormat, sizeof(DDPIXELFORMAT));
	memcpy(&sWinPixelFormat, &sFrontBufferPixelFormat, sizeof(DDPIXELFORMAT));

	displayMode = MODE_BOTH;




	/* If we're going to run in a palettised mode initialise the palette */
	if (bitDepth == PALETTISED_BITDEPTH)
	{
		memset(asPalEntries, 0, sizeof(PALETTEENTRY) * PAL_MAX);

		/* Bash in a range of colours */
		for(i=0; i<=4; i++)
		{
			for(j=0; j<=4; j++)
			{
				for(k=0; k<=4; k++)
				{
					index = i*25 + j*5 + k;
					asPalEntries[index].peRed = (UBYTE)(i*63);
					asPalEntries[index].peGreen = (UBYTE)(j*63);
					asPalEntries[index].peBlue = (UBYTE)(k*63);
				}
			}
		}

		/* Fill in a grey scale */
		for(i=0; i<64; i++)
		{
			asPalEntries[i+125].peRed = (UBYTE)(i<<2);
			asPalEntries[i+125].peGreen = (UBYTE)(i<<2);
			asPalEntries[i+125].peBlue = (UBYTE)(i<<2);
		}

		/* Colour 0 is always black */
		asPalEntries[0].peRed = 0;
		asPalEntries[0].peGreen = 0;
		asPalEntries[0].peBlue = 0;

		/* Colour 255 is always white */
		asPalEntries[255].peRed = 0xff;
		asPalEntries[255].peGreen = 0xff;
		asPalEntries[255].peBlue = 0xff;

		updateWindowsPalette(0, 256);
	}

	/* DD surface creation removed — D3D9 owns the swap chain.
	 * Just set the screen mode and initialise the flip synchronisation. */
	if (fullScreen)
	{
		screenMode = SCREEN_FULLSCREEN;
	}
	else
	{
		screenMode = SCREEN_WINDOWED;

		/* Set windowed style so the window has title bars etc. */
		(void)SetWindowLong(hWndMain, GWL_STYLE, WIN_STYLE);
		(void)SetWindowLong(hWndMain, GWL_EXSTYLE, WIN_EXSTYLE);
	}

	/* psFront/psBack are NULL — DD surfaces not created */
	psFront = NULL;
	psBack  = NULL;

	screenFlipState = FLIP_IDLE;
	InitializeCriticalSection(&sScreenFlipCritical);
	hScreenFlipSemaphore = CreateSemaphore(NULL,0,1,NULL);
	if (hScreenFlipSemaphore == NULL)
	{
		DBERROR(("Couldn't create flip semaphore"));
		return FALSE;
	}

	return TRUE;
}


/* Release resources */
void screenShutDown(void)
{
	/* DD surfaces are NULL — nothing to release */
	DeleteCriticalSection(&sScreenFlipCritical);
	CloseHandle(hScreenFlipSemaphore);

	RELEASE(psDD);
	RELEASE(psDD1);
}

BOOL screenReInit( void )
{
	BOOL	bFullScreen;

	if ( screenMode == SCREEN_FULLSCREEN )
	{
		bFullScreen = TRUE;
	}
	else
	{
		bFullScreen = FALSE;
	}

	screenShutDown();

	return screenInitialise( screenWidth, screenHeight, screenDepth, bFullScreen,
								g_bVidMem, TRUE, hWndMain );
}

/* Return a pointer to the Direct Draw 2 object */
LPDIRECTDRAW4 screenGetDDObject(void)
{
	return psDD;
}


/* Return a pointer to the Direct Draw back buffer surface */
LPDIRECTDRAWSURFACE4 screenGetSurface(void)
{
	return psBack;
}

/* Return a pointer to the Front buffer pixel format */
DDPIXELFORMAT *screenGetFrontBufferPixelFormat(void)
{
	if (psDD)
	{
		return &sFrontBufferPixelFormat;
	}
	else
	{
		return NULL;
	}
}

/* Return a pointer to the back buffer pixel format */
DDPIXELFORMAT *screenGetBackBufferPixelFormat(void)
{
	if (psDD)
	{
		return &sBackBufferPixelFormat;
	}
	else
	{
		return NULL;
	}
}

/*
 * screenRestoreSurfaces
 *
 * Restore the direct draw surfaces if they have been lost.
 *
 * This is only used internally within the library.
 */
void screenRestoreSurfaces(void)
{
	/* DD surface restore removed — D3D9 handles device-lost via Reset() */
}

void screen_SetBackDrop(UWORD* newBackDropBmp, UDWORD width, UDWORD height)
{
	bBackDrop = TRUE;
	pBackDropData = newBackDropBmp;
	backDropWidth = width;
	backDropHeight = height;
}

void screen_StopBackDrop(void)
{
	bBackDrop = FALSE;
}

void screen_RestartBackDrop(void)
{
	bBackDrop = TRUE;
}

UWORD* screen_GetBackDrop(void)
{
	if (bBackDrop == TRUE)
	{
		return pBackDropData;
	}
	return NULL;
}

UDWORD screen_GetBackDropWidth(void)
{
	if (bBackDrop == TRUE)
	{
		return backDropWidth;
	}
	return 0;
}

void screen_Upload(UWORD* newBackDropBmp)
{
	/* DD back buffer lock removed — no DD surfaces to upload from */
	(void)newBackDropBmp;
}

void screen_SetFogColour(UDWORD newFogColour)
{
	UDWORD red, green, blue;
	static UDWORD currentFogColour = 0;

	if (newFogColour != currentFogColour)
	{

		if (sBackBufferPixelFormat.dwRGBBitCount == 16)//only set in 16 bit modes
		{
			if (sBackBufferPixelFormat.dwGBitMask == 0x07e0)//565
			{
				red = newFogColour >> 8;
				red &= sBackBufferPixelFormat.dwRBitMask;
				green = newFogColour >> 5;
				green &= sBackBufferPixelFormat.dwGBitMask;
				blue = newFogColour >> 3;
				blue &= sBackBufferPixelFormat.dwBBitMask;
				fogColour = red + green + blue;
			}		
			else if (sBackBufferPixelFormat.dwGBitMask == 0x03e0)//555
			{
				red = newFogColour >> 9;
				red &= sBackBufferPixelFormat.dwRBitMask;
				green = newFogColour >> 6;
				green &= sBackBufferPixelFormat.dwGBitMask;
				blue = newFogColour >> 3;
				blue &= sBackBufferPixelFormat.dwBBitMask;
				fogColour = red + green + blue;
			}		
		}
		currentFogColour = newFogColour;
	}
	return;
}

/* Flip back and front buffers */
//always clears or renders backdrop
void screenFlip(BOOL clearBackBuffer)
{
	/* DD flip removed — D3D9 Present is handled by EndSceneD3D() in D3drender.c.
	 * This function now only maintains the flip synchronisation state
	 * that Cursor.c relies on. */

	EnterCriticalSection(&sScreenFlipCritical);
	screenFlipState = FLIP_STARTED;

	/* No DD surfaces to flip — D3D9 already presented the frame */

	screenFlipState = FLIP_FINISHED;

	LeaveCriticalSection(&sScreenFlipCritical);
	ReleaseSemaphore(hScreenFlipSemaphore,1,NULL);
}

/* Swap between windowed and full screen mode */
void screenToggleMode(void)
{
	/* DD mode toggling removed — D3D9 handles mode changes via device Reset */
	/* Toggling between windowed/fullscreen requires D3D9 device reset — not yet implemented */
}

/* Swap between windowed and full screen mode */
BOOL screenToggleVideoPlaybackMode(void)
{
	/* DD video playback mode toggling removed — D3D9 handles resolution via Reset */
	return TRUE;
}

SCREEN_MODE screenGetMode( void )
{
	return screenMode;
}

/* Set screen mode */
void screenSetMode(SCREEN_MODE mode)
{
	// disable this if there isn't any direct draw
	if (psDD == NULL)
	{
		return;
	}

	/* If the mode is the same as the current one, don't have to do
	 * anything.  Otherwise toggle the mode.
	 */
	if (mode != screenMode)
	{
		screenToggleMode();
	}
}

/* In full screen mode flip to the GDI buffer.
 * Use this if you want the user to see any GDI output.
 * This is mainly used so that ASSERTs and message boxes appear
 * even in full screen mode.
 */
void screenFlipToGDI(void)
{
	/* DD FlipToGDI removed — D3D9 windowed mode handles this automatically */
}

/* Set palette entries for the display buffer
 * first specifies the first palette entry. count the number of entries
 * The psPalette should have at least first + count entries in it.
 */
void screenSetPalette(UDWORD first, UDWORD count, PALETTEENTRY *psEntries)
{
	HRESULT					ddrval;
/*	LPDIRECTDRAWPALETTE		psTmpPal;
	PALETTEENTRY			aTmpEntries[PAL_MAX];*/

	ASSERT(((first+count-1 < PAL_MAX),
		"screenSetPalette: invalid entry range"));

	if (count == 0)
	{
		return;
	}

	/* ensure that colour 0 is black and 255 is white */
	if ((first == 0 || first == 255) && count == 1)
	{
		return;
	}

	if (first == 0)
	{
		first = 1;
		count -= 1;
	}
	if (first + count - 1 == PAL_MAX)
	{
		count -= 1;
	}

	memcpy(asPalEntries+first, psEntries+first,
			sizeof(PALETTEENTRY)*count);

	if (sBackBufferPixelFormat.dwRGBBitCount == 8 && psPalette != NULL) // palettised mode
	{
		ddrval = psPalette->lpVtbl->SetEntries(psPalette, 0, 0,PAL_MAX, asPalEntries);
		if (ddrval != DD_OK)
		{
			DBERROR(("Couldn't set palette entries:\n%s", DDErrorToString(ddrval)));
		}

		/* Update the true colour version of the palette for the windowed display */
		updateWindowsPalette(first, count);
	}
/* Some testing code to see what the palettes get set to - not really needed
	// Assign the palette to the front buffer
	if (displayMode != MODE_8BITFUDGE ||
		(displayMode == MODE_8BITFUDGE && screenMode == SCREEN_FULLSCREEN))
	{
		ddrval = psFront->lpVtbl->SetPalette(psFront, psPalette);
		if (ddrval != DD_OK)
		{
			DBERROR(("Couldn't set palette for front buffer:\n%s",
						DDErrorToString(ddrval)));
		}
	}

	// Get the palette of the front buffer 
	if (displayMode != MODE_8BITFUDGE ||
		(displayMode == MODE_8BITFUDGE && screenMode == SCREEN_FULLSCREEN))
	{
		ddrval = psFront->lpVtbl->GetPalette(psFront, &psTmpPal);
		if (ddrval != DD_OK)
		{
			DBERROR(("Couldn't get palette:\n%s", DDErrorToString(ddrval)));
		}
		ddrval = psTmpPal->lpVtbl->GetEntries(psTmpPal, 0,0, PAL_MAX, aTmpEntries);
		if (ddrval != DD_OK)
		{
			DBERROR(("Couldn't get palette entries:\n%s", DDErrorToString(ddrval)));
		}
	}*/
}

/* Return the best colour match when in a palettised mode */
UBYTE screenGetPalEntry(UBYTE red, UBYTE green, UBYTE blue)
{
	UDWORD	i, minDist, dist;
	UDWORD	redDiff,greenDiff,blueDiff;
	UBYTE	colour;

	ASSERT((sBackBufferPixelFormat.dwRGBBitCount == 8,
		"screenSetPalette: not in a palettised mode"));

	minDist = 0xff*0xff*0xff;
	colour = 0;
	for(i = 0; i < PAL_MAX; i++)
	{
		redDiff = asPalEntries[i].peRed - red;
		greenDiff = asPalEntries[i].peGreen - green;
		blueDiff = asPalEntries[i].peBlue - blue;
		dist = redDiff*redDiff + greenDiff*greenDiff + blueDiff*blueDiff;
		if (dist < minDist)
		{
			minDist = dist;
			colour = (UBYTE)i;
		}
		if (minDist == 0)
		{
			break;
		}
	}

	return colour;
}


/* Set the colour for text */
void screenSetTextColour(UBYTE red, UBYTE green, UBYTE blue)
{
	SDWORD		rShift,rPalShift;
	SDWORD		gShift,gPalShift;
	SDWORD		bShift,bPalShift;
	UDWORD		currMask;

	if (sBackBufferPixelFormat.dwRGBBitCount >= 16)
	{
		currMask = sBackBufferPixelFormat.dwRBitMask;
		for(rShift=0; !(currMask & 1); currMask >>= 1, rShift++);
		for(rPalShift=0; (currMask & 1); currMask >>=1, rPalShift++);
		rPalShift = PALETTEENTRY_BITS - rPalShift;

		currMask = sBackBufferPixelFormat.dwGBitMask;
		for(gShift=0; !(currMask & 1); currMask >>= 1, gShift++);
		for(gPalShift=0; (currMask & 1); currMask >>= 1, gPalShift++);
		gPalShift = PALETTEENTRY_BITS - gPalShift;

		currMask = sBackBufferPixelFormat.dwBBitMask;
		for(bShift=0; !(currMask & 1); currMask >>= 1, bShift++);
		for(bPalShift=0; (currMask & 1); currMask >>= 1, bPalShift++);
		bPalShift = PALETTEENTRY_BITS - bPalShift;
	}

	switch (sBackBufferPixelFormat.dwRGBBitCount)
	{
	case 8:
		textColour = screenGetPalEntry(red,green,blue);
		break;
	case 16:
	case 24:
	case 32:
		textColour = (red >> rPalShift) << rShift;
		textColour |= (green >> gPalShift) << gShift;
		textColour |= (blue >> bPalShift) << bShift;
		break;
	default:
		ASSERT((FALSE,"Unknown display pixel format"));
		break;
	}
}


/* Output text to the display screen at location x,y.
 * The remaining arguments are as printf.
 */
void screenTextOut(UDWORD x, UDWORD y, STRING *pFormat, ...)
{
	HRESULT		ddrval;
	STRING		aTxtBuff[1024];
	va_list		pArgs;
	DDSURFACEDESC2	sDDSD;
	UDWORD		strLen;
	UBYTE		*pSrc,*pDest,font;
	UWORD		*p16Dest;
	UDWORD		px,py;

	if (psBack == NULL) return;

	va_start(pArgs, pFormat);
	vsprintf(aTxtBuff, pFormat, pArgs);

	/* See if the string is offscreen */
	if ((y < 0) || (y >= screenHeight - FONT_HEIGHT))
	{
		return;
	}
	if (((SDWORD)x < - ((SDWORD)strlen(aTxtBuff) + 1) * FONT_HEIGHT) ||
		(x >= screenWidth))
	{
		return;
	}

	/* Clip the string to the size of the screen */
	if (x < 0)
	{
		/* Chop off the start of the string */
		pSrc = (UBYTE *)aTxtBuff - (x/FONT_WIDTH) + 1;
		x = FONT_WIDTH - ( (-(SDWORD)x) % FONT_WIDTH );
		pDest = (UBYTE *)aTxtBuff;
		while (*pSrc != '\0')
		{
			*pDest++ = *pSrc++;
		}
		*pDest = '\0';
	}
	strLen = strlen(aTxtBuff);
	if (x + strLen * FONT_WIDTH >= screenWidth)
	{
		/* Chop off the end of the string */
		aTxtBuff[strLen - 1 -
				 ((x + strLen * FONT_WIDTH) - screenWidth) / FONT_WIDTH] = '\0';
	}

	sDDSD.dwSize = sizeof(DDSURFACEDESC2);
	ddrval = psBack->lpVtbl->Lock(psBack, NULL, &sDDSD, DDLOCK_WAIT, NULL);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE,"screenTextOut: Couldn't lock back buffer"));
		return;
	}

	switch (sBackBufferPixelFormat.dwRGBBitCount)
	{
	case 8:
		for(py = 0; py < FONT_HEIGHT; py ++)
		{
			pDest = (UBYTE *)sDDSD.lpSurface + (y + py) * sDDSD.lPitch + x;
			for(pSrc = (UBYTE *)aTxtBuff; *pSrc != '\0'; pSrc++)
			{
				if ((*pSrc >= PRINTABLE_START) &&
					(*pSrc < PRINTABLE_START + PRINTABLE_CHARS))
				{
					font = aFontData[*pSrc - PRINTABLE_START][py];
					for(px = 0; px < FONT_WIDTH; px ++)
					{
						if (font & (1 << px))
						{
							*pDest = (UBYTE)textColour;
						}
						pDest ++;
					}
				}
				else
				{
					pDest += FONT_WIDTH;
				}
			}
		}
		break;
	case 16:
		for(py = 0; py < FONT_HEIGHT; py ++)
		{
			p16Dest = (UWORD *)((UBYTE *)sDDSD.lpSurface + (y + py) * sDDSD.lPitch)
						+ x;
			for(pSrc = (UBYTE *)aTxtBuff; *pSrc != '\0'; pSrc++)
			{
				if ((*pSrc >= PRINTABLE_START) &&
					(*pSrc < PRINTABLE_START + PRINTABLE_CHARS))
				{
					font = aFontData[*pSrc - PRINTABLE_START][py];
					for(px = 0; px < FONT_WIDTH; px ++)
					{
						if (font & (1 << px))
						{
							*p16Dest = (UWORD)textColour;
						}
						p16Dest ++;
					}
				}
				else
				{
					p16Dest += FONT_WIDTH;
				}
			}
		}
		break;
	case 24:
		ASSERT((FALSE,"24 bit text output not implemented"));
		break;
	case 32:
		ASSERT((FALSE,"32 bit text output not implemented"));
		break;
	default:
		ASSERT((FALSE,"Unknown display pixel format"));
		break;
	}

	ddrval = psBack->lpVtbl->Unlock(psBack, sDDSD.lpSurface);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE,"screenTextOut: Couldn;t unlock back buffer"));
		return;
	}
}


/* Blit the source rectangle of the surface
 * to the back buffer at the given location.
 * The blit is clipped to the screen size.
 */
void screenBlit(SDWORD destX, SDWORD destY,		// The location on screen
				LPDIRECTDRAWSURFACE4 psSurf,		// The surface to blit from
				UDWORD	srcX, UDWORD srcY,
				UDWORD	uwidth, UDWORD uheight)	// The source rectangle from the surface
{
	RECT		sSrcRect, sDestRect;
	HRESULT		ddrval;
	DDBLTFX		sBltFX;
	SDWORD		width, height;

	if (psBack == NULL) return;

	width = uwidth;
	height = uheight;

	/* Do clipping on the destination rect */
	if ((destX + width <= 0) || (destY + height <= 0) ||
		(destX >= (SDWORD)screenWidth) || (destY >= (SDWORD)screenHeight))
	{
		/* completely off screen */
		return;
	}
	if (destX < 0)
	{
		/* clip left side */
		srcX -= destX;
		width += destX;
		destX = 0;
	}
	else if (destX + width >= (SDWORD)screenWidth)
	{
		/* clip right side */
		width = (SDWORD)screenWidth - destX;
	}
	if (destY < 0)
	{
		/* clip top */
		srcY -= destY;
		height += destY;
		destY = 0;
	}
	else if (destY + height >= (SDWORD)screenHeight)
	{
		/* clip bottom */
		height = (SDWORD)screenHeight - destY;
	}

	ASSERT(( (destX >= 0) && (destX+width <= (SDWORD)screenWidth) &&
		     (destY >= 0) && (destY+height <= (SDWORD)screenHeight),
			 "screenBlit: Clipping failed"));

	/* now do the blit */
	(void)SetRect(&sSrcRect, srcX, srcY, srcX+width, srcY+height);
	(void)SetRect(&sDestRect, destX, destY, destX+width, destY+height);
	memset(&sBltFX, 0, sizeof(DDBLTFX));
	sBltFX.dwSize = sizeof(DDBLTFX);
	sBltFX.ddckSrcColorkey.dwColorSpaceLowValue = 0;
	sBltFX.ddckSrcColorkey.dwColorSpaceHighValue = 0;
	ddrval = psBack->lpVtbl->Blt(
						psBack, &sDestRect, 
						psSurf, &sSrcRect,
//						DDBLT_WAIT,
						DDBLT_WAIT | DDBLT_KEYSRCOVERRIDE,
						&sBltFX);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE,"Blit failed:\n%s",
				 DDErrorToString(ddrval)));
	}
}


/*
 * Blit the source rectangle to the back buffer scaling it to
 * the size of the destination rectangle.
 * This clips to the size of the back buffer.
 */
void screenScaleBlit(SDWORD destX, SDWORD destY,
					 SDWORD destWidth, SDWORD destHeight,
					 LPDIRECTDRAWSURFACE4 psSurf,
					 SDWORD srcX, SDWORD srcY,
					 SDWORD srcWidth, SDWORD srcHeight)
{
	RECT		sSrcRect, sDestRect;
	HRESULT		ddrval;
	SDWORD		lowShrink;
	SDWORD		highShrink;
	DDBLTFX		sBltFX;

	if (psBack == NULL) return;

	/* Do clipping on the destination rect */
	if ((destX + destWidth <= 0) || (destY + destHeight <= 0) ||
		(destX >= (SDWORD)screenWidth) || (destY >= (SDWORD)screenHeight))
	{
		/* completely off screen */
		return;
	}

	/* do the horizontal clipping */
	if ((destX < 0) || (destX + destWidth > (SDWORD)screenWidth))
	{
		lowShrink = destX < 0 ? -destX : 0;
		srcX += (SDWORD)(srcWidth * lowShrink / destWidth);

		highShrink = destX + destWidth > (SDWORD)screenWidth ?
					destX + destWidth - (SDWORD)screenWidth + 1 : 0;
		srcWidth -= (SDWORD)(srcWidth * (lowShrink + highShrink) / destWidth);

		destX = destX < 0 ? 0 : destX;
		destWidth -= lowShrink + highShrink;
	}
	
	/* do the vertical clipping */
	if ((destY < 0) || (destY + destHeight > (SDWORD)screenHeight))
	{
		lowShrink = destY < 0 ? -destY : 0;
		srcY += (SDWORD)(srcHeight * lowShrink / destHeight);

		highShrink = destY + destHeight > (SDWORD)screenHeight ?
			destY + destHeight - (SDWORD)screenHeight + 1 : 0;
		srcHeight -= (SDWORD)(srcHeight * (lowShrink + highShrink) / destHeight);

		destY = destY < 0 ? 0 : destY;
		destHeight -= lowShrink + highShrink;
	}

	ASSERT(((destX >= 0) && (destX + destWidth < (SDWORD)screenWidth) &&
			(destY >= 0) && (destY + destHeight < (SDWORD)screenHeight),
			"screenScaleBlit: Clip failed"));
	
	/* now do the blit */
	(void)SetRect(&sSrcRect, srcX, srcY, srcX+srcWidth, srcY+srcHeight);
	(void)SetRect(&sDestRect, destX, destY, destX+destWidth, destY+destHeight);
	memset(&sBltFX, 0, sizeof(DDBLTFX));
	sBltFX.dwSize = sizeof(DDBLTFX);
	sBltFX.ddckSrcColorkey.dwColorSpaceLowValue = 0;
	sBltFX.ddckSrcColorkey.dwColorSpaceHighValue = 0;
	ddrval = psBack->lpVtbl->Blt(
						psBack, &sDestRect,
						psSurf, &sSrcRect,
						DDBLT_WAIT | DDBLT_KEYSRCOVERRIDE, &sBltFX);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE,"scaleBlit failed\n%s", DDErrorToString(ddrval)));
	}
}


/* Blit a tile (rectangle) from the surface
 * to the back buffer at the given location.
 * The tile is specified by it's size and number, numbering
 * across from top left to bottom right.
 * The blit is clipped to the screen size.
 */
void screenBlitTile(SDWORD destX, SDWORD destY,	// The location on screen
				LPDIRECTDRAWSURFACE4 psSurf,		// The surface to blit from
				UDWORD	width, UDWORD height,	// The size of the tile
				UDWORD  tile)					// The tile number
{
	UDWORD		tilesPerLine, srcX, srcY;
	DDSURFACEDESC2	ddsd;
	HRESULT		ddrval;

	if (psBack == NULL || psSurf == NULL) return;

	/* Get the surface size */
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddrval = psSurf->lpVtbl->GetSurfaceDesc(psSurf, &ddsd);

	/* Calculate where the tile is on the surface */
	tilesPerLine = ddsd.dwWidth / width;
	srcX = (tile % tilesPerLine) * width;
	srcY = (tile / tilesPerLine) * height;

	ASSERT((((srcX + width) <= ddsd.dwWidth) && ((srcY + height) <= ddsd.dwHeight),
		"screenBlitTile: Tile off source surface"));

	/* blit the tile */
	screenBlit(destX,destY, psSurf, srcX,srcY, width,height);
}


/* Return the actual value that will be poked into screen memory
 * given an RGB value.  The value is padded with zeros up to a
 * UDWORD but is based on the bit depth of the screen mode.
 */
UDWORD screenGetCacheColour(UBYTE red, UBYTE green, UBYTE blue)
{
	SDWORD		rShift,rPalShift;
	SDWORD		gShift,gPalShift;
	SDWORD		bShift,bPalShift;
	UDWORD		currMask;
	UDWORD		colour;

// RGB = 000 will be transparent on most hardware so check for it and change to RGB = 111.
	if( (red == 0) && (green == 0) && (blue == 0) ) {
		red = green = blue = 1;
	}

	if (sBackBufferPixelFormat.dwRGBBitCount >= 16)
	{
		currMask = sBackBufferPixelFormat.dwRBitMask;
		for(rShift=0; !(currMask & 1); currMask >>= 1, rShift++);
		for(rPalShift=0; (currMask & 1); currMask >>=1, rPalShift++);
		rPalShift = PALETTEENTRY_BITS - rPalShift;

		currMask = sBackBufferPixelFormat.dwGBitMask;
		for(gShift=0; !(currMask & 1); currMask >>= 1, gShift++);
		for(gPalShift=0; (currMask & 1); currMask >>= 1, gPalShift++);
		gPalShift = PALETTEENTRY_BITS - gPalShift;

		currMask = sBackBufferPixelFormat.dwBBitMask;
		for(bShift=0; !(currMask & 1); currMask >>= 1, bShift++);
		for(bPalShift=0; (currMask & 1); currMask >>= 1, bPalShift++);
		bPalShift = PALETTEENTRY_BITS - bPalShift;
	}

	switch (sBackBufferPixelFormat.dwRGBBitCount)
	{
	case 8:
		colour = screenGetPalEntry(red,green,blue);
		break;
	case 16:
	case 24:
	case 32:
		colour = (red >> rPalShift) << rShift;
		colour |= (green >> gPalShift) << gShift;
		colour |= (blue >> bPalShift) << bShift;
		break;
	default:
		ASSERT((FALSE,"Unknown display pixel format"));
		break;
	}

	return colour;
}


/* Set the colour for drawing lines.
 *
 * This caches a colour value that can be poked directly into the
 * current screen mode's video memory.  There is some overhead to this
 * call so all lines of the same colour should be drawn at the same time.
 */
void screenSetLineColour(UBYTE red, UBYTE green, UBYTE blue)
{
	lineColour = screenGetCacheColour(red,green,blue);
}


/* Set the value to be poked into screen memory for line drawing.
 * The colour value used should be one returned by screenGetCacheColour.
 */
void screenSetLineCacheColour(UDWORD colour)
{
	lineColour = colour;
}


typedef enum _outcode
{
	OUT_TOP = 0x8,
	OUT_BOTTOM = 0x4,
	OUT_RIGHT = 0x2,
	OUT_LEFT = 0x1,
} OUTCODE;

__inline void compOutCode(SDWORD x, SDWORD y, OUTCODE *pCode)
{
	*pCode = 0;
	if (y >= (SDWORD)screenHeight)
	{
		*pCode |= OUT_TOP;
	}
	else if (y < 0)
	{
		*pCode |= OUT_BOTTOM;
	}

	if (x >= (SDWORD)screenWidth)
	{
		*pCode |= OUT_RIGHT;
	}
	else if (x < 0)
	{
		*pCode |= OUT_LEFT;
	}
}

/* Draw a line to the display in the current colour. */
void screenDrawLine(SDWORD x0, SDWORD y0, SDWORD x1, SDWORD y1)
{
	SDWORD			d, x,y, ax,ay, sx,sy, dx,dy;
	SDWORD			lineChange;
//	SDWORD			dx,dy, accX, currY;
//	SDWORD			xDir, yDir, endX, endY;
	UBYTE			*pOffset;
	HRESULT			ddrval;
	DDSURFACEDESC2	ddsd;
	BOOL			clipped;
	OUTCODE			code0, code1, code;
	float			cx,cy;
#ifdef DEBUG
	UDWORD			loops;
#endif

	if (psBack == NULL) return;

	clipped = FALSE;
	compOutCode(x0,y0, &code0);
	compOutCode(x1,y1, &code1);
#ifdef DEBUG
	loops = 0;
#endif

	/* Loop until the line is accepted. return if the line is rejected */
	do
	{
		ASSERT((loops++ < 20,
			"screenDrawLine: clipping loop reached 20 iterations\n"
			"                 - possible infinite loop ?"));
		if ((code0 == 0) && (code1 == 0))
		{
			/* trivial acceptance */
			clipped = TRUE;
		}
		else if (code0 & code1)
		{
			/* trivial reject */
			return;
		}
		else
		{
			/* At least one of the end points is out of the screen pick it and clip */
			if (code0 != 0)
			{
				code  = code0;
			}
			else
			{
				code = code1;
			}
			if (code & OUT_TOP)
			{
				cx = x0 + (float)(x1 -x0) *
					(float)((SDWORD)screenHeight - 1 - y0) / (y1 - y0);
				cy = (float)screenHeight - 1;
			}
			else if (code & OUT_BOTTOM)
			{
				cx = x0 + (float)(x1 - x0) * (float)-y0 / (float)(y1 - y0);
				cy = (float)0;
			}
			else if (code & OUT_RIGHT)
			{
				cy = y0 + (float)(y1 - y0) *
					(float)((SDWORD)screenWidth -1 - x0) / (float)(x1 - x0);
				cx = (float)screenWidth - 1;
			}
			else if (code & OUT_LEFT)
			{
				cy = y0 + (float)(y1 - y0)* (float)-x0 / (float)(x1 - x0);
				cx = (float)0;
			}

			/* calculated intesection point get ready for next pass */
			if (code == code0)
			{
				x0 = ROUND(cx);
				y0 = ROUND(cy);
				compOutCode(x0,y0,&code0);
			}
			else
			{
				x1 = ROUND(cx);
				y1 = ROUND(cy);
				compOutCode(x1,y1,&code1);
			}
		}
	} while (!clipped);

	ASSERT(((x0 >= 0) && (x0 < (SDWORD)screenWidth) &&
			(x1 >= 0) && (x1 < (SDWORD)screenWidth) &&
			(y0 >= 0) && (y0 < (SDWORD)screenHeight) &&
			(y1 >= 0) && (y1 < (SDWORD)screenHeight),
		"screenDrawLine: Failed to clip to screen"));

	/* Lock the texture surface to draw the line */
	memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	ddrval = psBack->lpVtbl->Lock(
					psBack,
					NULL, &ddsd, DDLOCK_WAIT, NULL);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE,"Lock failed for line draw:\n%s",
				 DDErrorToString(ddrval)));
		return;
	}

	/* have display mode specific versions for speed
	   - it only affects the write operation */
	switch (sBackBufferPixelFormat.dwRGBBitCount)
	{
	case 8:
		/* Do some initial set up for the line */
		dx = x1 - x0;
		dy = y1 - y0;
		ax = abs(dx) << 1;
		ay = abs(dy) << 1;
		sx = dx < 0 ? -1 : 1;
		sy = dy < 0 ? -1 : 1;

		x = x0;
		y = y0;
		pOffset = (UBYTE *)ddsd.lpSurface + ddsd.lPitch * y0 + x0;
		lineChange = dy < 0 ? -(SDWORD)ddsd.lPitch : (SDWORD)ddsd.lPitch;
		if (ax > ay)
		{
			/* x dominant */
			d = ay - ax/2;
			FOREVER
			{
				*pOffset = (UBYTE)lineColour;
				if (x == (SDWORD)x1)
				{
					/* Finished line - end loop */
					break;
				}
				if (d >= 0)
				{
					y = y + sy;
					d = d - ax;
					pOffset+=lineChange;
				}
				x = x + sx;
				d = d + ay;
				pOffset += sx;
			}
		}
		else
		{
			/* y dominant */
			d = ax - ay/2;
			FOREVER
			{
				*pOffset = (UBYTE)lineColour;
				if (y == (SDWORD)y1)
				{
					/* Finished line - end loop */
					break;
				}
				if (d >= 0)
				{
					x = x + sx;
					d = d - ay;
					pOffset += sx;
				}
				y = y + sy;
				d = d + ax;
				pOffset += lineChange;
			}
		}
		break;
	case 16:
		/* Do some initial set up for the line */
		dx = x1 - x0;
		dy = y1 - y0;
		ax = abs(dx) << 1;
		ay = abs(dy) << 1;
		sx = dx < 0 ? -1 : 1;
		sy = dy < 0 ? -1 : 1;

		x = x0;
		y = y0;
		pOffset = (UBYTE *)ddsd.lpSurface + ddsd.lPitch * y0 + (x0<<1);
		lineChange = dy < 0 ? -(SDWORD)ddsd.lPitch : (SDWORD)ddsd.lPitch;
		if (ax > ay)
		{
			/* x dominant */
			d = ay - ax/2;
			FOREVER
			{
				*((UWORD *)pOffset) = (UWORD)lineColour;
				if (x == (SDWORD)x1)
				{
					/* Finished line - end loop */
					break;
				}
				if (d >= 0)
				{
					y = y + sy;
					d = d - ax;
					pOffset+=lineChange;
				}
				x = x + sx;
				d = d + ay;
				pOffset += sx;
				pOffset += sx;
			}
		}
		else
		{
			/* y dominant */
			d = ax - ay/2;
			FOREVER
			{
				*((UWORD *)pOffset) = (UWORD)lineColour;
				if (y == (SDWORD)y1)
				{
					/* Finished line - end loop */
					break;
				}
				if (d >= 0)
				{
					x = x + sx;
					d = d - ay;
					pOffset += sx;
					pOffset += sx;
				}
				y = y + sy;
				d = d + ax;
				pOffset += lineChange;
			}
		}
		break;
	case 24:
		ASSERT((FALSE,"24 bit line drawing not implemented"));
		break;
	case 32:
		ASSERT((FALSE,"32 bit line drawing not implemented"));
		break;
	default:
		ASSERT((FALSE,"Unknown display pixel format"));
		break;
	}

	ddrval = psBack->lpVtbl->Unlock(psBack, NULL);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE,"Unlock failed for line draw:\n%s",
				 DDErrorToString(ddrval)));
	}
}

/* Draw a rectangle (unfilled) */
void screenDrawRect(SDWORD x0, SDWORD y0, SDWORD x1, SDWORD y1)
{
	/* Quick hack for now, could do a fast version if necessary */
	screenDrawLine(x0,y0, x1,y0);
	screenDrawLine(x1,y0, x1,y1);
	screenDrawLine(x1,y1, x0,y1);
	screenDrawLine(x0,y1, x0,y0);
}

/* Set the colour for fill operations */
void screenSetFillColour(UBYTE red, UBYTE green, UBYTE blue)
{
	fillColour = screenGetCacheColour(red,green,blue);
}


/* Set the value to be poked into screen memory for filling.
 * The colour value used should be one returned by screenGetCacheColour.
 */
void screenSetFillCacheColour(UDWORD colour)
{
	fillColour = colour;
}


/* Draw a filled rectangle */
void screenFillRect(SDWORD x0, SDWORD y0, SDWORD x1, SDWORD y1)
{
	RECT		sDestRect;
	HRESULT		ddrval;
	DDBLTFX		sDDBlitFX;
	SDWORD		tmp;

	if (psBack == NULL) return;

	/* make sure x0,y0 is top left */
	if (x1 < x0)
	{
		tmp = x1;
		x1 = x0;
		x0 = tmp;
	}
	if (y1 < y0)
	{
		tmp = y1;
		y1 = y0;
		y0 = tmp;
	}

	/* Do clipping on the destination rect */
	if (x1 < 0 || x0 >= (SDWORD)screenWidth ||
		y1 < 0 || y0 >= (SDWORD)screenHeight)
	{
		/* Completely off screen */
		return;
	}
	if (x0 < 0)
	{
		x0 = 0;
	}
	if (y0 < 0)
	{
		y0 = 0;
	}
	if (x1 >= (SDWORD)screenWidth)
	{
		x1 = screenWidth -1;
	}
	if (y1 >= (SDWORD)screenHeight)
	{
		y1 = screenHeight -1;
	}

	memset(&sDDBlitFX, 0, sizeof(DDBLTFX));
	sDDBlitFX.dwSize = sizeof(DDBLTFX);
	sDDBlitFX.dwFillColor = fillColour;
	(void)SetRect(&sDestRect, x0,y0, x1+1,y1+1);
	ddrval = psBack->lpVtbl->Blt(
						psBack, &sDestRect,
						NULL, NULL,
						DDBLT_COLORFILL | DDBLT_WAIT, &sDDBlitFX);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE,"Fill rect failed:\n%s",
			DDErrorToString(ddrval)));
		return;
	}
}


/* Draw a circle/ellipse */
void screenDrawEllipse(SDWORD x0, SDWORD y0, SDWORD x1, SDWORD y1)
{
HRESULT	ddrval;
HDC		hdc;
HPEN	hpen;

	if (psBack == NULL) return;

	/* This doesn't work in fudge mode so quit */
	if ((screenMode == SCREEN_WINDOWED) && (displayMode == MODE_8BITFUDGE))
	{
		return;
	}

	/* Get the device context for the back surface */
	ddrval = psBack->lpVtbl->GetDC(psBack,&hdc);

	/* Were we successfull? */
	if (ddrval!=DD_OK)
	{
		/* If not, then report the error */
		ASSERT((FALSE,"Elipse draw failed - couldn't get device context:\n%s",
				DDErrorToString(ddrval)));
	}

	/* Otherwise draw the ellipse */
	else
	{
	    hpen = CreatePen(PS_SOLID, 0, RGB(255, 255, 255));
		SelectObject(hdc, hpen);
		
		(void) Arc(hdc,x0,y0,x1,y1,x0,y0,x0,y0);
		/* Attempt to release the surface */
		ddrval = psBack->lpVtbl->ReleaseDC(psBack,hdc);

		/* Everything fine? */
		if (ddrval!=DD_OK)
		{
			/* No -report the error */
			ASSERT((FALSE,"Elipse draw failed - couldn't release context:\n%s",
				DDErrorToString(ddrval)));
		}

		DeleteObject(hpen);
	}
}

