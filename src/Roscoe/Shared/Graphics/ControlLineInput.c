#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"
#include "Shared/Graphics/Font.h"

// Assume worst case bytes per character for UTF-8
#define	BYTES_PER_CHARACTER		5

// Dump line input data
//#define	LINE_INPUT_DEBUG		1

typedef struct SControlLineInput
{
	EFontHandle eFont;							// Which font are we using
	UINT16 u16FontSize;							// Size of font
	UINT32 u32RGBAText;							// RGBA Color of text

	char *peText;								// Text accumulated so far
	UINT16 *pu16TextCharMap;					// Map marking all characters within the string (offsets from the base buffer)
	UINT32 u32CharCount;						// # Of characters entered so far
	UINT32 u32LengthCount;						// # Of bytes entered so far
	UINT32 u32CursorPosition;					// Position of cursor
	UINT32 u32MaxChars;							// Maximum # of characters for this text string

	// If password
	BOOL bPasswordEntry;						// Password entry
	EUnicode ePasswordChar;						// Character to use

	// Callback related
	void *pvUserData;							// User data for callback
	void (*Callback)(EControlLineInputHandle eLineInputHandle,
					 void *pvUserData,
					 EControlCallbackReason eLineInputCallbackReason,
					 char *peText,
					 UINT32 u32CharCount,
					 UINT32 u32CharSizeBytes);

	// Rendered image of text
	SGraphicsImage *psGraphicsImage;
	INT32 *ps32CharAdvanceX;					// Array of X advances for each character
	INT32 *ps32CharAdvanceOffsets;				// Array of X position offsets for each character
	UINT32 u32CharAdvanceY;						// 

	// Cursor
	UINT32 u32RGBACursor;						// RGBA Color of cursor
	BOOL bCursorShown;							// TRUE If cursor shown, FALSE if not (for blinking)
	INT32 s32BlinkCounterMs;					// Blink counter (counts down)
	UINT32 u32BlinkRateMs;						// How often do we toggle the cursor's state
	UINT32 u32CursorXSize;						// Size of cursor in pixels
	UINT32 u32CursorYSize;
	INT32 s32CursorXOffset;						// X/Y positioning offset from font where cursor is
	INT32 s32CursorYOffset;
	BOOL bOverride;								// Set TRUE if the cursor is overridden/blinking
} SControlLineInput;

// Dump the input line array
static void ControlInputDumpArray(SControlLineInput *psControlLineInput)
{
	UINT32 u32Loop;
	INT32 s32XOffsetCalc = 0;

	if (NULL == psControlLineInput->peText)
	{
#ifdef LINE_INPUT_DEBUG
		DebugOut("No text\n");
#endif
		return;
	}
#ifdef LINE_INPUT_DEBUG
	else
	{
		DebugOut("peText           ='%s'\n", psControlLineInput->peText);
		DebugOut("u32CharCount     =%u\n", psControlLineInput->u32CharCount);
		DebugOut("u32LengthCount   =%u\n", psControlLineInput->u32LengthCount);
		DebugOut("u32CursorPosition=%u\n", psControlLineInput->u32CursorPosition);
		DebugOut("u32MaxChars      =%u\n", psControlLineInput->u32MaxChars);
		DebugOut("Char info:\n");

		DebugOut("Idx  Map  XOff  AdvX  CalcX\n");
	}
#endif

	for (u32Loop = 0; u32Loop <= psControlLineInput->u32CharCount; u32Loop++)
	{
#ifdef LINE_INPUT_DEBUG
		// We can skip the final item if we are dumping out a "before" modification
		DebugOut("%.2u:  %.2u   %.4d  %.4d  %.4d\n", u32Loop, psControlLineInput->pu16TextCharMap[u32Loop], psControlLineInput->ps32CharAdvanceOffsets[u32Loop], psControlLineInput->ps32CharAdvanceX[u32Loop], s32XOffsetCalc);
#endif
		// Make sure calculated offsets are correct
		BASSERT(s32XOffsetCalc == psControlLineInput->ps32CharAdvanceOffsets[u32Loop]);
		s32XOffsetCalc += psControlLineInput->ps32CharAdvanceX[u32Loop];
	}
}

// Cause the cursor to be shown and the timer/counter reset
static void ControlLineInputResetCursor(SControlLineInput *psControlLineInput)
{
	if (psControlLineInput)
	{
		psControlLineInput->s32BlinkCounterMs = (INT32) psControlLineInput->u32BlinkRateMs;
	}
}

