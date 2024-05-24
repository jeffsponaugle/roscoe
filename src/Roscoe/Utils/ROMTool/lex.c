/***************************************************************************
 *
 * Lexical analyzer for default keymap parser
 * 																								
 ***************************************************************************/

/******************************************************************
 * INCLUDES GO HERE:																
 ******************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "lex.h"

/******************************************************************
 * DEFINES GO HERE:																	
 ******************************************************************/

#define	MAX_LEXSIZE		8192

/******************************************************************
 * GLOBAL VARIABLES GO HERE: 													
 ******************************************************************/

UINT8 *yytext = NULL;				/* Place to put everything */
static INT32 yylineno[MAX_INCLUDES];	/* Our current line # */
static INT32 yylength = 0;					/* YY Length! */
static FILE *yyin[MAX_INCLUDES];			/* Data stream */
static UINT8 *yyfilename[MAX_INCLUDES];/* Filenames of each open file */
static INT32 errors = 0;					/* No errors to start with */
static INT32 fatal = 0;
static INT32 warnings = 0;					/* Warnings */
static INT32 fileLevel = -1;				/* Our current file level */
static UINT32 yysavedtoken;				/* saved token for look-ahead */
static UINT8 yysavedvalid;					/* is saved token valid? */
static UINT8 bCurrentType = (UINT8) INVALID;	/* No current type */
static UINT8 *pbParsePtr = NULL;			/* Current parse pointer - for in-memory strings */
union uValue lexval;							/* Sub-value of the tokens */
static struct sReservedWords *psReserved = NULL;
struct sReservedWords *yyreserved = NULL;

// set this to non-null to have errors split to another stream.
FILE *yyerrf = NULL;
UINT8 yy_eol_comment_char = ';';
UINT8 yy_cpp_comments = FALSE;

void EatWhiteSpace(void);

/******************************************************************
 * PROCEDURES GO HERE:															
 ******************************************************************/


/****************************************************************************
 *																									 
 *  Procedure   :	yyerror(string, ... )
 *																									 
 *	 Inputs		 :	Error string to display												 
 *																									 
 *	 Outputs     : Nothing																	 
 *																									 
 *	 Description :																				 
 *																									 
 *	 This routine displays an error to the standard error display.				 
 *																									 
 ****************************************************************************/

static UINT8 yyerrbuf[512];

static void _yyerror(UINT8 *lvl, UINT8 *fmt, va_list ap)
{
	if (bCurrentType == LEX_STRING)
	{
		return;
	}

	vsprintf((char *)yyerrbuf,fmt,ap);

	fprintf(stderr, "\n%s(%ld): %s%s\n",
		yyfilename[fileLevel], yylineno[fileLevel],
		lvl, yyerrbuf);

	if (yyerrf)
		fprintf(yyerrf, "\n%s(%ld): %s%s\n",
			yyfilename[fileLevel], yylineno[fileLevel], lvl, yyerrbuf);
}


void yyerror(UINT8 *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	_yyerror("",fmt,ap);
	va_end(ap);
	++errors;
}

void yywarning(UINT8 *fmt, ... )
{
	va_list ap;
	va_start(ap,fmt);
	_yyerror("Warning: ",fmt,ap);
	va_end(ap);
	++warnings;
}

void yyfatal(UINT8 *fmt, ... )
{
	va_list ap;
	va_start(ap,fmt);
	_yyerror("FATAL ERROR: ",fmt,ap);
	va_end(ap);
	++fatal;
}

void yynotice(UINT8 *fmt, ... )
{
	va_list ap;
	va_start(ap,fmt);
	_yyerror("",fmt,ap);
	va_end(ap);
}

void yyshutdown()
{	
	if (yytext)
		free(yytext);
}

/****************************************************************************
 *																									 
 *  Procedure   :	yykill																	 
 *																									 
 *	 Inputs		 :	Nothing		  															 
 *																									 
 *	 Outputs     : Nothing																	 
 *																									 
 *	 Description :																				 
 *																									 
 *	 This routine shuts down any files that have been left open.				 
 *																									 
 ****************************************************************************/

void yykill(void)

{
	while (fileLevel != -1)
	{
		fclose(yyin[fileLevel]);
		yyin[fileLevel] = NULL;
		free(yyfilename[fileLevel]);
		fileLevel--;
	}

	bCurrentType = (UINT8) INVALID;
}

