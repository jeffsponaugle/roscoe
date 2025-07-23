#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "BIOS/OS.h"
#include "Shared/lex.h"

// Table of various ASCII flags
const uint8_t sg_u8CharFlags[256] =
{
	0,										// 00h
	0,										// 01h
	0,										// 02h
	0,										// 03h
	0,										// 04h
	0,										// 05h
	0,										// 06h
	0,										// 07h
	0,										// 08h
	C_SPACE,								// 09h
	C_NEWLINE | C_SPACE,					// 0ah - We consider an LF a newline
	C_SPACE,								// 0bh
	C_SPACE,								// 0ch
	C_NEWLINE | C_SPACE,					// 0dh - As well as a CR
	0,										// 0eh
	0,										// 0fh
	0,										// 10h
	0,										// 11h
	0,										// 12h
	0,										// 13h
	0,										// 14h
	0,										// 15h
	0,										// 16h
	0,										// 17h
	0,										// 18h
	0,										// 19h
	C_SPACE,								// 1ah - Ctrl-Z
	0,										// 1bh
	0,										// 1ch
	0,										// 1dh
	0,										// 1eh
	0,										// 1fh
	C_SPACE,								// 20h
	0,										// 21h
	0,										// 22h
	0,										// 23h
	0,										// 24h
	0,										// 25h
	0,										// 26h
	0,										// 27h
	0,										// 28h
	0,										// 29h
	0,										// 2ah
	0,										// 2bh
	0,										// 2ch
	0,										// 2dh
	C_FLOAT,								// 2eh
	0,										// 2fh
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 30h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 31h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 32h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 33h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 34h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 35h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 36h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 37h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 38h
	C_HEX | C_DIGIT | C_IDENT | C_FLOAT,	// 39h
	0,										// 3ah
	0,										// 3bh
	0,										// 3ch
	0,										// 3dh
	0,										// 3eh
	0,										// 3fh
	0,										// 40h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,	// 41h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,	// 42h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,	// 43h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN | C_FLOAT,	// 44h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN | C_FLOAT,	// 45h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,	// 46h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 47h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 48h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 49h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 4ah
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 4bh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 4ch
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 4dh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 4eh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 4fh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 50h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 51h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 52h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 53h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 54h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 55h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 56h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 57h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 58h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 59h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 5ah
	0,										// 5bh
	0,										// 5ch
	0,										// 5dh
	0,										// 5eh
	C_IDENT | C_IDENT_BEGIN,				// 5fh - Underscore
	0,										// 60h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,				// 61h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,				// 62h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,				// 63h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN | C_FLOAT,				// 64h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN | C_FLOAT,				// 65h
	C_HEX | C_ALPHA | C_IDENT | C_IDENT_BEGIN,				// 66h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 67h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 68h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 69h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 6ah
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 6bh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 6ch
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 6dh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 6eh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 6fh
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 70h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 71h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 72h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 73h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 74h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 75h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 76h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 77h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 78h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 79h
	C_ALPHA | C_IDENT | C_IDENT_BEGIN,		// 7ah
	0,										// 7bh
	0,										// 7ch
	0,										// 7dh
	0,										// 7eh
	0,										// 7fh
	0,										// 80h
	0,										// 81h
	0,										// 82h
	0,										// 83h
	0,										// 84h
	0,										// 85h
	0,										// 86h
	0,										// 87h
	0,										// 88h
	0,										// 89h
	0,										// 8ah
	0,										// 8bh
	0,										// 8ch
	0,										// 8dh
	0,										// 8eh
	0,										// 8fh
	0,										// 90h
	0,										// 91h
	0,										// 92h
	0,										// 93h
	0,										// 94h
	0,										// 95h
	0,										// 96h
	0,										// 97h
	0,										// 98h
	0,										// 99h
	0,										// 9ah
	0,										// 9bh
	0,										// 9ch
	0,										// 9dh
	0,										// 9eh
	0,										// 9fh
	0,										// a0h
	0,										// a1h
	0,										// a2h
	0,										// a3h
	0,										// a4h
	0,										// a5h
	0,										// a6h
	0,										// a7h
	0,										// a8h
	0,										// a9h
	0,										// aah
	0,										// abh
	0,										// ach
	0,										// adh
	0,										// aeh
	0,										// afh
	0,										// b0h
	0,										// b1h
	0,										// b2h
	0,										// b3h
	0,										// b4h
	0,										// b5h
	0,										// b6h
	0,										// b7h
	0,										// b8h
	0,										// b9h
	0,										// bah
	0,										// bbh
	0,										// bch
	0,										// bdh
	0,										// beh
	0,										// bfh
	0,										// c0h
	0,										// c1h
	0,										// c2h
	0,										// c3h
	0,										// c4h
	0,										// c5h
	0,										// c6h
	0,										// c7h
	0,										// c8h
	0,										// c9h
	0,										// cah
	0,										// cbh
	0,										// cch
	0,										// cdh
	0,										// ceh
	0,										// cfh
	0,										// d0h
	0,										// d1h
	0,										// d2h
	0,										// d3h
	0,										// d4h
	0,										// d5h
	0,										// d6h
	0,										// d7h
	0,										// d8h
	0,										// d9h
	0,										// dah
	0,										// dbh
	0,										// dch
	0,										// ddh
	0,										// deh
	0,										// dfh
	0,										// e0h
	0,										// e1h
	0,										// e2h
	0,										// e3h
	0,										// e4h
	0,										// e5h
	0,										// e6h
	0,										// e7h
	0,										// e8h
	0,										// e9h
	0,										// eah
	0,										// ebh
	0,										// ech
	0,										// edh
	0,										// eeh
	0,										// efh
	0,										// f0h
	0,										// f1h
	0,										// f2h
	0,										// f3h
	0,										// f4h
	0,										// f5h
	0,										// f6h
	0,										// f7h
	0,										// f8h
	0,										// f9h
	0,										// fah
	0,										// fbh
	0,										// fch
	0,										// fdh
	0,										// feh
	0										// ffh
};

