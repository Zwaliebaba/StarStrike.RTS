#pragma once

/*
 * EvntSave.h
 *
 * Save the state of the event system.
 *
 */


// Save the state of the event system
extern BOOL eventSaveState(SDWORD version, UBYTE **ppBuffer, UDWORD *pFileSize);

// Load the state of the event system
extern BOOL eventLoadState(UBYTE *pBuffer, UDWORD fileSize, BOOL bHashed);


