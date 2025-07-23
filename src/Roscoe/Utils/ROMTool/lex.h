#include "types.h"

#ifndef _LEX_H_
#define _LEX_H_

/* Maximum # of nested includes */

#define				MAX_INCLUDES				32
#define				MAX_LEXSIZE					8192

/* Numeric equivalents */

#define				LEX_DECIMAL						0
#define				LEX_HEX							1
#define				LEX_BINARY 						2

/* Cool stuff here for doing in-memory lexing */

#define				LEX_FILE							0
#define				LEX_STRING						1

/* Enumerated types used within the lexer and grammar parser */

enum {SYMBOL=257,STRING,INCLUDE,CONSTANT,

	// above tokens are handled "automaticly" by the lexer.
	// user tokens should start here.
	FIRST_USER_TOKEN,
};

/* Unions and structures */

struct sReservedWords
{
	UINT8 *reservedWordName;
	UINT32 tokenValue;
	UINT32 dwUserVal1;
	UINT32 dwUserVal2;
	UINT32 dwUserVal3;
};

union uValue
{
	UINT8 *string;						/* If it's a string... */
	INT32 integer;			/* Or an integer... */
	double floatingPoint;				/* Floating point */
};

extern void SetReservedWordlist(struct sReservedWords *psReservedWords);
extern UINT32 yyopen(UINT8 *filename, UINT8 bType);
extern UINT32 yylex(void);
void yyputback(UINT32 dwToken);
extern void yykill(void);
extern union uValue lexval;
extern void yyerror(UINT8 *fmt, ... );
extern void yynotice(UINT8 *fmt, ... );
extern void yyfatal(UINT8 *fmt, ... );
extern void yywarning(UINT8 *fmt, ... );
extern UINT32 yygetlineno(void);
extern UINT32 yygeterrcnt(void);
extern UINT32 yygetwarncnt(void);
extern UINT32 yygetfatalcnt(void);
extern UINT8 *LookupReservedWord(UINT32 dwTokenId);
extern struct sReservedWords *yyreserved;
extern FILE *yyerrf;
extern UINT8 yy_eol_comment_char;
extern UINT8 yy_cpp_comments;
extern UINT8 *yytext;

extern void yyshutdown();

#endif
