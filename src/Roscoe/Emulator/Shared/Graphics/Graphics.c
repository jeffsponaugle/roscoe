#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/Graphics/Graphics.h"
#include "Shared/libpng/png.h"
#include "Platform/Platform.h"
#include "Shared/UtilTask.h"

/*

Check out

glViewport
glScissor

For moving viewports around

*/

// Utiltask for publishing the graphics position and size
#define	UTILTASK_GRAPHICS_KEY	0x6b7afed5

// Physical X/Y size - common coordinates
#define	PHYSICAL_XSIZE			3840
#define	PHYSICAL_YSIZE			2160

#define	VIRTUAL_XSIZE			3840
#define	VIRTUAL_YSIZE			2160

static SDL_Window *sg_psSDLWindow = NULL;
static SDL_GLContext sg_sGLContext;

// Physical (display/window/viewport) size
static uint32_t sg_u32PhysicalXSize = PHYSICAL_XSIZE;
static uint32_t sg_u32PhysicalYSize = PHYSICAL_YSIZE;

// Virtual coordinate system size
static uint32_t sg_u32VirtualXSize = VIRTUAL_XSIZE;
static uint32_t sg_u32VirtualYSize = VIRTUAL_YSIZE;

// true If full screen
static bool sg_bFullscreen = false;

// true If we're connected to MQTT
static bool sg_bMQTTConnected = false;

// Returns true if the app is full screen
bool GraphicsIsFullscreen(void)
{
	return(sg_bFullscreen);
}

// This pushes window x/y position and size to MQTT
static EStatus GraphicsPublishPositionAndSize(void)
{
	int s32XPos;
	int s32YPos;
	int s32XSize;
	int s32YSize;
	int s32PayloadLength;
	char eTopicName[200];
	char ePayload[100];

	// Get this window's position
	SDL_GetWindowPosition(sg_psSDLWindow,
						  &s32XPos,
						  &s32YPos);

	// And its size
	SDL_GetWindowSize(sg_psSDLWindow,
					  &s32XSize,
					  &s32YSize);


	// Topic name is ProgramName/GRAPHICS_POSINFO_TOPIC
	snprintf(eTopicName, sizeof(eTopicName) - 1,
			 "%s/%s", PlatformGetProgramName(), GRAPHICS_POSINFO_TOPIC);

	// Now give the x/y position and x/y size
	s32PayloadLength = snprintf(ePayload, sizeof(ePayload) - 1,
								"%d,%d,%d,%d", s32XPos, s32YPos, s32XSize, s32YSize);

	// +1 So we send a NULL across so any subscriber can treat it as a string
	return(ESTATUS_OK);
}

// Gets the size of the physical surface size - windowed or full screen
void GraphicsGetPhysicalSurfaceSize(uint32_t *pu32PhysicalXSize,
									uint32_t *pu32PhysicalYSize)
{
	if (pu32PhysicalXSize)
	{
		*pu32PhysicalXSize = sg_u32PhysicalXSize;
	}

	if (pu32PhysicalYSize)
	{
		*pu32PhysicalYSize = sg_u32PhysicalYSize;
	}
}

void GraphicsGetVirtualSurfaceSize(uint32_t *pu32VirtualXSize,
								   uint32_t *pu32VirtualYSize)
{
	if (pu32VirtualXSize)
	{
		*pu32VirtualXSize = sg_u32VirtualXSize;
	}

	if (pu32VirtualYSize)
	{
		*pu32VirtualYSize = sg_u32VirtualYSize;
	}
}

static void *PNGMalloc(png_structp png_ptr, 
					  uint32_t u32MemSize)
{
	return(MemAllocNoClear(u32MemSize));
}

static void PNGFree(png_structp png_ptr, 
					void *pvMemoryBlock)
{
	SafeMemFree(pvMemoryBlock);
}

// Used by libpng to track state of graphics file decode
typedef struct SPNGReadState
{
	uint8_t *pu8GraphicsBase;			// Base of malloc'd area
	uint8_t *pu8DataPtr;				// Current data pointer
	uint64_t u64CurrentPosition;		// Current position within file
	uint64_t u64FileSize;				// Total file size
} SPNGReadState;

// In-memory data reader for the PNG library
static void PNGReadData(png_structp psPNGStruct, 
						png_bytep pu8Data, 
						png_size_t u32Length)
{
	SPNGReadState *psState = (SPNGReadState *) png_get_io_ptr(psPNGStruct);

	BASSERT(psState);

	// See if we're out of room
	if ((u32Length + psState->u64CurrentPosition) > psState->u64FileSize)
	{
		png_error(psPNGStruct, "Attempt to read past the end of the file\n");
	}
	else
	{
		// Lots of 32 bit reads
		if (sizeof(uint32_t) == u32Length)
		{
			*((uint32_t *) pu8Data) = *((uint32_t *) psState->pu8DataPtr);
		}
		else
		{
			memcpy((void *) pu8Data, 
				   (void *) psState->pu8DataPtr, 
				   u32Length);
		}

		psState->u64CurrentPosition += u32Length;
		psState->pu8DataPtr += u32Length;
	}
}