// This renders the text as-is to the control. It assumes the control is locked.
static EStatus ControlLineInputRenderText(SControl *psControl)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;
	char *peTextToDisplay = NULL;

	if (psControlLineInput)
	{
		char *peTextPassword = NULL;

		// Clear the image
		GraphicsFillImage(psControlLineInput->psGraphicsImage,
						  MAKERGBA(0, 0, 0, PIXEL_TRANSPARENT));

		// Which text are we showing?
		if (psControlLineInput->bPasswordEntry)
		{
			UINT32 u32Len = 0;
			char *pePtr = NULL;

			if (psControlLineInput->peText)
			{
				u32Len = UTF8strlen(psControlLineInput->peText);
			}

			MEMALLOC(peTextPassword, (u32Len * UnicodeToUTF8Len(psControlLineInput->ePasswordChar)) + 1);
			pePtr = peTextPassword;

			// Make a password string
			while (u32Len)
			{
				pePtr += UnicodeToUTF8(pePtr,
									   psControlLineInput->ePasswordChar);
				u32Len--;
			}

			peTextToDisplay = peTextPassword;
		}
		else
		{
			peTextToDisplay = psControlLineInput->peText;
		}

		eStatus = FontRenderStringClip(psControlLineInput->eFont,
										psControlLineInput->u16FontSize,
										peTextToDisplay,
										psControlLineInput->u32RGBAText,
										psControlLineInput->psGraphicsImage->pu32RGBA,
										psControlLineInput->psGraphicsImage->u32TotalXSize,
										0,
										0,
										psControl->u32XSize,
										psControl->u32YSize);
		SafeMemFree(peTextPassword);
		ERR_GOTO();

		// Signal the graphics engine that it's a new graphic
		GraphicsImageUpdated(psControlLineInput->psGraphicsImage);

		WindowUpdated();

		eStatus = ESTATUS_OK;
	}

errorExit:
	return(eStatus);
}

// Process the callback
static void ControlLineInputCallback(SControl *psControl,
									 SControlLineInput *psControlLineInput,
									 EControlCallbackReason eLineInputCallbackReason)
{
	if (psControlLineInput)
	{
		if (psControlLineInput->Callback)
		{
			UINT32 u32TextLen = 0;

			if (psControlLineInput->peText)
			{
				u32TextLen = (UINT32) strlen(psControlLineInput->peText);
			}

			psControlLineInput->Callback(psControl->eControlHandle,
										 psControlLineInput->pvUserData,
										 eLineInputCallbackReason,
										 psControlLineInput->peText,
										 psControlLineInput->u32CharCount,
										 u32TextLen);
		}
	}
}

// Called when someone wants to create a line input control
static EStatus ControlLineInputCreateMethod(SControl *psControl,
										    void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	// Create a place to render the text
	eStatus = GraphicsCreateImage(&psControlLineInput->psGraphicsImage,
								  TRUE,
								  psControl->u32XSize,
								  psControl->u32YSize);
	ERR_GOTO();

	// Make it focusable by default
	psControl->bFocusable = TRUE;
	
errorExit:
	return(eStatus);
}

// Called when drawing the line input control
static EStatus ControlLineInputDrawMethod(SControl *psControl,
										  UINT8 u8WindowIntensity,
										  INT32 s32WindowXPos,
										  INT32 s32WindowYPos)
{
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (psControlLineInput)
	{
		GraphicsSetIntensity(psControlLineInput->psGraphicsImage,
							 (UINT8) ((u8WindowIntensity * psControl->u8Intensity) >> 8));

		// Graphics attributes
		GraphicsSetAttributes(psControlLineInput->psGraphicsImage,
							  ControlGetGfxAttributes(psControl),
							  GFXATTR_SWAP_HORIZONAL | GFXATTR_SWAP_VERTICAL);

		GraphicsSetGreyscale(psControlLineInput->psGraphicsImage,
							 psControl->bDisabled);

		GraphicsDraw(psControlLineInput->psGraphicsImage,
					 psControl->s32XPos + s32WindowXPos,
					 psControl->s32YPos + s32WindowYPos,
					 psControl->bOriginCenter,
					 psControl->dAngle);

		if ((psControlLineInput->u32CursorXSize) && 
			(psControlLineInput->u32CursorYSize) &&
			(psControl->bFocused || psControlLineInput->bOverride) &&
			(psControlLineInput->bCursorShown))
		{
			INT32 s32XLineOffset = 0;

			if ((psControlLineInput->u32CharCount) &&
				(psControlLineInput->ps32CharAdvanceOffsets) &&
				(psControlLineInput->ps32CharAdvanceX))
			{
				// Move the quad/cursor past the end of the last character
				if (psControlLineInput->u32CursorPosition)
				{
					s32XLineOffset = psControlLineInput->ps32CharAdvanceOffsets[psControlLineInput->u32CursorPosition - 1] + 
									 psControlLineInput->ps32CharAdvanceX[psControlLineInput->u32CursorPosition - 1];
				}
			}

			// Only show the cursor if it's not none, the control is in focus, and the cursor is shown
			GraphicsDrawQuad(s32WindowXPos + psControl->s32XPos + psControlLineInput->s32CursorXOffset + s32XLineOffset,
							 s32WindowYPos + psControl->s32YPos + psControlLineInput->s32CursorYOffset,
							 psControlLineInput->u32CursorXSize,
							 psControlLineInput->u32CursorYSize,
							 psControlLineInput->u32RGBACursor);
		}
	}

	return(ESTATUS_OK);
}