UINT32 yygetlineno(void)
{
	return(yylineno[fileLevel]);
}

UINT32 yygeterrcnt(void)
{
	return(errors);
}

UINT32 yygetwarncnt(void)
{
	return(warnings);
}

UINT32 yygetfatalcnt(void)
{
	return(fatal);
}

/****************************************************************************
 *																									 
 *  Procedure   :	yyopen(filename)														 
 *																									 
 *	 Inputs		 :	Filename to open up (one more layer)							 
 *																									 
 *	 Outputs     : 0 If successful, -1 if failed										 
 *																									 
 *	 Description :																				 
 *																									 
 *	 This routine gets everything ready for another file to be in the file	 
 *  stream.																					    
 *																									 
 ****************************************************************************/

UINT32 yyopen(UINT8 *filename, UINT8 bType)

{
	INT32 loop = 0;

	if (LEX_FILE == bType)
	{
		if (fileLevel == -1)
		{
			errors = 0;	 					/* Reset our error counter */
			fatal = 0;
			warnings = 0;					/* And our warning counter */
			yysavedtoken = 0xFFFFFFFF;	/* reset 'unget' token */
			yysavedvalid = 0;
		}

		if (fileLevel + 1 >= MAX_INCLUDES)
		{
			yyerror("Exceeded maximum # of include depths");
			return(FALSE);
		}

		while (loop <= fileLevel)
			if (strcasecmp(yyfilename[loop], filename) == 0)
			{
				yyerror("Can't recursively nest includes");
				return(FALSE);
			}
		else
			loop++;

		++fileLevel;									/* Next level! */
		yylineno[fileLevel] = 1;					/* Start off on line 1! */
		yyfilename[fileLevel] = malloc(strlen(filename) + 1);
		strcpy(yyfilename[fileLevel], filename);	/* New filename! */
		yyin[fileLevel] = fopen(filename, "r");	/* Open the file! */

		if (yyin[fileLevel] == NULL)				/* Couldn't open it? */
		{
			fileLevel--;
			yyerror("Can't open file %s", filename);
			return(FALSE);
		}

		bCurrentType = LEX_FILE;	
		return(TRUE);										/* We're successful! */
	}
	else
	if (LEX_STRING == bType)
	{
		pbParsePtr = filename;
		bCurrentType = LEX_STRING;
		return(TRUE);
	}
	else
	{
		assert(0);
		return(FALSE);
	}
}

/****************************************************************************
 *																									 
 *  Procedure   :	lexfgetc(void)															 
 *																									 
 *	 Inputs		 :	Nothing		  															 
 *																									 
 *	 Outputs     : Byte gotten, or -1 if finally eof								 
 *																									 
 *	 Description :																				 
 *																									 
 *	 This routine gets the next byte out of the current file and the prior	 
 *  stack of files as well.																 
 *																									 
 ****************************************************************************/

UINT32 lexfgetc(void)

{
	UINT8 byte;

	if (LEX_FILE == bCurrentType)
	{
		if (fileLevel < 0)
			return(-1);			/* End of file! */

		if (feof(yyin[fileLevel]))
		{
			fclose(yyin[fileLevel]);
			yyin[fileLevel] = NULL;
			free(yyfilename[fileLevel]);
			fileLevel--;
			if (fileLevel == -1)
				return(-1);								/* End of file! */
			EatWhiteSpace();							/* Take up whitespace slack */
			return(lexfgetc());						/* Get byte! */
		}

		byte = fgetc(yyin[fileLevel]);
	
		if (byte == '\n')
			++yylineno[fileLevel];

		return(byte);			/* Return a byte! */
	}
	else
	if (LEX_STRING == bCurrentType)
	{
		if (*pbParsePtr == '\0')
		{
			return(-1);			// End of file
		}

		return(*pbParsePtr++);
	}
	else		// Unknown type
	{
		assert(0);
		return(-1);
	}
}

/****************************************************************************
 *																									 
 *  Procedure   :	EatWhiteSpace															  
 *																									 
 *	 Inputs		 :	None			  															 
 *																									 
 *	 Outputs     : None 																		 
 *																									 
 *	 Description :																				 
 *																									 
 *	 This routine will eat whitespace in the current file stream.				 
 *																									 
 ****************************************************************************/

