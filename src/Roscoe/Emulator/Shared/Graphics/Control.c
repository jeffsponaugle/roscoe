#include <math.h>
#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"
#include "Shared/HandlePool.h"
#include "Shared/Timer.h"

// Maximum # of controls
#define	MAX_CONTROLS		1024

// Control repeat timer interval (in milliseconds)
#define	CONTROL_REPEAT_TIMER_INTERVAL		10

// Handle pool for our controls
static SHandlePool *sg_psControlHandlePool;

// Methods for our various known controls
static SControlMethods *sg_psControlMethods[ECTRLTYPE_MAX];

// Linked list of controls that have active repeat schedules
static SControlList *sg_psControlRepeatSchedules;
static SOSCriticalSection sg_sControlRepeatCriticalSection;
static STimer *sg_psControlRepeatTimer;

// Register a specific control type
EStatus ControlRegister(EControlType eType,
						SControlMethods *psMethods)
{
	// Make sure the control type is in range
	BASSERT(eType < ECTRLTYPE_MAX);

	// Make sure this control type hasn't been registered before
	BASSERT(NULL == sg_psControlMethods[eType]);

	sg_psControlMethods[eType] = psMethods;
	return(ESTATUS_OK);
}

// Called from the window thread when a control is to be created
EStatus ControlCreateCallback(SControl *psControl,
							  void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;

	// This control had better have some methods
	BASSERT(psControl->psMethods);

	// Create it!
	if (psControl->psMethods->Create)
	{
		eStatus = psControl->psMethods->Create(psControl,
											   pvControlInitConfiguration);
	}

	return(eStatus);
}

// This function is called when the control is being deallocated
static EStatus ControlDestroyHandleCallback(EHandleGeneric eHandleGeneric,
									  void *pvHandleStructBase)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = (SControl *) pvHandleStructBase;

	BASSERT(psControl->psMethods);

	eStatus = WindowRemoveControl(psControl->eWindowHandle,
								  eHandleGeneric);
	ERR_GOTO();

	// If this method has a destroy method, call it
	if (psControl->psMethods->Destroy)
	{
		eStatus = psControl->psMethods->Destroy(psControl);
		ERR_GOTO();
	}

	// Clear out all the control stuff
	psControl->s32XPos = 0;
	psControl->s32YPos = 0;

	psControl->u32XSize = 0;
	psControl->u32YSize = 0;

	psControl->bVisible = false;
	psControl->bDisabled = false;
	psControl->bFocusable = false;

	HandleSetInvalid((EHandleGeneric *) &psControl->eWindowHandle);
	HandleSetInvalid((EHandleGeneric *) &psControl->eControlHandle);
	HandleSetInvalid((EHandleGeneric *) &psControl->eControlInterceptorHandle);

	psControl->psMethods = NULL;

	// And control specific data
	SafeMemFree(psControl->pvControlSpecificData);

errorExit:
	return(eStatus);
}

// Called from the window thread when a control is destroyed
EStatus ControlDestroyCallback(EControlHandle eControlHandle)
{
	EStatus eStatus;
	bool bUpdate = false;
	SControl *psControl = NULL;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	// Only do a window update if the control is visible
	bUpdate = psControl->bVisible;

	// Destroy the control
	eStatus = HandleDeallocate(sg_psControlHandlePool,
							   &eControlHandle,
							   ESTATUS_OK);

errorExit:
	if (bUpdate)
	{
		WindowUpdated();
	}

	return(eStatus);
}

// Called from the window thread when a control is to be configured
EStatus ControlConfigureCallback(EControlHandle eControlHandle,
								 void *pvConfigurationStructure)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	// Get the psControl pointer
	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	// If we have a configuration method, then call it
	if (psControl->psMethods->Configure)
	{
		eStatus = psControl->psMethods->Configure(psControl,
												  pvConfigurationStructure);
	}
	else
	{
		// Otherwise, incidate not implemented
		eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
	}

	// Destroy the control
	eStatus = ControlSetLock(eControlHandle,
							 false,
							 &psControl,
							 ESTATUS_OK);
errorExit:
	return(eStatus);
}

// Destroy a control
EStatus ControlDestroy(EControlHandle *peControlHandle)
{
	EStatus eStatus;

	eStatus = WindowControlDestroy(peControlHandle);
	ERR_GOTO();

	// Set the handle invalid
	HandleSetInvalid((EHandleGeneric *) peControlHandle);
	
errorExit:
	return(eStatus);
}

EStatus ControlSetVisible(EControlHandle eControlHandle,
						  bool bVisible)
{
	EStatus eStatus;
	bool bUpdate = false;
	SControl *psControl = NULL;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	if (bVisible != psControl->bVisible)
	{
		bUpdate = true;
		psControl->bVisible = bVisible;
	}

	if (psControl->psMethods->Visible)
	{
		eStatus = psControl->psMethods->Visible(psControl,
												bVisible);
	}

errorExit:
	if (bUpdate)
	{
		WindowUpdated();
	}

	return(eStatus);
}

EStatus ControlSetDisable(EControlHandle eControlHandle,
						  bool bDisabled)
{
	EStatus eStatus;
	bool bUpdate = false;
	SControl *psControl = NULL;
	bool bOldState;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	bOldState = psControl->bDisabled;
	if (bDisabled != psControl->bDisabled)
	{
		bUpdate = true;
		psControl->bDisabled = bDisabled;
	}

	if (psControl->psMethods->Disable)
	{
		eStatus = psControl->psMethods->Disable(psControl,
												bOldState,
												bDisabled);
	}

errorExit:
	if (bUpdate)
	{
		WindowUpdated();
	}

	return(eStatus);
}


EStatus ControlSetAngle(EControlHandle eControlHandle,
						double dAngle)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	bool bUpdate = false;

	// Only 0-359.9999 allowed
	dAngle = fmod(dAngle, 360.0);

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	if (dAngle != psControl->dAngle)
	{
		bUpdate = true;
	}

	if (false == psControl->bVisible)
	{
		bUpdate = false;
	}

	psControl->dAngle = dAngle;

	if (psControl->psMethods->Angle)
	{
		eStatus = psControl->psMethods->Angle(psControl,
											  dAngle);
	}

	// If the angle has changed and the control is visible, then update it
	if (bUpdate)
	{
		WindowUpdated();
	}

errorExit:
	return(eStatus);
}

EStatus ControlSetOriginCenter(EControlHandle eControlHandle,
							   bool bOriginCenter)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	bool bUpdate = false;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	// If the origin changes, then we need to update
	if (psControl->bOriginCenter != bOriginCenter)
	{
		bUpdate = true;
	}

	// IF we're not visible, then no update
	if (false == psControl->bVisible)
	{
		bUpdate = false;
	}

	// Set center origin regardless of whether or not we're updating the window
	psControl->bOriginCenter = bOriginCenter;

	// If the angle has changed and the control is visible, then update it
	if (bUpdate)
	{
		WindowUpdated();
	}

errorExit:
	return(eStatus);
}

