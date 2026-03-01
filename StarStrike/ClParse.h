#pragma once

#include "Types.h"

/*
 * clParse.h
 *
 * All the command line values
 *
 */

// whether to start windowed
extern BOOL	clStartWindowed;
// whether to play the intro video
extern BOOL	clIntroVideo;
// parse the commandline
extern BOOL ParseCommandLine( char* psCmdLineBOOL, BOOL bGlideDllPresent);




