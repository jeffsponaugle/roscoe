#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

// Control button structure
typedef struct SControlButton
{
	void (*ButtonCallback)(EControlButtonHandle eButtonHandle,		// User callback when button pressed
						   EControlCallbackReason eReason,
						   UINT32 u32ReasonData,
						   INT32 s32ControlRelativeXPos,
						   INT32 s32ControlRelativeYPos);

	BOOL bHitIgnoreTransparent;		// When doing a hit test, ignore transparencies if set to TRUE
	SGraphicsImage *psNotPressed;	// Not pressed button image
	SGraphicsImage *psPressed;		// Pressed image

	// Text handle
	EControlTextHandle eTextHandle;	// Text control handle 
	BOOL bCenterOrigin;				// Text is centered

	// Nonpressed offset
	INT32 s32TextXNonPressedOffset;	// Text X/Y offset from main control for nonpressed image
	INT32 s32TextYNonPressedOffset;

	// Pressed offset
	INT32 s32TextXPressedOffset;	// Text X/Y offset from main control for pressed image
	INT32 s32TextYPressedOffset;

	// Set TRUE if it can be triggered by keyboard operation
	BOOL bKeyboardTriggerable;

} SControlButton;

// Called when someone wants to create a button control
static EStatus ControlButtonCreateMethod(SControl *psControl,
									     void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	// Create a not pressed image
	eStatus = GraphicsCreateImage(&psControlButton->psNotPressed,
								  TRUE,
								  0, 0);
	ERR_GOTO();

	// Now the pressed image
	eStatus = GraphicsCreateImage(&psControlButton->psPressed,
								  TRUE,
								  0, 0);
	ERR_GOTO();

	// Make it focusable by default
	psControl->bFocusable = TRUE;
	
errorExit:
	return(eStatus);
}

// Based on incoming settings, calculate the x/y offset of the text object
static void ControlCalculateTextOffset(SControl *psControl,
									   SControlButton *psControlButton,
									   INT32 *ps32TextXOffset,
									   INT32 *ps32TextYOffset)

{
	INT32 s32XOffset = 0;
	INT32 s32YOffset = 0;
	SGraphicsImage *psDrawImage = NULL;

	if (psControl->bAsserted)
	{
		psDrawImage = psControlButton->psPressed;
	}
	else
	{
		psDrawImage = psControlButton->psNotPressed;
	}

	// If we're centered on the button, then figure out the button/control size and 
	if (psControlButton->bCenterOrigin)
	{
		if (psDrawImage)
		{
			s32XOffset = psDrawImage->u32TotalXSize >> 1;
			s32YOffset = psDrawImage->u32TotalYSize >> 1;
		}
	}

	if (FALSE == psControl->bAsserted)
	{
		// Button is not pressed
		s32XOffset += psControlButton->s32TextXNonPressedOffset;
		s32YOffset += psControlButton->s32TextYNonPressedOffset;
	}
	else
	{
		// Button is pressed
		s32XOffset += psControlButton->s32TextXPressedOffset;
		s32YOffset += psControlButton->s32TextYPressedOffset;
	}

	if (ps32TextXOffset)
	{
		*ps32TextXOffset = s32XOffset;
	}

	if (ps32TextYOffset)
	{
		*ps32TextYOffset = s32YOffset;
	}
}

// Destroys a button control
static EStatus ControlButtonDestroyMethod(SControl *psControl)
{
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;
	SGraphicsImage *psDrawImage = NULL;

	if (psControlButton)
	{
		GraphicsDestroyImage(&psControlButton->psPressed);
		GraphicsDestroyImage(&psControlButton->psNotPressed);

		(void) ControlDestroy((EControlHandle *) &psControlButton->eTextHandle);
	}

	return(ESTATUS_OK);
}