// Whenever a mouse button is pressed or released
static EStatus ControlLineInputButtonMethod(SControl *psControl,
										    UINT8 u8ButtonMask,
										    INT32 s32ControlRelativeXPos,
										    INT32 s32ControlRelativeYPos,
										    BOOL bPressed)
{
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	// If we've clicked on our control, reset
	if (bPressed)
	{
		UINT32 u32Loop;

		// Find out where we've clicked on the line and move the cursor position there
		for (u32Loop = 0; u32Loop < psControlLineInput->u32CharCount; u32Loop++)
		{
			if ((s32ControlRelativeXPos >= psControlLineInput->ps32CharAdvanceOffsets[u32Loop]) &&
				(s32ControlRelativeXPos < (psControlLineInput->ps32CharAdvanceOffsets[u32Loop] + psControlLineInput->ps32CharAdvanceX[u32Loop])))
			{
				break;
			}
		}

		psControlLineInput->u32CursorPosition = u32Loop;

		ControlLineInputResetCursor(psControlLineInput);
		WindowUpdated();
	}

	return(ESTATUS_OK);
}

// This adds a UTF8 character to the input buffer and updates everything necessary
static EStatus ControlInputCharProcess(SControl *psControl,
									   SControlLineInput *psControlLineInput,
									   char *peUTF8Char)
{
	EStatus eStatus = ESTATUS_OK;
	UINT32 u32CharLen;
	INT32 s32CharAdvanceX;
	EUnicode eUnicode;
	UINT32 u32Loop;

	// If we blink, we reset and show
	ControlLineInputResetCursor(psControlLineInput);
	psControlLineInput->bCursorShown = TRUE;

	// If we are full up, drop the character
	if (psControlLineInput->u32CharCount >= psControlLineInput->u32MaxChars)
	{
		goto errorExit;
	}

	// Get the unicode character
	eUnicode = UTF8toUnicode(peUTF8Char);

	// We have room! Let's copy things in.
	u32CharLen = UTF8charlen(peUTF8Char);

	// If we're doing a password entry, then use the password char
	if (psControlLineInput->bPasswordEntry)
	{
		eUnicode = psControlLineInput->ePasswordChar;
	}

	// Get this new character's advance
	eStatus = FontGetCharInfo(psControlLineInput->eFont,
							  psControlLineInput->u16FontSize,
							  eUnicode,
							  NULL,
							  NULL,
							  &s32CharAdvanceX,
							  NULL);
	ERR_GOTO();

#ifdef LINE_INPUT_DEBUG
	DebugOut("Before:\n");
	ControlInputDumpArray(psControlLineInput);
#endif

	// Ajust the char map offsets
	u32Loop = psControlLineInput->u32CursorPosition + 1;
	while (u32Loop <= (psControlLineInput->u32CharCount))
	{
		psControlLineInput->pu16TextCharMap[u32Loop] += u32CharLen;
		psControlLineInput->ps32CharAdvanceOffsets[u32Loop] += s32CharAdvanceX;

		u32Loop++;
	}

	// Make some space in the character buffer
	memmove((void *) (psControlLineInput->peText + psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition] + u32CharLen), 
			(void *) (psControlLineInput->peText + psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition]), 
		    ((psControlLineInput->u32LengthCount - psControlLineInput->u32CursorPosition) + 1) * sizeof(*psControlLineInput->peText + psControlLineInput->pu16TextCharMap));

	// Migrate the character offset map
	memmove((void *) &psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition + 1],
		    (void *) &psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition],
		    sizeof(*psControlLineInput->pu16TextCharMap) * ((psControlLineInput->u32CharCount + 1) - psControlLineInput->u32CursorPosition));

	// And the X sizes and advances for character offset displays
	memmove((void *) &psControlLineInput->ps32CharAdvanceX[psControlLineInput->u32CursorPosition + 1],
		    (void *) &psControlLineInput->ps32CharAdvanceX[psControlLineInput->u32CursorPosition],
		    sizeof(*psControlLineInput->ps32CharAdvanceX) * (psControlLineInput->u32CharCount - psControlLineInput->u32CursorPosition));
	memmove((void *) &psControlLineInput->ps32CharAdvanceOffsets[psControlLineInput->u32CursorPosition + 1],
		    (void *) &psControlLineInput->ps32CharAdvanceOffsets[psControlLineInput->u32CursorPosition],
		    sizeof(*psControlLineInput->ps32CharAdvanceOffsets) * ((psControlLineInput->u32CharCount + 1) - psControlLineInput->u32CursorPosition));

	// Copy in the UTF8 character
	memcpy((void *) (psControlLineInput->peText + psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition]),
		   (void *) peUTF8Char,
		   u32CharLen);

	psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition + 1] = psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition] + u32CharLen;
	psControlLineInput->ps32CharAdvanceX[psControlLineInput->u32CursorPosition] = s32CharAdvanceX; 
	psControlLineInput->ps32CharAdvanceOffsets[psControlLineInput->u32CursorPosition + 1] = psControlLineInput->ps32CharAdvanceOffsets[psControlLineInput->u32CursorPosition] + s32CharAdvanceX;
	psControlLineInput->u32LengthCount += u32CharLen;
	psControlLineInput->u32CharCount++;
	psControlLineInput->u32CursorPosition++;
	*(psControlLineInput->peText + psControlLineInput->u32LengthCount) = '\0';