void LexEmitError(SLex *psLex,
				  SToken *psToken,
				  char *peFormat,
				  ...)
{
	va_list ap;
	char eErrorString[100];

	va_start(ap, peFormat);
	vsnprintf(eErrorString,
			  sizeof(eErrorString) - 1,
			  peFormat,
			  ap);
	va_end(ap);

	printf("%s\n", eErrorString);
}

bool LexCheckNextToken(SLex *psLex,
					   ETokenType eTokenExpected,
					   SToken *psToken)
{
	ETokenType eToken;
	
	memset((void *) psToken, 0, sizeof(*psToken));
	
	eToken = LexGetNextToken(psLex,
							 psToken);
	
	if (ELEX_EOF == eToken)
	{
		LexEmitError(psLex,
					 psToken,
					 "Expected '%c', not end of file",
					 eToken);
		LexClearToken(psToken);
		return(false);
	}
	
	if (eToken != eTokenExpected)
	{
		LexEmitError(psLex,
					 psToken,
					 "Expected '%c', not '%s' end of file",
					 eToken,
					 psLex->eParseString);
		LexClearToken(psToken);
		return(false);
	}
	else
	{
		return(true);
	}
}

static ETokenType LexGetChar(SLex *psLex,
							 uint32_t *pu32LineNumber,
							 uint32_t *pu32ColumnNumber)
{
	ETokenType eChar;
	EStatus eStatus;
	
	// If psLex is NULL or there is no open file, just return EOF, but only if our
	// source is a file and not an in memory buffer
	if (NULL == psLex->peBufferHead)
	{
		if (NULL == psLex)
		{
			return(ELEX_EOF);
		}
	}

	if (psLex->u32CharHead != psLex->u32CharTail)
	{
		// This means we've rewound a character and we need to pull it off the stack.
		if (pu32LineNumber)
		{
			*pu32LineNumber = psLex->sChar[psLex->u32CharTail].u32Line;
		}
		
		if (pu32ColumnNumber)
		{
			*pu32ColumnNumber = psLex->sChar[psLex->u32CharTail].u32Column;
		}
		
		eChar = psLex->sChar[psLex->u32CharTail].eCharacter;
		psLex->u32CharTail++;
		if (psLex->u32CharTail >= (sizeof(psLex->sChar) / sizeof(psLex->sChar[0])))
		{
			psLex->u32CharTail = 0;
		}
	}
	else
	{
		if (psLex->peBufferPtr)
		{
			// If our buffer pointer is pointing to \0, then we're at the end of the buffer
			if ('\0' == *psLex->peBufferPtr)
			{
				return(ELEX_EOF);
			}

			eChar = (ETokenType) *psLex->peBufferPtr;
			++psLex->peBufferPtr;
		}
		else
		{
			if (psLex->u32FileBufferPos >= psLex->u32FileBufferValid)
			{
				uint32_t u32DataRead = 0;

				// Must get data out of the file buffer from disk, here.
				eStatus = psLex->psStreamFunctions->Read(psLex,
														 (void *) psLex->u8FileBuffer,
														 sizeof(psLex->u8FileBuffer),
														 &u32DataRead,
														 psLex->pvStreamData);
				if (eStatus != ESTATUS_OK)
				{
					// Bad file read
					eChar = ELEX_EOF;
					goto tokenizeExit;
				}
				
				psLex->u32FileBufferValid = u32DataRead;
				psLex->u32FileBufferPos = 0;
				
				// No characters to be had. End of file.
				if (0 == psLex->u32FileBufferValid)
				{
					return(ELEX_EOF);
				}
			}

			// Get the character
			eChar = (ETokenType) psLex->u8FileBuffer[psLex->u32FileBufferPos++];
		}
		
		if (pu32LineNumber)
		{
			*pu32LineNumber = psLex->u32LineNumber;
		}
		
		if (pu32ColumnNumber)
		{
			*pu32ColumnNumber = psLex->u32ColumnNumber;
		}
   	
		psLex->sChar[psLex->u32CharHead].eCharacter = eChar;
		psLex->sChar[psLex->u32CharHead].u32Line = psLex->u32LineNumber;
		psLex->sChar[psLex->u32CharHead++].u32Column = psLex->u32ColumnNumber;
		
		if (psLex->u32CharHead >= (sizeof(psLex->sChar) / sizeof(psLex->sChar[0])))
		{
			psLex->u32CharHead = 0;
		}
		
		psLex->u32CharTail = psLex->u32CharHead;
	}

	// Advance the column andline #s.
	if ('\t' == eChar)
	{
		// Assume tab stop of 8 characters
		assert(psLex->u32ColumnNumber);
		psLex->u32ColumnNumber = (((psLex->u32ColumnNumber - 1) + 7) & 0xfffffff8) + 1;
	}
	else
	if ('\r' == eChar)
	{
		psLex->u32ColumnNumber = 1;
	}
	else
	if ('\n' == eChar)
	{
		psLex->u32LineNumber++;
		psLex->u32ColumnNumber = 1;
	}
	else
	{
		psLex->u32ColumnNumber++;
	}
	
tokenizeExit:
	return(eChar);
}

