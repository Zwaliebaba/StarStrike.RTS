#pragma once

/*
 * types.h
 *
 * Simple type definitions.
 *
 */


/* Basic numeric types */
typedef unsigned	char	UBYTE;
typedef	signed		char	SBYTE;
typedef	char		STRING;
typedef	unsigned	short	UWORD;
typedef	signed		short	SWORD;
typedef	unsigned	int		UDWORD;
typedef	signed		int		SDWORD;

typedef	int	BOOL;

/* Numeric size defines */
#define UBYTE_MAX	0xff
#define SBYTE_MIN	(-128) //(0x80)
#define SBYTE_MAX	0x7f
#define UWORD_MAX	0xffff
#define SWORD_MIN	(-32768) //(0x8000)
#define SWORD_MAX	0x7fff
#define UDWORD_MAX	0xffffffff
#define SDWORD_MIN	(0x80000000)
#define SDWORD_MAX	0x7fffffff

/* Standard Defines */
#ifndef NULL
#define NULL	(0)
#endif

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

/* locale types */

#define LOCAL                   static
#define STATIC                  static
#define REGISTER                register
#define FAST                    register
#define IMPORT                  extern
#define VOID                    void



/* Probability helper: returns non-zero (true) with probability 1-in-n */
#define oneIn(n) (rand() % (n) == 0)


#define	ABSDIF(a,b) ((a)>(b) ? (a)-(b) : (b)-(a))
#define CAT(a,b) a##b

// now in fractions.h #define ROUND(x) ((x)>=0 ? (SDWORD)((x) + 0.5) : (SDWORD)((x) - 0.5))

