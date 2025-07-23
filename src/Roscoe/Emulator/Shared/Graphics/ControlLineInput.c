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
	uint16_t u16FontSize;							// Size of font
	uint32_t u32RGBAText;							// RGBA Color of text

	char *peText;								// Text accumulated so far
	uint16_t *pu16TextCharMap;					// Map marking all characters within the string (offsets from the base buffer)
	uint32_t u32CharCount;						// # Of characters entered so far
	uint32_t u32LengthCount;						// # Of bytes entered so far
	uint32_t u32CursorPosition;					// Position of cursor
	uint32_t u32MaxChars;							// Maximum # of characters for this text string

	// If password
	bool bPasswordEntry;						// Password entry
	EUnicode ePasswordChar;						// Character to use

	// Callback related
	void *pvUserData;							// User data for callback
	void (*Callback)(EControlLineInputHandle eLineInputHandle,
					 void *pvUserData,
					 EControlCallbackReason eLineInputCallbackReason,
					 char *peText,
					 uint32_t u32CharCount,
					 uint32_t u32CharSizeBytes);

	// Rendered image of text
	SGraphicsImage *psGraphicsImage;
	int32_t *ps32CharAdvanceX;					// Array of X advances for each character
	int32_t *ps32CharAdvanceOffsets;				// Array of X position offsets for each character
	uint32_t u32CharAdvanceY;						// 

	// Cursor
	uint32_t u32RGBACursor;						// RGBA Color of cursor
	bool bCursorShown;							// true If cursor shown, false if not (for blinking)
	int32_t s32BlinkCounterMs;					// Blink counter (counts down)
	uint32_t u32BlinkRateMs;						// How often do we toggle the cursor's state
	uint32_t u32CursorXSize;						// Size of cursor in pixels
	uint32_t u32CursorYSize;
	int32_t s32CursorXOffset;						// X/Y positioning offset from font where cursor is
	int32_t s32CursorYOffset;
	bool bOverride;								// Set true if the cursor is overridden/blinking
} SControlLineInput;

// Dump the input line array
static void ControlInputDumpArray(SControlLineInput *psControlLineInput)
{
	uint32_t u32Loop;
	int32_t s32XOffsetCalc = 0;

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
		psControlLineInput->s32BlinkCounterMs = (int32_t) psControlLineInput->u32BlinkRateMs;
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
			uint32_t u32Len = 0;
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
			uint32_t u32TextLen = 0;

			if (psControlLineInput->peText)
			{
				u32TextLen = (uint32_t) strlen(psControlLineInput->peText);
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
								  true,
								  psControl->u32XSize,
								  psControl->u32YSize);
	ERR_GOTO();

	// Make it focusable by default
	psControl->bFocusable = true;
	
errorExit:
	return(eStatus);
}

