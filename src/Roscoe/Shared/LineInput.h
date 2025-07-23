#ifndef _LINEINPUT_H_
#define _LINEINPUT_H_

// Used to record line input history
typedef struct SLineInputHistory
{
	char *peLine;
	struct SLineInputHistory *psNextLink;
} SLineInputHistory;

typedef struct SLineInput
{
	// Fill out this data when calling the line input functions
	uint16_t u16InputMax;		// Maximum input length
	uint8_t u8HistoryMaxDepth;	// Maximum line history depth
	EStatus (*InputGet)(char *peInputChar);		// Wait for a character
	void (*Output)(char *peText,	// Output text routine
				   uint16_t u16Length);

	// This is all internal state keeping
	struct SLineInputHistory *psInputHistory;	// History of lines input
	uint16_t u16LineLength;		// Current line length
	uint16_t u16CursorPosition;	// Where is the cursor on the line?
	
	// Used for interpreting incoming ANSI character sequences
	uint8_t u8ANSIActiveCount;	// If set to 2 or greater, the proper ANSI start escape sequence has been received
	char eANSI[10];				// ANSI Character buffer

} SLineInput;

// Line input functions
extern EStatus LineInputInit(SLineInput *psInput);
extern EStatus LineInputGet(SLineInput *psInput,
							char **ppeLine,
							uint16_t *pu16LineLength);
extern void LineInputDestroy(SLineInput *psInput);

#endif