void EatWhiteSpace(void)

{
	UINT8 byte;								/* Temporary byte storage */

	while ((byte = lexfgetc()) == '\n' || byte == '\r' || byte == '\t' ||
			 byte == ' ')
	{
	}

	if (byte != 0xff)
	{
		if (LEX_FILE == bCurrentType)
		{
			ungetc(byte, yyin[fileLevel]);
			return;
		}
		else
		if (LEX_STRING == bCurrentType)
		{
			if (byte != '\0')
				--pbParsePtr;
			return;
		}
		else
		{
			assert(0);
		}
	}
}

UINT32 ishex(UINT8 byte)

{
	if (isdigit(byte))
		return(TRUE);
	if ((toupper(byte) >= 'A') && (toupper(byte) <= 'F'))
		return(TRUE);
	return(FALSE);
}

/****************************************************************************
 *																									 
 *  Procedure   :	yyputback
 *									
 *	 Inputs		 :	token to "unget"
 *																									 
 *	 Outputs     : nothing
 *																									 
 *	 Description :																				 
 *																									 
 *	 This routine puts back a gotten lexical token; NOTE: it does not change
 *  any other state, like the yyval.  This function may be called ONCE before
 *  the next call to yylex().
 *																									 
 ****************************************************************************/

void yyputback(UINT32 dwToken)
{
	if (yysavedvalid) {
		yyerror("internal parser error; duplicate unget of token");
		fatal++;
	}
	yysavedtoken = dwToken;
	yysavedvalid = 1;
}

/****************************************************************************
 *																									 
 *  Procedure   :	yylex																		 
 *																									 
 *	 Inputs		 :	Nothing		  															 
 *																									 
 *	 Outputs     : -1 If end of file, otherwise token								 
 *																									 
 *	 Description :																				 
 *																									 
 *	 This routine pulls out the next lexical token from the yyin stream.		 
 *																									 
 ****************************************************************************/

UINT32 yylex(void)

