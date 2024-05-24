#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

// Control slider structure
typedef struct SControlSlider
{
	// Function to call when the slider changes
	void *pvUserData;
	void (*SliderCallback)(EControlSliderHandle eSliderHandle,
						   void *pvUserData,
						   BOOL bDrag,
						   BOOL bSet,
						   INT32 s32SliderValue);

	// Value of slider
	INT32 s32Value;

	// Low/high range
	INT32 s32Low;
	INT32 s32High;

	// Track length (the # of pixels the thumb moves in the track)
	UINT32 u32TrackLength;

	// We keep the thumb X and Y size around for convenience
	INT32 s32XPosThumb;
	UINT32 u32XSizeThumb;
	UINT32 u32YSizeThumb;
	BOOL bThumbDrag;				// Set TRUE if we hit on the thumb and are dragging it
	INT32 s32ThumbDragOffset;		// Offset from center to normalize to track position

	SGraphicsImage *psTrack;		// Slider track graphic
	EControlImageHandle eThumb;		// Thumb graphic

} SControlSlider;

// This converts an incoming value to its track position
static INT32 ControlSliderGetThumbPos(SControlSlider *psControlSlider,
									  INT32 s32Value)
{
	INT32 s32XPosThumb = 0;
	double dPercent;

	if (psControlSlider->s32Low <= psControlSlider->s32High)
	{
		// Low is higher than high
		s32Value -= psControlSlider->s32Low;
		dPercent = ((double) s32Value) / ((double) ((psControlSlider->s32High - psControlSlider->s32Low) + 1));
	}
	else
	{
		// High is lower than low
		s32Value -= psControlSlider->s32High;
		dPercent = 1.0 - (((double) s32Value) / ((double) ((psControlSlider->s32Low - psControlSlider->s32High) + 1)));
	}

	// Set the slider
	s32XPosThumb = (INT32) ((double) (psControlSlider->u32TrackLength - 1) * dPercent);
	if (s32XPosThumb >= (INT32) (psControlSlider->u32TrackLength))
	{
		s32XPosThumb = (INT32) (psControlSlider->u32TrackLength - 1);
	}

	return(s32XPosThumb);
}

// This converts an incoming track position to its value
static INT32 ControlSliderGetFromTrackPos(SControlSlider *psControlSlider,
										  INT32 s32TrackPosition)
{
	double dPercent;
	INT32 s32Value;

	dPercent = (double) s32TrackPosition / ((double) (psControlSlider->u32TrackLength - 1));
	if (psControlSlider->s32High > psControlSlider->s32Low)
	{
		s32Value = (INT32) ((double) ((psControlSlider->s32High - psControlSlider->s32Low)) * dPercent);
		s32Value += psControlSlider->s32Low;
	}
	else
	{
		s32Value = (INT32) ((double) ((psControlSlider->s32Low - psControlSlider->s32High)) * dPercent);
		s32Value = psControlSlider->s32Low - s32Value;
	}

	return(s32Value);
}

static EStatus ControlSliderLockPointers(EControlSliderHandle eControlSliderHandle,
										 BOOL bLock,
										 SControl **ppsControl,
										 SControlSlider **ppsControlSlider,
										 EStatus eStatusIncoming)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlSlider *psControlSlider = NULL;

	eStatus = ControlSetLock(eControlSliderHandle,
							 bLock,
							 &psControl,
							 eStatusIncoming);
	ERR_GOTO();

	if (ppsControl)
	{
		*ppsControl = psControl;
	}

	psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	if (ppsControlSlider)
	{
		*ppsControlSlider = psControlSlider;
	}

errorExit:
	return(eStatus);
}

// Called when someone wants to create a slider control
static EStatus ControlSliderCreateMethod(SControl *psControl,
									     void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	// Create a blank track image
	eStatus = GraphicsCreateImage(&psControlSlider->psTrack,
								  TRUE,
								  0, 0);
	ERR_GOTO();

	// 0-100 Range by default
	psControlSlider->s32Low = 0;
	psControlSlider->s32High = 100;

	// Make it focusable by default
	psControl->bFocusable = TRUE;
	
errorExit:
	if (eStatus != ESTATUS_OK)
	{
		if (psControlSlider)
		{
			GraphicsDestroyImage(&psControlSlider->psTrack);
		}
	}

	return(eStatus);
}