// Draws a button control
static EStatus ControlButtonDrawMethod(SControl *psControl,
									   UINT8 u8WindowIntensity,
									   INT32 s32WindowXPos,
									   INT32 s32WindowYPos)
{
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	if (psControlButton)
	{
		SGraphicsImage *psDrawImage = NULL;
		INT32 s32TextXOffset = 0;
		INT32 s32TextYOffset = 0;

		ControlCalculateTextOffset(psControl,
								   psControlButton,
								   &s32TextXOffset,
								   &s32TextYOffset);

		// See what the state of the button is
		if (psControl->bAsserted)
		{
			psDrawImage = psControlButton->psPressed;
		}
		else
		{
			psDrawImage = psControlButton->psNotPressed;
		}

		GraphicsSetGreyscale(psDrawImage,
							 psControl->bDisabled);

		GraphicsDraw(psDrawImage,
					 psControl->s32XPos + s32WindowXPos,
					 psControl->s32YPos + s32WindowYPos,
					 psControl->bOriginCenter,
					 psControl->dAngle);

		ControlRotateClockwise(psControl->dAngle,
							   &s32TextXOffset,
							   &s32TextYOffset);

		(void) ControlSetDisable((EControlHandle) psControlButton->eTextHandle,
								 psControl->bDisabled);

		// Set the text control position if there is one
		(void) ControlSetPosition((EControlHandle) psControlButton->eTextHandle,
								  psControl->s32XPos + s32TextXOffset,
								  psControl->s32YPos + s32TextYOffset);
	}

	return(ESTATUS_OK);
}

static EStatus ControlButtonSetPositionMethod(SControl *psControl,			// When a new position is set
											  INT32 s32WindowXPos,
											  INT32 s32WindowYPos)
{
	EStatus eStatus;
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;
	INT32 s32TextXOffset = 0;
	INT32 s32TextYOffset = 0;

	ControlCalculateTextOffset(psControl,
								psControlButton,
								&s32TextXOffset,
								&s32TextYOffset);

	// Set the text image position, too, if there is one. We ignore the
	// result code
	eStatus = ControlSetPosition((EControlHandle) psControlButton->eTextHandle,
								 s32WindowXPos + s32TextXOffset,
								 s32WindowYPos + s32TextYOffset);

	return(ESTATUS_OK);
}

static EStatus ControlButtonSetVisibleMethod(SControl *psControl,
											 BOOL bVisible)
{
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	// Set/clear the text control's visibility as well
	(void) ControlSetVisible((EControlHandle) psControlButton->eTextHandle,
							 bVisible);

	return(ESTATUS_OK);
}

// Button pressed
static EStatus ControlButtonPressedMethod(SControl *psControl,
										  UINT8 u8ButtonMask,
										  INT32 s32ControlRelativeXPos,
										  INT32 s32ControlRelativeYPos,
										  BOOL bPressed)
{
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;
	UINT32 u32ReasonMask = u8ButtonMask;

	if (bPressed)
	{
		u32ReasonMask |= BUTTON_PRESSED;
	}

	if (psControlButton->ButtonCallback)
	{
		psControlButton->ButtonCallback(psControl->eControlHandle,
										ECTRLACTION_MOUSEBUTTON,
										u32ReasonMask,
										s32ControlRelativeXPos,
										s32ControlRelativeYPos);
	}

	if (bPressed)
	{
		DebugOut("%.8x Pressed\n", (UINT32) psControl->eControlHandle);
	}
	else
	{
		DebugOut("%.8x Released\n", (UINT32) psControl->eControlHandle);
	}

	// Update the button state
	WindowUpdated();

	return(ESTATUS_OK);
}

// Hit test - look for transparent graphics
static EStatus ControlButtonHitTestMethod(SControl *psControl,				// Hit test call (assume hit if NULL)
										  UINT8 u8ButtonMask,				// Which buttons are being pressed?
										  INT32 s32ControlRelativeXPos,
										  INT32 s32ControlRelativeYPos)
{
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;
	SGraphicsImage *psShownImage = NULL;

	if (psControlButton)
	{
		// See what the state of the button is
		if (psControl->bAsserted)
		{
			psShownImage = psControlButton->psPressed;
		}
		else
		{
			psShownImage = psControlButton->psNotPressed;
		}

		if (psControlButton->bHitIgnoreTransparent)
		{
			// Ignore transparent pixels and consider the bounding box a hit
			return(ESTATUS_OK);
		}
		else
		{
			// Only register a hit if it's an opaque pixel
			return(ControlHitTestImage(psShownImage,
									   s32ControlRelativeXPos,
									   s32ControlRelativeYPos));
		}
	}
	else
	{
		return(ESTATUS_OK);
	}
}

// Set angle
static EStatus ControlButtonSetAngleMethod(SControl *psControl,
										   double dAngle)
{
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	if (psControlButton)
	{
		(void) ControlSetAngle(psControlButton->eTextHandle,
							   dAngle);
	}

	return(ESTATUS_OK);
}