// Called when drawing the line input control
static EStatus ControlLineInputDrawMethod(SControl *psControl,
										  uint8_t u8WindowIntensity,
										  int32_t s32WindowXPos,
										  int32_t s32WindowYPos)
{
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (psControlLineInput)
	{
		GraphicsSetIntensity(psControlLineInput->psGraphicsImage,
							 (uint8_t) ((u8WindowIntensity * psControl->u8Intensity) >> 8));

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
			int32_t s32XLineOffset = 0;

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
										    uint8_t u8ButtonMask,
										    int32_t s32ControlRelativeXPos,
										    int32_t s32ControlRelativeYPos,
										    bool bPressed)
{
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	// If we've clicked on our control, reset
	if (bPressed)
	{
		uint32_t u32Loop;

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
	uint32_t u32CharLen;
	int32_t s32CharAdvanceX;
	EUnicode eUnicode;
	uint32_t u32Loop;

	// If we blink, we reset and show
	ControlLineInputResetCursor(psControlLineInput);
	psControlLineInput->bCursorShown = true;

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
static bool ControlLineInputProcessDelete(SControl *psControl,
										  SControlLineInput *psControlLineInput)
{
	uint32_t u32CharLen;
	uint32_t u32Loop;
	int32_t s32XOffsetLen;
	if (psControlLineInput->u32CharCount == psControlLineInput->u32CursorPosition)
	{
		return(false);
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
	return(true);
}

// Whenever a UTF8 character is input
static EStatus ControlLineInputUTF8Input(SControl *psControl,			// UTF8 input from keyboard
										 EControlHandle eControlInterceptedHandle,
										 char *peUTF8Character)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if ((psControlLineInput) &&
		(false == psControl->bDisabled))
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
										   uint8_t u8ButtonMask,
										   int32_t s32WindowRelativeX,
										   int32_t s32WindowRelativeY)
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
		psControlLineInput->bCursorShown = false;
	}

	// This will get rid or show our cursor
	WindowUpdated();

	return(ESTATUS_OK);
}

// Called during scancode entry
static EStatus ControlLineInputScancode(SControl *psControl,			// Keyscan code input
										EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call (or invalid if none)
										SDL_Scancode eScancode,				// SDL Scancode for key hit
										bool bPressed)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;
	EControlCallbackReason eReason = ECTRLACTION_UNKNOWN;

	// This routine only looks for enter, backspace, tab, etc...
	if ((bPressed) &&
		(psControlLineInput))
	{
		bool bLineInputUpdated = false;

		// Get the normal tab/enter/shift tab checks
		eReason = ControlKeyscanToCallbackReason(eScancode);

		// Move cursor left
		if (SDL_SCANCODE_LEFT == eScancode)
		{
			if (psControlLineInput->u32CursorPosition)
			{
				--psControlLineInput->u32CursorPosition;
				bLineInputUpdated = true;
			}
		}

		// Move cursor right
		if (SDL_SCANCODE_RIGHT == eScancode)
		{
			if (psControlLineInput->u32CursorPosition != psControlLineInput->u32CharCount)
			{
				++psControlLineInput->u32CursorPosition;
				bLineInputUpdated = true;
			}
		}

		// Home cursor (beginning of line)
		if (SDL_SCANCODE_HOME == eScancode)
		{
			psControlLineInput->u32CursorPosition = 0;
			bLineInputUpdated = true;
		}

		// End of line
		if (SDL_SCANCODE_END == eScancode)
		{
			psControlLineInput->u32CursorPosition = psControlLineInput->u32CharCount;
			bLineInputUpdated = true;
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
											 uint32_t u32TimerIntervalMs)
{
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if ((false == psControl->bDisabled) &&
		(psControl->bFocused || psControlLineInput->bOverride) &&
		(psControlLineInput->u32CursorXSize) &&
		(psControlLineInput->u32CursorYSize) &&
		(psControlLineInput->u32BlinkRateMs))
	{
		bool bUpdated = false;

		psControlLineInput->s32BlinkCounterMs -= (int32_t) u32TimerIntervalMs;
		while (psControlLineInput->s32BlinkCounterMs <= 0)
		{
			psControlLineInput->s32BlinkCounterMs += (int32_t) psControlLineInput->u32BlinkRateMs;
			if (psControlLineInput->bCursorShown)
			{
				psControlLineInput->bCursorShown = false;
			}
			else
			{
				psControlLineInput->bCursorShown = true;
			}

			bUpdated = true;
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
									   uint32_t u32XSize,
									   uint32_t u32YSize)
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
												bool bDisableOldState,
												bool bDisable)
{
	EStatus eStatus = ESTATUS_OK;
	SControlLineInput *psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;
	bool bUpdated = false;

	// If we are enabled and now we're disabled, then disable the cursor
	if ((false == bDisableOldState) &&
		(bDisable))
	{
		psControlLineInput->bCursorShown = false;
		ControlLineInputResetCursor(psControlLineInput);
		bUpdated = true;
	}

	if ((false == bDisable) &&
		(bDisableOldState))
	{
		// We've been enabled
		bUpdated = true;
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
							   int32_t s32XPos,
							   int32_t s32YPos,
							   uint32_t u32XSize,
							   uint32_t u32YSize)
{
	EStatus eStatus;
	bool bLocked = false;
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
	uint32_t u32CharCount = psControlLineInput->u32CharCount;
	int32_t s32XOffset = 0;
	int32_t *ps32XAdvanceOffsets = NULL;
	int32_t *ps32XAdvance = NULL;

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
		uint32_t u32CharLen = 0;
		int32_t s32XAdvance = 0;
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
								  uint16_t u16FontSize,
								  uint32_t u32RGBATextColor)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	bool bLocked = false;
	bool bChanged = false;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (psControlLineInput->eFont != eFont)
	{
		bChanged = true;
		psControlLineInput->eFont = eFont;
	}

	if (psControlLineInput->u16FontSize != u16FontSize)
	{
		bChanged = true;
		psControlLineInput->u16FontSize = u16FontSize;
	}

	if (psControlLineInput->u32RGBAText != u32RGBATextColor)
	{
		bChanged = true;
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
								 false,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

// Sets a line edit control 
EStatus ControlLineInputSetText(EControlLineInputHandle eLineInputHandle,
								char *peTextInput,
								uint32_t u32MaxLengthChars)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	bool bLocked = false;
	bool bChanged = false;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

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
		uint32_t u32CharCount = 0;
		uint16_t u16Offset = 0;
		uint32_t u32Loop = 0;
		char *peTextPtr = NULL;
		int32_t s32XAdvanceTotal = 0;
		uint32_t u32CharLen = 0;
		uint32_t u32Offset = 0;

		MEMALLOC(psControlLineInput->peText, (psControlLineInput->u32MaxChars * BYTES_PER_CHARACTER) + 1);
		MEMALLOC(psControlLineInput->pu16TextCharMap, (psControlLineInput->u32MaxChars + 1) * sizeof(*psControlLineInput->pu16TextCharMap));
		MEMALLOC(psControlLineInput->ps32CharAdvanceOffsets, sizeof(*psControlLineInput->ps32CharAdvanceOffsets) * (psControlLineInput->u32MaxChars + 1));
		MEMALLOC(psControlLineInput->ps32CharAdvanceX, sizeof(*psControlLineInput->ps32CharAdvanceX) * (psControlLineInput->u32MaxChars + 1));

		strcpy(psControlLineInput->peText, peTextInput);
		psControlLineInput->u32LengthCount = (uint32_t) strlen(psControlLineInput->peText);
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
								 false,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

// Gets the current line of text input
EStatus ControlLineInputGetText(EControlLineInputHandle eLineInputHandle,
								char *peTextBuffer,
								uint32_t u32MaxLengthBytes,
								uint32_t *pu32LengthInBytes,
								uint32_t *pu32LengthInCharacters)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	bool bLocked = false;
	bool bChanged = false;
	uint32_t u32LengthInBytes = 0;
	uint32_t u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (psControlLineInput->peText)
	{
		u32LengthInBytes = (uint32_t) strlen(psControlLineInput->peText);
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
								 false,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

EStatus ControlLineInputSetPasswordEntry(EControlLineInputHandle eLineInputHandle,
										 bool bPasswordEntry,
										 EUnicode ePasswordChar)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	bool bLocked = false;
	bool bChanged = false;
	uint32_t u32LengthInBytes = 0;
	uint32_t u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	if (bPasswordEntry != psControlLineInput->bPasswordEntry)
	{
		psControlLineInput->bPasswordEntry = bPasswordEntry;
		bChanged = true;
	}

	if (ePasswordChar != psControlLineInput->ePasswordChar)
	{	
		psControlLineInput->ePasswordChar = ePasswordChar;
		bChanged = true;
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
								 false,
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
													 uint32_t u32CharCount,
													 uint32_t u32CharSizeBytes))
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	bool bLocked = false;
	bool bChanged = false;
	uint32_t u32LengthInBytes = 0;
	uint32_t u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlLineInput = (SControlLineInput *) psControl->pvControlSpecificData;

	psControlLineInput->pvUserData = pvUserData;
	psControlLineInput->Callback = Callback;

errorExit:
	eStatus = ControlSetLock(eLineInputHandle,
							 false,
							 NULL,
							 eStatus);
	return(eStatus);
}

// Sets/clears a cursor
EStatus ControlLineInputSetCursor(EControlLineInputHandle eLineInputHandle,
										 uint32_t u32BlinkRateMs,
										 int32_t s32CursorXOffset,
										 int32_t s32CursorYOffset,
										 uint32_t u32CursorXSize,
										 uint32_t u32CursorYSize,
										 uint32_t u32RGBACursor)
{
	EStatus eStatus;
	SControl *psControl;
	SControlLineInput *psControlLineInput;
	bool bLocked = false;
	bool bChanged = false;
	uint32_t u32LengthInBytes = 0;
	uint32_t u32LengthInChars = 0;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 true,
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
		psControlLineInput->s32BlinkCounterMs = (int32_t) psControlLineInput->u32BlinkRateMs;
		psControlLineInput->bCursorShown = true;
	}

	// Cause the cursor to be drawn/redrawn
	WindowUpdated();

errorExit:
	eStatus = ControlSetLock(eLineInputHandle,
							 false,
							 NULL,
							 eStatus);
	return(eStatus);
}

EStatus ControlLineInputSetCursorOverride(EControlLineInputHandle eLineInputHandle,
										  bool bOverride)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlLineInput *psControlLineInput;

	// Lock the control
	eStatus = ControlSetLock(eLineInputHandle,
							 true,
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
							 false,
							 NULL,
							 eStatus);
	return(eStatus);
}

EStatus ControlLineInputInit(void)
{
	return(ControlRegister(sg_sControlLineInputMethods.eType,
						   &sg_sControlLineInputMethods));
}