// Destroys a slider
static EStatus ControlSliderDestroyMethod(SControl *psControl)
{
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;
	SGraphicsImage *psDrawImage = NULL;

	if (psControlSlider)
	{
		GraphicsDestroyImage(&psControlSlider->psTrack);
		(void) ControlDestroy((EControlHandle *) &psControlSlider->eThumb);
	}

	return(ESTATUS_OK);
}

// Draws a slider control
static EStatus ControlSliderDrawMethod(SControl *psControl,
									   UINT8 u8WindowIntensity,
									   INT32 s32WindowXPos,
									   INT32 s32WindowYPos)
{
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	if (psControlSlider)
	{
		// Draw the track first
		if (psControlSlider->psTrack)
		{
			INT32 s32XTrackPos = 0;
			INT32 s32YTrackPos = 0;

			s32XTrackPos = (psControl->u32XSize >> 1) - (psControlSlider->psTrack->u32TotalXSize >> 1);
			s32YTrackPos = (psControl->u32YSize >> 1) - (psControlSlider->psTrack->u32TotalYSize >> 1);

			// Rotate the track in to place
			ControlRotateClockwise(psControl->dAngle,
								   &s32XTrackPos,
								   &s32YTrackPos);

			GraphicsSetGreyscale(psControlSlider->psTrack,
								 psControl->bDisabled);

			// Draw the track
			GraphicsDraw(psControlSlider->psTrack,
						 s32WindowXPos + psControl->s32XPos + s32XTrackPos,
						 s32WindowYPos + psControl->s32YPos + s32YTrackPos,
						 psControl->bOriginCenter,
						 psControl->dAngle);
		}

		// Set the thumb control position if there is one
		if (psControlSlider->u32XSizeThumb)
		{
			INT32 s32XOffsetThumb = 0;
			INT32 s32YOffsetThumb = 0;

			s32XOffsetThumb = (psControl->u32XSize >> 1) - (psControlSlider->u32TrackLength >> 1) + psControlSlider->s32XPosThumb - (psControlSlider->u32XSizeThumb >> 1);
			s32YOffsetThumb = (psControl->u32YSize >> 1) - (psControlSlider->u32YSizeThumb >> 1);

			// Rotate the thumb in to place
			ControlRotateClockwise(psControl->dAngle,
								   &s32XOffsetThumb,
								   &s32YOffsetThumb);

			(void) ControlSetAngle((EControlHandle) psControlSlider->eThumb,
									psControl->dAngle);

			(void) ControlSetPosition((EControlHandle) psControlSlider->eThumb,
									  psControl->s32XPos + s32XOffsetThumb,
									  psControl->s32YPos + s32YOffsetThumb);
		}
	}

	return(ESTATUS_OK);
}

static EStatus ControlSliderSetPositionMethod(SControl *psControl,			// When a new position is set
											  INT32 s32WindowXPos,
											  INT32 s32WindowYPos)
{
	EStatus eStatus = ESTATUS_OK;
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	// Set the thumb control position if there is one
	if (psControlSlider->u32XSizeThumb)
	{
		eStatus = ControlSetPosition((EControlHandle) psControlSlider->eThumb,
									 psControl->s32XPos + (psControl->u32XSize >> 1) - (psControlSlider->u32TrackLength >> 1) + psControlSlider->s32XPosThumb,
									 psControl->s32YPos + (psControl->u32YSize >> 1));
	}

	return(eStatus);
}

static EStatus ControlSliderSetVisibleMethod(SControl *psControl,
											 BOOL bVisible)
{
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	// Set/clear the thumb control's visibility as well
	(void) ControlSetVisible((EControlHandle) psControlSlider->eThumb,
							 bVisible);

	return(ESTATUS_OK);
}