// Called when we get a key scancode
static EStatus ControlButtonScancodeMethod(SControl *psControl,
										   EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call (or invalid if none)
										   SDL_Scancode eScancode,
										   BOOL bPressed)
{
	SControlButton *psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	// Only allow it to be triggerable by a keyboard if configured to do so
	if (psControlButton->bKeyboardTriggerable)
	{
		// Figure out what we should be set to
		if ((SDL_SCANCODE_KP_ENTER == eScancode) ||
			(SDL_SCANCODE_RETURN == eScancode) ||
			(SDL_SCANCODE_SPACE == eScancode))
		{
			EControlCallbackReason eReason;
			UINT32 u32ReasonData = 0;

			psControl->bAsserted = bPressed;

			eReason = ControlKeyscanToCallbackReason( eScancode );

			if( bPressed )
			{
				u32ReasonData |= BUTTON_PRESSED;
			}

			if (psControlButton->ButtonCallback)
			{
				// Indicate that we've hit/released a spacebar/enter
				psControlButton->ButtonCallback(psControl->eControlHandle,
												eReason,
												u32ReasonData,
												0,
												0);
			}

			WindowUpdated();
		}
	}

	DebugOut("%s: Scancode=%u\n", __FUNCTION__, eScancode);

	return(ESTATUS_OK);
}



// List ofmethods for this control
static SControlMethods sg_sControlButtonMethods =
{
	ECTRLTYPE_BUTTON,							// Type of control
	"Button",									// Name of control
	sizeof(SControlButton),						// Size of control specific structure

	ControlButtonCreateMethod,					// Create a button
	ControlButtonDestroyMethod,					// Destroy a button
	NULL,										// Control enable/disable
	ControlButtonSetPositionMethod,				// New position
	ControlButtonSetVisibleMethod,				// Set visible
	ControlButtonSetAngleMethod,				// Set angle
	ControlButtonDrawMethod,					// Draw the button
	NULL,										// Runtime configuration
	ControlButtonHitTestMethod,					// Hit test
	NULL,										// Focus set/loss
	ControlButtonPressedMethod,					// Button press/release
	NULL,										// Mouseover when selected
	NULL,										// Mouse wheel
	NULL,										// UTF8 Input
	ControlButtonScancodeMethod,				// Scancode input
	NULL,										// Periodic timer
	NULL,										// Mouseover
	NULL,										// Size
};

// Creates a button
EStatus ControlButtonCreate(EWindowHandle eWindowHandle,
							EControlButtonHandle *peButtonHandle,
							INT32 s32XPos,
							INT32 s32YPos)
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlButton *psControlButton;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peButtonHandle,
							s32XPos,
							s32YPos,
							0,
							0,
							ECTRLTYPE_BUTTON,
							NULL);
	ERR_GOTO();

	// Now create a text control
	eStatus = ControlSetLock(*peButtonHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	eStatus = ControlTextCreate(eWindowHandle,
								&psControlButton->eTextHandle,
								0, 0);

	eStatus = ControlSetLock(*peButtonHandle,
							 FALSE,
							 &psControl,
							 eStatus);
errorExit:
	return(eStatus);
}

// Sets the art assets for a button
EStatus ControlButtonSetAssets(EControlButtonHandle eButtonHandle,
							   SControlButtonConfig *psControlButtonConfig)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlButton *psControlButton;
	BOOL bLocked = FALSE;
	BOOL bUpdated = FALSE;

	eStatus = ControlSetLock(eButtonHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;
	psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	// Set the assets

	// Not pressed image
	if (psControlButtonConfig->pu32RGBAButtonNotPressed)
	{
		GraphicsSetImage(psControlButton->psNotPressed,
						 psControlButtonConfig->pu32RGBAButtonNotPressed,
						 psControlButtonConfig->u32XSizeNotPressed,
						 psControlButtonConfig->u32YSizeNotPressed,
						 psControlButtonConfig->bAutoDeallocate);

		bUpdated = TRUE;
	}

	// Pessed image
	if (psControlButtonConfig->pu32RGBAButtonPressed)
	{
		GraphicsSetImage(psControlButton->psPressed,
						 psControlButtonConfig->pu32RGBAButtonPressed,
						 psControlButtonConfig->u32XSizePressed,
						 psControlButtonConfig->u32YSizePressed,
						 psControlButtonConfig->bAutoDeallocate);
		bUpdated = TRUE;
	}

	// Set the appropriate size
	if (psControl->bAsserted)
	{
		psControl->u32XSize = psControlButtonConfig->u32XSizePressed;
		psControl->u32YSize = psControlButtonConfig->u32YSizePressed;
	}
	else
	{
		psControl->u32XSize = psControlButtonConfig->u32XSizeNotPressed;
		psControl->u32YSize = psControlButtonConfig->u32YSizeNotPressed;
	}

	psControlButton->bHitIgnoreTransparent = psControlButtonConfig->bHitIgnoreTransparent;

	// If the button isn't visible, then don't do an update
	if (FALSE == psControl->bVisible)
	{
		bUpdated = FALSE;
	}

errorExit:
	eStatus = ControlSetLock(eButtonHandle,
							 FALSE,
							 &psControl,
							 eStatus);

	if (bUpdated)
	{
		WindowUpdated();
	}

	return(eStatus);
}