static void LexRewindChar(SLex *psLex,
						  uint8_t u8RewindCount)
{
	uint32_t u32Tail = psLex->u32CharTail;

	while (u8RewindCount)
	{
		if (0 == u32Tail)
		{
			u32Tail = (sizeof(psLex->sChar) / sizeof(psLex->sChar[0])) - 1;
		}
		else
		{
			u32Tail--;
		}
	
		// If this asserts, it means you've rewound beyond the history of the character buffer
		assert(u32Tail != psLex->u32CharHead);
		
		u8RewindCount--;
	}
	
	psLex->u32CharTail = u32Tail;
}

static ETokenType LexEatWhitespace(SLex *psLex,
								   uint32_t *pu32LineNumber,
								   uint32_t *pu32ColumnNumber)
{
	ETokenType eToken;
	
	do
	{
		eToken = LexGetChar(psLex,
							pu32LineNumber,
							pu32ColumnNumber);
		
		if (eToken >= ELEX_IDENTIFIER)
		{
			return(eToken);
		}
	}
	while (ISSPACE(eToken));
	
	return(eToken);
}

static bool LexConvertIntParse(SLex *psLexContext,
							   SToken *psToken)
{
	bool bLeadingMinus = false;
	char *peChar;

	// Skip +/-
	peChar = psLexContext->eParseString;
	if (('+' == *peChar) ||
		('-' == *peChar))
	{
		if ('-' == *peChar)
		{
			bLeadingMinus = true;
		}
		
		++peChar;
	}

	if (bLeadingMinus)
	{
		psToken->eTokenType = ELEX_INT_SIGNED;
	}
	else
	{
		psToken->eTokenType = ELEX_INT_UNSIGNED;
	}
	
	psToken->uData.s64IntValue = 0;

	while (*peChar)
	{
		psToken->uData.s64IntValue = (psToken->uData.s64IntValue * 10) + (*peChar - '0');
		if (psToken->uData.s64IntValue >> 63)
		{
			// We've overflowed
			LexEmitError(psLexContext,
						 psToken,
						 "Constant smaller than -2^63");
			return(false);
	}
		
		++peChar;
	}

	if (bLeadingMinus)
	{
		psToken->uData.s64IntValue = -psToken->uData.s64IntValue;
	}

	return(true);
}