// This will bump a slider thumb position advance/retreat
static void ControlSliderAdvanceRetreat(SControl *psControl,
										SControlSlider *psControlSlider,
										BOOL bAdvance)
{
	INT32 s32Value;

#define	STEP_PERCENT	10

	if (FALSE == bAdvance)
	{
		// Reduce by STEP_PERCENT
		psControlSlider->s32XPosThumb -= ((INT32) ((double) psControlSlider->u32TrackLength * (double) (STEP_PERCENT / 100.0)));
	}
	else
	{
		// Increase by STEP_PERCENT
		psControlSlider->s32XPosThumb += ((INT32) ((double) psControlSlider->u32TrackLength * (double) (STEP_PERCENT / 100.0)));
	}

	if (psControlSlider->s32XPosThumb < 0)
	{
		psControlSlider->s32XPosThumb = 0;
	}

	if (psControlSlider->s32XPosThumb >= (INT32) psControlSlider->u32TrackLength)
	{
		psControlSlider->s32XPosThumb = (INT32) (psControlSlider->u32TrackLength - 1);
	}

	s32Value = ControlSliderGetFromTrackPos(psControlSlider,
											psControlSlider->s32XPosThumb);

	if (s32Value != psControlSlider->s32Value)
	{
		psControlSlider->s32Value = s32Value;
	}

	if (psControlSlider->SliderCallback)
	{
		psControlSlider->SliderCallback((EControlSliderHandle) psControl->eControlHandle,
										psControlSlider->pvUserData,
										FALSE,
										FALSE,
										s32Value);
	}

	WindowUpdated();
}


// Slider pressed
static EStatus ControlSliderPressedMethod(SControl *psControl,
										  UINT8 u8ButtonMask,
										  INT32 s32ControlRelativeXPos,
										  INT32 s32ControlRelativeYPos,
										  BOOL bPressed)
{
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;
	INT32 s32WindowRelativeXPos;
	INT32 s32WindowRelativeYPos;

	// Rotate the thumb into
	ControlRotateClockwise(psControl->dAngle,
						   &s32ControlRelativeXPos,
						   &s32ControlRelativeYPos);

	// Add in the control's relative position so we're now window relative
	s32WindowRelativeXPos = psControl->s32XPos + s32ControlRelativeXPos;
	s32WindowRelativeYPos = psControl->s32YPos + s32ControlRelativeYPos;

	if (bPressed)
	{
		EStatus eStatus;

		// Figure out if we have a hit on the thumb

		// See if we have a hit on the thumb
		eStatus = ControlHitTest(psControl->eWindowHandle,
								 psControlSlider->eThumb,
								 u8ButtonMask,
								 &s32WindowRelativeXPos,
								 &s32WindowRelativeYPos,
								 TRUE);

		if (ESTATUS_OK == eStatus)
		{
			// We're a hit! Drag the thumb
			psControlSlider->bThumbDrag = TRUE;
			psControlSlider->s32ThumbDragOffset = (s32WindowRelativeXPos - (psControlSlider->u32XSizeThumb >> 1));
		}
		else
		{
			BOOL bAdvance = TRUE;	// Assume we advance

			// Rotate screen relative control coordinates to control relative coordinates
			ControlRotateCounterClockwise(psControl->dAngle,
										  &s32ControlRelativeXPos,
										  &s32ControlRelativeYPos);

			if (s32ControlRelativeXPos < psControlSlider->s32XPosThumb)
			{
				bAdvance = FALSE;	// We retreat
			}

			ControlSliderAdvanceRetreat(psControl,
										psControlSlider,
										bAdvance);
		}
	}
	else
	{
		// If we're dragging, let everyone know we've released
		if (psControlSlider->bThumbDrag)
		{
			INT32 s32Value;

			s32Value = ControlSliderGetFromTrackPos(psControlSlider,
													psControlSlider->s32XPosThumb);

			// Move the thumb to its final resting place
			psControlSlider->s32XPosThumb = ControlSliderGetThumbPos(psControlSlider,
																	 s32Value);

			if (psControlSlider->SliderCallback)
			{
				psControlSlider->SliderCallback((EControlSliderHandle) psControl->eControlHandle,
												psControlSlider->pvUserData,
												FALSE,
												FALSE,
												s32Value);
			}

			// Need to update the display
			WindowUpdated();
		}

		// Not dragging nor doing anything else now
		psControlSlider->bThumbDrag = FALSE;
	}

	return(ESTATUS_OK);
}

