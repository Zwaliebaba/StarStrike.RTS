#pragma once

/*
 * StrRes.h
 *
 * String resource interface functions
 *
 */

/* A string block */
typedef struct _str_block
{
	char **apStrings;
	UDWORD	idStart, idEnd;

#ifdef DEBUG
	UDWORD	*aUsage;
#endif

	struct _str_block *psNext;
} STR_BLOCK;

/* An ID entry */
typedef struct _str_id
{
	UDWORD	id;
	char *pIDStr;
} STR_ID;


/* A String Resource */
typedef struct _str_res
{
	TREAP			*psIDTreap;		// The treap to store string identifiers
	STR_BLOCK		*psStrings;		// The store for the strings themselves
	UDWORD			init,ext;		// Sizes for the string blocks
	UDWORD			nextID;			// The next free ID
} STR_RES;

/* Create a string resource object */
extern BOOL strresCreate(STR_RES **ppsRes, UDWORD init, UDWORD ext);

/* Release a string resource object */
extern void strresDestroy(STR_RES *psRes);

/* Release the id strings, but !the strings themselves,
 * (they can be accessed only by id number).
 */
extern void strresReleaseIDStrings(STR_RES *psRes);

/* Load a list of string ID's from a memory buffer
 * id == 0 should be a default string which is returned if the
 * requested string is !found.
 */
extern BOOL strresLoadFixedID(STR_RES *psRes, STR_ID *psID, UDWORD numID);

/* Return the ID number for an ID char */
extern BOOL strresGetIDNum(STR_RES *psRes, char *pIDStr, UDWORD *pIDNum);

/* Return the stored ID string that matches the string passed in */
extern BOOL strresGetIDString(STR_RES *psRes, char *pIDStr, char **ppStoredID);

/* Get the string from an ID number */
extern char *strresGetString(STR_RES *psRes, UDWORD id);

/* Store a string value for a given key (key must already be registered via strresLoadFixedID,
 * or a new numeric ID will be allocated automatically for unknown keys).
 */
extern BOOL strresStoreString(STR_RES *psRes, char *pID, char *pString);

/* Load a string resource file (no-op: strings are now loaded from resources.pri) */
extern BOOL strresLoad(STR_RES *psRes, UBYTE *pData, UDWORD size);

/* Return the the length of a char */
extern UDWORD stringLen(char *pStr);

/* Copy a char */
extern void stringCpy(char *pDest, char *pSrc);

/* Get the ID number for a char */
extern UDWORD strresGetIDfromString(STR_RES *psRes, char *pString);

