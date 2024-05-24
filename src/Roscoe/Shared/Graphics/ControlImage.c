#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

// Control image structure
typedef struct SControlImage
{
	SGraphicsImage *psGraphicsImage;				// This control's image
} SControlImage;

// Called when someone wants to control an image
static EStatus ControlImageCreateMethod(SControl *psControl,
										void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;
	SControlImage *psControlImage = (SControlImage *) psControl->pvControlSpecificData;

	eStatus = GraphicsCreateImage(&psControlImage->psGraphicsImage,
								  TRUE,
								  0, 0);

	return(eStatus);
}

// Destroys an image
static EStatus ControlImageDestroyMethod(SControl *psControl)
{
	SControlImage *psControlImage = (SControlImage *) psControl->pvControlSpecificData;

	if (psControlImage)
	{
		BASSERT(psControlImage);

		GraphicsDestroyImage(&psControlImage->psGraphicsImage);
	}

	return(ESTATUS_OK);
}

static EStatus ControlImageDrawMethod(SControl *psControl,
									  UINT8 u8WindowIntensity,
									  INT32 s32WindowXPos,
									  INT32 s32WindowYPos)
{
	SControlImage *psControlImage = (SControlImage *) psControl->pvControlSpecificData;

	if (psControlImage)
	{
		GraphicsSetIntensity(psControlImage->psGraphicsImage,
							 (UINT8) ((u8WindowIntensity * psControl->u8Intensity) >> 8));

		// Graphics attributes
		GraphicsSetAttributes(psControlImage->psGraphicsImage,
							  ControlGetGfxAttributes(psControl),
							  GFXATTR_SWAP_HORIZONAL | GFXATTR_SWAP_VERTICAL);

		GraphicsSetGreyscale(psControlImage->psGraphicsImage,
							 psControl->bDisabled);

		GraphicsDraw(psControlImage->psGraphicsImage,
					 psControl->s32XPos + s32WindowXPos,
					 psControl->s32YPos + s32WindowYPos,
					 psControl->bOriginCenter,
					 psControl->dAngle);
	}

	return(ESTATUS_OK);
}

// Hit test - look for transparent graphics
static EStatus ControlImageHitTestMethod(SControl *psControl,				// Hit test call (assume hit if NULL)
										 UINT8 u8ButtonMask,				// Which buttons are being pressed?
										 INT32 s32ControlRelativeXPos,
										 INT32 s32ControlRelativeYPos)
{
	EStatus eStatus = ESTATUS_OK;
	SControlImage *psControlImage = (SControlImage *) psControl->pvControlSpecificData;

	if (psControlImage)
	{
		eStatus = ControlHitTestImage(psControlImage->psGraphicsImage,
									  s32ControlRelativeXPos,
									  s32ControlRelativeYPos);
	}

	return(eStatus);
}

// List ofmethods for this control
static SControlMethods sg_sControlImageMethods =
{
	ECTRLTYPE_IMAGE,							// Type of control
	"Image",									// Name of control
	sizeof(SControlImage),						// Size of control specific structure

	ControlImageCreateMethod,					// Create an image
	ControlImageDestroyMethod,					// Destroy an image
	NULL,										// Control enable/disable
	NULL,										// New position
	NULL,										// Set visible
	NULL,										// Set angle
	ControlImageDrawMethod,						// Draw the image
	NULL,										// No runtime configuration
	ControlImageHitTestMethod,					// Hit test
	NULL,										// Focus set/loss
	NULL,										// Button press/release
	NULL,										// Mouseover when selected
	NULL,										// Mouse wheel
	NULL,										// UTF8 Input
	NULL,										// Scancode input
	NULL,										// Periodic timer
	NULL,										// Mouseover
	NULL,										// Size
};

EStatus ControlImageCreate(EWindowHandle eWindowHandle,
						   EControlImageHandle *peControlImageHandle,
						   INT32 s32XPos,
						   INT32 s32YPos)
{
	EStatus eStatus;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peControlImageHandle,
							s32XPos,
							s32YPos,
							0,
							0,
							ECTRLTYPE_IMAGE,
							NULL);

	return(eStatus);
}

EStatus ControlImageSetAssets(EControlImageHandle eControlImageHandle,
							  UINT32 *pu32RGBAImage,
							  UINT32 u32XSize,
							  UINT32 u32YSize,
							  BOOL bAutoDeallocate)
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlImage *psControlImage = NULL;
	UINT32 *pu32RGBA = NULL;

	eStatus = ControlSetLock(eControlImageHandle,
							 TRUE,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	psControlImage = (SControlImage *) psControl->pvControlSpecificData;

	if (pu32RGBAImage == psControlImage->psGraphicsImage->pu32RGBA)
	{
		// We're just signaling for the image to be updated - don't clear it
	}
	else
	{
		// Clear the existing image
		GraphicsClearImage(psControlImage->psGraphicsImage);
	}

	// Now copy in the goodies
	GraphicsSetImage(psControlImage->psGraphicsImage,
					 pu32RGBAImage,
					 u32XSize,
					 u32YSize,
					 bAutoDeallocate);

	psControl->u32XSize = u32XSize;
	psControl->u32YSize = u32YSize;

	// If the image is visible and since we've just loaded an image, then
	// do a window update
	if (psControl->bVisible)
	{
		WindowUpdated();
	}

errorExit:
	eStatus = ControlSetLock(eControlImageHandle,
							 FALSE,
							 NULL,
							 eStatus);

	return(eStatus);
}

EStatus ControlImageSetFromFile(EControlImageHandle eControlImageHandle,
								char *peImageFilename)
{
	EStatus eStatus;
	BOOL bLocked = FALSE;
	SControl *psControl = NULL;
	SControlImage *psControlImage = NULL;
	UINT32 *pu32RGBA = NULL;
	UINT32 u32XSize;
	UINT32 u32YSize;

	// Load an image
	eStatus = GraphicsLoadImage(peImageFilename,
								&pu32RGBA,
								&u32XSize,
								&u32YSize);
	ERR_GOTO();

	// Go set the assets
	eStatus = ControlImageSetAssets(eControlImageHandle,
									pu32RGBA,
									u32XSize,
									u32YSize,
									TRUE);
	ERR_GOTO();

	// Make sure nothing gets cleared
	pu32RGBA = NULL;

errorExit:
	SafeMemFree(pu32RGBA);
	if (bLocked)
	{
		eStatus = ControlSetLock(eControlImageHandle,
								 FALSE,
								 NULL,
								 eStatus);
	}

	return(eStatus);
}

EStatus ControlImageGetFramebuffer(EControlImageHandle eControlImageHandle,
								   UINT32 *pu32FrameBuffer)
{
}

EStatus ControlImageInit(void)
{
	return(ControlRegister(sg_sControlImageMethods.eType,
						   &sg_sControlImageMethods));
}