// Drag (move mouse while control is asserted) - coordinates are control relative 
// in native (unrotated) axes
static EStatus ControlSliderDragMethod(SControl *psControl,
									   INT32 s32ControlRelativeXPos,
									   INT32 s32ControlRelativeYPos)
{
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	// Only pay attention if we're dragging the thumb
	if (psControlSlider->bThumbDrag)
	{
		// Normalize the mouse from 0-track length
		s32ControlRelativeXPos -= ((psControl->u32XSize >> 1) - (psControlSlider->u32TrackLength >> 1)) + psControlSlider->s32ThumbDragOffset;

		if ((s32ControlRelativeXPos < 0) || (s32ControlRelativeXPos >= (INT32) psControlSlider->u32TrackLength))
		{
			// Don't touch it
		}
		else
		{
			if (psControlSlider->s32XPosThumb != s32ControlRelativeXPos)
			{
				INT32 s32Value;

				s32Value = ControlSliderGetFromTrackPos(psControlSlider,
														s32ControlRelativeXPos);

				if (s32Value != psControlSlider->s32Value)
				{
					psControlSlider->s32Value = s32Value;
					if (psControlSlider->SliderCallback)
					{
						psControlSlider->SliderCallback((EControlSliderHandle) psControl->eControlHandle,
														psControlSlider->pvUserData,
														TRUE,
														FALSE,
														s32Value);
					}
				}

				psControlSlider->s32XPosThumb = s32ControlRelativeXPos;
				WindowUpdated();
			}
		}
	}

	return(ESTATUS_OK);
}


// Called when we get a key scancode
static EStatus ControlSliderScancodeMethod(SControl *psControl,
										   EControlHandle eControlInterceptedHandle,	// The handle that intercepted this call (or invalid if none)
										   SDL_Scancode eScancode,
										   BOOL bPressed)
{
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	// If someone hits a key...
	if (bPressed)
	{
		// And it's either the up or down arrow
		if ((SDL_SCANCODE_UP == eScancode) ||
			(SDL_SCANCODE_LEFT == eScancode))
		{
			// Bump the thumb left/up
			ControlSliderAdvanceRetreat(psControl,
										psControlSlider,
										FALSE);
		}

		if ((SDL_SCANCODE_RIGHT == eScancode) ||
			(SDL_SCANCODE_DOWN == eScancode))
		{
			// Bump the thumb right/down
			ControlSliderAdvanceRetreat(psControl,
										psControlSlider,
										TRUE);
		}
	}

	return(ESTATUS_OK);
}

static EStatus ControlSliderWheelMethod(SControl *psControl,
										INT32 s32WheelChange)
{
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	// We've moved by some amount
	while (s32WheelChange)
	{
		BOOL bAdvance = FALSE;

		if (s32WheelChange < 0)
		{
			bAdvance = TRUE;
			++s32WheelChange;
		}

		if (s32WheelChange > 0)
		{
			--s32WheelChange;
		}

		ControlSliderAdvanceRetreat(psControl,
									psControlSlider,
									bAdvance);
	}

	return(ESTATUS_OK);
}

static EStatus ControlSliderSetDisableMethod(SControl *psControl,
											 BOOL bDisabledBefore,
											 BOOL bDisabled)
{
	EStatus eStatus = ESTATUS_OK;
	SControlSlider *psControlSlider = (SControlSlider *) psControl->pvControlSpecificData;

	if (psControlSlider)
	{
		eStatus = ControlSetDisable((EControlHandle) psControlSlider->eThumb,
									 bDisabled);
	}

	return(eStatus);
}

// List ofmethods for this control
static SControlMethods sg_sControlSliderMethods =
{
	ECTRLTYPE_SLIDER,							// Type of control
	"Slider",									// Name of control
	sizeof(SControlSlider),						// Size of control specific structure

	ControlSliderCreateMethod,					// Create a slider
	ControlSliderDestroyMethod,					// Destroy a slider
	ControlSliderSetDisableMethod,				// Control enable/disable
	ControlSliderSetPositionMethod,				// New position
	ControlSliderSetVisibleMethod,				// Set visible
	NULL,										// Set angle
	ControlSliderDrawMethod,					// Draw the slider
	NULL,										// Runtime configuration
	NULL,										// Hit test
	NULL,										// Focus set/loss
	ControlSliderPressedMethod,					// Mouse button press/release
	ControlSliderDragMethod,					// Drag when selected
	ControlSliderWheelMethod,					// Mouse wheel
	NULL,										// UTF8 Input
	ControlSliderScancodeMethod,				// Scancode input
	NULL,										// Periodic timer
	NULL,										// Mouseover
	NULL,										// Size
};