ETokenType LexGetNextToken(SLex *psLex,
						   SToken *psToken)
{
	ETokenType eChar;
	
parseAgain:
	// If this asserts, it means you forgot to clear out an ELEX_STRING. It is the callee's responsibility
	assert(NULL == psToken->peString);
	
	// Clear out our accumulated string value
	psLex->u32StringOffset = 0;
	psLex->eParseString[0] = '\0';
	
	// Set it invalid to start with (just in case we have a bug)
	psToken->eTokenType = ELEX_EOF;
	psToken->u32TokenLength = 0;
	memset((void *) &psToken->uData, 0, sizeof(psToken->uData));
	
	// Get rid of any whitespace
	eChar = LexEatWhitespace(psLex,
							 &psToken->u32LineNumber,
							 &psToken->u32ColumnNumber);
	if (ELEX_EOF == eChar)
	{
		goto tokenizeExit;
	}

	// Is it a comment character sequence possibly?
	if ('/' == eChar)
	{
		// Looking for double slashes or slash asterisk
		eChar = LexGetChar(psLex,
						   NULL,
						   NULL);
		if ((ETokenType) '/' == eChar)
		{
			// Loop until we hit a carraige return/end of line
			while ((eChar < ELEX_IDENTIFIER) &&
				   (ISNEWLINE(eChar) == false))
			{
				eChar = LexGetChar(psLex,
								   NULL,
								   NULL);
			}
			
			// If this is greater than ELEX_IDENTIFIER, we've got another issue (like end of file)
			if (eChar >= ELEX_IDENTIFIER)
			{
				goto tokenizeExit;
			}
			
			// Go parse this again
			goto parseAgain;
		}
		else
		if ((ETokenType) '*' == eChar)
		{
			// Loop until we see */ to terminate
			while (1)
			{
				eChar = LexGetChar(psLex,
								   NULL,
								   NULL);
				if (eChar >= ELEX_IDENTIFIER)
				{
					goto tokenizeExit;
				}

				if ((ETokenType) '*' == eChar)
				{
					// Let's see if this is a /. If it is, we're done with the comment
					eChar = LexGetChar(psLex,
									   NULL,
									   NULL);
					if (eChar >= ELEX_IDENTIFIER)
					{
						goto tokenizeExit;
					}
					
					if ((ETokenType) '/' == eChar)
					{
						// Yup, that's it. Go parse again.
						goto parseAgain;
					}
					else
					{
						// Nope, not it. Back up one character and keep going
						LexRewindChar(psLex, 1);
					}
				}
			}
		}
		else
		{
			// It's a single character (slash). Rewind the stream by 1 character
			LexRewindChar(psLex, 1);
			
			psToken->u32TokenLength = 1;
			goto tokenizeExit;
		}
	}
	
	psLex->eParseString[psLex->u32StringOffset++] = (char) eChar;
	psLex->eParseString[psLex->u32StringOffset] = '\0';
	
	// Here we know it's actually something. Time for the real parsing to begin.
	if (ISIDENT_BEGIN(eChar))
	{
		const SReservedWord *psReserved = NULL;

		while (1)
		{
			eChar = LexGetChar(psLex,
							   NULL,
							   NULL);
			if (eChar >= ELEX_IDENTIFIER)
			{
				// Internal issue of some sort
				break;
			}
			
			if (false == ISIDENT(eChar))
			{
				// No longer an identifier character
				break;
			}
			
			psLex->eParseString[psLex->u32StringOffset++] = (char) eChar;
			psLex->eParseString[psLex->u32StringOffset] = '\0';
			
			if (psLex->u32StringOffset >= (sizeof(psLex->eParseString) - 1))
			{
				LexEmitError(psLex,
							 psToken,
							 "Identifier too long - reached maximum of %u characters\n",
							 sizeof(psLex->eParseString) - 1);
				eChar = ELEX_EOF;
				goto tokenizeExit;
			}
		}
		
		if (eChar != ELEX_EOF)
		{
			// Back up one character
			LexRewindChar(psLex,
						  1);
		}
		
		// Record the length of the identifier/token
		psToken->u32TokenLength = psLex->u32StringOffset;
							 
		// We're done accumulating the parse string. Let's see if it's a reserved word.
		psReserved = psLex->psReservedWords;
		if (psReserved)
		{
			while (psReserved->peReservedWord)
			{
				if (strcmp(psReserved->peReservedWord, psLex->eParseString) == 0)
				{
					// Found it
					break;
				}
				
				++psReserved;
			}
		}
		
		if ((psReserved) && (psReserved->peReservedWord))
		{
			// It's a reserved word. Tokenize it, and point peString to the reserved word text
			eChar = psReserved->eToken;
			psToken->peString = (char *) psReserved->peReservedWord;
		}
		else
		{
			// Not a reserved word. It's an identifier.
			eChar = ELEX_IDENTIFIER;
			psToken->peString = malloc(psToken->u32TokenLength + 1);
			if (NULL == psToken->peString)
			{
				LexEmitError(psLex,
							 psToken,
							 "Out of memory while allocating memory for identifier '%s'\n", psLex->eParseString);
				eChar = ELEX_EOF;
			}
			else
			{
				strcpy(psToken->peString, psLex->eParseString);
			}
		}
		
		goto tokenizeExit;
	}
	

	// Turn +/- into unary qualifiers
	if (((ETokenType) '+' == eChar) ||
		((ETokenType) '-' == eChar))
	{
		psLex->eParseString[psLex->u32StringOffset++] = (char) eChar;
		goto tokenizeExit;
	}
	
	// String?
	if ((ETokenType) '\"' == eChar)
	{
		// Yup! Let's start over on the string that we're absorbing
		psLex->u32StringOffset = 0;
		
		while (1)
		{
			eChar = LexGetChar(psLex,
							   NULL,
							   NULL);
			
			if (eChar >= ELEX_IDENTIFIER)
			{
				LexEmitError(psLex,
							 psToken,
							 "End of file - unterminated string");
				goto tokenizeExit;
			}
			
			if ((ETokenType) '\"' == eChar)
			{
				// End of string!
				break;
			}
			else
			if ((ETokenType) '\\' == eChar)
			{
				// Backslash?
				eChar = LexGetChar(psLex,
								   NULL,
								   NULL);
				if (eChar >= ELEX_IDENTIFIER)
				{
					LexEmitError(psLex,
								 psToken,
								 "End of file - escape sequence ");
					goto tokenizeExit;
				}
				
				if ((ETokenType) 'n' == eChar)
				{
					eChar = (ETokenType) '\n';
				}
				else
				if ((ETokenType) 'r' == eChar)
				{
					eChar = (ETokenType) '\r';
				}
				else
				if ((ETokenType) 't' == eChar)
				{
					eChar = (ETokenType) '\t';
				}
				else
				{
					// Just use the character as-is
				}
			}
			
			psLex->eParseString[psLex->u32StringOffset++] = (char) eChar;	
			if (psLex->u32StringOffset >= (sizeof(psLex->eParseString) - 1))
			{
				LexEmitError(psLex,
							 psToken,
							 "String too long - reached maximum of %u characters\n",
							 sizeof(psLex->eParseString) - 1);
				eChar = ELEX_EOF;
				goto tokenizeExit;
			}
		}
		
		// All done. Now we copy the string.
		psLex->eParseString[psLex->u32StringOffset] = '\0';
		
		psToken->peString = malloc(psLex->u32StringOffset + 1);
		if (NULL == psToken->peString)
		{
			LexEmitError(psLex,
						 psToken,
						 "Out of memory while allocating memory for identifier '%s'\n", psLex->eParseString);
			eChar = ELEX_EOF;
		}
		else
		{
			strcpy(psToken->peString, psLex->eParseString);
			eChar = ELEX_STRING;
		}
		
		psToken->u32TokenLength = psLex->u32StringOffset;
		psToken->eTokenType = eChar;
		goto tokenizeExit;
	}

	if (ISDIGIT(eChar))
	{
		uint8_t u8Digits = 0;
		uint64_t u64OldInt;

		if ((ETokenType) '0' == eChar)
		{
			ETokenType eChar2;
			bool bBinary = false;

			eChar2 = LexGetChar(psLex,
								NULL,
								NULL);

			// 0x - Hex
			// 0y - Binary

			if (((ETokenType) 'x' == eChar2) ||
				(((ETokenType) 'X') == eChar2) ||
				((ETokenType) 'b' == eChar2) ||
				(((ETokenType) 'B') == eChar2))
			{
				if (((ETokenType)'b' == eChar2) ||
					(((ETokenType) 'B') == eChar2))
				{
					bBinary = true;
				}
				
				// Hex number
				while (1)
				{

					eChar2 = LexGetChar(psLex,
										NULL,
										NULL);
					if (eChar2 >= ELEX_IDENTIFIER)
					{
						// All done
						break;
					}
					
					if (bBinary)
					{
						if ((((ETokenType) '0') == eChar2) ||
							(((ETokenType) '1') == eChar2))
						{
							u64OldInt = psToken->uData.u64IntValue;
							psToken->uData.u64IntValue = (psToken->uData.u64IntValue << 1) | (eChar2 - '0');
							if (psToken->uData.u64IntValue < u64OldInt)
							{
								// This means we've overflowed.
								LexEmitError(psLex,
											 psToken,
											 "Too many binary digits - beyond 64 bit - overflow");
								eChar = ELEX_EOF;
								goto tokenizeExit;
							}

							u8Digits++;
						}
						else
						{
							// Rewind a character - no longer binary
							LexRewindChar(psLex, 1);
							break;
						}
					}
					else
					{
						// Is it a hex digit?
						if (ISHEX(eChar2))
						{
							// Yup! Convert from ASCII to binary
							if ((eChar2 >= 'a') && (eChar2 <= 'f'))
							{
								// Convert to upper case
								eChar2 -= 0x20;
							}
							
							eChar2 -= '0';
							if (eChar2 > 9)
							{
								eChar2 -= 7;
							}
							
							u64OldInt = psToken->uData.u64IntValue;
							psToken->uData.u64IntValue = (psToken->uData.u64IntValue << 4) | eChar2;
							if (psToken->uData.u64IntValue < u64OldInt)
							{
								// This means we've overflowed.
								LexEmitError(psLex,
											 psToken,
											 "Too many hex digits - beyond 64 bit - overflow");
								eChar = ELEX_EOF;
								goto tokenizeExit;
							}
							
							u8Digits++;
						}
						else
						{
							// No. Let's rewind a character, then exit.
							LexRewindChar(psLex, 1);
							break;
						}
					}
				}
				
				if (0 == u8Digits)
				{
					// This means we didn't get anything. Inform the user there's a syntax error.
					LexEmitError(psLex,
								 psToken,
								 "0x/0y requires one or more hex/binary digits to follow");
					eChar = ELEX_EOF;
					goto tokenizeExit;
				}
				
				psToken->u8Digits = u8Digits;				
				eChar = ELEX_INT_UNSIGNED;
				goto tokenizeExit;
			}
			
			// Back up over the characters we just ate
			LexRewindChar(psLex, 1);
		}

		// If we get here, we assume it's decimal entirely
		while (1)
		{
			uint64_t u64OldInt;

			if (ISDIGIT(eChar))
			{
				eChar -= '0';
				u64OldInt = psToken->uData.u64IntValue;
				psToken->uData.u64IntValue = (psToken->uData.u64IntValue * 10) + eChar;
				if (psToken->uData.u64IntValue < u64OldInt)
				{
					// This means we've overflowed.
					LexEmitError(psLex,
								 psToken,
								 "Too many decimal digits - beyond 64 bit - overflow");
					eChar = ELEX_EOF;
					goto tokenizeExit;
				}

				u8Digits++;
				eChar = LexGetChar(psLex,
									NULL,
									NULL);
				if (eChar >= ELEX_IDENTIFIER)
				{
					// All done
					break;
				}
			}
			else
			{
				// No. Let's rewind a character, then exit.
				LexRewindChar(psLex, 1);
				break;
			}
		}

		psToken->u8Digits = u8Digits;				
		eChar = ELEX_INT_UNSIGNED;
		goto tokenizeExit;
	}

tokenizeExit:
	psToken->eTokenType = eChar;
	return(eChar);
}