EStatus ControlSetUserData(EControlHandle eControlHandle,
						   void *pvUserData)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	psControl->pvUserData = pvUserData;

errorExit:
	return(eStatus);
}

EStatus ControlGetUserData(EControlHandle eControlHandle,
						   void **ppvUserData)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	if (ppvUserData)
	{
		*ppvUserData = psControl->pvUserData;
	}

errorExit:
	return(eStatus);
}

uint32_t ControlGetGfxAttributes(SControl *psControl)
{
	uint32_t u32Attributes = 0;

	if (psControl->bHFlip)
	{
		u32Attributes |= GFXATTR_SWAP_HORIZONAL;
	}

	if (psControl->bVFlip)
	{
		u32Attributes |= GFXATTR_SWAP_VERTICAL;
	}

	return(u32Attributes);
}

EStatus ControlSetFlip(EControlHandle eControlHandle,
							  bool bHFlip,
							  bool bVFlip)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	bool bUpdate = false;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	// If the angle has changed and the control is visible, then update it
	if (bHFlip != psControl->bHFlip)
	{
		psControl->bHFlip = bHFlip;
		bUpdate = true;
	}

	if (bVFlip != psControl->bVFlip)
	{
		psControl->bVFlip = bVFlip;
		bUpdate = true;
	}

	if (false == psControl->bVisible)
	{
		bUpdate = false;
	}

errorExit:
	if (bUpdate)
	{
		WindowUpdated();
	}

	return(eStatus);
}

EStatus ControlDraw(EControlHandle eControlHandle,
					uint8_t u8WindowIntensity,
					int32_t s32WindowXPos,
					int32_t s32WindowYPos)
{
	EStatus eStatus;
	bool bUpdate = false;
	SControl *psControl = NULL;

	// Get the psControl pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_LOCK,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	// Only draw if visible
	if (psControl->bVisible)
	{
		if (psControl->psMethods->Draw)
		{
			psControl->psMethods->Draw(psControl,
									   u8WindowIntensity,
									   s32WindowXPos,
									   s32WindowYPos);
		}
	}

	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_UNLOCK,
							(void **) &psControl,
							true,
							ESTATUS_OK);

errorExit:
	return(eStatus);
}

EStatus ControlConfigure(EControlHandle eControlHandle,
						 void *pvConfigurationStructure)
{
	EStatus eStatus;

	eStatus = WindowControlConfigure(eControlHandle,
									 pvConfigurationStructure);
	return(eStatus);
}

bool ControlHandleIsValid(EControlHandle eControlHandle)
{
	if (ESTATUS_OK == HandleSetLock(sg_psControlHandlePool,
									NULL,
									(EHandleGeneric) eControlHandle,
									EHANDLE_NONE,
									NULL,
									true,
									ESTATUS_OK))
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// This will return ESTATUS_OK if the incoming coordinates are over the top of an opaque pixel.
// Otherwise, ESTATUS_UI_CONTROL_NOT_HIT is returned. Coordinates need to be within the
// bounding box of the unrotated image.
EStatus ControlHitTestImage(SGraphicsImage *psGraphicsImage,
							int32_t s32XPos,
							int32_t s32YPos)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t *pu32RGBAPtr;

	// If there isn't a graphics image, just return that we don't hit it
	if ((NULL == psGraphicsImage) ||
		(NULL == psGraphicsImage->pu32RGBA))
	{
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
		goto errorExit;
	}

	// Bounds check on the image - top and left
	if ((s32XPos < 0) || (s32YPos < 0))
	{
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
		goto errorExit;
	}

	// Now right and bottom
	if ((s32XPos >= (int32_t) psGraphicsImage->u32TotalXSize) ||
		(s32YPos >= (int32_t) psGraphicsImage->u32TotalYSize))
	{
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
		goto errorExit;
	}

	// Check out our alpha channel for this pixel. Anything below 0xf0 shall be
	// considered solid and a hit
	pu32RGBAPtr = psGraphicsImage->pu32RGBA + (s32YPos * psGraphicsImage->u32TotalXSize) + s32XPos;

	if (((*pu32RGBAPtr) >> 24) >= 0x10)
	{
		eStatus = ESTATUS_OK;
	}
	else
	{
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
	}

errorExit:
	return(eStatus);
}

// Call to move a control's position within a window
EStatus ControlSetPosition(EControlHandle eControlHandle,
						   int32_t s32WindowXPos,
						   int32_t s32WindowYPos)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;
	bool bUpdated = false;
	
	// Get the control pointer
	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	// If our x/y position has changed and the control is visible, do an update
	if ((psControl->s32XPos != s32WindowXPos) ||
		(psControl->s32YPos != s32WindowYPos))
	{
		bUpdated = true;
	}
	
	// Set the new X/Y position of this control
	psControl->s32XPos = s32WindowXPos;
	psControl->s32YPos = s32WindowYPos;

	// If we have a position callback, then call it
	if (psControl->psMethods->Position)
	{
		eStatus = psControl->psMethods->Position(psControl,
												 s32WindowXPos,
												 s32WindowYPos);
	}

	// Time to unlock
	eStatus = ControlSetLock(eControlHandle,
							 false,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

errorExit:
	if (bUpdated)
	{
		WindowUpdated();
	}

	return(eStatus);
}

// Call to move a control as focusable
EStatus ControlSetFocusable(EControlHandle eControlHandle,
							bool bFocusable)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;

	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);

	if (ESTATUS_OK == eStatus)
	{
		psControl->bFocusable = bFocusable;
	}

	return(eStatus);
}

// Call to see if a control is asserted (button pressed and held on it)
EStatus ControlIsAsserted(EControlHandle eControlHandle,
						  bool *pbIsAsserted)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;

	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);

	if ((ESTATUS_OK == eStatus) &&
		(pbIsAsserted))
	{
		*pbIsAsserted = psControl->bAsserted;
	}

	return(eStatus);
}


// Called when the windowing code is trying to figure out if there's a mouse
// click on a control.
EStatus ControlHitTest(EWindowHandle eWindowHandle,
					   EControlHandle eControlHandle,
					   uint8_t u8ButtonMask,
					   int32_t *ps32WindowRelativeX,		// Incoming WINDOW relative x/y coordinates
					   int32_t *ps32WindowRelativeY,
					   bool bTestOnly)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;
	
	// Get the control pointer
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_NONE,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	// If we're just testing for intersection, then allow it regardless of whether
	// or not the control is focusable or disabled.
	if (false == bTestOnly)
	{
		if (((false == psControl->bFocusable) ||
			 (psControl->bDisabled)) &&
			 (false == psControl->bHitTestAlways))
		{
			eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
			goto errorExit;
		}

		// If this control doesn't have a response to button presses, it 'aint us
		if (NULL == psControl->psMethods->Button)
		{
			eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
			goto errorExit;
		}
	}

	if ((false == psControl->bVisible) &&
		(false == psControl->bHitTestAlways))
	{
		// If you can't see the control, then it's not hit
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
		goto errorExit;
	}

	// Adjust the coordinates to be control relative
	*ps32WindowRelativeX -= psControl->s32XPos;
	*ps32WindowRelativeY -= psControl->s32YPos;

	// Normalize the rotation of this control as it is now control relative
	ControlRotateCounterClockwise(psControl->dAngle,
								  ps32WindowRelativeX,
								  ps32WindowRelativeY);

	// Now we do a bounding box check on the control
	if ((*ps32WindowRelativeX < 0) || (*ps32WindowRelativeY < 0))
	{
		// Off to the left/top of the control
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
		goto errorExit;
	}

	// Off to the right/bottom of the control
	if ((*ps32WindowRelativeX >= (int32_t) psControl->u32XSize) ||
		(*ps32WindowRelativeY >= (int32_t) psControl->u32YSize))
	{
		eStatus = ESTATUS_UI_CONTROL_NOT_HIT;
		goto errorExit;
	}

	// It's within the bounding box of the control and we have a hit test.
	// Ask it where!
	if (psControl->psMethods->HitTest)
	{
		eStatus = psControl->psMethods->HitTest(psControl,
												u8ButtonMask,
												*ps32WindowRelativeX,
												*ps32WindowRelativeY);
		ERR_GOTO();
	}
	else
	{
		// It's a hit! No additional test needed
	}
	