#ifdef LINE_INPUT_DEBUG
	DebugOut("After:\n");
#endif
	ControlInputDumpArray(psControlLineInput);

	// Go render the updated text
	eStatus = ControlLineInputRenderText(psControl);

	// Potentially let someone know it changed
	ControlLineInputCallback(psControl,
							 psControlLineInput,
							 ECTRLACTION_TEXTCHANGED);

errorExit:
	WindowUpdated();

	return(eStatus);
}

// Processes delete operation
static BOOL ControlLineInputProcessDelete(SControl *psControl,
										  SControlLineInput *psControlLineInput)
{
	UINT32 u32CharLen;
	UINT32 u32Loop;
	INT32 s32XOffsetLen;
	if (psControlLineInput->u32CharCount == psControlLineInput->u32CursorPosition)
	{
		return(FALSE);
	}
	
#ifdef LINE_INPUT_DEBUG
	DebugOut("Before:\n");
	ControlInputDumpArray(psControlLineInput);
#endif

	// How many bytes are we subtracting?
	u32CharLen = psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition + 1] -
				 psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition];

	// Size of this character pixel-wise
	s32XOffsetLen = psControlLineInput->ps32CharAdvanceX[psControlLineInput->u32CursorPosition];

	// Get rid of the deleted character
	memcpy((void *) (psControlLineInput->peText + psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition]), 
		   (void *) (psControlLineInput->peText + psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition + 1]), 
		   ((psControlLineInput->u32LengthCount - u32CharLen) - psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition]) + 1);

	psControlLineInput->u32LengthCount -= (psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition + 1] - psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition]);

	// Migrate the character offset map
	memcpy((void *) &psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition],
		   (void *) &psControlLineInput->pu16TextCharMap[psControlLineInput->u32CursorPosition + 1],
		   sizeof(*psControlLineInput->pu16TextCharMap) * (psControlLineInput->u32CharCount - psControlLineInput->u32CursorPosition));

	// And the X sizes and advances for character offset displays
	memcpy((void *) &psControlLineInput->ps32CharAdvanceX[psControlLineInput->u32CursorPosition],
		   (void *) &psControlLineInput->ps32CharAdvanceX[psControlLineInput->u32CursorPosition + 1],
		   sizeof(*psControlLineInput->ps32CharAdvanceX) * (psControlLineInput->u32CharCount - psControlLineInput->u32CursorPosition));
	memcpy((void *) &psControlLineInput->ps32CharAdvanceOffsets[psControlLineInput->u32CursorPosition],
		   (void *) &psControlLineInput->ps32CharAdvanceOffsets[psControlLineInput->u32CursorPosition + 1],
		   sizeof(*psControlLineInput->ps32CharAdvanceOffsets) * (psControlLineInput->u32CharCount - psControlLineInput->u32CursorPosition));

	// Now adjust the char map offsets
	u32Loop = psControlLineInput->u32CursorPosition;
	while (u32Loop <= (psControlLineInput->u32CharCount - 1))
	{
		psControlLineInput->pu16TextCharMap[u32Loop] -= u32CharLen;
		psControlLineInput->ps32CharAdvanceOffsets[u32Loop] -= s32XOffsetLen;

		BASSERT(psControlLineInput->ps32CharAdvanceOffsets[u32Loop] >= 0);

		u32Loop++;
	}

	--psControlLineInput->u32CharCount;
	*(psControlLineInput->peText + psControlLineInput->u32LengthCount) = '\0';

#ifdef LINE_INPUT_DEBUG
	DebugOut("After:\n");
	ControlInputDumpArray(psControlLineInput);
#endif
	return(TRUE);
}

// Whenever a UTF8 character is input
static EStatus ControlLineInputUTF8Input(SControl *psControl,			// UTF8 input from keyboard
										 EControlHandle eControlInterceptedHandle,
										 char *peUTF8Character)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if ((psControlLineInput) &&
		(FALSE == psControl->bDisabled))
	{
		// Cause the cursor to appear if it's enabled
		ControlLineInputResetCursor(psControlLineInput);

		// Go add this to our buffer
		eStatus = ControlInputCharProcess(psControl,
										  psControlLineInput,
										  peUTF8Character);
	}

	return(eStatus);
}