// Creates a slider
EStatus ControlSliderCreate(EWindowHandle eWindowHandle,
							EControlSliderHandle *peControlSliderHandle,
							INT32 s32XPos,
							INT32 s32YPos)
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControlSlider *psControlSlider;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peControlSliderHandle,
							s32XPos,
							s32YPos,
							0,
							0,
							ECTRLTYPE_SLIDER,
							NULL);
	ERR_GOTO();

	eStatus = ControlSliderLockPointers(*peControlSliderHandle,
										TRUE,
										NULL,
										&psControlSlider,
										eStatus);

	if (ESTATUS_OK == eStatus)
	{
		eStatus = ControlImageCreate(eWindowHandle,
									 &psControlSlider->eThumb,
									 0, 0);
	}

	eStatus = ControlSliderLockPointers(*peControlSliderHandle,
										FALSE,
										NULL,
										NULL,
										eStatus);
errorExit:
	return(eStatus);
}

// Binds the s32Value to the range in s32Low/s32High
static void ControlSliderClipValue(SControlSlider *psControlSlider)
{
	if (psControlSlider->s32Low < psControlSlider->s32High)
	{
		// Low to high
		if (psControlSlider->s32Value < psControlSlider->s32Low)
		{
			psControlSlider->s32Value = psControlSlider->s32Low;
		}
		if (psControlSlider->s32Value > psControlSlider->s32High)
		{
			psControlSlider->s32Value = psControlSlider->s32High;
		}
	}
	else
	{
		// High to low
		if (psControlSlider->s32Value < psControlSlider->s32High)
		{
			psControlSlider->s32Value = psControlSlider->s32High;
		}
		if (psControlSlider->s32Value > psControlSlider->s32Low)
		{
			psControlSlider->s32Value = psControlSlider->s32Low;
		}
	}
}

// Sets the slider's graphical assets (thumb and track)
EStatus ControlSliderSetAssets(EControlSliderHandle eControlSliderHandle,
							   SControlSliderConfig *psControlSlierConfig)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlSlider *psControlSlider = NULL;
	UINT32 u32XSize = 0;
	UINT32 u32YSize = 0;

	// Track length cannot exceed the track's X size
	if (psControlSlierConfig->u32TrackLength > psControlSlierConfig->u32XSizeTrack)
	{
		eStatus = ESTATUS_UI_TRACK_LENGTH_EXCEEDS_TRACK_SIZE;
		goto errorExit;
	}

	// Lock the slider and set the range
	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										TRUE,
										&psControl,
										&psControlSlider,
										ESTATUS_OK);
	ERR_GOTO();

	// Set the track assets
	GraphicsClearImage(psControlSlider->psTrack);

	GraphicsSetImage(psControlSlider->psTrack,
					 psControlSlierConfig->pu32RGBATrack,
					 psControlSlierConfig->u32XSizeTrack,
					 psControlSlierConfig->u32YSizeTrack,
					 psControlSlierConfig->bAutoDeallocate);

	// Now set the thumb image
	eStatus = ControlImageSetAssets(psControlSlider->eThumb,
									psControlSlierConfig->pu32RGBAThumb,
									psControlSlierConfig->u32XSizeThumb,
									psControlSlierConfig->u32YSizeThumb,
									psControlSlierConfig->bAutoDeallocate);
	ERR_GOTO();

	// Record the thumb size for later drawing and positioning
	psControlSlider->u32XSizeThumb = psControlSlierConfig->u32XSizeThumb;
	psControlSlider->u32YSizeThumb = psControlSlierConfig->u32YSizeThumb;

	// Compute the Y size - If the thumb is bigger, use that
	u32YSize = psControlSlierConfig->u32YSizeTrack;
	if (psControlSlierConfig->u32YSizeThumb > u32YSize)
	{
		u32YSize = psControlSlierConfig->u32YSizeThumb;
	}

	// Now the X size. Figure out if the thumb overhang is
	u32XSize = psControlSlierConfig->u32XSizeTrack - psControlSlierConfig->u32TrackLength;
	if (psControlSlierConfig->u32XSizeThumb > u32XSize)
	{
		u32XSize = psControlSlierConfig->u32XSizeThumb;
	}

	// Add the track length back in
	u32XSize += psControlSlierConfig->u32TrackLength;

	psControlSlider->u32TrackLength = psControlSlierConfig->u32TrackLength;
	psControl->u32XSize = u32XSize;
	psControl->u32YSize = u32YSize;

	// If the slider is visible, go display it
	if (psControl->bVisible)
	{
		WindowUpdated();
	}