EStatus ControlButtonSetText(EControlButtonHandle eButtonHandle,
							 INT32 s32TextXOffsetNonPressed,
							 INT32 s32TextYOffsetNonPressed,
							 INT32 s32TextXOffsetPressed,
							 INT32 s32TextYOffsetPressed,
							 BOOL bCenterOrigin,
							 EFontHandle eFont,
							 UINT16 u16FontSize,
							 char *peString,
							 BOOL bSetColor,
							 UINT32 u32RGBAColor)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlButton *psControlButton;
	BOOL bLocked = FALSE;
	BOOL bUpdated = FALSE;

	eStatus = ControlSetLock(eButtonHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = TRUE;
	psControlButton = (SControlButton *) psControl->pvControlSpecificData;

	psControlButton->s32TextXNonPressedOffset = s32TextXOffsetNonPressed;
	psControlButton->s32TextYNonPressedOffset = s32TextYOffsetNonPressed;
	psControlButton->s32TextXPressedOffset = s32TextXOffsetPressed;
	psControlButton->s32TextYPressedOffset = s32TextYOffsetPressed;
	psControlButton->bCenterOrigin = bCenterOrigin;

	eStatus = ControlTextSet(psControlButton->eTextHandle,
							 eFont,
							 u16FontSize,
							 peString,
							 bSetColor,
							 u32RGBAColor);
	ERR_GOTO();

	eStatus = ControlSetOriginCenter((EControlHandle) psControlButton->eTextHandle,
									 bCenterOrigin);
	ERR_GOTO();

	// If the button isn't visible, then don't do an update
	if (FALSE == psControl->bVisible)
	{
		bUpdated = FALSE;
	}

errorExit:
	eStatus = ControlSetLock(eButtonHandle,
							 FALSE,
							 &psControl,
							 eStatus);

	if (bUpdated)
	{
		WindowUpdated();
	}

	return(eStatus);
}

EStatus ControlButtonSetCallback(EControlButtonHandle eButtonHandle,
								 void (*ButtonCallback)(EControlButtonHandle eButtonHandle,
														EControlCallbackReason eReason,
														UINT32 u32ReasonData,
														INT32 s32ControlRelativeXPos,
														INT32 s32ControlRelativeYPos))
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlButton *psControlButton;

	eStatus = ControlSetLock(eButtonHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlButton = (SControlButton *) psControl->pvControlSpecificData;
	psControlButton->ButtonCallback = ButtonCallback;

	eStatus = ControlSetLock(eButtonHandle,
							 FALSE,
							 &psControl,
							 eStatus);

errorExit:
	return(eStatus);
}

// Set TRUE to allow a button to be triggered by a keyboard press (space/enter) or
// FALSE to prohibit it.
EStatus ControlButtonSetKeyboardTriggerable(EControlButtonHandle eButtonHandle,
											BOOL bKeyboardTriggerable)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlButton *psControlButton;

	eStatus = ControlSetLock(eButtonHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlButton = (SControlButton *) psControl->pvControlSpecificData;
	psControlButton->bKeyboardTriggerable = bKeyboardTriggerable;

	eStatus = ControlSetLock(eButtonHandle,
							 FALSE,
							 &psControl,
							 eStatus);

errorExit:
	return(eStatus);
}

EStatus ControlButtonInit(void)
{
	return(ControlRegister(sg_sControlButtonMethods.eType,
						   &sg_sControlButtonMethods));
}