// Whenever focus is gained or lost
static EStatus ControlLineInputFocusMethod(SControl *psControl,					// Called when this control gains focus
										   EControlHandle eControlHandleOld,
										   EControlHandle eControlHandleNew,
										   UINT8 u8ButtonMask,
										   INT32 s32WindowRelativeX,
										   INT32 s32WindowRelativeY)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	ControlLineInputResetCursor(psControlLineInput);

	if (ControlHandleIsValid(eControlHandleNew) &&
		(eControlHandleNew != psControl->eControlHandle))
	{
		// Lost focus
		ControlLineInputCallback(psControl,
								 psControlLineInput,
								 ECTRLACTION_LOSTFOCUS);

		// Turn off the cursor
		psControlLineInput->bCursorShown = FALSE;
	}

	// This will get rid or show our cursor
	WindowUpdated();

	return(ESTATUS_OK);
}

// Called during scancode entry
static EStatus ControlLineInputScancode(SControl *psControl,			// Keyscan code input
										EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call (or invalid if none)
										SDL_Scancode eScancode,				// SDL Scancode for key hit
										BOOL bPressed)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;
	EControlCallbackReason eReason = ECTRLACTION_UNKNOWN;

	// This routine only looks for enter, backspace, tab, etc...
	if ((bPressed) &&
		(psControlLineInput))
	{
		BOOL bLineInputUpdated = FALSE;

		// Get the normal tab/enter/shift tab checks
		eReason = ControlKeyscanToCallbackReason(eScancode);

		// Move cursor left
		if (SDL_SCANCODE_LEFT == eScancode)
		{
			if (psControlLineInput->u32CursorPosition)
			{
				--psControlLineInput->u32CursorPosition;
				bLineInputUpdated = TRUE;
			}
		}

		// Move cursor right
		if (SDL_SCANCODE_RIGHT == eScancode)
		{
			if (psControlLineInput->u32CursorPosition != psControlLineInput->u32CharCount)
			{
				++psControlLineInput->u32CursorPosition;
				bLineInputUpdated = TRUE;
			}
		}

		// Home cursor (beginning of line)
		if (SDL_SCANCODE_HOME == eScancode)
		{
			psControlLineInput->u32CursorPosition = 0;
			bLineInputUpdated = TRUE;
		}

		// End of line
		if (SDL_SCANCODE_END == eScancode)
		{
			psControlLineInput->u32CursorPosition = psControlLineInput->u32CharCount;
			bLineInputUpdated = TRUE;
		}

		// Backspace
		if ((SDL_SCANCODE_BACKSPACE == eScancode) &&
			(psControlLineInput->u32CursorPosition))
		{
			// Back up the cursor and delete a character
			psControlLineInput->u32CursorPosition--;
			bLineInputUpdated = ControlLineInputProcessDelete(psControl,
															  psControlLineInput);
			eReason = ECTRLACTION_TEXTCHANGED;
		}

		// Delete a character
		if (SDL_SCANCODE_DELETE == eScancode)
		{
			bLineInputUpdated = ControlLineInputProcessDelete(psControl,
															  psControlLineInput);
			eReason = ECTRLACTION_TEXTCHANGED;
		}

		if (bLineInputUpdated)
		{
			ControlLineInputResetCursor(psControlLineInput);
			eStatus = ControlLineInputRenderText(psControl);
			WindowUpdated();
		}

		if (eReason != ECTRLACTION_UNKNOWN)
		{
			ControlLineInputCallback(psControl,
									 psControlLineInput,
									 eReason);
		}
	}

	return(eStatus);
}

// Called periodically - used for blinking cursor
static EStatus ControlLineInputPeriodicTimer(SControl *psControl,
											 UINT32 u32TimerIntervalMs)
{
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if ((FALSE == psControl->bDisabled) &&
		(psControl->bFocused || psControlLineInput->bOverride) &&
		(psControlLineInput->u32CursorXSize) &&
		(psControlLineInput->u32CursorYSize) &&
		(psControlLineInput->u32BlinkRateMs))
	{
		BOOL bUpdated = FALSE;

		psControlLineInput->s32BlinkCounterMs -= (INT32) u32TimerIntervalMs;
		while (psControlLineInput->s32BlinkCounterMs <= 0)
		{
			psControlLineInput->s32BlinkCounterMs += (INT32) psControlLineInput->u32BlinkRateMs;
			if (psControlLineInput->bCursorShown)
			{
				psControlLineInput->bCursorShown = FALSE;
			}
			else
			{
				psControlLineInput->bCursorShown = TRUE;
			}

			bUpdated = TRUE;
		}

		if (bUpdated)
		{
			WindowUpdated();
		}
	}

	return(ESTATUS_OK);
}

// Sets the line input's overall size

static EStatus ControlLineInputSetSize(SControl *psControl,
									   UINT32 u32XSize,
									   UINT32 u32YSize)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	// Destroy the existing graphic if there is one
	GraphicsClearImage(psControlLineInput->psGraphicsImage);

	// Create a place to render the text
	eStatus = GraphicsAllocateImage(psControlLineInput->psGraphicsImage,
									psControl->u32XSize,
									psControl->u32YSize);
	return(eStatus);
}