void LexClearToken(SToken *psToken)
{
	// If it's an identifier or a string, then we need to deallocate psToken->peString if it's allocated
	if ((ELEX_IDENTIFIER == psToken->eTokenType) ||
		(ELEX_STRING == psToken->eTokenType))
	{
		free(psToken->peString);
	}
	
	// Clear out the token entirely
	memset((void *) psToken, 0, sizeof(*psToken));
}

void LexClose(SLex **ppsLex)
{
	if (*ppsLex)
	{
		// If we have stream functions
		if ((*ppsLex)->psStreamFunctions)
		{
			// Close down the stream
			if ((*ppsLex)->psStreamFunctions->Close)
			{
				EStatus eStatus;
				eStatus = (*ppsLex)->psStreamFunctions->Close(*ppsLex,
															  (*ppsLex)->pvStreamData);
			}
		}
		
		free(*ppsLex);
		*ppsLex = NULL;
	}
}

EStatus LexOpen(const SLexStreamFunctions *psLexStreamFunctions,
				void *pvStreamData,
				SLex **ppsLex,
				const SReservedWord *psReservedWord)
{
	EStatus eStatus = ESTATUS_OK;
	
	// Create a lexical context
	*ppsLex = calloc(1, sizeof(**ppsLex));
	if (NULL == *ppsLex)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto tokenizeExit;
	}
	
	(*ppsLex)->psStreamFunctions = psLexStreamFunctions;
	(*ppsLex)->pvStreamData = pvStreamData;
	(*ppsLex)->psReservedWords = psReservedWord;
	(*ppsLex)->u32CharHead = 0;
	(*ppsLex)->u32CharTail = 0;
	(*ppsLex)->u32LineNumber = 1;
	(*ppsLex)->u32ColumnNumber = 1;

	// Call the open function
	eStatus = psLexStreamFunctions->Open(*ppsLex,
										 pvStreamData);
	