// Loads up a graphics file
EStatus GraphicsLoadImage(char *peImageFilename,
						  uint32_t **ppu32RGBA,
						  uint32_t *pu32XSize,
						  uint32_t *pu32YSize)
{
	EStatus eStatus;
	png_structp psPNGStruct = NULL;
	png_infop psPNGInfo = NULL;
	SPNGReadState sPNGReadState;
	png_bytepp psRowPointers = NULL;
	uint32_t u32Loop;
	uint32_t u32YPos;

	ZERO_STRUCT(sPNGReadState);

	// Load up the image
	eStatus = FileLoad(peImageFilename,
					   &sPNGReadState.pu8GraphicsBase,
					   &sPNGReadState.u64FileSize,
					   0);
	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Error while loading file '%s' - %s\n", peImageFilename, GetErrorText(eStatus));
	}

	ERR_GOTO();

	sPNGReadState.pu8DataPtr = sPNGReadState.pu8GraphicsBase;

	// See if this is a PNG file
	if (png_sig_cmp((png_bytep) sPNGReadState.pu8DataPtr, 
					0, 
					(size_t) sPNGReadState.u64FileSize))
	{
		// Not a PNG file
		eStatus = ESTATUS_GFX_FILE_FORMAT_UNKNOWN;
		SyslogFunc("File '%s' - not a graphics file\n", peImageFilename);
		goto errorExit;
	}
	else
	{
		int s32BPP;
		int s32ColorType;

		// It's a PNG file!
		psPNGStruct = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
											   NULL,
											   NULL,
											   NULL,
											   NULL,
											   (png_malloc_ptr) PNGMalloc,
											   (png_free_ptr) PNGFree
											   );
		if (NULL == psPNGStruct)
		{
			SyslogFunc("Out of memory while loading file '%s'\n", peImageFilename);
			eStatus = ESTATUS_OUT_OF_MEMORY;
			goto errorExit;
		}

		// STFU, PNG lib
		png_set_crc_action(psPNGStruct,
						   PNG_CRC_QUIET_USE,
						   PNG_CRC_QUIET_USE);

		// Need information about this image
		psPNGInfo = png_create_info_struct(psPNGStruct);
		if (NULL == psPNGInfo)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			SyslogFunc("Out of memory while loading file '%s'\n", peImageFilename);
			goto errorExit;
		}

		// Skip past the signature bytes
		png_set_sig_bytes(psPNGStruct,
						  8);
		sPNGReadState.u64CurrentPosition += 8;
		sPNGReadState.pu8DataPtr += 8;

		// Set up our read data function
		png_set_read_fn(psPNGStruct,
						(png_voidp) &sPNGReadState,
						(png_rw_ptr) PNGReadData);

		// And our info
		png_read_info(psPNGStruct, 
					  psPNGInfo);

		// Get some info on this image
		if (0 == png_get_IHDR(psPNGStruct, 
							  psPNGInfo, 
							  (png_uint_32*) pu32XSize, 
							  (png_uint_32*) pu32YSize, 
							  &s32BPP, 
							  &s32ColorType, 
							  NULL, 
							  NULL, 
							  NULL))
		{
			// Bad file
			eStatus = ESTATUS_GFX_FILE_CORRUPT;
			SyslogFunc("PNG File '%s' corrupt\n", peImageFilename);
			goto errorExit;
		}

		// Expect that we're looking for PNG_COLOR_TYPE_RGBA_ALPHA. If not, say it's an invalid image
		if (s32ColorType != PNG_COLOR_TYPE_RGB_ALPHA)
		{
			SyslogFunc("Color type not PNG_COLOR_TYPE_RGB_ALPHA - got %u\n", s32ColorType);
			eStatus = ESTATUS_GFX_FILE_NOT_SUPPORTED;
			goto errorExit;
		}

		// Make an array of row pointers
		psRowPointers = (png_bytepp) png_malloc(psPNGStruct,
												sizeof(*psRowPointers) * *pu32YSize);
		if (NULL == psRowPointers)
		{
			eStatus = ESTATUS_OUT_OF_MEMORY;
			SyslogFunc("Out of memory while creating row pointers for file '%s'\n", peImageFilename);
			goto errorExit;
		}

		// Now allowcate some rows
		for (u32Loop = 0; u32Loop < *pu32YSize; u32Loop++)
		{
			// PNG_ROWBYTES(pixel_depth, row_width) - (*ppsGraphicsImage)->u32YSize
			psRowPointers[u32Loop] = (png_bytep) png_malloc(psPNGStruct,
															*pu32XSize * (s32BPP >> 3) * png_get_channels(psPNGStruct, psPNGInfo));
			if (NULL == psRowPointers[u32Loop])
			{
				eStatus = ESTATUS_OUT_OF_MEMORY;
				SyslogFunc("Out of memory while creating row pointers for file '%s'\n", peImageFilename);
				goto errorExit;
			}
		}

		// Set up the row pointers
		png_set_rows(psPNGStruct,
					 psPNGInfo,
					 psRowPointers);

		// Now go read the image
		png_read_image(psPNGStruct,
					   psRowPointers);

		// Now end the whole thing
		png_read_end(psPNGStruct, 
					 psPNGInfo);

		MEMALLOC(*ppu32RGBA, *pu32XSize * 
							 *pu32YSize *
						 	 sizeof(**ppu32RGBA));

		if (16 == s32BPP)
		{
			// Here we assume RGBA, 16BPP from the source row data to the target data
			for (u32YPos = 0; u32YPos < *pu32YSize; u32YPos++)
			{
				uint32_t *pu32RGBAPtr;
				uint16_t *pu16SrcPtr;
				uint8_t u8Pixel = 0;

				pu32RGBAPtr = *ppu32RGBA + (u32YPos * *pu32XSize);
				pu16SrcPtr = (uint16_t *) psRowPointers[u32YPos];
				u32Loop = *pu32XSize;
				while (u32Loop--)
				{
					uint32_t u32Pixel;

					// Red
					u32Pixel = ((uint8_t) *pu16SrcPtr);
					pu16SrcPtr++;

					// Green
					u32Pixel |= ((uint8_t) *pu16SrcPtr) << 8;
					pu16SrcPtr++;

					// Blue
					u32Pixel |= ((uint8_t) *pu16SrcPtr) << 16;
					pu16SrcPtr++;

					// Alpha
					*pu32RGBAPtr = (u32Pixel) | (((uint8_t) (*pu16SrcPtr)) << 24);
					++pu32RGBAPtr;
					pu16SrcPtr++;
					u8Pixel++;
				}
			}
		}
		else
		if (8 == s32BPP)
		{
			// 8BPP source image
			uint32_t *pu32RGBAPtr = *ppu32RGBA;

			// Here we assume RGBA, 8BPP from the source row data to the target data
			for (u32YPos = 0; u32YPos < *pu32YSize; u32YPos++)
			{
				memcpy((void *) pu32RGBAPtr, (void *) psRowPointers[u32YPos], *pu32XSize * sizeof(*pu32RGBAPtr));
				pu32RGBAPtr += *pu32XSize;
			}
		}
		else
		{
			eStatus = ESTATUS_GFX_FILE_FORMAT_UNKNOWN;
			goto errorExit;
		}

		SyslogFunc("PNG File '%s' - %ux%u %ubpp, channels=%u\n", peImageFilename, *pu32XSize, *pu32YSize, s32BPP, png_get_channels(psPNGStruct, psPNGInfo));

		eStatus = ESTATUS_OK;
	}