errorExit:
	return(eStatus);
}

// Called from the window code when a control has focus and the
EStatus ControlButtonProcess(EControlHandle eControlHandle,
							 uint8_t u8ButtonMask,
							 int32_t s32NormalizedControlRelativeX,
							 int32_t s32NormalizedControlRelativeY,
							 bool bPressed)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	// Lock this control
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_LOCK,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	// If this is an initial press, arm the repeat schedule
	if ((false == psControl->bAsserted) && 
		(bPressed) &&
		(psControl->psRepeatScheduleHead) &&
		(psControl->u8ButtonMap == u8ButtonMask))
	{
		psControl->u64HeldTimestampCounter = 0;
		psControl->u32RepeatTimeAccumulator = 0;
		psControl->psRepeatSchedule = psControl->psRepeatScheduleHead;
		psControl->u32RepeatRateMS = psControl->psRepeatSchedule->u32RepeatRateMS;
		psControl->s32RepeatNormalizedRelativeX = s32NormalizedControlRelativeX;
		psControl->s32RepeatNormalizedRelativeY = s32NormalizedControlRelativeY;
	}

	// Set this control's asserted state
	psControl->bAsserted = bPressed;

	// If this control has a button press callback, then call it
	if (psControl->psMethods->Button)
	{
		eStatus = psControl->psMethods->Button(psControl,
											   u8ButtonMask,
											   s32NormalizedControlRelativeX,
											   s32NormalizedControlRelativeY,
											   bPressed);
	}

	// Time to unlock
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_UNLOCK,
							NULL,
							true,
							ESTATUS_OK);

errorExit:
	return(eStatus);
}

// This is called when a drag or mosueover occurs.
EStatus ControlProcessDragMouseover(EControlHandle eControlHandle,
									uint8_t u8ButtonMask,
									int32_t s32WindowRelativeX,
									int32_t s32WindowRelativeY)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	// Lock this control
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_LOCK,
							(void **) &psControl,
							true,
							ESTATUS_OK);
	ERR_GOTO();

	// We only pass along mouseover messages if asserted
	if (psControl->bAsserted && 
        psControl->bVisible && 
        (false == psControl->bDisabled))
	{
		// If this control has a drag callback, then call it
		if (psControl->psMethods->Drag)
		{
			// Subtract off the control's X/Y position in the window
			s32WindowRelativeX -= psControl->s32XPos;
			s32WindowRelativeY -= psControl->s32YPos;

			// Derotate the coordinates if necessary for this control
			ControlRotateCounterClockwise(psControl->dAngle,
										  &s32WindowRelativeX,
										  &s32WindowRelativeY);

			// And now call the mouseover function if this control needs it
			eStatus = psControl->psMethods->Drag(psControl,
												 s32WindowRelativeX,
												 s32WindowRelativeY);
		}
	}
	else
	{
		// Not asserted, so let's do a mouseover on this control
		if (psControl->psMethods->Mouseover)
		{
			// Subtract off the control's X/Y position in the window
			s32WindowRelativeX -= psControl->s32XPos;
			s32WindowRelativeY -= psControl->s32YPos;

			// Derotate the coordinates if necessary for this control
			ControlRotateCounterClockwise(psControl->dAngle,
											&s32WindowRelativeX,
											&s32WindowRelativeY);

			// MAke sure the mouseover is in range
			if ((s32WindowRelativeX < 0) ||
				(s32WindowRelativeY < 0) ||
				(s32WindowRelativeX >= (int32_t) psControl->u32XSize) ||
				(s32WindowRelativeY >= (int32_t) psControl->u32YSize))
			{
				// Don't do anything
			}
			else
			{
				// And now call the mouseover function if this control needs it
				eStatus = psControl->psMethods->Mouseover(psControl,
														  s32WindowRelativeX,
														  s32WindowRelativeY);
			}
		}
	}

	// Time to unlock
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) eControlHandle,
							EHANDLE_UNLOCK,
							NULL,
							true,
							eStatus);

errorExit:
	return(eStatus);
}

// Handle a UTF8 character input
EStatus ControlUTF8InputProcess(EControlHandle eControlHandle,
								char *peUTF8,
								bool bDirect)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	EControlHandle eHandleInvalid;

	HandleSetInvalid((EHandleGeneric *) &eHandleInvalid);

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	// If we have an interception method, then give it a chance
	if ((psControl->psControlInterceptedMethods) &&
		(psControl->psControlInterceptedMethods->UTF8Input) &&
		(false == bDirect))
	{
		// Yep. Got one. Lock the interceptor.
		eStatus = ControlSetLock(psControl->eControlInterceptorHandle,
								 true,
								 NULL,
								 eStatus);
		if (eStatus != ESTATUS_OK)
		{
			SyslogFunc("SetLock of intercepted handle failed - %s\n", GetErrorText(eStatus));
		}
		else
		{
			eStatus = psControl->psControlInterceptedMethods->UTF8Input(psControl,
																		psControl->eControlInterceptorHandle,
																		peUTF8);
		}

		// Unlock the intercepted module
		eStatus = ControlSetLock(psControl->eControlInterceptorHandle,
								 false,
								 NULL,
								 eStatus);
	}
	else
	{
		eStatus = ESTATUS_UI_UNHANDLED;
	}

	if ((ESTATUS_UI_UNHANDLED == eStatus) && 
		(psControl->psMethods->UTF8Input))
	{
		eStatus = psControl->psMethods->UTF8Input(psControl,
												  eHandleInvalid,
												  peUTF8);
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 eStatus);

errorExit:
	return(eStatus);
}