// Called when a line input control has been enabled or disabled
static EStatus ControlLineInputSetEnableDisable(struct SControl *psControl,			// Set control disabled/enabled
												BOOL bDisableOldState,
												BOOL bDisable)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;
	BOOL bUpdated = FALSE;

	// If we are enabled and now we're disabled, then disable the cursor
	if ((FALSE == bDisableOldState) &&
		(bDisable))
	{
		psControlLineInput->bCursorShown = FALSE;
		ControlLineInputResetCursor(psControlLineInput);
		bUpdated = TRUE;
	}

	if ((FALSE == bDisable) &&
		(bDisableOldState))
	{
		// We've been enabled
		bUpdated = TRUE;
	}

	// Cause a redraw
	if (bUpdated)
	{
		WindowUpdated();
	}

	return(eStatus);
}

// List of methods for this control
static SControlMethods sg_sControlLineInputMethods =
{
	ECTRLTYPE_LINEINPUT,						// Type of control
	"Line input",								// Name of control
	sizeof(SControlLineInput),					// Size of control specific structure

	ControlLineInputCreateMethod,				// Create a control
	NULL,										// Destroy a control
	ControlLineInputSetEnableDisable,			// Control enable/disable
	NULL,										// New position
	NULL,										// Set visible
	NULL,										// Set angle
	ControlLineInputDrawMethod,					// Draw the control
	NULL,										// Runtime configuration
	NULL,										// Hit test
	ControlLineInputFocusMethod,				// Focus set/loss
	ControlLineInputButtonMethod,				// Button press/release
	NULL,										// Mouseover when selected
	NULL,										// Mouse wheel
	ControlLineInputUTF8Input,					// UTF8 Input
	ControlLineInputScancode,					// Scancode input
	ControlLineInputPeriodicTimer,				// Periodic timer
	NULL,										// Mouseover
	ControlLineInputSetSize,					// Size
};

// Creates a line input 
EStatus ControlLineInputCreate(EWindowHandle eWindowHandle,
							   EControlLineInputHandle *peLineInputHandle,
							   INT32 s32XPos,
							   INT32 s32YPos,
							   UINT32 u32XSize,
							   UINT32 u32YSize)
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peLineInputHandle,
							s32XPos,
							s32YPos,
							u32XSize,
							u32YSize,
							ECTRLTYPE_LINEINPUT,
							NULL);

	return(eStatus);
}

// This runs through a string and calculates all of the positions/offsets in the graphical image
static EStatus ControlLineInputCalculateCharOffsets(SControlLineInput *psControlLineInput)
{
	EStatus eStatus = ESTATUS_OK;
	char *peTextSrc = psControlLineInput->peText;
	UINT32 u32CharCount = psControlLineInput->u32CharCount;
	INT32 s32XOffset = 0;
	INT32 *ps32XAdvanceOffsets = NULL;
	INT32 *ps32XAdvance = NULL;

	SafeMemFree(psControlLineInput->ps32CharAdvanceOffsets);

	if (psControlLineInput->u32MaxChars)
	{
		MEMALLOC(psControlLineInput->ps32CharAdvanceOffsets, sizeof(*psControlLineInput->ps32CharAdvanceOffsets) * (psControlLineInput->u32MaxChars + 1));
		MEMALLOC(psControlLineInput->ps32CharAdvanceX, sizeof(*psControlLineInput->ps32CharAdvanceX) * (psControlLineInput->u32MaxChars + 1));
		ps32XAdvanceOffsets = psControlLineInput->ps32CharAdvanceOffsets;
		ps32XAdvance = psControlLineInput->ps32CharAdvanceX;

		// Get the Y advance (character doesn't matter - it's the same for all)
		eStatus = FontGetCharInfo(psControlLineInput->eFont,
								  psControlLineInput->u16FontSize,
								  (EUnicode) ' ',
								  NULL,
								  NULL,
								  NULL,
								  &psControlLineInput->u32CharAdvanceY);
	}

	// Internal consistency check
	BASSERT(u32CharCount <= psControlLineInput->u32MaxChars);

	while (u32CharCount)
	{
		UINT32 u32CharLen = 0;
		INT32 s32XAdvance = 0;
		EUnicode eUnicode;

		// Get how long the UTF8 character is
		u32CharLen = UTF8charlen(peTextSrc);
	
		// Now get its unicode equivalent
		eUnicode = UTF8toUnicode(peTextSrc);

		if (psControlLineInput->bPasswordEntry)
		{
			eUnicode = psControlLineInput->ePasswordChar;
		}

		// Now we figure out its X advance
		eStatus = FontGetCharInfo(psControlLineInput->eFont,
								  psControlLineInput->u16FontSize,
								  eUnicode,
								  NULL,
								  NULL,
								  ps32XAdvance,
								  &psControlLineInput->u32CharAdvanceY);

		*ps32XAdvanceOffsets = s32XOffset;
		++ps32XAdvanceOffsets;
		s32XOffset += *ps32XAdvance;
		ps32XAdvance++;
		peTextSrc += u32CharLen;
		u32CharCount--;
	}

	if (ps32XAdvanceOffsets)
	{
		// Always do one past the end so we know where the next character goes
		*ps32XAdvanceOffsets = s32XOffset;
	}

errorExit:
	return(eStatus);
}



