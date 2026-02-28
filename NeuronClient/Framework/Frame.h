#pragma once

/*
 * Frame.h
 *
 * The framework library initialisation && shutdown routines.
 *
 */

#pragma warning (disable : 4201 4214 4115 4514)
#include <windows.h>
#pragma warning (default : 4201 4214 4115)


#include "Types.h"
#include "Debug.h"
#include "Mem.h"
#include "Screen.h"
#include "DdrawCompat.h"
#include "Dderror.h"
#include "Input.h"
#include "Surface.h"
#include "Image.h"
#include "Font.h"
#include "Heap.h"
#include "Treap.h"
#include "W95trace.h"
#include "Fractions.h"
#include "Trig.h"
#include "FrameResource.h"
#include "StrRes.h"
#include "DXInput.h"
#include "Block.h"
#include "ListMacs.h"

/* Initialise the frame work library */
extern BOOL frameInitialise(HINSTANCE hInstance,		// The windows application instance
					 char *pWindowName,	// The text to appear in the window title bar
					 UDWORD	width,			// The display width
					 UDWORD height,			// The display height
					 UDWORD bitDepth,		// The display bit depth
					 BOOL	fullScreen,		// Whether to start full screen or windowed
					 BOOL	bVidMem,	 	// Whether to put surfaces in video memory
					 BOOL	bGlide );		// Whether to create surfaces

/* Shut down the framework library.
 * This clears up all the Direct Draw stuff && ensures
 * that Windows gets restored properly after Full screen mode.
 */
extern void frameShutDown(void);

/* The current status of the framework */
typedef enum _frame_status
{
	FRAME_OK,			// Everything normal
	FRAME_KILLFOCUS,	// The main app window has lost focus (might well want to pause)
	FRAME_SETFOCUS,		// The main app window has focus back
	FRAME_QUIT,			// The main app window has been told to quit
} FRAME_STATUS;

/* Call this each cycle to allow the framework to deal with
 * windows messages, && do general house keeping.
 *
 * Returns FRAME_STATUS.
 */
extern FRAME_STATUS frameUpdate(void);

/* If cursor on is TRUE the windows cursor will be displayed over the game window
 * (&& in full screen mode).  If it is FALSE the cursor will !be displayed.
 */
extern void frameShowCursor(BOOL cursorOn);

/* Set the current cursor from a cursor handle */
extern void frameSetCursor(HCURSOR hNewCursor);

/* Set the current cursor from a Resource ID
 * This is the same as calling:
 *       frameSetCursor(LoadCursor(MAKEINTRESOURCE(resID)));
 * but with a bit of extra error checking.
 */
extern void frameSetCursorFromRes(WORD resID);

/* Returns the current frame we're on - used to establish whats on screen */
extern UDWORD	frameGetFrameNumber(void);

/* Return the current frame rate */
extern UDWORD frameGetFrameRate(void);

/* Return the overall frame rate */
extern UDWORD frameGetOverallRate(void);

/* Return the frame rate for the last second */
extern UDWORD frameGetRecentRate(void);

/* The handle for the application window */
extern HWND	frameGetWinHandle(void);

//enumerate all available direct draw devices
extern BOOL frameDDEnumerate(void);

extern SDWORD frameGetNumDDDevices(void);

extern char* frameGetDDDeviceName(SDWORD);

// Return a string for a windows error code
extern char *winErrorToString(SDWORD error);

/* The default window procedure for the library.
 * This is initially set to the standard DefWindowProc, but can be changed
 * by this function.
 * Call this function with NULL to reset to DefWindowProc.
 */
typedef LRESULT (* DEFWINPROCTYPE)(HWND hWnd, UINT Msg,
										 WPARAM wParam, LPARAM lParam);
extern void frameSetWindowProc(DEFWINPROCTYPE winProc);


/* Load the file with name pointed to by pFileName into a memory buffer. */
extern BOOL loadFile(char *pFileName,		// The filename
					 UBYTE **ppFileData,	// A buffer containing the file contents
					 UDWORD *pFileSize);	// The size of this buffer

/* Load the file with name pointed to by pFileName into a memory buffer. */
// if allocate mem is true then the memory is allocated ... else it is already in ppFileData, and the max size is in pFileSize ... this is adjusted to the actual loaded file size
//   
BOOL loadFile2(char *pFileName, UBYTE **ppFileData, UDWORD *pFileSize, BOOL AllocateMem );

/* Save the data in the buffer into the given file */
extern BOOL saveFile(char *pFileName, UBYTE *pFileData, UDWORD fileSize);

// load a file from disk into a fixed memory buffer
extern BOOL loadFileToBuffer(char *pFileName, UBYTE *pFileBuffer, UDWORD bufferSize, UDWORD *pSize);
// as above but returns quietly if no file found
extern BOOL loadFileToBufferNoError(char *pFileName, UBYTE *pFileBuffer, UDWORD bufferSize, UDWORD *pSize);

extern SDWORD ftol(float f);
extern BOOL	bRunningUnderGlide;


UINT HashString( const char *String );
UINT HashStringIgnoreCase( const char *String );