// Handle a keyscan code
EStatus ControlScancodeInputProcess(EControlHandle eControlHandle,
									SDL_Scancode eScancode,
									bool bPressed,
									bool bDirect)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	EControlHandle eHandleInvalid;

	HandleSetInvalid((EHandleGeneric *) &eHandleInvalid);

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	if ((psControl->psControlInterceptedMethods) &&
		(psControl->psControlInterceptedMethods->UTF8Input) &&
		(false == bDirect))
	{
		// Yep. Got one. Lock the interceptor.
		eStatus = ControlSetLock(psControl->eControlInterceptorHandle,
								 true,
								 NULL,
								 eStatus);
		if (eStatus != ESTATUS_OK)
		{
			SyslogFunc("SetLock of intercepted handle failed - %s\n", GetErrorText(eStatus));
		}
		else
		{
			eStatus = psControl->psControlInterceptedMethods->Keyscan(psControl,
																	  psControl->eControlInterceptorHandle,
																	  eScancode,
																	  bPressed);
		}

		// Unlock the intercepted module
		eStatus = ControlSetLock(psControl->eControlInterceptorHandle,
								 false,
								 NULL,
								 eStatus);
	}
	else
	{
		eStatus = ESTATUS_UI_UNHANDLED;
	}

	if ((ESTATUS_UI_UNHANDLED == eStatus) && 
		(psControl->psMethods->Keyscan))
	{
		eStatus = psControl->psMethods->Keyscan(psControl,
												eHandleInvalid,
												eScancode,
												bPressed);
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 eStatus);

errorExit:
	return(eStatus);
}

// Handle mouse wheel input
EStatus ControlProcessWheel(EControlHandle eControlHandle,
							int32_t s32Wheel)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	if (psControl->psMethods->Wheel)
	{
		eStatus = psControl->psMethods->Wheel(psControl,
											  s32Wheel);
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 eStatus);

errorExit:
	return(eStatus);
}

// Processes a single focus change
static EStatus ControlProcessFocusSingle(EControlHandle eControlHandle,
										 EControlHandle eControlHandleOld,
										 EControlHandle eControlHandleNew,
										 uint8_t u8ButtonMask,
										 int32_t s32WindowRelativeX,
										 int32_t s32WindowRelativeY,
										 bool bFocused)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;
	bool bLocked = false;

	// If we have an old control handle, indicate to it they've lost the focus
	if (ControlHandleIsValid(eControlHandle))
	{
		eStatus = ControlSetLock(eControlHandle,
								 true,
								 &psControl,
								 ESTATUS_OK);
		ERR_GOTO();
		bLocked = true;

		// Set focus
		psControl->bFocused = bFocused;

		// Subtract off the control's X/Y position in the window
		s32WindowRelativeX -= psControl->s32XPos;
		s32WindowRelativeY -= psControl->s32YPos;

		// Derotate the coordinates if necessary for this control
		ControlRotateCounterClockwise(psControl->dAngle,
										&s32WindowRelativeX,
										&s32WindowRelativeY);

		// If we have an interception method, then give it a chance
		if ((psControl->psControlInterceptedMethods) &&
			(psControl->psControlInterceptedMethods->Focus))
		{
			SControl *psControlIntercept = NULL;

			// Provide psControl as the informed control
			eStatus = ControlSetLock(psControl->eControlInterceptorHandle,
									 true,
									 &psControlIntercept,
									 eStatus);
			ERR_GOTO();

			eStatus = psControl->psControlInterceptedMethods->Focus(psControlIntercept,
																	eControlHandleOld,
																	eControlHandleNew,
																	u8ButtonMask,
																	s32WindowRelativeX,
																	s32WindowRelativeY);

			eStatus = ControlSetLock(psControl->eControlInterceptorHandle,
									 false,
									 NULL,
									 eStatus);
			ERR_GOTO();
		}
		else
		{
			eStatus = ESTATUS_UI_UNHANDLED;
		}

		if (ESTATUS_UI_UNHANDLED == eStatus)
		{
			if (psControl->psMethods->Focus)
			{
				eStatus = psControl->psMethods->Focus(psControl,
													  eControlHandleOld,
													  eControlHandleNew,
													  u8ButtonMask,
													  s32WindowRelativeX,
													  s32WindowRelativeY);
			}
			else
			{
				// It's unhandled, but it's OK if there's no focus method for this control
				eStatus = ESTATUS_OK;
			}
		}
	}


errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eControlHandle,
								 false,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}


// Handle focus changes
EStatus ControlProcessFocus(EControlHandle eControlOld,
							EControlHandle eControlNew,
							uint8_t u8ButtonMask,
							int32_t s32WindowRelativeXOriginal,
							int32_t s32WindowRelativeYOriginal)
{
	EStatus eStatus;
	bool bOldFocus = true;
	bool bNewFocus = true;

	if (eControlOld == eControlNew)
	{
		// Both should be in focus - this should never happen - means something
		// is submitting messages it shouldn't
		BASSERT(0);
	}
	else
	if (ControlHandleIsValid(eControlOld) &&
		ControlHandleIsValid(eControlNew))
	{
		// Losing old focus
		bOldFocus = false;
	}

	// Lost control - maybe
	eStatus = ControlProcessFocusSingle(eControlOld,
										eControlOld,
										eControlNew,
										u8ButtonMask,
										s32WindowRelativeXOriginal,
										s32WindowRelativeYOriginal,
										bOldFocus);
	if ((ESTATUS_UI_UNHANDLED == eStatus) ||
		(ESTATUS_OK == eStatus))
	{
		// An unhandled focus is OK
	}
	else
	{
		ERR_GOTO();
	}

	// Gained control
	eStatus = ControlProcessFocusSingle(eControlNew,
										eControlOld,
										eControlNew,
										u8ButtonMask,
										s32WindowRelativeXOriginal,
										s32WindowRelativeYOriginal,
										true);
	ERR_GOTO();

errorExit:
	return(eStatus);
}


// Create a new control
EStatus ControlCreate(EWindowHandle eWindowHandle,
					  EControlHandle *peControlHandle,
					  int32_t s32XPos,
					  int32_t s32YPos,
					  uint32_t u32XSize,
					  uint32_t u32YSize,
					  EControlType eType,
					  void *pvControlInitConfiguration)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	bool bHandleLocked = false;

	// See if the window handle is valid
	if (false == WindowHandleIsValid(eWindowHandle))
	{
		eStatus = ESTATUS_HANDLE_INVALID;
		goto errorExit;
	}

	// Check the control type
	if (eType >= ECTRLTYPE_MAX)
	{
		eStatus = ESTATUS_UI_UNKNOWN_CONTROL_TYPE;
		goto errorExit;
	}

	// And see if we have a handle type
	if (NULL == sg_psControlMethods[eType])
	{
		eStatus = ESTATUS_UI_CONTROL_NOT_REGISTERED;
		goto errorExit;
	}

	// Control type is OK. Allocate a handle
	eStatus = HandleAllocate(sg_psControlHandlePool,
							 peControlHandle,
							 (void **) &psControl);
	ERR_GOTO();
	bHandleLocked = true;

	// Hook up this control's methods
	psControl->psMethods = sg_psControlMethods[eType];

	// Copy in the initial data
	psControl->eWindowHandle = eWindowHandle;
	psControl->eControlHandle = *peControlHandle;

	// X/Y Relative position
	psControl->s32XPos = s32XPos;
	psControl->s32YPos = s32YPos;

	// Set the size
	psControl->u32XSize = u32XSize;
	psControl->u32YSize = u32YSize;

	// Initial state of stuff
	psControl->bVisible = false;
	psControl->bDisabled = false;
	psControl->bFocusable = false;

	// Clear out any parents - top or direct parent
	HandleSetInvalid((EHandleGeneric *) &psControl->eControlTop);
	HandleSetInvalid((EHandleGeneric *) &psControl->eControlParent);
	
	psControl->u8Intensity = 0xff;

	// If this control has a control specific data structure, allocate it
	if (psControl->psMethods->u32ControlSpecificStructureSize)
	{
		MEMALLOC(psControl->pvControlSpecificData, psControl->psMethods->u32ControlSpecificStructureSize);
	}

	// Unlock the handle. This needs to be done or we can get a race condition
	// between the draw and the handle getting unlocked
	eStatus = HandleSetLock(sg_psControlHandlePool,
							NULL,
							(EHandleGeneric) *peControlHandle,
							EHANDLE_UNLOCK,
							NULL,
							true,
							eStatus);
	ERR_GOTO();

	bHandleLocked = false;

	// Go create a control and attach it to this window
	eStatus = WindowControlCreate(eWindowHandle,
								  psControl,
								  (void *) pvControlInitConfiguration);