// Set the line input assets/details
EStatus ControlLineInputSetAssets(EControlLineInputHandle eLineInputHandle,
								  EFontHandle eFont,
								  UINT16 u16FontSize,
								  UINT32 u32RGBATextColor)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	BOOL bLocked = FALSE;
	BOOL bChanged = FALSE;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (psControlLineInput->eFont != eFont)
	{
		bChanged = TRUE;
		psControlLineInput->eFont = eFont;
	}

	if (psControlLineInput->u16FontSize != u16FontSize)
	{
		bChanged = TRUE;
		psControlLineInput->u16FontSize = u16FontSize;
	}

	if (psControlLineInput->u32RGBAText != u32RGBATextColor)
	{
		bChanged = TRUE;
		psControlLineInput->u32RGBAText = u32RGBATextColor;
	}

	// If something changed...
	if (bChanged)
	{
		// Go rerender the text
		eStatus = ControlLineInputRenderText(psControl);

		ControlLineInputResetCursor(psControlLineInput);
	}

errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eLineInputHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

// Sets a line edit control 
EStatus ControlLineInputSetText(EControlLineInputHandle eLineInputHandle,
								char *peTextInput,
								UINT32 u32MaxLengthChars)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	BOOL bLocked = FALSE;
	BOOL bChanged = FALSE;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	// 0 Signifies "use what's there"
	if (0 == u32MaxLengthChars)
	{
		u32MaxLengthChars = psControlLineInput->u32MaxChars;
	}

	SafeMemFree(psControlLineInput->peText);
	SafeMemFree(psControlLineInput->pu16TextCharMap);
	SafeMemFree(psControlLineInput->ps32CharAdvanceOffsets);
	SafeMemFree(psControlLineInput->ps32CharAdvanceX);
	psControlLineInput->u32CharCount = 0;
	psControlLineInput->u32CursorPosition = 0;
	psControlLineInput->u32MaxChars = u32MaxLengthChars;
	psControlLineInput->u32LengthCount = 0;

	if (peTextInput)
	{
		UINT32 u32CharCount = 0;
		UINT16 u16Offset = 0;
		UINT32 u32Loop = 0;
		char *peTextPtr = NULL;
		INT32 s32XAdvanceTotal = 0;
		UINT32 u32CharLen = 0;
		UINT32 u32Offset = 0;

		MEMALLOC(psControlLineInput->peText, (psControlLineInput->u32MaxChars * BYTES_PER_CHARACTER) + 1);
		MEMALLOC(psControlLineInput->pu16TextCharMap, (psControlLineInput->u32MaxChars + 1) * sizeof(*psControlLineInput->pu16TextCharMap));
		MEMALLOC(psControlLineInput->ps32CharAdvanceOffsets, sizeof(*psControlLineInput->ps32CharAdvanceOffsets) * (psControlLineInput->u32MaxChars + 1));
		MEMALLOC(psControlLineInput->ps32CharAdvanceX, sizeof(*psControlLineInput->ps32CharAdvanceX) * (psControlLineInput->u32MaxChars + 1));

		strcpy(psControlLineInput->peText, peTextInput);
		psControlLineInput->u32LengthCount = (UINT32) strlen(psControlLineInput->peText);
		psControlLineInput->u32CursorPosition = UTF8strlen(peTextInput);
		psControlLineInput->u32CharCount = psControlLineInput->u32CursorPosition;

		peTextPtr = psControlLineInput->peText;
		u32Loop = 0;
		while (u32Loop < psControlLineInput->u32CharCount)
		{
			u32CharLen = UTF8charlen(peTextPtr);
			peTextPtr += u32CharLen;
			psControlLineInput->pu16TextCharMap[u32Loop] = u32Offset;
			u32Loop++;
			u32Offset += u32CharLen;
		}

		psControlLineInput->pu16TextCharMap[u32Loop] = u32Offset;
	}

	// Recalculate character offsets if necessary
	(void) ControlLineInputCalculateCharOffsets(psControlLineInput);

	// Go rerender the text
	eStatus = ControlLineInputRenderText(psControl);

	ControlLineInputResetCursor(psControlLineInput);

errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eLineInputHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

