#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

// Hit test structure
typedef struct SControlHit
{
	void (*HitCallback)(EControlHitHandle eHitHandle,
						EControlCallbackReason eReason,
						uint32_t u32ReasonData,
						int32_t s32ControlRelativeXPos,
						int32_t s32ControlRelativeYPos);
} SControlHit;

// Called when this control is called
static EStatus ControlHitPressedMethod(SControl *psControl,
									   uint8_t u8ButtonMask,
									   int32_t s32ControlRelativeXPos,
									   int32_t s32ControlRelativeYPos,
									   bool bPressed)
{
	SControlHit *psControlHit = (SControlHit *) psControl->pvControlSpecificData;

	return(ESTATUS_UI_CONTROL_NOT_HIT);
}

// Called when a hit has been detected within this control's bounding box
static EStatus ControlHitTestMethod(SControl *psControl,
									uint8_t u8ButtonMask,
									int32_t s32ControlRelativeXPos,
									int32_t s32ControlRelativeYPos)
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
						 int32_t s32XPos,
						 int32_t s32YPos,
						 uint32_t u32XSize,
						 uint32_t u32YSize)
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
								true);
	ERR_GOTO();

	// Always set it to a hit
	eStatus = ControlSetHitTestAlways((EControlHandle) *peHitHandle,
									  true);
	ERR_GOTO();


errorExit:
	return(eStatus);
}

EStatus ControlHitSetCallback(EControlHitHandle eHitHandle,
							  void (*HitCallback)(EControlHitHandle eHitHandle,
												  EControlCallbackReason eReason,
												  uint32_t u32ReasonData,
												  int32_t s32ControlRelativeXPos,
												  int32_t s32ControlRelativeYPos))
{
	EStatus eStatus;
	SControl *psControl = NULL;
	SControlHit *psControlHit;

	eStatus = ControlSetLock(eHitHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlHit = (SControlHit *) psControl->pvControlSpecificData;
	psControlHit->HitCallback = HitCallback;

	eStatus = ControlSetLock(eHitHandle,
							 false,
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