errorExit:
	if (bHandleLocked)
	{
		// Unlock the handle
		eStatus = HandleSetLock(sg_psControlHandlePool,
								NULL,
								(EHandleGeneric) *peControlHandle,
								EHANDLE_UNLOCK,
								NULL,
								true,
								eStatus);
	}

	if (eStatus != ESTATUS_OK)
	{

		// Destroy whatever we created - if anything
		(void) ControlDestroy(peControlHandle);
	}

	return(eStatus);
}

EStatus ControlSetLock(EControlHandle eControlHandle,
					   bool bLock,
					   SControl **ppsControl,
					   EStatus eStatusIncoming)
{
	EHandleLock eLock = EHANDLE_UNLOCK;

	if (bLock)
	{
		eLock = EHANDLE_LOCK;
	}

	return(HandleSetLock(sg_psControlHandlePool,
						 NULL,
						 (EHandleGeneric) eControlHandle,
						 eLock,
						 (void **) ppsControl,
						 true,
						 eStatusIncoming));
}

// Gets a control's X/Y size and/or position
EStatus ControlGetSizePos(EControlHandle eControlHandle,
						  int32_t *ps32WindowRelativeXPos,
						  int32_t *ps32WindowRelativeYPos,
						  uint32_t *pu32XSize,
						  uint32_t *pu32YSize)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	if (ps32WindowRelativeXPos)
	{
		*ps32WindowRelativeXPos = psControl->s32XPos;
	}

	if (ps32WindowRelativeYPos)
	{
		*ps32WindowRelativeYPos = psControl->s32YPos;
	}

	if (pu32XSize)
	{
		*pu32XSize = psControl->u32XSize;
	}

	if (pu32YSize)
	{
		*pu32YSize = psControl->u32YSize;
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 &psControl,
							 eStatus);

errorExit:
	return(eStatus);
}

EStatus ControlSetSize(EControlHandle eControlHandle,
					   uint32_t u32XSize,
					   uint32_t u32YSize)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControl->u32XSize = u32XSize;
	psControl->u32YSize = u32YSize;

	if (psControl->psMethods->Size)
	{
		eStatus = psControl->psMethods->Size(psControl,
											 u32XSize,
											 u32YSize);
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 &psControl,
							 eStatus);
errorExit:
	if (ESTATUS_OK == eStatus)
	{
		WindowUpdated();
	}

	return(eStatus);
}

EStatus ControlSetHitTestAlways(EControlHandle eControlHandle,
								bool bHitTestAlways)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControl->bHitTestAlways = bHitTestAlways;

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 eStatus);
errorExit:
	return(eStatus);
}

EStatus ControlPeriodicTimer(EControlHandle eControlHandle,
							 uint32_t u32IntervalTimerMs)
{
	EStatus eStatus;
	SControl *psControl = NULL;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	if (psControl->psMethods->Periodic)
	{
		eStatus = psControl->psMethods->Periodic(psControl,
												 u32IntervalTimerMs);
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 &psControl,
							 eStatus);

errorExit:
	return(eStatus);

}

void ControlRotateClockwise(double dAngle,
							int32_t *ps32XPos,
							int32_t *ps32YPos)
{
	int32_t s32OrgX;
	double dRadians;

	// Rotate clockwise!
	dRadians = dAngle * (M_PI / 180.0);
	
	s32OrgX = (*ps32XPos);
	if (ps32XPos)
	{
		*ps32XPos = (int32_t) ((((double) *ps32XPos) * cos(dRadians)) - (((double) *ps32YPos) * sin(dRadians)));
	}

	if (ps32YPos)
	{
		*ps32YPos = (int32_t) ((((double) s32OrgX) * sin(dRadians)) + (((double) *ps32YPos) * cos(dRadians)));
	}
}

void ControlRotateCounterClockwise(double dAngle,
								   int32_t *ps32XPos,
								   int32_t *ps32YPos)
{
	int32_t s32OrgX;
	double dRadians;

	// Now we're control relative and we need to potentially rotate the coordinates
	// counter-clockwise in case the control is rotated
	dRadians = dAngle * (M_PI / 180.0);
	
	s32OrgX = -(*ps32XPos);
	if (ps32XPos)
	{
		*ps32XPos = (int32_t) ((((double) *ps32XPos) * cos(dRadians)) + (((double) *ps32YPos) * sin(dRadians)));
	}

	if (ps32YPos)
	{
		*ps32YPos = (int32_t) ((((double) s32OrgX) * sin(dRadians)) + (((double) *ps32YPos) * cos(dRadians)));
	}
}