// Gets the current line of text input
EStatus ControlLineInputGetText(EControlLineInputHandle eLineInputHandle,
								char *peTextBuffer,
								UINT32 u32MaxLengthBytes,
								UINT32 *pu32LengthInBytes,
								UINT32 *pu32LengthInCharacters)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	BOOL bLocked = FALSE;
	BOOL bChanged = FALSE;
	UINT32 u32LengthInBytes = 0;
	UINT32 u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (psControlLineInput->peText)
	{
		u32LengthInBytes = (UINT32) strlen(psControlLineInput->peText);
		u32LengthInChars = UTF8strlen(psControlLineInput->peText);

		strncpy(peTextBuffer, psControlLineInput->peText, u32MaxLengthBytes);
	}

	if (pu32LengthInBytes)
	{
		*pu32LengthInBytes = u32LengthInBytes;
	}

	if (pu32LengthInCharacters)
	{
		*pu32LengthInCharacters = u32LengthInChars;
	}

	// All good, unless u32MaxLengthBytes < u32LengthInBytes
	if (u32MaxLengthBytes < u32LengthInBytes)
	{
		// Indicate truncation.
		eStatus = ESTATUS_TRUNCATED;
	}

	(void) ControlLineInputCalculateCharOffsets(psControlLineInput);

errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eLineInputHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

EStatus ControlLineInputSetPasswordEntry(EControlLineInputHandle eLineInputHandle,
										 BOOL bPasswordEntry,
										 EUnicode ePasswordChar)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	BOOL bLocked = FALSE;
	BOOL bChanged = FALSE;
	UINT32 u32LengthInBytes = 0;
	UINT32 u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (bPasswordEntry != psControlLineInput->bPasswordEntry)
	{
		psControlLineInput->bPasswordEntry = bPasswordEntry;
		bChanged = TRUE;
	}

	if (ePasswordChar != psControlLineInput->ePasswordChar)
	{	
		psControlLineInput->ePasswordChar = ePasswordChar;
		bChanged = TRUE;
	}

	// Recalculate character offsets if necessary
	(void) ControlLineInputCalculateCharOffsets(psControlLineInput);

	// Go rerender the text
	eStatus = ControlLineInputRenderText(psControl);

	ControlLineInputResetCursor(psControlLineInput);

errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eLineInputHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

// Sets up a callback for line input info
EStatus ControlLineInputSetCallback(EControlLineInputHandle eLineInputHandle,
									void *pvUserData,
									void (*Callback)(EControlLineInputHandle eLineInputHandle,
													 void *pvUserData,
													 EControlCallbackReason eLineInputReason,
													 char *peText,
													 UINT32 u32CharCount,
													 UINT32 u32CharSizeBytes))
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	BOOL bLocked = FALSE;
	BOOL bChanged = FALSE;
	UINT32 u32LengthInBytes = 0;
	UINT32 u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	psControlLineInput->pvUserData = pvUserData;
	psControlLineInput->Callback = Callback;

errorExit:
	eStatus = ControlSetLock(eLineInputHandle,
							 FALSE,
							 NULL,
							 eStatus);
	return(eStatus);
}

// Sets/clears a cursor
EStatus ControlLineInputSetCursor(EControlLineInputHandle eLineInputHandle,
										 UINT32 u32BlinkRateMs,
										 INT32 s32CursorXOffset,
										 INT32 s32CursorYOffset,
										 UINT32 u32CursorXSize,
										 UINT32 u32CursorYSize,
										 UINT32 u32RGBACursor)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	BOOL bLocked = FALSE;
	BOOL bChanged = FALSE;
	UINT32 u32LengthInBytes = 0;
	UINT32 u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	psControlLineInput->u32BlinkRateMs = u32BlinkRateMs;
	psControlLineInput->u32RGBACursor = u32RGBACursor;
	psControlLineInput->s32CursorXOffset = s32CursorXOffset;
	psControlLineInput->s32CursorYOffset = s32CursorYOffset;
	psControlLineInput->u32CursorXSize = u32CursorXSize;
	psControlLineInput->u32CursorYSize = u32CursorYSize;

	// If we're
	if (u32CursorXSize && u32CursorYSize)
	{
		// We're turning the cursor on. If the blink rate is nonzero, then we set up the counter
		psControlLineInput->u32BlinkRateMs = u32BlinkRateMs;
		psControlLineInput->s32BlinkCounterMs = (INT32) psControlLineInput->u32BlinkRateMs;
		psControlLineInput->bCursorShown = TRUE;
	}

	// Cause the cursor to be drawn/redrawn
	WindowUpdated();

errorExit:
	eStatus = ControlSetLock(eLineInputHandle,
							 FALSE,
							 NULL,
							 eStatus);
	return(eStatus);
}

EStatus ControlLineInputSetCursorOverride(EControlLineInputHandle eLineInputHandle,
										  BOOL bOverride)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlLineInput *psControlLineInput;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (psControlLineInput->bOverride != bOverride)
	{
		psControlLineInput->bOverride = bOverride;
		WindowUpdated();
	}

errorExit:
	eStatus = ControlSetLock(eLineInputHandle,
							 FALSE,
							 NULL,
							 eStatus);
	return(eStatus);
}

EStatus ControlLineInputInit(void)
{
	return(ControlRegister(sg_sControlLineInputMethods.eType,
						   &sg_sControlLineInputMethods));
}
