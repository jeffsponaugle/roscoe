#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

// For OpenGL stuff
#include "Shared/libsdl/libsdlsrc/include/SDL.h"
#include "Shared/libsdl/libsdlsrc/include/SDL_opengl.h"

#include <GL/glu.h>

// Graphics attributes
#define	GFXATTR_SWAP_HORIZONAL			0x00000001
#define GFXATTR_SWAP_VERTICAL			0x00000002

// Make an RGBA
#define MAKERGBA(r, g, b, a)	((((UINT8) a)<<24)|(((UINT8) b)<<16)|(((UINT8) g)<<8)|((UINT8) r))
#define	PIXEL_TRANSPARENT		0x00		// Completely transparent pixel
#define	PIXEL_OPAQUE			0xff		// Completely solid pixel

// Topic name for graphical position/info
#define	GRAPHICS_POSINFO_TOPIC	"WindowInfo"

// Basic graphics image
typedef struct SGraphicsImage
{
	// OpenGL texture handle for this image
	BOOL bOpenGLTexture;			// TRUE If a texture handle has been allocated
	BOOL bGreyscale;				// TRUE If the image should be displayed in black and white
	GLuint eTextureHandle;

	// Intensity of image (0 black, 255=full)
	UINT8 u8Intensity;

	// Attributes
	UINT32 u32Attribute;

	// X/Y Total image size
	UINT32 u32TotalXSize;
	UINT32 u32TotalYSize;

	// Y Viewport size (0=Disabled)
	UINT32 u32ViewportYSize;

	// Y Viewport offset (ignored if u32ViewportYSize is 0)
	UINT32 u32ViewportYOffset;

	// Pixel data
	UINT32 *pu32RGBA;
	BOOL bAutoDeallocate;		// TRUE If the image should be deallocated when destroyed

	// Runtime data
	BOOL bTextureAssigned;		// FALSE If the texture hasn't been assigned yet

	// Prior viewport data
	UINT32 u32ViewportYSizePrior;
	UINT32 u32ViewportYOffsetPrior;
} SGraphicsImage;

extern EStatus GraphicsLoadImage(char *peImageFilename,
								 UINT32 **ppu32RGBA,
								 UINT32 *pu32XSize,
								 UINT32 *pu32YSize);
extern EStatus GraphicsCreateImage(SGraphicsImage **ppsGraphicsImage,
								   BOOL bOpenGLTexture,
								   UINT32 u32XSize,
								   UINT32 u32YSize);
extern EStatus GraphicsAllocateImage(SGraphicsImage *psGraphicsImage,
									 UINT32 u32XSize,
									 UINT32 u32YSize);
extern void GraphicsClearImage(SGraphicsImage *psGraphicsImage);
extern void GraphicsFillImage(SGraphicsImage *psGraphicsImage,
							  UINT32 u32RGBAFillColor);
extern void GraphicsFillImageRegion(SGraphicsImage *psGraphicsImage,
									 INT32 s32XPos,
									 INT32 s32YPos,
									 UINT32 u32XSize,
									 UINT32 u32YSize,
									 UINT32 u32RGBAFillColor);
extern void GraphicsDestroyImage(SGraphicsImage **ppsGraphicsImage);
extern EStatus GraphicsDraw(SGraphicsImage *psGraphicsImage,
							INT32 s32X,
							INT32 s32Y,
							BOOL bOriginCenter,
							double dAngle);
extern void GraphicsDrawQuad(INT32 s32X,
							 INT32 s32Y,
							 UINT32 u32XSize,
							 UINT32 u32YSize,
							 UINT32 u32RGBA);
extern void GraphicsSetAttributes(SGraphicsImage *psGraphicsImage,
								  UINT32 u32Attributes,
								  UINT32 u32AttributeMask);
extern void GraphicsSetViewportSize(SGraphicsImage *psGraphicsImage,
									UINT32 u32ViewportYSize);
extern void GraphicsSetViewportOffset(SGraphicsImage *psGraphicsImage,
									  UINT32 u32ViewportYOffset);
extern void GraphicsSetIntensity(SGraphicsImage *psGraphicsImage,
								 UINT8 u8Intensity);
extern void GraphicsSetImage(SGraphicsImage *psGraphicsImage,
							 UINT32 *pu32RGBA,
							 UINT32 u32XSize,
							 UINT32 u32YSize,
							 BOOL bAutoDeallocate);
extern void GraphicsImageUpdated(SGraphicsImage *psGraphicsImage);
extern void GraphicsGetVirtualSurfaceSize(UINT32 *pu32VirtualXSize,
										  UINT32 *pu32VirtualYSize);
extern void GraphicsGetPhysicalSurfaceSize(UINT32 *pu32PhysicalXSize,
										   UINT32 *pu32PhysicalYSize);
extern void GraphicsSetGreyscale(SGraphicsImage *psGraphicsImage,
								 BOOL bGreyscale);
extern void GraphicsWindowSetPosition(UINT32 u32XPos,
									  UINT32 u32YPos);
extern EStatus GraphicsSignalPositionUpdate(void);
extern void GraphicsSetMinimize(void);
extern void GraphicsSetMaximize(void);
extern BOOL GraphicsIsFullscreen(void);
extern void GraphicsStartFrame(void);
extern void GraphicsEndFrame(void);
extern EStatus GraphicsInit(UINT32 *pu32FPS);
extern void GraphicsGLSetCurrent(void);
extern EStatus GraphicsShutdown(void);

#endif