EStatus ControlRenderStretch(uint32_t *pu32RGBALeftImage,
							 uint32_t u32LeftXSize,
							 uint32_t u32LeftYSize,
							 uint32_t *pu32RGBAStretchImage,
							 uint32_t u32StretchXSize,
							 uint32_t u32StretchYSize,
							 uint32_t *pu32RGBARightImage,
							 uint32_t u32RightXSize,
							 uint32_t u32RightYSize,
							 uint32_t u32XSize,
							 uint32_t *pu32YSize,
							 uint32_t **ppu32RGBARenderedImage)
{
	EStatus eStatus;
	uint32_t *pu32RGBA;
	uint32_t u32Loop;
	uint32_t *pu32Baseline;
	uint32_t u32Pitch = u32XSize;

	BASSERT(pu32YSize);
	*pu32YSize = 0;

	// Figure out how big Y-size the image is
	if (u32LeftYSize > *pu32YSize)
	{
		*pu32YSize = u32LeftYSize;
	}

	if (u32StretchYSize > *pu32YSize)
	{
		*pu32YSize = u32StretchYSize;
	}

	if (u32RightYSize > *pu32YSize)
	{
		*pu32YSize = u32RightYSize;
	}

	// If we have nothing to render, then complain about it
	if (0 == *pu32YSize)
	{
		eStatus = ESTATUS_UI_NO_AXIS;
		goto errorExit;
	}

	// If the left + right X size exceeds or is equal to the u32XSize, then it's too small
	if ((u32LeftXSize + u32RightXSize) > u32XSize)
	{
		eStatus = ESTATUS_UI_TOO_SMALL;
		goto errorExit;
	}

	// Allocate some memory
	MEMALLOC(*ppu32RGBARenderedImage, sizeof(**ppu32RGBARenderedImage) *
									  u32XSize * *pu32YSize);

	// Fill in all pixels as black and completely transparent by default
	u32Loop = u32XSize * *pu32YSize;
	pu32RGBA = *ppu32RGBARenderedImage;
	while (u32Loop--)
	{
		*pu32RGBA = MAKERGBA(0, 0, 0, PIXEL_TRANSPARENT);
		++pu32RGBA;
	}

	// Copy the left image in
	pu32RGBA = *ppu32RGBARenderedImage + (((*pu32YSize >> 1) - (u32LeftYSize >> 1)) * u32XSize);
	while (u32LeftYSize--)
	{
		memcpy((void *) pu32RGBA, (void *) pu32RGBALeftImage, u32LeftXSize * sizeof(*pu32RGBALeftImage));
		pu32RGBA += u32XSize;
		pu32RGBALeftImage += u32LeftXSize;
	}

	// Copy the right image in
	pu32RGBA = *ppu32RGBARenderedImage + (((*pu32YSize >> 1) - (u32RightYSize >> 1)) * u32XSize) + (u32XSize - u32RightXSize);
	while (u32RightYSize--)
	{
		memcpy((void *) pu32RGBA, (void *) pu32RGBARightImage, u32RightXSize * sizeof(*pu32RGBARightImage));
		pu32RGBA += u32XSize;
		pu32RGBARightImage += u32RightXSize;
	}

	// Figure out our baseline
	pu32RGBA = *ppu32RGBARenderedImage + (((*pu32YSize >> 1) - (u32StretchYSize >> 1)) * u32Pitch) + u32LeftXSize;

	// Now the stretch image
	u32XSize -= (u32LeftXSize + u32RightXSize);

	// u32XSize contains the # of pixels we need to fill in. Stretch to fit the available space.
	while (u32XSize)
	{
		uint32_t u32PixelsToMove;
		uint32_t *pu32RGBASrc = NULL;

		pu32Baseline = pu32RGBA;

		u32PixelsToMove = u32XSize;
		if (u32PixelsToMove > u32StretchXSize)
		{
			u32PixelsToMove = u32StretchXSize;
		}

		pu32RGBASrc = pu32RGBAStretchImage;
		u32Loop = u32StretchYSize;
		while (u32Loop--)
		{
			memcpy((void *) pu32RGBA, (void *) pu32RGBASrc, u32PixelsToMove * sizeof(*pu32RGBASrc));
			pu32RGBA += u32Pitch;
			pu32RGBASrc += u32StretchXSize;
		}
		
		u32XSize -= u32PixelsToMove;
		pu32Baseline += u32PixelsToMove;
		pu32RGBA = pu32Baseline;
	}

	eStatus = ESTATUS_OK;


errorExit:
	return(eStatus);
}

// Converts an incoming scancode to a callback reason if it finds one
EControlCallbackReason ControlKeyscanToCallbackReason(SDL_Scancode eScancode)
{
	EControlCallbackReason eReason = ECTRLACTION_UNKNOWN;

	if ((SDL_SCANCODE_RETURN == eScancode) ||
		(SDL_SCANCODE_KP_ENTER == eScancode))
	{
		// Pressed enter
		eReason = ECTRLACTION_ENTER;
	}
	else
	if (SDL_SCANCODE_TAB == eScancode)
	{
		eReason = ECTRLACTION_TAB;

		if (WindowScancodeGetState(SDL_SCANCODE_LSHIFT) ||
			WindowScancodeGetState(SDL_SCANCODE_RSHIFT))
		{
			eReason = ECTRLACTION_SHIFTTAB;
		}
	}
	else
	if (SDL_SCANCODE_ESCAPE == eScancode)
	{
		// Hit escape
		eReason = ECTRLACTION_ESCAPE;
	}
		
	return(eReason);
}

