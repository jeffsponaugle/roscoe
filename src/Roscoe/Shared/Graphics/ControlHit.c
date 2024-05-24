#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

// Hit test structure
typedef struct SControlHit
{
	void (*HitCallback)(EControlHitHandle eHitHandle,
						EControlCallbackReason eReason,
						UINT32 u32ReasonData,
						INT32 s32ControlRelativeXPos,
						INT32 s32ControlRelativeYPos);
} SControlHit;

// Called when this control is called
static EStatus ControlHitPressedMethod(SControl *psControl,
									   UINT8 u8ButtonMask,
									   INT32 s32ControlRelativeXPos,
									   INT32 s32ControlRelativeYPos,
									   BOOL bPressed)
{
	SControlHit *psControlHit = (SControlHit *) psControl->pvControlSpecificData;

	return(ESTATUS_UI_CONTROL_NOT_HIT);
}

// Called when a hit has been detected within this control's bounding box
static EStatus ControlHitTestMethod(SControl *psControl,
									UINT8 u8ButtonMask,
									INT32 s32ControlRelativeXPos,
									INT32 s32ControlRelativeYPos)
{
	SControlHit *psControlHit = (SControlHit *) psControl->pvControlSpecificData;

	if (psControlHit->HitCallback)
	{
		psControlHit->HitCallback((EControlHitHandle) psControl->eControlHandle,
								  ECTRLACTION_MOUSEBUTTON,
								  u8ButtonMask,
								  s32ControlRelativeXPos,
								  s32ControlRelativeYPos);
	}

	return(ESTATUS_UI_CONTROL_NOT_HIT);
}

// List of methods for this control
static SControlMethods sg_sControlHitMethods =
{
	ECTRLTYPE_HIT,								// Type of control
	"Hit test",									// Name of control
	sizeof(SControlHit),						// Size of control specific structure

	NULL,										// Create a control
	NULL,										// Destroy a control
	NULL,										// Control enable/disable
	NULL,										// New position
	NULL,										// Set visible
	NULL,										// Set angle
	NULL,										// Draw the control
	NULL,										// Runtime configuration
	ControlHitTestMethod,						// Hit test
	NULL,										// Focus set/loss
	ControlHitPressedMethod,					// Button press/release
	NULL,										// Mouseover when selected
	NULL,										// Mouse wheel
	NULL,										// UTF8 Input
	NULL,										// Scancode input
	NULL,										// Periodic timer
	NULL,										// Mouseover
	NULL,										// Size
};

EStatus ControlHitCreate(EWindowHandle eWindowHandle,
						 EControlHitHandle *peHitHandle,
						 INT32 s32XPos,
						 INT32 s32YPos,
						 UINT32 u32XSize,
						 UINT32 u32YSize)
{
	EStatus eStatus;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peHitHandle,
							s32XPos,
							s32YPos,
							u32XSize,
							u32YSize,
							sg_sControlHitMethods.eType,
							NULL);
	ERR_GOTO();

	// Hit test always is "visible" since it doesn't show anything
	eStatus = ControlSetVisible((EControlHandle) *peHitHandle,
								TRUE);
	ERR_GOTO();

	// Always set it to a hit
	eStatus = ControlSetHitTestAlways((EControlHandle) *peHitHandle,
									  TRUE);
	ERR_GOTO();


errorExit:
	return(eStatus);
}

EStatus ControlHitSetCallback(EControlHitHandle eHitHandle,
							  void (*HitCallback)(EControlHitHandle eHitHandle,
												  EControlCallbackReason eReason,
												  UINT32 u32ReasonData,
												  INT32 s32ControlRelativeXPos,
												  INT32 s32ControlRelativeYPos))
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlHit *psControlHit;

	eStatus = ControlSetLock(eHitHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlHit = (SControlHit *) psControl->pvControlSpecificData;
	psControlHit->HitCallback = HitCallback;

	eStatus = ControlSetLock(eHitHandle,
							 FALSE,
							 NULL,
							 eStatus);

errorExit:
	return(eStatus);
}

EStatus ControlHitInit(void)
{
	return(ControlRegister(sg_sControlHitMethods.eType,
						   &sg_sControlHitMethods));
}