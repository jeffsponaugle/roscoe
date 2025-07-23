#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"

// Control text structure
typedef struct SControlText
{
	EFontHandle eFont;							// Font in use
	uint16_t u16FontSize;							// Font size
	char *peString;								// Text string

	// Text color
	uint32_t u32RGBAColor;

	// Text image for graphics
	SGraphicsImage *psGraphicsTextImage;		// Text image
} SControlText;

// Called when someone wants to create a text control
static EStatus ControlTextCreateMethod(SControl *psControl,
									   void *pvControlInitConfiguration)
{
	EStatus eStatus = ESTATUS_OK;
	SControlText *psControlText = (SControlText *) psControl->pvControlSpecificData;

	// Create a graphics image 
	if (psControlText)
	{
		eStatus = GraphicsCreateImage(&psControlText->psGraphicsTextImage,
									  true,
									  0, 0);
	}
	
	return(eStatus);
}

// Destroys a text control
static EStatus ControlTextDestroyMethod(SControl *psControl)
{
	SControlText *psControlText = (SControlText *) psControl->pvControlSpecificData;

	if (psControlText)
	{
		GraphicsDestroyImage(&psControlText->psGraphicsTextImage);

		HandleSetInvalid((EHandleGeneric *) &psControlText->eFont);
		psControlText->u16FontSize = 0;
		SafeMemFree(psControlText->peString);
		psControlText->u32RGBAColor = 0;
	}

	return(ESTATUS_OK);
}

// Draws a text control
static EStatus ControlTextDrawMethod(SControl *psControl,
									 uint8_t u8WindowIntensity,
									 int32_t s32WindowXPos,
									 int32_t s32WindowYPos)
{
	SControlText *psControlText = (SControlText *) psControl->pvControlSpecificData;

	if (psControlText)
	{
		// Only draw something if there's a text image to draw
		if (psControlText->psGraphicsTextImage)
		{
			GraphicsSetIntensity(psControlText->psGraphicsTextImage,
								 (uint8_t) ((u8WindowIntensity * psControl->u8Intensity) >> 8));

			// Graphics attributes
			GraphicsSetAttributes(psControlText->psGraphicsTextImage,
								  ControlGetGfxAttributes(psControl),
								  GFXATTR_SWAP_HORIZONAL | GFXATTR_SWAP_VERTICAL);

			GraphicsSetGreyscale(psControlText->psGraphicsTextImage,
								 psControl->bDisabled);

			GraphicsDraw(psControlText->psGraphicsTextImage,
						 psControl->s32XPos + s32WindowXPos,
						 psControl->s32YPos + s32WindowYPos,
						 psControl->bOriginCenter,
						 psControl->dAngle);
		}
		else
		{
			// No text graphic - ignore it
		}
	}

	return(ESTATUS_OK);
}


// List ofmethods for this control
static SControlMethods sg_sControlTextMethods =
{
	ECTRLTYPE_TEXT,								// Type of control
	"Text",										// Name of control
	sizeof(SControlText),						// Size of control specific structure

	ControlTextCreateMethod,					// Create a text control
	ControlTextDestroyMethod,					// Destroy a text control
	NULL,										// Control enable/disable
	NULL,										// New position
	NULL,										// Set visible
	NULL,										// Set angle
	ControlTextDrawMethod,						// Draw the text control
	NULL,										// Runtime configuration
	NULL,										// Hit test
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

EStatus ControlTextSet(EControlTextHandle eControlTextHandle,
					   EFontHandle eFont,
					   uint16_t u16FontSize,
					   char *peString,
					   bool bSetColor,
					   uint32_t u32RGBAColor)
{
	EStatus eStatus;
	bool bLocked = false;
	SControl *psControl = NULL;
	SControlText *psControlText;
	bool bDestroyTextImage = false;

	eStatus = ControlSetLock(eControlTextHandle,
							 true,
							 &psControl,
							 ESTATUS_OK);
	ERR_GOTO();

	bLocked = true;

	psControlText = (SControlText *) psControl->pvControlSpecificData;

	// Figure out if we should destroy the existing text control
	// Perhaps changing fonts!
	if ((eFont) &&
		(eFont != psControlText->eFont))
	{
		bDestroyTextImage = true;
		psControlText->eFont = eFont;
	}

	// See if the font size changed
	if ((u16FontSize) && 
		(u16FontSize != psControlText->u16FontSize))
	{
		bDestroyTextImage = true;
		psControlText->u16FontSize = u16FontSize;
	}

	// See if they're passing us a string
	if (peString)
	{
		if ('\0' == peString[0])
		{
			// Null string - make it NULL
			peString = NULL;
			SafeMemFree(psControlText->peString);
			bDestroyTextImage = true;
		}
		else
		{
			// If there's an old string, compare it. If they're the same, ignore.
			if ((psControlText->peString) &&
				(strcmp(peString, psControlText->peString) == 0))
			{
				// Same text. Leave it alone.
			}
			else
			{
				// It's different. Destroy it
				SafeMemFree(psControlText->peString);
				bDestroyTextImage = true;
			}
		}
	}

	// Set the new text if applicable
	if (peString)
	{
		psControlText->peString = strdupHeap(peString);
		if (NULL == psControlText->peString)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			goto errorExit;
		}
	}

	if (bSetColor)
	{
		psControlText->u32RGBAColor = u32RGBAColor;
	}
	
	if ((psControlText->peString) && (strcmp(psControlText->peString, "Treatment Type") == 0))
	{
		bSetColor = bSetColor;
	}

	if (bDestroyTextImage)
	{
		psControl->u32XSize = 0;
		psControl->u32YSize = 0;

		// Clear any existing image we might have
		GraphicsClearImage(psControlText->psGraphicsTextImage);

		if( psControlText->peString )
		{
			// Figure out how big this new image nees
			eStatus = FontGetOverallStringSize(psControlText->eFont,
											   psControlText->u16FontSize,
											   &psControl->u32XSize,
											   &psControl->u32YSize,
											   psControlText->peString);
			ERR_GOTO();

			// Now go create a place to render it
			eStatus = GraphicsAllocateImage(psControlText->psGraphicsTextImage,
											psControl->u32XSize,
											psControl->u32YSize);
			ERR_GOTO();
		}
	}

	if ((bDestroyTextImage) ||
		(bSetColor))
	{
		if( psControlText->peString )
		{
			// Rerender the text
			eStatus = FontRenderString(psControlText->eFont,
									   psControlText->u16FontSize,
									   psControlText->psGraphicsTextImage->pu32RGBA,
									   psControl->u32XSize,
									   psControlText->peString,
									   psControlText->u32RGBAColor);

			GraphicsImageUpdated(psControlText->psGraphicsTextImage);
		}

		// Force a repaint
		WindowUpdated();
	}

errorExit:
	if (bLocked)
	{
		eStatus = ControlSetLock(eControlTextHandle,
								 false,
								 NULL,
								 eStatus);
	}

	return(eStatus);
		
}

EStatus ControlTextCreate(EWindowHandle eWindowHandle,
						  EControlTextHandle *peControlTextHandle,
						  int32_t s32XPos,
						  int32_t s32YPos)
{
	EStatus eStatus;

	// Go create and load the thing
	eStatus = ControlCreate(eWindowHandle,
							(EControlHandle *) peControlTextHandle,
							s32XPos,
							s32YPos,
							0,
							0,
							ECTRLTYPE_TEXT,
							NULL);

	return(eStatus);
}

EStatus ControlTextInit(void)
{
	return(ControlRegister(sg_sControlTextMethods.eType,
						   &sg_sControlTextMethods));
}