static EStatus ControlSetMethodInterceptInternal(EControlHandle eControlTargetHandle,
												 EControlHandle eControlInterceptorHandle,
												 SControlMethods *psInterceptedMethodTable)
{
	EStatus eStatus;
	bool bLocked = false;
	SControl *psControl = NULL;
	SControlList *psChild = NULL;

	eStatus = ControlSetLock(eControlTargetHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLocked = true;

	// Run through all of the children and 
	psChild = psControl->psControlChildren;
	while (psChild)
	{
		eStatus = ControlSetMethodInterceptInternal(psChild->eControlHandle,
													eControlInterceptorHandle,
													psInterceptedMethodTable);
		ERR_GOTO();
		psChild = psChild->psNextLink;
	}

	psControl->psControlInterceptedMethods = psInterceptedMethodTable;
	psControl->eControlInterceptorHandle = eControlInterceptorHandle;

errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eControlTargetHandle,
								 false,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}

// This will set all children of eControlInterceptorHandle to the interception method
// table provided
EStatus ControlSetMethodInterceptTree(EControlHandle eControlInterceptorHandle,
									  SControlMethods *psInterceptedMethodTable)
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlList *psChild = NULL;
	bool bLockSource = false;

	eStatus = ControlSetLock(eControlInterceptorHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();
	bLockSource = true;

	// Run through all of the children and 
	psChild = psControl->psControlChildren;
	while (psChild)
	{
		eStatus = ControlSetMethodInterceptInternal(psChild->eControlHandle,
													eControlInterceptorHandle,
													psInterceptedMethodTable);
		ERR_GOTO();
		psChild = psChild->psNextLink;
	}

errorExit:
	if (bLockSource)
	{
		eStatus = ControlSetLock(eControlInterceptorHandle,
								 false,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}

char *ControlGetName(EControlHandle eControlSourceHandle)
{
	EStatus eStatus;
	char *peName = "(unknown)";
	SControl *psControl;

	eStatus = ControlSetLock(eControlSourceHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	peName = psControl->psMethods->peControlName;

errorExit:
	eStatus = ControlSetLock(eControlSourceHandle,
							 false,
							 NULL,
							 eStatus);
	return(peName);
}

// This sets the top parent for an entire control tree
static EStatus ControlSetTopHandle(EControlHandle eControlHandleChild,
								   EControlHandle eControlTop)
{
	EStatus eStatus;
	SControl *psControlChild = NULL;
	bool bChildLocked = false;
	SControlList *psChild = NULL;

	eStatus = ControlSetLock(eControlHandleChild,
							 true,
							 &psControlChild,
							 ESTATUS_OK);
	ERR_GOTO();
	bChildLocked = true;

	// Run through all of the children and set the top level handle
	psChild = psControlChild->psControlChildren;
	while (psChild)
	{
		eStatus = ControlSetTopHandle(psControlChild->eControlHandle,
									  eControlTop);
		ERR_GOTO();
		psChild = psChild->psNextLink;
	}

	psControlChild->eControlTop = eControlTop;

errorExit:
	if (bChildLocked)
	{
		eStatus = ControlSetLock(eControlHandleChild,
								 false,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}

// Adds a control as a child to eControlHandleParent
EStatus ControlAddChild(EControlHandle eControlHandleParent,
						EControlHandle eControlHandleChild)
{
	EStatus eStatus;
	SControl *psControlParent = NULL;
	SControl *psControlChild = NULL;
	bool bParentLocked = false;
	bool bChildLocked = false;
	SControlList *psChild = NULL;

	eStatus = ControlSetLock(eControlHandleParent,
							 true,
							 &psControlParent,
							 ESTATUS_OK);
	ERR_GOTO();
	bParentLocked = true;

	// See if this is already a child of this control
	psChild = psControlParent->psControlChildren;
	while (psChild)
	{
		if (psChild->eControlHandle == eControlHandleChild)
		{
			eStatus = ESTATUS_UI_ALREADY_ASSIGNED;
			goto errorExit;
		}

		psChild = psChild->psNextLink;
	}

	// Lock the child
	eStatus = ControlSetLock(eControlHandleChild,
							 true,
							 &psControlChild,
							 ESTATUS_OK);
	ERR_GOTO();
	bChildLocked = true;

	// If this child already has a parent or top level parent, it's an error
	if (ControlHandleIsValid(psControlChild->eControlParent) ||
		ControlHandleIsValid(psControlChild->eControlTop))
	{
		eStatus = ESTATUS_UI_CHILD_ALREADY_HAS_PARENT;
		goto errorExit;
	}

	// Now allocate a structure for this child
	MEMALLOC(psChild, sizeof(*psChild));

	psChild->eControlHandle = eControlHandleChild;
	psChild->psNextLink = psControlParent->psControlChildren;
	psControlParent->psControlChildren = psChild;

	// Now adjust the children so they know who their daddy is
	psControlChild->eControlParent = psControlParent->eControlHandle;
	psControlChild->eControlTop = psControlParent->eControlTop;

	// If the top control is an invalid handle, then the parent is the top as well
	if (ControlHandleIsValid(psControlChild->eControlTop))
	{
		// It's valid - don't do anything
	}
	else
	{
		// It's not valid - so the parent is the top
		psControlChild->eControlTop = psControlParent->eControlHandle;
	}

	// Walk the child tree (sans this child) and update their top
	psChild = psControlParent->psControlChildren;
	while (psChild)
	{
		if (psChild->eControlHandle == eControlHandleChild)
		{
			// Skip the child we just assigned
		}
		else
		{
			eStatus = ControlSetTopHandle(psChild->eControlHandle,
										  psControlChild->eControlTop);
			ERR_GOTO();
		}

		psChild = psChild->psNextLink;
	}

	// All good!

errorExit:
	if (bChildLocked)
	{
		eStatus = ControlSetLock(eControlHandleChild,
								 false,
								 NULL,
								 eStatus);
	}

	if (bParentLocked)
	{
		eStatus = ControlSetLock(eControlHandleParent,
								 false,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}

EStatus ControlIsInTree(EControlHandle eControlTop,
						EControlHandle eControlToFind,
						EControlHandle eControlLockSkip,
						bool *pbIsControlInTree)
{
	EStatus eStatus;
	bool bLocked = false;
	SControl *psControlTop = NULL;
	bool bLockIt = true;
	SControlList *psChild = NULL;

	// If it's the control we don't want to lock, then don't
	if (eControlLockSkip)
	{
		bLockIt = false;
		eStatus = HandleSetLock(sg_psControlHandlePool,
								NULL,
								(EHandleGeneric) eControlTop,
								EHANDLE_NONE,
								(void **) &psControlTop,
								true,
								ESTATUS_OK);
		ERR_GOTO();
	}
	else
	{
		eStatus = ControlSetLock(eControlTop,
								 bLockIt,
								 &psControlTop,
								 ESTATUS_OK);
		ERR_GOTO();
		bLocked = true;
	}

	if (psControlTop->eControlHandle == eControlToFind)
	{
		*pbIsControlInTree = true;
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	psChild = psControlTop->psControlChildren;
	while (psChild)
	{
		eStatus = ControlIsInTree(psChild->eControlHandle,
								  eControlToFind,
								  eControlLockSkip,
								  pbIsControlInTree);
		ERR_GOTO();

		// If we've found the control in the tree, then return
		if (*pbIsControlInTree)
		{
			eStatus = ESTATUS_OK;
			goto errorExit;
		}

		psChild = psChild->psNextLink;
	}

errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eControlTop,
								 false,
								 NULL,
								 eStatus);
	}
	return(eStatus);
}

static bool ControlIsChildTree(EControlHandle eControlHandle,
							   EControlHandle eChildHandle)
{
	EStatus eStatus;
	SControl *psControl;
	SControlList *psChild = NULL;
	bool bIsChild = false;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psChild = psControl->psControlChildren;
	while (psChild)
	{
		if (psChild->eControlHandle == eChildHandle)
		{
			bIsChild = true;
			break;
		}

		if (ControlIsChildTree(psChild->eControlHandle,
							   eChildHandle))
		{
			bIsChild = true;
			break;
		}

		psChild = psChild->psNextLink;
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 ESTATUS_OK);
	ERR_GOTO();

errorExit:
	BASSERT(ESTATUS_OK == eStatus);
	return(bIsChild);
}

// Returns true if eChildHandle is one of psControl's direct or descendants. It assumes
// psControl is locked.
bool ControlIsChild(SControl *psControl,
					EControlHandle eChildHandle)
{
	SControlList *psChild = NULL;

	psChild = psControl->psControlChildren;
	while (psChild)
	{
		if (psChild->eControlHandle == eChildHandle)
		{
			return(true);
		}

		if (ControlIsChildTree(psChild->eControlHandle,
							   eChildHandle))
		{
			return(true);
		}

		psChild = psChild->psNextLink;
	}

	return(false);
}

// Sets a user-defined name for this control
EStatus ControlSetUserName(EControlHandle eControlHandle,
						   char *peControlUserName)
{
	EStatus eStatus;
	SControl *psControl;
	bool bIsChild = false;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	SafeMemFree(psControl->peControlUserName);

	psControl->peControlUserName = strdupHeap(peControlUserName);

	if (NULL == psControl->peControlUserName)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 eStatus);

errorExit:
	return(eStatus);
}

// Gets a user-defined name for this control
EStatus ControlGetUserName(EControlHandle eControlHandle,
						   char **ppeControlUserName)
{
	EStatus eStatus;
	SControl *psControl;
	bool bIsChild = false;

	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	if (ppeControlUserName)
	{
		*ppeControlUserName = psControl->peControlUserName;
	}

	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 eStatus);

errorExit:
	return(eStatus);
}

// Called once every CONTROL_REPEAT_TIMER_INTERVAL milliseconds. This processes
// all controls that 
static void ControlRepeatTimerCallback(STimer *psTimer,
									   void *pvCallbackValue)
{
	struct SControlList *psControlRepeatSchedules;

	psControlRepeatSchedules = sg_psControlRepeatSchedules;

	while (psControlRepeatSchedules)
	{
		EStatus eStatus;
		SControl *psControl = NULL;

		eStatus = ControlSetLock(psControlRepeatSchedules->eControlHandle,
								 true,
								 &psControl,
								 ESTATUS_OK);
		if (ESTATUS_OK == eStatus)
		{
			// It's good! Let's see if this control has any work to do on the 
			// autorepeat timer.
			if ((psControl->bAsserted) &&
				(psControl->bFocused) &&
				(psControl->psRepeatScheduleHead))
			{
				// We only case if it's asserted and in focus and there's a schedule
				psControl->u64HeldTimestampCounter += CONTROL_REPEAT_TIMER_INTERVAL;
				while ((psControl->u64HeldTimestampCounter > psControl->psRepeatSchedule->u64RepeatTimestamp) &&
					   (psControl->psRepeatSchedule->u32RepeatTime))
				{
					psControl->psRepeatSchedule++;
					psControl->u32RepeatRateMS = psControl->psRepeatSchedule->u32RepeatRateMS;
					psControl->u32RepeatTimeAccumulator = 0;
				}

				// Now increment the accumluator
				if(psControl->u32RepeatRateMS)
				{
					psControl->u32RepeatTimeAccumulator += CONTROL_REPEAT_TIMER_INTERVAL;
					if (psControl->u32RepeatTimeAccumulator >= psControl->u32RepeatRateMS)
					{
						psControl->u32RepeatTimeAccumulator -= psControl->u32RepeatRateMS;

						// Fire off another pulse to the button for this control
						DebugOut("Repeat pulse\n");

						eStatus = ControlButtonProcess(psControlRepeatSchedules->eControlHandle,
													   psControl->u8ButtonMap,
													   psControl->s32RepeatNormalizedRelativeX,
													   psControl->s32RepeatNormalizedRelativeY,
													   true);
						if (eStatus != ESTATUS_OK)
						{
							SyslogFunc("Failed to deposit a button press for control 0x%.8x\n", psControlRepeatSchedules->eControlHandle);
						}
					}
				}
			}

			// Unlock it
			eStatus = ControlSetLock(psControlRepeatSchedules->eControlHandle,
									 false,
									 NULL,
									 ESTATUS_OK);
			if (eStatus != ESTATUS_OK)
			{
				Syslog("Failed to unlock control 0x%.8x - %s\n", psControlRepeatSchedules->eControlHandle, GetErrorText(eStatus));
			}
		}
		else
		{
			Syslog("Failed to lock control 0x%.8x - %s\n", psControlRepeatSchedules->eControlHandle, GetErrorText(eStatus));
		}

		// Next control
		psControlRepeatSchedules = psControlRepeatSchedules->psNextLink;
	}
}

EStatus ControlInit(void)
{
	EStatus eStatus;

	// Set up repeat timer and critical sections
	eStatus = OSCriticalSectionCreate(&sg_sControlRepeatCriticalSection);
	ERR_GOTO();

	eStatus = TimerCreate(&sg_psControlRepeatTimer);
	ERR_GOTO();

	eStatus = TimerSet(sg_psControlRepeatTimer,
					   true,
					   CONTROL_REPEAT_TIMER_INTERVAL,
					   CONTROL_REPEAT_TIMER_INTERVAL,
					   ControlRepeatTimerCallback,
					   NULL);
	ERR_GOTO();

	// Create a handle pool for all controls
	eStatus = HandlePoolCreate(&sg_psControlHandlePool,
							   "Control handle pool",
							   MAX_CONTROLS,
							   sizeof(SControl),
							   offsetof(SControl, sCriticalSection),
							   NULL,
							   ControlDestroyHandleCallback);
	ERR_GOTO();

	// Init all of the control types
	eStatus = ControlImageInit();
	ERR_GOTO();
	eStatus = ControlTextInit();
	ERR_GOTO();
	eStatus = ControlButtonInit();
	ERR_GOTO();
	eStatus = ControlSliderInit();
	ERR_GOTO();
	eStatus = ControlLineInputInit();
	ERR_GOTO();
	eStatus = ControlComboBoxInit();
	ERR_GOTO();
	eStatus = ControlListBoxInit();
	ERR_GOTO();
	eStatus = ControlHitInit();
	ERR_GOTO();

	eStatus = TimerStart(sg_psControlRepeatTimer);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

EStatus ControlSetButtonRepeatSchedule(EControlHandle eControlHandle,
									   uint8_t u8ButtonMap,
									   SControlRepeatSchedule *psRepeatSchedule)
{
	EStatus eStatus = ESTATUS_OK;
	SControl *psControl = NULL;

	// Lock this control
	eStatus = ControlSetLock(eControlHandle,
							 true,
							 &psControl,
							 eStatus);
	ERR_GOTO();

	if (NULL == psRepeatSchedule)
	{
		// Stop any pending repeats
		psControl->psRepeatSchedule = NULL;
		psControl->psRepeatScheduleHead = NULL;
		psControl->u64HeldTimestampCounter = 0;
		psControl->u32RepeatTimeAccumulator = 0;
		psControl->u32RepeatRateMS = 0;
		psControl->u8ButtonMap = 0;
	}
	else
	{
		SControlList *psList;
		uint64_t u64RepeatTimestamp = 0;

		// New repeat schedule. Calculate the start time for the whole schedule
		psControl->psRepeatScheduleHead = psRepeatSchedule;
		psControl->psRepeatSchedule = psRepeatSchedule;

		while (psRepeatSchedule->u32RepeatTime)
		{
			psRepeatSchedule->u64RepeatTimestamp = u64RepeatTimestamp + psRepeatSchedule->u32RepeatTime;
			u64RepeatTimestamp = psRepeatSchedule->u64RepeatTimestamp;
			++psRepeatSchedule;
		}

		psControl->u64HeldTimestampCounter = 0;
		psControl->u32RepeatTimeAccumulator = 0;
		psControl->u8ButtonMap = u8ButtonMap;

		MEMALLOC(psList, sizeof(*psList));
		psList->eControlHandle = eControlHandle;

		// Now add it to the master list
		eStatus = OSCriticalSectionEnter(sg_sControlRepeatCriticalSection);
		ERR_GOTO();

		// Link this control in
		psList->psNextLink = sg_psControlRepeatSchedules;
		sg_psControlRepeatSchedules = psList;

		eStatus = OSCriticalSectionLeave(sg_sControlRepeatCriticalSection);
		ERR_GOTO();
	}

	// Now unlock it
	eStatus = ControlSetLock(eControlHandle,
							 false,
							 NULL,
							 eStatus);
errorExit:
	return(eStatus);
}