{
	INT32 loop = 0;										/* Loop variable */
	INT32 radix = LEX_DECIMAL;								/* Assume radix is decimal */
	INT32 byte;											/* Byte gotten! */
	INT32 byte2;											/* Second byte! */
	INT32 minusSign = 0;								/* Found a minus sign? */

	if (fatal) return 0xFFFFFFFF;		// fatal error.

	// if there's a token to unget, then unget it.
	if (yysavedvalid) {
		yysavedvalid = 0;
		return yysavedtoken;
	}

	yyreserved = NULL;

	if (yytext == NULL)
		yytext = malloc(MAX_LEXSIZE+2);			/* Here for yytext */

	yylength = 0;											/* String is null! */
	*yytext = '\0';									/* Null out the string */

	EatWhiteSpace();									/* Eat any whitespace */
	byte = lexfgetc();								/* Get next byte! */	

/* Now let's check for comments */

	/* Comment! */

/*
	if (yy_eol_comment_char && byte == yy_eol_comment_char)
	{
		while ((byte != '\n') && (byte != -1))
			byte = lexfgetc();

		return(yylex());
	} */

	if (byte == '/')
	{
		byte2 = lexfgetc();

		if (byte == '/')	// C++ style comment? Allow it!
		{
			while (1)
			{
				byte = lexfgetc();

				if (byte == -1)
					return(0);

				if (byte == '\n')
				{
					return(yylex());
				}
			}
		}

		if (byte2 == '*') /* If true, it's a comment */
		{
			while (1)
			{
				byte = lexfgetc();

				if (byte == -1) 			/* End of file in comment */
					return(0);

				if (byte == '*')
				{
					byte2 = lexfgetc();

					if (byte2 == -1) 		/* End of file in comment */
						return(0);

					if (byte2 == '/')		/* End comment! */
						return(yylex());	/* Continue lexing... */

					if (byte2 == '\n')
						yylineno[fileLevel]--;

					if (LEX_FILE == bCurrentType)
					{
						ungetc(byte2, yyin[fileLevel]);	/* Nope... put it back... */
					}
					else
					if (LEX_STRING == bCurrentType)
					{
						--pbParsePtr;
					}
					else
					{
						assert(0);
					}
				}
			}
		} else if (yy_cpp_comments && byte2 == '/') {

			// handle C++ style end-of-line comments.
			while ((byte != '\n') && (byte != -1))
				byte = lexfgetc();
	
			return(yylex());
		}


		if (byte2 == '\n')
			yylineno[fileLevel]--;

		if (LEX_FILE == bCurrentType)
		{
			ungetc(byte2, yyin[fileLevel]);	/* Nope... put it back... */
		}
		else
		if (LEX_STRING == bCurrentType)
		{
			--pbParsePtr;
		}
		else
		{
			assert(0);
		}
	}

/* Let's see if it's a string */

	if (byte == '\"')
	{
		while (1)
		{
			if (byte == -1) 
			{
				yyerror("End of file found in string literal");
				return(-1);
			}

			byte = lexfgetc();				/* Get next byte */

			if (byte == '\"')				/* End of string */
			{
				yytext[yylength] = '\0';
				lexval.string = yytext;
				return(STRING);				/* It's a string literal */
			}

			if (byte == '\\')				/* Escaped char! */
			{
				byte = lexfgetc();			/* Get next character */

				if (byte == -1) 
				{
					yyerror("End of file found in escape sequence");
					return(-1);
				}

				if (byte <= ' ')
				{
					yyerror("Invalid escape sequence");
					return(-1);
				}

				/* Escape these characters to their equivalents, otherwise
  				 * just accept the backslashed character as a literal
				 */

				if (byte == 't')
					byte = '\t';			/* Tab */
				if (byte == 'n')
					byte = '\n';			/* Linefeed */
				if (byte == 'b')
					byte = '\b';			/* Backspace */
				if (byte == 'r')
					byte = '\r';			/* Carriage return */
			}
			else
			if ((byte == '\n') || (byte == '\r')) /* Newline! */
			{
				yyerror("Newline in string literal");
				return(-1);
			}

			yytext[yylength++] = byte;			/* Add in our byte */

			if (yylength >= MAX_LEXSIZE)
			{
				yyerror("String size exceeds lexical buffer");
				return(-1);
			}

			yytext[yylength] = '\0';
		}
	}

/* Symbol matcher: [A-Za-z_]+[A-Za-z0-9_]* */

	if (((toupper(byte) >= 'A') && (toupper(byte) <= 'Z')) ||
			byte == '_')								/* Symbol match! */
	{
		yytext[yylength++] = byte;					/* Store byte away */

		while (1)
		{
			byte = lexfgetc();						/* Get next character */

			if (((byte == '_') || ((byte >= '0')) && (byte <='9')) ||
				((toupper(byte) >= 'A') && (toupper(byte) <= 'Z')))
			{
	 			yytext[yylength++] = byte;

				if (yylength >= MAX_LEXSIZE)
				{
					yyerror("Symbol size exceeds lexical buffer");
					assert(0);
				}

				yytext[yylength] = '\0';
			}
			else
			{
				if (byte != -1)
				{
					if (byte == '\n')
						yylineno[fileLevel]--;

					if (LEX_FILE == bCurrentType)
					{
						ungetc(byte, yyin[fileLevel]);	/* Nope... put it back... */
					}
					else
					if (LEX_STRING == bCurrentType)
					{
						--pbParsePtr;
					}
					else
					{
						assert(0);
					}
				}

				break;
			}
		}

		yytext[yylength] = '\0';				/* Null terminate it */
		lexval.string = yytext;				/* Update our union */

		/* Check here to see if it's a reserved word */

		loop = 0;

		yyreserved = NULL;
	
		while (psReserved && psReserved[loop].reservedWordName)
		{
			if (strcmp(psReserved[loop].reservedWordName, yytext) == 0)
			{
				yyreserved = &psReserved[loop];
				return(psReserved[loop].tokenValue);
			}
			else
				loop++;
		}

		return(SYMBOL);						/* It's a symbol! */
	}

/* How about the numeric equivalent? */

	if (byte == '-')							/* Sign or unary minus? */
	{
		yytext[yylength++] = byte;

		byte = lexfgetc();

		if (isdigit(byte) == 0)
		{
			yytext[yylength] = '\0';

			if (byte == '\n')
				yylineno[fileLevel]--;

			if (LEX_FILE == bCurrentType)
			{
				ungetc(byte, yyin[fileLevel]);	/* Nope... put it back... */
			}
			else
			if (LEX_STRING == bCurrentType)
			{
				--pbParsePtr;
			}
			else
			{
				assert(0);
			}

			return('-');						/* It's a unary minus */
		}

		minusSign = 1;							/* It's digit equivalent! */
	}

	if (isdigit(byte))
	{
		lexval.integer = 0;
		radix = LEX_DECIMAL;						/* Assume it's decimal first */

		while (isdigit(byte) || ((toupper(byte) >= 'A') && (toupper(byte) <= 'F')))
		{
			yytext[yylength++] = byte;
			byte = lexfgetc();				/* Get next byte */
		}

		if (toupper(byte) == 'H')			/* Is it hex? */
		{
			radix = LEX_HEX;
			yytext[yylength++] = byte;
		}
		else
		if (byte != -1)
		{	
			if (byte == '\n')
				yylineno[fileLevel]--;

			if (LEX_FILE == bCurrentType)
			{
				ungetc(byte, yyin[fileLevel]);	/* Nope... put it back... */
			}
			else
			if (LEX_STRING == bCurrentType)
			{
				--pbParsePtr;
			}
			else
			{
				assert(0);
			}
		}

		if ((radix != LEX_HEX) && (toupper(yytext[yylength - 1]) == 'B'))
			radix = LEX_BINARY;

		loop = minusSign;
		yytext[yylength] = '\0';

		if (radix == LEX_DECIMAL)
		{
			while ((yytext[loop] != '\0') && (isdigit(yytext[loop])))
				++loop;

			if (yytext[loop] != '\0')
			{
				yyerror("Invalid characters in decimal value");
				return(-1);
			}
			lexval.integer = atol(yytext);
		}

		if (radix == LEX_BINARY)
		{
			if (minusSign)
				yyerror("Negative binary value invalid");

			lexval.integer = 0;

			while ((yytext[loop] != '\0') &&
					 ((yytext[loop] == '0') || (yytext[loop] == '1')))
			{
				lexval.integer = (signed long int) (lexval.integer << 1);

				if (yytext[loop] == '1')
					lexval.integer = lexval.integer | 1;
				++loop;
			}

			if (toupper(yytext[loop]) != 'B')
			{
				yyerror("Invalid characters in binary number");
				return(-1);
			}

			++loop;

			if (yytext[loop] != '\0')
			{
				yyerror("Extra character beside binary number");
				return(-1);
			}
		}

		if (radix == LEX_HEX)
		{
			while ((yytext[loop] != '\0') && (ishex(yytext[loop])))
			{
				lexval.integer <<= 4;				/* Move over for next nibble */	

				if (isdigit(yytext[loop]) == 0)
					byte = toupper(yytext[loop]) - 55;
				else
					byte = yytext[loop] - '0';
				lexval.integer |= byte;
				++loop;
			}

			if (toupper(yytext[loop]) != 'H')
			{
				yyerror("Invalid characters following hex number");
				return(-1);
			}
		}
	
		return(CONSTANT);
	}

	*yytext = byte;
	yytext[1] = '\0';
	return(byte);
}

/************************************************************************
 *
 * Name : SetReservedWordlist
 *
 * Entry: Pointer to list of reserved word structures
 *
 * Exit : Nothing
 *
 * Description:
 *
 * This routine sets the reserved word list to be used when parsing
 *
 ************************************************************************/

void SetReservedWordlist(struct sReservedWords *psReservedWords)
{
	assert(psReservedWords);
	psReserved = psReservedWords;
}

/************************************************************************
 *					
 * Name : LookupReservedWord()
 *			 
 * Entry: Token ID of reserved word
 *			 
 * Exit : NULL If not found, otherwise pointer to the token's name
 *					
 * Description:
 *					
 * This routine will look up a token based upon its ID and return the name
 * 				
 ************************************************************************/

UINT8 *LookupReservedWord(UINT32 dwTokenId)
{
	UINT32 dwLoop = 0;

	while (psReserved[dwLoop].reservedWordName)
	{
		if (psReserved[dwLoop].tokenValue == dwTokenId)
			return(psReserved[dwLoop].reservedWordName);

		++dwLoop;
	}

	return(NULL);
}