errorExit:
	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										FALSE,
										NULL,
										NULL,
										eStatus);

	return(eStatus);
}

EStatus ControlSliderSetRange(EControlSliderHandle eControlSliderHandle,
							  INT32 s32Low,
							  INT32 s32High)
{
	EStatus eStatus;
	SControlSlider *psControlSlider;
	INT32 s32XPosThumbOld;

	// Lock the slider and set the range
	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										TRUE,
										NULL,
										&psControlSlider,
										ESTATUS_OK);
	ERR_GOTO();

	psControlSlider->s32Low = s32Low;
	psControlSlider->s32High = s32High;

	ControlSliderClipValue(psControlSlider);

	s32XPosThumbOld = psControlSlider->s32XPosThumb;
	psControlSlider->s32XPosThumb = ControlSliderGetThumbPos(psControlSlider,
															 psControlSlider->s32Value);

	if (s32XPosThumbOld != psControlSlider->s32XPosThumb)
	{
		// It moved
		WindowUpdated();
	}

	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										FALSE,
										NULL,
										NULL,
										eStatus);

errorExit:
	return(eStatus);
}

EStatus ControlSliderSetValue(EControlSliderHandle eControlSliderHandle,
							  INT32 s32Value)
{
	EStatus eStatus;
	SControlSlider *psControlSlider;
	INT32 s32XPosThumbOld;

	// Lock the slider and set the value
	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										TRUE,
										NULL,
										&psControlSlider,
										ESTATUS_OK);
	ERR_GOTO();

	if (psControlSlider->s32Value != s32Value)
	{
		psControlSlider->s32Value = s32Value;

		// If there's a callback, let 'em know
		if (psControlSlider->SliderCallback)
		{
			psControlSlider->SliderCallback(eControlSliderHandle,
											psControlSlider->pvUserData,
											FALSE,
											TRUE,
											s32Value);
		}
	}

	ControlSliderClipValue(psControlSlider);

	s32XPosThumbOld = psControlSlider->s32XPosThumb;
	psControlSlider->s32XPosThumb = ControlSliderGetThumbPos(psControlSlider,
															 s32Value);

	if (s32XPosThumbOld != psControlSlider->s32XPosThumb)
	{
		// It moved
		WindowUpdated();
	}

	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										FALSE,
										NULL,
										NULL,
										eStatus);

errorExit:
	return(eStatus);
}

EStatus ControlSliderGetValue(EControlSliderHandle eControlSliderHandle,
							  INT32 *ps32Value)
{
	EStatus eStatus;
	SControlSlider *psControlSlider;

	// Lock the slider and set the callback
	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										TRUE,
										NULL,
										&psControlSlider,
										ESTATUS_OK);
	ERR_GOTO();

	if (ps32Value)
	{
		*ps32Value = psControlSlider->s32Value;
	}

	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										FALSE,
										NULL,
										NULL,
										eStatus);

errorExit:
	return(eStatus);
}

EStatus ControlSliderSetCallback(EControlSliderHandle eControlSliderHandle,
								 void *pvUserData,
								 void (*SliderCallback)(EControlSliderHandle eControlSliderHandle,
														void *pvUserData,
														BOOL bDrag,
														BOOL bSet,
														INT32 s32Value))
{
	EStatus eStatus;
	SControlSlider *psControlSlider;

	// Lock the slider and set the callback
	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										TRUE,
										NULL,
										&psControlSlider,
										ESTATUS_OK);
	ERR_GOTO();

	psControlSlider->pvUserData = pvUserData;
	psControlSlider->SliderCallback = SliderCallback;

	eStatus = ControlSliderLockPointers(eControlSliderHandle,
										FALSE,
										NULL,
										NULL,
										eStatus);

errorExit:
	return(eStatus);
}

EStatus ControlSliderInit(void)
{
	return(ControlRegister(sg_sControlSliderMethods.eType,
						   &sg_sControlSliderMethods));
}