tokenizeExit:
	return(eStatus);
}

EStatus LexOpenBuffer(char *peBufferHead,
					  SLex **ppsLex,
					  const SReservedWord *psReservedWord)
{
	EStatus eStatus = ESTATUS_OK;

	// Create a lexical context
	*ppsLex = calloc(1, sizeof(**ppsLex));
	if (NULL == *ppsLex)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto tokenizeExit;
	}

	(*ppsLex)->psReservedWords = psReservedWord;
	(*ppsLex)->peBufferHead = peBufferHead;
	(*ppsLex)->peBufferPtr = peBufferHead;
	(*ppsLex)->u32CharHead = 0;
	(*ppsLex)->u32CharTail = 0;
	(*ppsLex)->u32LineNumber = 1;
	(*ppsLex)->u32ColumnNumber = 1;

	eStatus = ESTATUS_OK;

tokenizeExit:
	return(eStatus);
}

EStatus LexGetBufferPosition(SLex *psLex,
							 char **ppeBufferPtr)
{
	EStatus eStatus;

	if (psLex->peBufferHead)
	{
		if (ppeBufferPtr)
		{
			*ppeBufferPtr = psLex->peBufferPtr;
		}

		eStatus = ESTATUS_OK;
	}
	else
	{
		eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
	}

	return(eStatus);
}


// This routine will expect a string followed by semicolon, and will return
// the string to the user-supplied buffer

EStatus LexGetString(SLex *psLex,
					 char *peStringBuffer,
					 uint32_t u32StringBufferSize,
					 bool *pbTruncated)
{
	EStatus eStatus = ESTATUS_OK;
	SToken sToken;
	
	memset((void *) &sToken, 0, sizeof(sToken));
	
	// Assume we didn't truncate
	if (pbTruncated)
	{
		*pbTruncated = false;
	}
	
	// Thia had better be a string
	if (false == LexCheckNextToken(psLex,
								   ELEX_STRING,
								   &sToken))
	{
		eStatus = ESTATUS_PARSE_STRING_EXPECTED;
		goto errorExit;
	}

	if (strlen(sToken.peString) >> (u32StringBufferSize))
	{
		if (pbTruncated)
		{
			*pbTruncated = true;
		}
	}
	
	strncpy(peStringBuffer, sToken.peString, u32StringBufferSize - 1);
	LexClearToken(&sToken);
	
	// Needs to be a semicolon - end of line
	if (false == LexCheckNextToken(psLex,
								   (ETokenType) ';',
								   &sToken))
	{
		eStatus = ESTATUS_PARSE_SEMICOLON_EXPECTED;
	}

errorExit:
	return(eStatus);
}

/*
extern EStatus LexGetNumericType(SLex *psLex,
								 ELexNumericTypes eNumType,
								 void *pvTargetData);

#define LexGetUINT8(x, y)		LexGetNumericType(x, ELEXNUMTYPE_UINT8, (void *) y)
#define LexGetINT8(x, y)		LexGetNumericType(x, ELEXNUMTYPE_INT8, (void *) y)
#define LexGetUINT16(x, y)		LexGetNumericType(x, ELEXNUMTYPE_UINT16, (void *) y)
#define LexGetINT16(x, y)		LexGetNumericType(x, ELEXNUMTYPE_INT16, (void *) y)
#define LexGetUINT32(x, y)		LexGetNumericType(x, ELEXNUMTYPE_UINT32, (void *) y)
#define LexGetINT32(x, y)		LexGetNumericType(x, ELEXNUMTYPE_INT32, (void *) y)
#define LexGetUINT64(x, y)		LexGetNumericType(x, ELEXNUMTYPE_UINT64, (void *) y)
#define LexGetINT64(x, y)		LexGetNumericType(x, ELEXNUMTYPE_INT64, (void *) y)
#define	LexGetFloat(x, y)		LexGetNumericType(x, ELEXNUMTYPE_FLOAT, (void *) y)
#define LexGetDouble(x, y)		LexGetNumericType(x, ELEXNUMTYPE_DOUBLE, (void *) y)
*/

