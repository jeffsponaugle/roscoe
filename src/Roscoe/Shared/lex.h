#ifndef _LEX_H_
#define _LEX_H_

typedef enum
{
	// Skip 0-255, as they are the ASCII characters 
	
	ELEX_IDENTIFIER=0x100,
	ELEX_INT_SIGNED,
	ELEX_INT_UNSIGNED,
	ELEX_STRING,
	
	// Start or reserved words. Do not put them above this line.
	ELEX_RESERVED_WORDS_START,
	
	// End of reserved words. Do not put them below this line.
	ELEX_RESERVED_WORDS_END,

	// Leave these here
	ELEX_INTERNAL_CONTROL=0x7ffffff0,
	ELEX_FERR=0xfffffff8,
	ELEX_EOF=0xfffffff9,
	ELEX_END_OF_LIST=0xffffffff
} ETokenType;

// Token structure
typedef struct SToken
{
	ETokenType eTokenType;				// What is this?
	char *peString;						// String representing this token
	uint32_t u32TokenLength;			// How many bytes is this token?
	uint8_t u8Digits;					// # Of supplied significant digits (for numeric values)
	
	union
	{
		uint64_t u64IntValue;			// Up to 64 bit integer values
		int64_t s64IntValue;			// Signed integer value
	} uData;
	
	uint32_t u32LineNumber;				// What line # is this
	uint32_t u32ColumnNumber;			// What column # is this?
} SToken;

// Reserved word structure
typedef struct SReservedWord
{
	const char *peReservedWord;			// Text of the reserved word
	ETokenType eToken;					// Token type for this reserved word
} SReservedWord;

// Character history
typedef struct SCharHistory
{
	ETokenType eCharacter;
	uint32_t u32Line;
	uint32_t u32Column;
} SCharHistory;

// Character history depth
#define	LEX_CHAR_HISTORY_DEPTH		10

// File buffer
#define	LEX_FILE_BUFFER_SIZE		512

// Biggest string size supported
#define	MAX_PARSE_STRING_SIZE		256

struct SLex;

// List of access functions for lexing
typedef struct SLexStreamFunctions
{
	EStatus (*Open)(struct SLex *psLex,
					void *pvStreamData);
	EStatus (*Read)(struct SLex *psLex,
					void *pvBuffer,
					uint32_t u32BytesToRead,
					uint32_t *pu8BytesRead,
					void *pvStreamData);
	EStatus (*Close)(struct SLex *psLex,
					 void **ppvStreamData);
} SLexStreamFunctions;

// Structure for lexical processing
typedef struct SLex
{
	// Head of text buffer when parsing from memory
	char *peBufferHead;
	char *peBufferPtr;

	// Function pointers to the stream data
	const SLexStreamFunctions *psStreamFunctions;
	void *pvStreamData;
	
	// Current parse string
	char eParseString[MAX_PARSE_STRING_SIZE];
	uint32_t u32StringOffset;
	
	// Character buffer
	SCharHistory sChar[LEX_CHAR_HISTORY_DEPTH];
	uint32_t u32CharHead;
	uint32_t u32CharTail;
	
	// File buffer
	uint8_t u8FileBuffer[LEX_FILE_BUFFER_SIZE];
	uint32_t u32FileBufferValid;
	uint32_t u32FileBufferPos;
	
	// Line and column number of current character
	uint32_t u32LineNumber;
	uint32_t u32ColumnNumber;

	// Reserved words
	const SReservedWord *psReservedWords;
} SLex;

typedef enum
{
	ELEXNUMTYPE_UINT8,
	ELEXNUMTYPE_INT8,
	ELEXNUMTYPE_UINT16,
	ELEXNUMTYPE_INT16,
	ELEXNUMTYPE_UINT32,
	ELEXNUMTYPE_INT32,
	ELEXNUMTYPE_UINT64,
	ELEXNUMTYPE_INT64,
	ELEXNUMTYPE_FLOAT,
	ELEXNUMTYPE_DOUBLE
} ELexNumericTypes;

extern EStatus LexOpen(const SLexStreamFunctions *psLexStreamFunctions,
					   void *pvStreamData,
					   SLex **ppsLex,
					   const SReservedWord *psReservedWord);
extern EStatus LexOpenBuffer(char *peBuffer,
							 SLex **ppsLex,
							 const SReservedWord *psReservedWord);
extern EStatus LexGetBufferPosition(SLex *psLex,
									char **ppeBufferPtr);
extern void LexClose(SLex **ppsLex);
extern ETokenType LexGetNextToken(SLex *psLex,
								  SToken *psToken);
extern void LexClearToken(SToken *psToken);
extern void LexEmitError(SLex *psLexContext,
						 SToken *psToken,
						 char *peFormat,
						 ...);
extern bool LexCheckNextToken(SLex *psLex,
							  ETokenType eTokenExpected,
							  SToken *psToken);
extern ETokenType LexGetNextToken(SLex *psLex,
								  SToken *psToken);
extern EStatus LexGetString(SLex *psLex,
							char *peStringBuffer,
							uint32_t u32StringBufferSize,
							bool *pbTruncated);
extern EStatus LexGetNumericType(SLex *psLex,
								 ELexNumericTypes eNumType,
								 void *pvTargetData);
extern int Lexstrcasecmp(char *peString1,
						 char *peString2);
extern int Lexstrncasecmp(char *peString1,
						  char *peString2,
						  int s32Count);

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

// Character types
#define	C_SPACE			0x01
#define C_HEX			0x02
#define C_DIGIT			0x04
#define C_ALPHA			0x08
#define C_NEWLINE		0x10
#define C_IDENT			0x20
#define C_IDENT_BEGIN	0x40
#define C_FLOAT			0x80

// Character macro - C library functions are available to do this but speed is
// of the essence and its macros are significantly slower on a per character basis
#define	ISSPACE(x)			(sg_u8CharFlags[x] & C_SPACE)
#define	ISHEX(x)			(sg_u8CharFlags[x] & C_HEX)
#define ISDIGIT(x)			(sg_u8CharFlags[x] & C_DIGIT)
#define	ISALPHA(x)			(sg_u8CharFlags[x] & C_ALPHA)
#define ISNEWLINE(x)		(sg_u8CharFlags[x] & C_NEWLINE)
#define ISIDENT(x)			(sg_u8CharFlags[x] & C_IDENT)
#define ISIDENT_BEGIN(x)	(sg_u8CharFlags[x] & C_IDENT_BEGIN)
#define ISFLOAT(x)			(sg_u8CharFlags[x] & C_FLOAT)

extern const uint8_t sg_u8CharFlags[256];

#endif
