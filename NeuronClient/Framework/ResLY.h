#pragma once

/*
 * ResLY.h
 *
 * Interface to the RES file lex && yacc functions.
 */

/* Maximum number of characters in a directory entry */
#define FILE_MAXCHAR		255

/* Maximum number of TEXT items in any one Yacc rule */
#define TEXT_BUFFERS	10

/* The initial resource directory && the current resource directory */
extern STRING	aResDir[FILE_MAXCHAR];
extern STRING	aCurrResDir[FILE_MAXCHAR];

/* Set the current input buffer for the lexer - used by resLoad */
extern void resSetInputBuffer(UBYTE *pBuffer, UDWORD size);

/* Give access to the line number && current text for error messages */
extern void resGetErrorData(int *pLine, char **ppText);

/* Call the yacc parser */
extern int res_parse(void);

