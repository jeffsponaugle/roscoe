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
#define MAKERGBA(r, g, b, a)	((((uint8_t) a)<<24)|(((uint8_t) b)<<16)|(((uint8_t) g)<<8)|((uint8_t) r))
#define	PIXEL_TRANSPARENT		0x00		// Completely transparent pixel
#define	PIXEL_OPAQUE			0xff		// Completely solid pixel

// Topic name for graphical position/info
#define	GRAPHICS_POSINFO_TOPIC	"WindowInfo"

// Basic graphics image
typedef struct SGraphicsImage
{
	// OpenGL texture handle for this image
	bool bOpenGLTexture;			// true If a texture handle has been allocated
	bool bGreyscale;				// true If the image should be displayed in black and white
	GLuint eTextureHandle;

	// Intensity of image (0 black, 255=full)
	uint8_t u8Intensity;

	// Attributes
	uint32_t u32Attribute;

	// X/Y Total image size
	uint32_t u32TotalXSize;
	uint32_t u32TotalYSize;

	// Y Viewport size (0=Disabled)
	uint32_t u32ViewportYSize;

	// Y Viewport offset (ignored if u32ViewportYSize is 0)
	uint32_t u32ViewportYOffset;

	// Pixel data
	uint32_t *pu32RGBA;
	bool bAutoDeallocate;		// true If the image should be deallocated when destroyed

	// Runtime data
	bool bTextureAssigned;		// false If the texture hasn't been assigned yet

	// Prior viewport data
	uint32_t u32ViewportYSizePrior;
	uint32_t u32ViewportYOffsetPrior;
} SGraphicsImage;

extern EStatus GraphicsLoadImage(char *peImageFilename,
								 uint32_t **ppu32RGBA,
								 uint32_t *pu32XSize,
								 uint32_t *pu32YSize);
extern EStatus GraphicsCreateImage(SGraphicsImage **ppsGraphicsImage,
								   bool bOpenGLTexture,
								   uint32_t u32XSize,
								   uint32_t u32YSize);
extern EStatus GraphicsAllocateImage(SGraphicsImage *psGraphicsImage,
									 uint32_t u32XSize,
									 uint32_t u32YSize);
extern void GraphicsClearImage(SGraphicsImage *psGraphicsImage);
extern void GraphicsFillImage(SGraphicsImage *psGraphicsImage,
							  uint32_t u32RGBAFillColor);
extern void GraphicsFillImageRegion(SGraphicsImage *psGraphicsImage,
									 int32_t s32XPos,
									 int32_t s32YPos,
									 uint32_t u32XSize,
									 uint32_t u32YSize,
									 uint32_t u32RGBAFillColor);
extern void GraphicsDestroyImage(SGraphicsImage **ppsGraphicsImage);
extern EStatus GraphicsDraw(SGraphicsImage *psGraphicsImage,
							int32_t s32X,
							int32_t s32Y,
							bool bOriginCenter,
							double dAngle);
extern void GraphicsDrawQuad(int32_t s32X,
							 int32_t s32Y,
							 uint32_t u32XSize,
							 uint32_t u32YSize,
							 uint32_t u32RGBA);
extern void GraphicsSetAttributes(SGraphicsImage *psGraphicsImage,
								  uint32_t u32Attributes,
								  uint32_t u32AttributeMask);
extern void GraphicsSetViewportSize(SGraphicsImage *psGraphicsImage,
									uint32_t u32ViewportYSize);
extern void GraphicsSetViewportOffset(SGraphicsImage *psGraphicsImage,
									  uint32_t u32ViewportYOffset);
extern void GraphicsSetIntensity(SGraphicsImage *psGraphicsImage,
								 uint8_t u8Intensity);
extern void GraphicsSetImage(SGraphicsImage *psGraphicsImage,
							 uint32_t *pu32RGBA,
							 uint32_t u32XSize,
							 uint32_t u32YSize,
							 bool bAutoDeallocate);
extern void GraphicsImageUpdated(SGraphicsImage *psGraphicsImage);
extern void GraphicsGetVirtualSurfaceSize(uint32_t *pu32VirtualXSize,
										  uint32_t *pu32VirtualYSize);
extern void GraphicsGetPhysicalSurfaceSize(uint32_t *pu32PhysicalXSize,
										   uint32_t *pu32PhysicalYSize);
extern void GraphicsSetGreyscale(SGraphicsImage *psGraphicsImage,
								 bool bGreyscale);
extern void GraphicsWindowSetPosition(uint32_t u32XPos,
									  uint32_t u32YPos);
extern EStatus GraphicsSignalPositionUpdate(void);
extern void GraphicsSetMinimize(void);
extern void GraphicsSetMaximize(void);
extern bool GraphicsIsFullscreen(void);
extern void GraphicsStartFrame(void);
extern void GraphicsEndFrame(void);
extern EStatus GraphicsInit(uint32_t *pu32FPS);
extern void GraphicsGLSetCurrent(void);
extern EStatus GraphicsShutdown(void);

#endif