errorExit:
	SafeMemFree(sPNGReadState.pu8GraphicsBase);

	// If we have row pointers, delete them
	if (psRowPointers)
	{
		for (u32Loop = 0; u32Loop < *pu32YSize; u32Loop++)
		{
			SafeMemFree(psRowPointers[u32Loop]);
		}

		SafeMemFree(psRowPointers);
	}

	png_destroy_read_struct(&psPNGStruct,
							&psPNGInfo,
							NULL);

	return(eStatus);
}

// Creates a blank graphic image
EStatus GraphicsCreateImage(SGraphicsImage **ppsGraphicsImage,
							bool bOpenGLTexture,
							uint32_t u32XSize,
							uint32_t u32YSize)
{
	EStatus eStatus = ESTATUS_OK;

	// Allocate a graphics image structure
	MEMALLOC(*ppsGraphicsImage, sizeof(**ppsGraphicsImage));

	(*ppsGraphicsImage)->u32TotalXSize = u32XSize;
	(*ppsGraphicsImage)->u32TotalYSize = u32YSize;

	// And allocate an RGBA pixel buffer if there is one
	if (u32XSize && u32YSize)
	{
		MEMALLOC((*ppsGraphicsImage)->pu32RGBA, (*ppsGraphicsImage)->u32TotalXSize * 
												(*ppsGraphicsImage)->u32TotalYSize *
												sizeof(*((*ppsGraphicsImage)->pu32RGBA)));
	}

	// Default intensity - full
	(*ppsGraphicsImage)->u8Intensity = 0xff;

	if (bOpenGLTexture)
	{
		glGenTextures(1, &(*ppsGraphicsImage)->eTextureHandle);
		(*ppsGraphicsImage)->bOpenGLTexture = true;
	}

errorExit:
	return(eStatus);
}

