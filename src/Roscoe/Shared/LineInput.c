#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "BIOS/OS.h"
#include "../Shared/LineInput.h"

// Destroy all input history lines
static void LineInputHistoryDestroy(SLineInputHistory **ppsLineInputHistory)
{
	while (*ppsLineInputHistory)
	{
		SLineInputHistory *psInputHistory;

		psInputHistory = *ppsLineInputHistory;
		*ppsLineInputHistory = (*ppsLineInputHistory)->psNextLink;
		free(psInputHistory->peLine);
		free(psInputHistory);
	}
}

EStatus LineInputGet(SLineInput *psInput,
					 char **ppeLine,
					 uint16_t *pu16LineLength)
{
	EStatus eStatus;
	char *peInput = NULL;

	// Pull in the maximum length for this input line
	psInput->u16InputMax = *pu16LineLength;
	peInput = *ppeLine;

	if (NULL == peInput)
	{
		// Need to allocate some buffer space for this line input
		if (0 == psInput->u16InputMax)
		{
			eStatus = ESTATUS_BAD_LENGTH;
			goto errorExit;
		}

		// Allocate some space for the line
		peInput = (char *) malloc(psInput->u16InputMax + 1);

		if (NULL == peInput)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			goto errorExit;
		}

		 *peInput = '\0';
		 *ppeLine = peInput;
	}

	// Cursor goes to the end of the line by default
	psInput->u16LineLength = strlen(peInput);
	psInput->u16CursorPosition = psInput->u16LineLength;

	// Display whatever string we may have been handed
	if (psInput->u16LineLength)
	{
		psInput->Output(peInput,
						psInput->u16LineLength);
	}

	// Loop forever until the user hits enter or ctrl-c
	for (;;)
	{
		char eChar;

		// Wait for a character
		eStatus = psInput->InputGet(&eChar);

		// If the input get returns something other than ESTATUS_OK, treat it like a
		// premature exit.
		if (eStatus != ESTATUS_OK)
		{
			psInput->Output("\n",
							2);
			goto errorExit;
		}

		if (0x1b == eChar)
		{
			// Escape? Start the ANSI eating sequence
			psInput->u8ANSIActiveCount = 1;
		}
		else
		// If the character is in the range of printable, allow it
		if ((eChar >= ' ') && (eChar <= 0x7e))
		{
			bool bConsumeCharacter = true;

			if (psInput->u8ANSIActiveCount)
			{
				bConsumeCharacter = false;

				// If we're looking for '[' and we're at the first position, then advance
				if (1 == psInput->u8ANSIActiveCount)
				{
					if ('[' == eChar)
					{
						// We have our escape sequence! Advance the state machine
						++psInput->u8ANSIActiveCount;
					}
					else
					{
						// Not our expected escape sequence - reset the stream
						psInput->u8ANSIActiveCount = 0;
						bConsumeCharacter = true;
					}
				}
				else
				{
					// We should be consuming now. Provided we have room.
					if ((psInput->u8ANSIActiveCount - 2) > (sizeof(psInput->eANSI) - 1))
					{
						// Sequence is too long. Kill the ANSI consuming sequence
						psInput->u8ANSIActiveCount = 0;
						bConsumeCharacter = true;
					}
					else
					{
						// We consume the ANSI sequence
						psInput->eANSI[psInput->u8ANSIActiveCount - 2] = eChar;
						psInput->eANSI[psInput->u8ANSIActiveCount - 1] = 0x0;

						if (isalpha(toupper(eChar)))
						{
							int s32Number;

							// Consume a number if there is one
							s32Number = atol(psInput->eANSI);
							if (0 == s32Number)
							{
								s32Number = 1;
							}

							// Let's see what we've got.
							if (('D' == eChar) ||
								('H' == eChar))
							{
								// If we're home, then set s32Number to the cursor position value
								// so it backs up to the beginning of the line
								if ('H' == eChar)
								{
									s32Number = psInput->u16CursorPosition;
								}

								// Move cursor left/home 
								while ((psInput->u16CursorPosition) && (s32Number))
								{
									psInput->Output("\b",
													1);

									psInput->u16CursorPosition--;
									s32Number--;
								}
							}

							if (('C' == eChar) ||
								('F' == eChar))
							{
								// If we're moving to the end, then set s32Number to the # of characters
								// from the cursor position to the end of the string
								if ('F' == eChar)
								{
									s32Number = psInput->u16LineLength - psInput->u16CursorPosition;
								}

								// Move cursor right
								while ((psInput->u16CursorPosition != psInput->u16LineLength) && (s32Number))
								{
									psInput->Output(peInput + psInput->u16CursorPosition,
													1);

									psInput->u16CursorPosition++;
									s32Number--;
								}
							}

							// Kill the ANSI consumption sequence
							psInput->u8ANSIActiveCount = 0;
						}
						else
						{
							// Keep consuming!
							++psInput->u8ANSIActiveCount;
						}
					}
				}
			}

			if (bConsumeCharacter)
			{
				// Add a character at the cursor position.
				if (psInput->u16LineLength != psInput->u16InputMax)
				{
					uint16_t u16Loop;
				
					// We're somewhere in the middle of the line. Insert a character.
					u16Loop = psInput->u16LineLength;
					while (u16Loop >= psInput->u16CursorPosition)
					{
						*(peInput + u16Loop + 1) = *(peInput + u16Loop);
						if (0 == u16Loop)
						{
							break;
						}
						u16Loop--;
					}
		
					// Add in the character
					*(peInput + psInput->u16CursorPosition) = eChar;
					++psInput->u16CursorPosition;
					++psInput->u16LineLength;
				
					// And ensure the string is 0x00 terminated
					*(peInput + psInput->u16LineLength) = '\0';
				
					// Output the characters after the cursor position
					psInput->Output(peInput + psInput->u16CursorPosition - 1,
									(psInput->u16LineLength - psInput->u16CursorPosition) + 1);
				
					// Now back the cursor up to the cursor position
					u16Loop = psInput->u16LineLength - 1;
					while (u16Loop >= psInput->u16CursorPosition)
					{
						psInput->Output("\b",
										1);
						--u16Loop;
					}
				}
			}
			else
			{
				// No room left in the buffer
			}
		}
		else
		if (('\b' == eChar) ||
			(0x7f == eChar))
		{
			// Must be something in the buffer
			if (psInput->u16LineLength)
			{
				// Backspace or delete
				if (('\b' == eChar) && 
					(0 == psInput->u16CursorPosition))
				{
					// Don't do anything if they've hit backspace and the cursor is at
					// the beginning of the line
				}
				else
				if ((0x7f == eChar) &&
					(psInput->u16CursorPosition == psInput->u16LineLength))
				{
					// End of the string - delete doesn't do anything
				}
				else
				{
					uint16_t u16Loop;

					// If we're doing a backsapce, back the cursor up one position first
					if ('\b' == eChar)
					{
						--psInput->u16CursorPosition;
						psInput->Output("\b",
										1);
					}
					
					// If not the end of the line, we need to delete a character 
					// and add 1 for the 0x00 character at the end)
					memcpy((void *) (peInput + psInput->u16CursorPosition),
						   (void *) (peInput + psInput->u16CursorPosition + 1),
						   (psInput->u16LineLength - psInput->u16CursorPosition) + 1);

					// We've deleted a character. Back up a character
					--psInput->u16LineLength;

					// Output from cursor to end of string
					psInput->Output(peInput + psInput->u16CursorPosition,
									psInput->u16LineLength - psInput->u16CursorPosition);

					// Erase the character at the end
					psInput->Output(" ",
									1);

					// Back up the cursor if necessary
					u16Loop = (psInput->u16LineLength - psInput->u16CursorPosition) + 1;
					while (u16Loop)
					{
						psInput->Output("\b",
										1);
						--u16Loop;
					}

				}
			}
		}
		else
		if (0x03 == eChar)
		{
			// Ctrl-C!

			psInput->Output("^C\n",
							4);
			eStatus = ESTATUS_CTRL_C;
			goto errorExit;
		}
		else
		if ('\r' == eChar)
		{
			// Enter

			psInput->Output("\n",
							2);
			eStatus = ESTATUS_OK;
			*pu16LineLength = psInput->u16LineLength;
			goto errorExit;
		}
	}

errorExit:
	// Take the us out of ANSI consumption mode
	psInput->u8ANSIActiveCount = 0;
	return(eStatus);
}

// Frees up a line input structure
void LineInputDestroy(SLineInput *psInput)
{
	if (psInput)
	{
		LineInputHistoryDestroy(&psInput->psInputHistory);
	}
}

EStatus LineInputInit(SLineInput *psInput)
{
	EStatus eStatus;

	// Check to ensure we have appropriate input and output routines
	if ((NULL == psInput->InputGet) ||
		(NULL == psInput->Output))
	{
		eStatus = ESTATUS_MISSING_FUNCTION;
		goto errorExit;
	}

	// Sanity check the inputs
	if (0 == psInput->u16InputMax)
	{
		eStatus = ESTATUS_BAD_LENGTH;
		goto errorExit;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