EStatus LexGetNumericType(SLex *psLex,
						  ELexNumericTypes eNumType,
						  void *pvTargetData)
{
	EStatus eStatus = ESTATUS_OK;
	SToken sToken;
	ETokenType eToken;
	bool bUnaryMinus = false;

	memset((void *) &sToken, 0, sizeof(sToken));

	// Get the next token
	eToken = LexGetNextToken(psLex,
							 &sToken);

	// Unary minus?
	if ((ETokenType) '-' == eToken)
	{
		// Yes!
		bUnaryMinus = true;
		LexClearToken(&sToken);

		// Get next token
		eToken = LexGetNextToken(psLex,
								 &sToken);

	}

	if (ELEX_EOF == eToken)
	{
		LexEmitError(psLex,
					 &sToken,
					 "Expected numeric value, not end of file",
					 eToken);
		eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
		goto clearAndExit;
	}


	// This had better be a numeric quantity
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		// All good.
	}
	else
	{
		LexEmitError(psLex,
					 &sToken,
					 "Expected numeric value\n");
		eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
		goto clearAndExit;
	}

	// If we have a unary minus, then fix up the number
	if (bUnaryMinus)
	{
		if (ELEX_INT_UNSIGNED == eToken)
		{
			// Make it negative (unsigned and signed occupy the same place in the union)
			eToken = ELEX_INT_SIGNED;
			sToken.uData.s64IntValue = -sToken.uData.s64IntValue;
		}
		else
		if (ELEX_INT_SIGNED == eToken)
		{
			// Let's see if we're positive or negative now.
			sToken.uData.s64IntValue = -sToken.uData.s64IntValue;
			if (sToken.uData.s64IntValue >= 0)
			{
				// Make it unsigned
				eToken = ELEX_INT_UNSIGNED;
			}
		}
		else
		{
			assert(0);
		}
	}

	// Now we check every variable type
	switch (eNumType)
	{
		case ELEXNUMTYPE_UINT8:
		{
			// If it's a signed number, it's not allowed
			if (ELEX_INT_SIGNED == eToken)
			{	
				LexEmitError(psLex,
							 &sToken,
							 "Expecting an unsigned 8 bit number, not %lld", sToken.uData.s64IntValue);
				eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
				goto clearAndExit;
			}

			// If it's unsigned, let's make sure it's in range
			if (ELEX_INT_UNSIGNED == eToken)
			{
				if (sToken.uData.u64IntValue > 255)
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting unsigned 8 bit number, not %lld", sToken.uData.u64IntValue);
					goto clearAndExit;
				}

				// If not, we are good to go
				*((uint8_t *) pvTargetData) = (uint8_t) sToken.uData.u64IntValue;
			}

			break;
		}
		case ELEXNUMTYPE_INT8:
		{
			// If it's a signed number, make sure it's in range
			if (ELEX_INT_SIGNED == eToken)
			{	
				if ((sToken.uData.s64IntValue < -128) ||
					(sToken.uData.s64IntValue > 127))
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting signed 8 bit number, not %lld", sToken.uData.s64IntValue);
					eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
					goto clearAndExit;
				}

				*((int8_t *) pvTargetData) = (int8_t) sToken.uData.s64IntValue;
			}

			// If it's unsigned, let's make sure it's in range
			if (ELEX_INT_UNSIGNED == eToken)
			{
				if (sToken.uData.u64IntValue > 127)
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting signed 8 bit number, not %llu", sToken.uData.u64IntValue);
					eStatus = ESTATUS_PARSE_VALUE_OUT_OF_RANGE;
					goto clearAndExit;
				}

				// If not, we are good to go
				*((int8_t *) pvTargetData) = (int8_t) sToken.uData.u64IntValue;
			}

			break;
		}
		case ELEXNUMTYPE_UINT16:
		{
			// If it's a signed number, it's not allowed
			if (ELEX_INT_SIGNED == eToken)
			{	
				LexEmitError(psLex,
							 &sToken,
							 "Expecting an unsigned 16 bit number, not %lld", sToken.uData.s64IntValue);
				eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
				goto clearAndExit;
			}

			// If it's unsigned, let's make sure it's in range
			if (ELEX_INT_UNSIGNED == eToken)
			{
				if (sToken.uData.u64IntValue > 65535)
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting unsigned 16 bit number, not %llu", sToken.uData.u64IntValue);
					eStatus = ESTATUS_PARSE_VALUE_OUT_OF_RANGE;
					goto clearAndExit;
				}

				// If not, we are good to go
				*((uint16_t *) pvTargetData) = (uint16_t) sToken.uData.u64IntValue;
			}

			break;
		}
		case ELEXNUMTYPE_INT16:
		{
			// If it's a signed number, make sure it's in range
			if (ELEX_INT_SIGNED == eToken)
			{	
				if ((sToken.uData.s64IntValue < -32768) ||
					(sToken.uData.s64IntValue > 32767))
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting an signed 16 bit number, not %lld", sToken.uData.s64IntValue);
					eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
					goto clearAndExit;
				}

				*((int16_t *) pvTargetData) = (int16_t) sToken.uData.s64IntValue;
			}

			// If it's unsigned, let's make sure it's in range
			if (ELEX_INT_UNSIGNED == eToken)
			{
				if (sToken.uData.u64IntValue > 32767)
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting signed 16 bit number, not %llu", sToken.uData.u64IntValue);
					eStatus = ESTATUS_PARSE_VALUE_OUT_OF_RANGE;
					goto clearAndExit;
				}

				// If not, we are good to go
				*((int16_t *) pvTargetData) = (int16_t) sToken.uData.u64IntValue;
			}

			break;
		}
		case ELEXNUMTYPE_UINT32:
		{
			// If it's a signed number, it's not allowed
			if (ELEX_INT_SIGNED == eToken)
			{	
				LexEmitError(psLex,
							 &sToken,
							 "Expecting an unsigned 32 bit number, not %lld", sToken.uData.s64IntValue);
				eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
				goto clearAndExit;
			}

			// If it's unsigned, let's make sure it's in range
			if (ELEX_INT_UNSIGNED == eToken)
			{
				if (sToken.uData.u64IntValue > 4294967295)
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting unsigned 32 bit number, not %llu", sToken.uData.u64IntValue);
					eStatus = ESTATUS_PARSE_VALUE_OUT_OF_RANGE;
					goto clearAndExit;
				}

				// If not, we are good to go
				*((uint32_t *) pvTargetData) = (uint32_t) sToken.uData.u64IntValue;
			}

			break;
		}
		case ELEXNUMTYPE_INT32:
		{
			// If it's a signed number, make sure it's in range
			if (ELEX_INT_SIGNED == eToken)
			{	
				if ((sToken.uData.s64IntValue < -2147483648) ||
					(sToken.uData.s64IntValue > 2147483647))
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting an signed 32 bit number, not %lld", sToken.uData.s64IntValue);
					eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
					goto clearAndExit;
				}

				*((int32_t *) pvTargetData) = (int32_t) sToken.uData.s64IntValue;
			}

			// If it's unsigned, let's make sure it's in range
			if (ELEX_INT_UNSIGNED == eToken)
			{
				if (sToken.uData.u64IntValue > 2147483647)
				{
					LexEmitError(psLex,
								 &sToken,
								 "Expecting signed 32 bit number, not %llu", sToken.uData.u64IntValue);
					eStatus = ESTATUS_PARSE_VALUE_OUT_OF_RANGE;
					goto clearAndExit;
				}

				// If not, we are good to go
				*((int32_t *) pvTargetData) = (int32_t) sToken.uData.u64IntValue;
			}

			break;
		}
		case ELEXNUMTYPE_UINT64:
		{
			// If it's a signed number, it's not allowed
			if (ELEX_INT_SIGNED == eToken)
			{	
				LexEmitError(psLex,
							 &sToken,
							 "Expecting an unsigned 64 bit number, not %lld", sToken.uData.s64IntValue);
				eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
				goto clearAndExit;
			}

			// If it's an integer, just copy the value. Overflows will be detected
			// by the lexer.
			if (ELEX_INT_UNSIGNED == eToken)
			{
				*((uint64_t *) pvTargetData) = sToken.uData.u64IntValue;
			}

			break;
		}
		case ELEXNUMTYPE_INT64:
		{
			// If it's a signed number, it's not allowed
			if (ELEX_INT_SIGNED == eToken)
			{	
				*((int64_t *) pvTargetData) = sToken.uData.s64IntValue;
			}

			// If it's an integer, just copy the value. Overflows will be detected
			// by the lexer.
			if (ELEX_INT_UNSIGNED == eToken)
			{
				if (sToken.uData.u64IntValue > 9223372036854775807)
				LexEmitError(psLex,
							 &sToken,
							 "Expecting an signed 64 bit number, not %lld", sToken.uData.s64IntValue);
				eStatus = ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED;
				goto clearAndExit;
			}

			break;
		}
		default:
		{
			// Bogus numeric type value
			assert(0);
			break;
		}
	}

	// Clear out the token
	LexClearToken(&sToken);

	eStatus = ESTATUS_OK;
	return(eStatus);

clearAndExit:
	LexClearToken(&sToken);
	return(eStatus);
}

int Lexstrcasecmp(char *peString1,
				  char *peString2)
{
	while (*peString1 && *peString2)
	{
		if (toupper(*peString1) != toupper(*peString2))
		{
			return(toupper(*peString1) - toupper(*peString2));
		}

		++peString1;
		++peString2;
	}

	return(*peString1 - *peString2);
}

int Lexstrncasecmp(char *peString1,
				   char *peString2,
				   int s32Chars)
{
	while (*peString1 && *peString2 && s32Chars)
	{
		if (toupper(*peString1) != toupper(*peString2))
		{
			return(toupper(*peString1) - toupper(*peString2));
		}

		++peString1;
		++peString2;
		--s32Chars;
	}

	if (0 == s32Chars)
	{
		return(0);
	}

	if ('\0' == *peString1)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}