// Allocates a graphics array but does not touch the openGL context if it exists
EStatus GraphicsAllocateImage(SGraphicsImage *psGraphicsImage,
							  uint32_t u32XSize,
							  uint32_t u32YSize)
{
	EStatus eStatus;

	// Make sure we have a graphics image structure
	BASSERT(psGraphicsImage);

	// Make sure we don't already have an image here
	BASSERT(NULL == psGraphicsImage->pu32RGBA);

	MEMALLOC(psGraphicsImage->pu32RGBA, u32XSize * u32YSize * sizeof(*psGraphicsImage->pu32RGBA));

	psGraphicsImage->u32TotalXSize = u32XSize;
	psGraphicsImage->u32TotalYSize = u32YSize;
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

void GraphicsFillImage(SGraphicsImage *psGraphicsImage,
					   uint32_t u32RGBAFillColor)
{
	if (psGraphicsImage)
	{
		uint32_t *pu32RGBA = psGraphicsImage->pu32RGBA;
		uint32_t u32Loop = (psGraphicsImage->u32TotalXSize * psGraphicsImage->u32TotalYSize);

		while (u32Loop)
		{
			*pu32RGBA = u32RGBAFillColor;
			++pu32RGBA;
			u32Loop--;
		}

		psGraphicsImage->bTextureAssigned = false;
	}
}

void GraphicsFillImageRegion(SGraphicsImage *psGraphicsImage,
							 int32_t s32XPos,
							 int32_t s32YPos,
							 uint32_t u32XSize,
							 uint32_t u32YSize,
							 uint32_t u32RGBAFillColor)
{
	if (s32XPos < 0)
	{
		if (-s32XPos >= (int32_t) u32XSize)
		{
			// Off the image entirely
			return;
		}

		// Otherwise adjust the size
		u32XSize -= -s32XPos;
		s32XPos = 0;
	}

	if (s32YPos < 0)
	{
		if (-s32YPos >= (int32_t) u32YSize)
		{
			// Off the image entirely
			return;
		}

		// Otherwise adjust the size
		u32YSize -= -s32YPos;
		s32YPos = 0;
	}

	if (s32XPos >= (int32_t) psGraphicsImage->u32TotalXSize)
	{
		// Off the image entirely
		return;
	}

	if (s32YPos >= (int32_t) psGraphicsImage->u32TotalYSize)
	{
		// Off the image entirely
		return;
	}

	// See if we need to clip the size
	if ((s32XPos + (int32_t) u32XSize) > (int32_t) psGraphicsImage->u32TotalXSize)
	{
		u32XSize = (uint32_t) (psGraphicsImage->u32TotalXSize - (uint32_t) s32XPos);
	}

	if ((s32YPos + (int32_t) u32YSize) > (int32_t) psGraphicsImage->u32TotalYSize)
	{
		u32YSize = (uint32_t) (psGraphicsImage->u32TotalYSize - (uint32_t) s32YPos);
	}

	if ((u32XSize) && (u32YSize))
	{
		uint32_t *pu32RGBABase = psGraphicsImage->pu32RGBA + s32XPos + (s32YPos * psGraphicsImage->u32TotalXSize);
		uint32_t *pu32RGBA;

		while (u32YSize)
		{
			uint32_t u32Count;

			pu32RGBA = pu32RGBABase;
			u32Count = u32XSize;
			while (u32Count)
			{
				*pu32RGBA = u32RGBAFillColor; 
				++pu32RGBA;
				--u32Count;
			}

			pu32RGBABase += psGraphicsImage->u32TotalXSize;
			u32YSize--;
		}
	}
}

void GraphicsClearImage(SGraphicsImage *psGraphicsImage)
{
	if (psGraphicsImage)
	{
		if(psGraphicsImage->bAutoDeallocate)
		{
			SafeMemFree(psGraphicsImage->pu32RGBA);
		}
		psGraphicsImage->pu32RGBA = NULL;
		psGraphicsImage->u32TotalXSize = 0;
		psGraphicsImage->u32TotalYSize = 0;
		psGraphicsImage->u32ViewportYOffset = 0;
		psGraphicsImage->u32ViewportYOffsetPrior = 0;
		psGraphicsImage->u32ViewportYSize = 0;
		psGraphicsImage->u32ViewportYSizePrior = 0;
		psGraphicsImage->u8Intensity = 0xff;
		psGraphicsImage->bTextureAssigned = false;
		psGraphicsImage->u32Attribute = 0;
	}
}

// Causes the texture to be reloaded from main memory to the card
void GraphicsImageUpdated(SGraphicsImage *psGraphicsImage)
{
	psGraphicsImage->bTextureAssigned = false;
}

// Set graphical attributes
void GraphicsSetAttributes(SGraphicsImage *psGraphicsImage,
						   uint32_t u32Attributes,
						   uint32_t u32AttributeMask)
{
	psGraphicsImage->u32Attribute  = (psGraphicsImage->u32Attribute & ~u32AttributeMask) |
									 u32Attributes;
}

// Set graphic intensity for this image - 0=dark, 255=Full
void GraphicsSetIntensity(SGraphicsImage *psGraphicsImage,
						  uint8_t u8Intensity)
{
	psGraphicsImage->u8Intensity = u8Intensity;
}

// Sets viewport's size
void GraphicsSetViewportSize(SGraphicsImage *psGraphicsImage,
							 uint32_t u32ViewportYSize)
{
	psGraphicsImage->u32ViewportYSize = u32ViewportYSize;
}

// Set a new graphics image for this texture (does not clear prior image)
void GraphicsSetImage(SGraphicsImage *psGraphicsImage,
					  uint32_t *pu32RGBA,
					  uint32_t u32XSize,
					  uint32_t u32YSize,
					  bool bAutoDeallocate)
{
	psGraphicsImage->pu32RGBA = pu32RGBA;
	psGraphicsImage->u32TotalXSize = u32XSize;
	psGraphicsImage->u32TotalYSize = u32YSize;
	psGraphicsImage->bTextureAssigned = false;
	psGraphicsImage->bAutoDeallocate = bAutoDeallocate;
}


// Destroy a graphics image
void GraphicsDestroyImage(SGraphicsImage **ppsGraphicsImage)
{
	if (*ppsGraphicsImage)
	{
		if ((*ppsGraphicsImage)->bAutoDeallocate)
		{
			// We auto-dealllocate the RGBA image
			SafeMemFree((*ppsGraphicsImage)->pu32RGBA);
		}
		else
		{
			// The image data is being handled elsewhere so ignore here.
		}

		// If this has an OpenGL texture, then deallocate it
		if ((*ppsGraphicsImage)->bOpenGLTexture)
		{
			glDeleteTextures(1, &(*ppsGraphicsImage)->eTextureHandle);
			(*ppsGraphicsImage)->bOpenGLTexture = false;
		}

		SafeMemFree(*ppsGraphicsImage);
	}
}

// Sets viewport's Y offset
void GraphicsSetViewportOffset(SGraphicsImage *psGraphicsImage,
							   uint32_t u32ViewportYOffset)
{
	psGraphicsImage->u32ViewportYOffset = u32ViewportYOffset;
}

// Submits a graphics image to the OpenGL queue
EStatus GraphicsDraw(SGraphicsImage *psGraphicsImage,
					 int32_t s32X,
					 int32_t s32Y,
					 bool bOriginCenter,
					 double dAngle)
{
	EStatus eStatus;
	uint32_t u32YSize;
	uint32_t u32YOrigin = 0;
	GLfloat fIntensity = 0.0;
	GLfloat fX[4], fY[4];
	GLfloat fTexLeft = 0.f;
	GLfloat fTexTop = 0.f;
	GLfloat fTexRight = 1.0f;
	GLfloat fTexBottom = 1.0f;
	GLfloat fXOffset = 0.0f;
	GLfloat fYOffset = 0.0f;

	// If this is a NULL graphics image, just return everything's OK
	if (NULL == psGraphicsImage)
	{
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	u32YSize = psGraphicsImage->u32TotalYSize;

	// Create the intensity
	fIntensity = (GLfloat) psGraphicsImage->u8Intensity / (GLfloat) 255.0;

	// If the viewport Y size or offset changes, reassign the texture
	if ((psGraphicsImage->u32ViewportYSize != psGraphicsImage->u32ViewportYSizePrior) ||
		(psGraphicsImage->u32ViewportYOffset != psGraphicsImage->u32ViewportYOffsetPrior))
	{
		// Cause the texture to get reassigned
		psGraphicsImage->bTextureAssigned = false;
	}

	// Figure out if we need to deal with any viewport
	if (psGraphicsImage->u32ViewportYSize)
	{
		// Clip to viewport
		if (psGraphicsImage->u32ViewportYSize < u32YSize)
		{
			u32YSize = psGraphicsImage->u32ViewportYSize;
		}

		// See if our origin is larger than the region (just clip it)
		if (psGraphicsImage->u32ViewportYOffset >= psGraphicsImage->u32TotalYSize)
		{
			// Too big
			u32YSize = 0;
		}
		else
		{
			uint32_t u32YSizeDelta = psGraphicsImage->u32TotalYSize - psGraphicsImage->u32ViewportYOffset;

			if (u32YSizeDelta < u32YSize)
			{
				u32YSize = u32YSizeDelta;
			}
		}
	}

	// If there's nothing to draw, then just exit.
	if (0 == u32YSize)
	{
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	// If we have a center origin, then adjust X/Y offsets to be half the width and height
	if (bOriginCenter)
	{
		fXOffset = (GLfloat) (0.0f - (((GLfloat) psGraphicsImage->u32TotalXSize) / 2.0));
		fYOffset = (GLfloat) (0.0f - (((GLfloat) u32YSize) / 2.0));
	}

	// Set up default vertexes in native orientation

	// Upper left
	fX[0] = (GLfloat) (0.0 + fXOffset); fY[0] = (GLfloat) (0.0 + fYOffset);

	// Upper right
	fX[1] = ((GLfloat) psGraphicsImage->u32TotalXSize) + fXOffset; fY[1] = fY[0];

	// Lower right
	fX[2] = ((GLfloat) psGraphicsImage->u32TotalXSize) + fXOffset; fY[2] = ((GLfloat) u32YSize) + fYOffset;

	// Lower left
	fX[3] = ((GLfloat) (0.0 + fXOffset)); fY[3] = ((GLfloat) u32YSize) + fYOffset;

	// Let's see if we need to rotate everything
	if (dAngle != 0.0 )
	{
		uint8_t u8Loop = 0;
		double dRadians;

		dRadians = dAngle * (M_PI / 180.0);

		// Yup. Time to rotate
		for (u8Loop = 0; u8Loop < (sizeof(fX) / sizeof(fX[0])); u8Loop++)
		{
			double dOrgX;

			// https://stackoverflow.com/questions/2259476/rotating-a-point-about-another-point-2d
			dOrgX = fX[u8Loop];
			fX[u8Loop] = (GLfloat) ((fX[u8Loop] * cos(dRadians)) - (fY[u8Loop] * sin(dRadians)));
			fY[u8Loop] = (GLfloat) ((dOrgX * sin(dRadians)) + (fY[u8Loop] * cos(dRadians)));
		}
	}

	// For this image, see if we need to flip horizontal or vertical
	if (psGraphicsImage->u32Attribute & GFXATTR_SWAP_HORIZONAL)
	{
		GLfloat fTemp;

		fTemp = fTexLeft;
		fTexLeft = fTexRight;
		fTexRight = fTemp;
	}

	if (psGraphicsImage->u32Attribute & GFXATTR_SWAP_VERTICAL)
	{
		GLfloat fTemp;

		fTemp = fTexTop;
		fTexTop = fTexBottom;
		fTexBottom = fTemp;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, psGraphicsImage->eTextureHandle);

	if (false == psGraphicsImage->bTextureAssigned)
	{
		glTexImage2D(GL_TEXTURE_2D, 
					 0, 
					 GL_RGBA, 
					 psGraphicsImage->u32TotalXSize, 
					 u32YSize, 
					 0, 
					 GL_RGBA, 
					 GL_UNSIGNED_BYTE,
					 (const GLvoid *) (psGraphicsImage->pu32RGBA + (psGraphicsImage->u32TotalXSize * psGraphicsImage->u32ViewportYOffset)));
		psGraphicsImage->bTextureAssigned = true;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (psGraphicsImage->bGreyscale)
	{
		GLfloat greyscaleFactor[3] = { (GLfloat) (0.3), (GLfloat) (0.59), (GLfloat) (0.11) };

		greyscaleFactor[0] = (GLfloat) 0.628;
		greyscaleFactor[1] = (GLfloat) 1.0484;
		greyscaleFactor[2] = (GLfloat) 0.2036;

		glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, greyscaleFactor);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glBegin(GL_QUADS);
	glColor3f(fIntensity, fIntensity, fIntensity);
	glTexCoord2f(fTexLeft, fTexTop); 
	glVertex3f(fX[0] + ((GLfloat) s32X), fY[0] + ((GLfloat) s32Y), (GLfloat) 0.0f);	// Left top

	glTexCoord2f(fTexRight, fTexTop);
	glVertex3f(fX[1] + ((GLfloat) s32X), fY[1] + ((GLfloat) s32Y), (GLfloat) 0.0f);		// Right top

	glTexCoord2f(fTexRight,fTexBottom);
	glVertex3f(fX[2] + ((GLfloat) s32X), fY[2] + ((GLfloat) s32Y), (GLfloat) 0.0f);	// Right bottom

	glTexCoord2f(fTexLeft, fTexBottom);
	glVertex3f(fX[3] + ((GLfloat) s32X), fY[3] + ((GLfloat) s32Y), (GLfloat) 0.0f);		// Left bottom

	glEnd();

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

void GraphicsDrawQuad(int32_t s32X,
					  int32_t s32Y,
					  uint32_t u32XSize,
					  uint32_t u32YSize,
					  uint32_t u32RGBA)
{
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
		glColor4f(((GLfloat) (u32RGBA & 0xff)) / 255.0f,
				  ((GLfloat) ((u32RGBA >> 8) & 0xff)) / 255.0f,
				  ((GLfloat) ((u32RGBA >> 16) & 0xff)) / 255.0f,
				  ((GLfloat) ((u32RGBA >> 24) & 0xff)) / 255.0f);
		glVertex3f((GLfloat) s32X, (GLfloat) s32Y, -1.0f);
		glVertex3f((GLfloat) (s32X + (int32_t) u32XSize), (GLfloat) s32Y, 0.0f);
		glVertex3f((GLfloat) (s32X + (int32_t) u32XSize), (GLfloat) (s32Y + (int32_t) u32YSize), 0.0f);
		glVertex3f((GLfloat) s32X, (GLfloat) (s32Y + (int32_t) u32YSize), 0.0f);
	glEnd();
}


void GraphicsStartFrame(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GraphicsEndFrame(void)
{
	SDL_GL_SwapWindow(sg_psSDLWindow);
}

void GraphicsSetMinimize(void)
{
	SDL_MinimizeWindow(sg_psSDLWindow);
}

void GraphicsSetMaximize(void)
{
	SDL_RestoreWindow(sg_psSDLWindow);
}

void GraphicsSetGreyscale(SGraphicsImage *psGraphicsImage,
						  bool bGreyscale)
{
	psGraphicsImage->bGreyscale = bGreyscale;
}
void GraphicsGLSetCurrent(void)
{
	SDL_GL_MakeCurrent(sg_psSDLWindow,
					   sg_sGLContext);
}

// Full screen resolutions
typedef struct SResolutions
{
	uint32_t u32XSize;
	uint32_t u32YSize;
	uint8_t u8Hz;
	int s32Mode;
} SResolutions;

// Preferred resolutions in decreasing order of preference.
static struct SResolutions sg_sPreferredResolutions[] =
{
	// 16:9 ratios
	{3840,	2160},
	{2560,	1440},
	{2048,	1152},
	{1920,	1080},
	{1600,   900},
	{1360,	 768},

	// 16:10 ratios
	{2560,	1600},
	{1920,	1200},
	{1680,	1050},
	{1440,	 900},
	{1280,	 800},
};

// Called when a UtilTask for graphics position change happens
static int64_t GraphicsPositionChangeCallback(uint32_t u32UtilTaskKey,
											int64_t s64Message)
{
	EStatus eStatus;

	eStatus = GraphicsPublishPositionAndSize();
	if (eStatus != ESTATUS_OK)
	{
		SyslogFunc("Call to GraphicsPublishPositionAndSize failed - %s\n", GetErrorText(eStatus));
	}

	return(0);
}

// Signal a UtilTask for this operation
EStatus GraphicsSignalPositionUpdate(void)
{
	return(UtilTaskSignal(UTILTASK_GRAPHICS_KEY,
						  0,
						  NULL,
						  NULL));
}

void GraphicsWindowSetPosition(uint32_t u32XPos,
							   uint32_t u32YPos)
{
	SDL_SetWindowPosition(sg_psSDLWindow,
						  (int) u32XPos,
						  (int) u32YPos);

	GraphicsSignalPositionUpdate();
}

EStatus GraphicsInit(uint32_t *pu32FPS)
{
	EStatus eStatus;
	SDL_DisplayMode sMode;
	int s32DisplayModeCount;
	int s32Loop = 0;
	uint32_t u32WindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
	GLenum error = GL_NO_ERROR;
	uint32_t u32FPS = 0;
	bool bBorderless = false;
	uint32_t u32InitialXPos;
	uint32_t u32InitialYPos;
	double dXPhysicalScale = 1.0;
	double dYPhysicalScale = 1.0;

	// Get platform settings
	PlatformGetGraphicsSettings(&sg_u32PhysicalXSize,
								&sg_u32PhysicalYSize,
								&sg_u32VirtualXSize,
								&sg_u32VirtualYSize,
								&sg_bFullscreen,
								&bBorderless);


	// If we're windowed, halve the size of the display on each axis
	if (CmdLineOption("-windowed"))
	{
		// Halve the size of display
		sg_u32PhysicalXSize >>= 1;
		sg_u32PhysicalYSize >>= 1;
	}

	if (bBorderless)
	{
		u32WindowFlags |= SDL_WINDOW_BORDERLESS;
	}

	// Clear out the video modes and highest refresh found
	for (s32Loop = 0; s32Loop < (sizeof(sg_sPreferredResolutions) / sizeof(sg_sPreferredResolutions[0])); s32Loop++)
	{
		sg_sPreferredResolutions[s32Loop].u8Hz = 0;
		sg_sPreferredResolutions[s32Loop].s32Mode = -1;
	}

	// Init SDL
	if (SDL_Init(SDL_INIT_TIMER |
				 SDL_INIT_VIDEO |
				 SDL_INIT_HAPTIC |
				 SDL_INIT_JOYSTICK |
				 SDL_INIT_GAMECONTROLLER |
				 SDL_INIT_EVENTS))
	{
		Syslog("SDL Init failure - %s\n", SDL_GetError());
		eStatus = ESTATUS_GFX_INIT_FAULT;
		goto errorExit;
	}

	// Use OpenGL 2.1
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 
						2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 
						1);

	if (false == sg_bFullscreen)
	{
		SDL_DisplayMode sDisplayMode;

		// Lock it to 60FPS for windowed mode
		u32FPS = 60;

		// Let's see what our desktop display mode is
		s32Loop = SDL_GetDesktopDisplayMode(0,
											&sDisplayMode);
		if (s32Loop)
		{
			SyslogFunc("SDL_GetDesktopDisplayMode failed - %d\n", s32Loop);
		}
		else
		{
			// Got it!
			sg_u32PhysicalXSize = (uint32_t) ((double) sDisplayMode.w / ((double) PHYSICAL_XSIZE / (double) sg_u32PhysicalXSize));
			sg_u32PhysicalYSize = (uint32_t) ((double) sDisplayMode.h / ((double) PHYSICAL_YSIZE / (double) sg_u32PhysicalYSize));
		}
	}
	else
	{
		int s32BestMode = -1;
		uint32_t u32BestXSize = 0;
		uint32_t u32BestYSize = 0;
		uint8_t u8BestHz = 0;
		int s32Loop2;

		u32WindowFlags |= SDL_WINDOW_FULLSCREEN;

		// Always dump the available full screen resolutions to syslog
		s32DisplayModeCount = SDL_GetNumDisplayModes(0);

		Syslog("Full screen display modes available:\n");

		for (s32Loop = 0; s32Loop < s32DisplayModeCount; s32Loop++)
		{
			if (SDL_GetDisplayMode(0,
								   s32Loop,
								   &sMode))
			{
				Syslog("Failed to get display mode index %d - %s\n", s32Loop, SDL_GetError());
				eStatus = ESTATUS_GFX_DISPLAY_MODE_FAULT;
				goto errorExit;
			}

			// If the card is returning a 0hz refresh rate, we can assume it's 60hz
			if (0 == sMode.refresh_rate)
			{
				Syslog("Driver reports 0hz refresh rate for %ux%u, adjusting to 60Hz\n", sMode.w, sMode.h);
				sMode.refresh_rate = 60;
			}

			// Look through all of our resolutions and see if we have a match, and if this is better than what we have.
			for (s32Loop2 = 0; s32Loop2 < (sizeof(sg_sPreferredResolutions) / sizeof(sg_sPreferredResolutions[0])); s32Loop2++)
			{
				if ((sg_sPreferredResolutions[s32Loop2].u32XSize == (uint32_t) sMode.w) &&
					(sg_sPreferredResolutions[s32Loop2].u32YSize == (uint32_t) sMode.h))
				{
					// We have a resolution match. Let's see what the refresh rate. If it's better, record it.
					if (sMode.refresh_rate > sg_sPreferredResolutions[s32Loop2].u8Hz)
					{
						sg_sPreferredResolutions[s32Loop2].u8Hz = (uint8_t) sMode.refresh_rate;
						sg_sPreferredResolutions[s32Loop2].s32Mode = s32Loop;
					}
				}
			}

			Syslog(" Mode %u: BPP=%u, %ux%u (%uhz) - %4.4f\n", s32Loop, SDL_BITSPERPIXEL(sMode.format), sMode.w, sMode.h, sMode.refresh_rate, (double) sMode.w / (double) sMode.h);
		}

		// Let's rip through and see what we've found and pick
		for (s32Loop = 0; s32Loop < (sizeof(sg_sPreferredResolutions) / sizeof(sg_sPreferredResolutions[0])); s32Loop++)
		{
			if (sg_sPreferredResolutions[s32Loop].s32Mode >= 0)
			{
				break;
			}
		}

		if (s32Loop == (sizeof(sg_sPreferredResolutions) / sizeof(sg_sPreferredResolutions[0])))
		{
			Syslog("No suitable display modes found. Exiting.\n");
			eStatus = ESTATUS_GFX_DISPLAY_MODE_FAULT;
			goto errorExit;
		}

		Syslog("Chose full screen mode %ux%u %uhz, video mode %d\n", sg_sPreferredResolutions[s32Loop].u32XSize, sg_sPreferredResolutions[s32Loop].u32YSize, sg_sPreferredResolutions[s32Loop].u8Hz, sg_sPreferredResolutions[s32Loop].s32Mode);

		sg_u32PhysicalXSize = sg_sPreferredResolutions[s32Loop].u32XSize;
		sg_u32PhysicalYSize = sg_sPreferredResolutions[s32Loop].u32YSize;
		u32FPS = sg_sPreferredResolutions[s32Loop].u8Hz;
	}

	sg_u32PhysicalXSize = (uint32_t) ((double) sg_u32PhysicalXSize * dXPhysicalScale);
	sg_u32PhysicalYSize = (uint32_t) ((double) sg_u32PhysicalYSize * dYPhysicalScale);

	// Figure out where to put this viewport
	eStatus = PlatformGetViewportInitialPosition(&u32InitialXPos,
												 &u32InitialYPos);
	if (eStatus != ESTATUS_OK)
	{
		u32InitialXPos = SDL_WINDOWPOS_UNDEFINED;
		u32InitialYPos = SDL_WINDOWPOS_UNDEFINED;
	}
	else
	{
		u32InitialXPos = (uint32_t) ((double) u32InitialXPos * dXPhysicalScale);
		u32InitialYPos = (uint32_t) ((double) u32InitialYPos * dYPhysicalScale);
	}

	// Now create an OpenGL window/full screen
	sg_psSDLWindow = SDL_CreateWindow(PlatformGetClientName(),
									  u32InitialXPos,
									  u32InitialYPos,
									  sg_u32PhysicalXSize,
									  sg_u32PhysicalYSize,
									  u32WindowFlags);
	if (NULL == sg_psSDLWindow)
	{
		SyslogFunc("Failed to create SDL window - %s\n", SDL_GetError());
		goto errorExit;
	}

	if (sg_bFullscreen)
	{
		SDL_SetWindowFullscreen(sg_psSDLWindow,
								u32WindowFlags);
	}

	sg_sGLContext = SDL_GL_CreateContext(sg_psSDLWindow);
	BASSERT(sg_sGLContext);

	// Use Vsync
	if (SDL_GL_SetSwapInterval(1) < 0)
	{
		Syslog("SDL_GL_SetSwapInterval(1) not supported - %s\n", SDL_GetError());
	}

    //Initialize Modelview Matrix
    glMatrixMode(GL_PROJECTION);

    //Check for error
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
		BASSERT(0);
    }

	//Initialize clear color
    glClearColor( 0.f, 0.f, 0.f, 1.f );

    //Check for error
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
		BASSERT(0);
    }

	glEnable(GL_TEXTURE_2D);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	glOrtho(0, sg_u32VirtualXSize - 1, 
			sg_u32VirtualYSize, 0, 
			0, 32768.0); 

	// Make sure the frame rate is nonzero
	BASSERT(u32FPS);
	*pu32FPS = u32FPS;

	// Register
	eStatus = UtilTaskRegister("Graphics position",
							   UTILTASK_GRAPHICS_KEY,
							   GraphicsPositionChangeCallback);
	ERR_GOTO();


errorExit:
	return(eStatus);
}

// Shuts down the entire graphics subsystem
EStatus GraphicsShutdown(void)
{
	SDL_Quit();
	return(ESTATUS_OK);
}
