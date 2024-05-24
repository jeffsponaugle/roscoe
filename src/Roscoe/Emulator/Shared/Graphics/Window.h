#ifndef _WINDOW_H_
#define _WINDOW_H_

#include "Shared/HandlePool.h"
#include "Shared/libsdl/libsdlsrc/include/SDL.h"

typedef EHandleGeneric EWindowHandle;

// Mouse buttons
#define	WINDOW_MOUSEBUTTON_LEFT		0x01
#define	WINDOW_MOUSEBUTTON_MIDDLE	0x02
#define	WINDOW_MOUSEBUTTON_RIGHT	0x04
#define	WINDOW_MOUSEBUTTON_X1		0x08
#define	WINDOW_MOUSEBUTTON_X2		0x10

// Internal window library APIs
extern EStatus WindowInit(void);
extern EStatus WindowShutdown(void);
extern void WindowUpdatedInternal(char *peModule, uint32_t u32LineNumber);
#define	WindowUpdated()	WindowUpdatedInternal((char *) __FILE__, (uint32_t) __LINE__);
extern void WindowSetFrameLock(bool bLock);
extern bool WindowScancodeGetState(SDL_Scancode eScancode);
extern EStatus WindowRemoveControl(EWindowHandle eWindowHandle,
								   EHandleGeneric eControlHandle);
struct SControl;
extern EStatus WindowControlCreate(EWindowHandle eWindowHandle,
								   struct SControl *pvControlStructure,
								   void *pvConfigurationStructure);
extern EStatus WindowControlConfigure(EWindowHandle eWindowHandle,
									  void *pvConfigurationStructure);
extern EStatus WindowControlDestroy(EHandleGeneric *peControlHandle);

// External window library APIs
extern void WindowGetVirtualSize(uint32_t *pu32XSize,
								 uint32_t *pu32YSize);
extern bool WindowHandleIsValid(EWindowHandle eWindowHandle);
extern EStatus WindowCreate(EWindowHandle *peWindowHandle,
							int32_t s32XPos,
							int32_t s32YPos,
							uint32_t u32XSize,
							uint32_t u32YSize);
extern EStatus WindowDestroy(EWindowHandle *peWindowHandle);
extern EStatus WindowSetImageFromFile(EWindowHandle eWindowHandle,
									  char *peWindowImageFilename);
extern EStatus WindowSetImage(EWindowHandle eWindowHandle,
							  uint32_t *pu32RGBA,
							  uint32_t u32XSize,
							  uint32_t u32YSize,
							  bool bAutoDeallocate);
extern EStatus WindowSetVisible(EWindowHandle eWindowHandle,
								bool bVisible);
extern EStatus WindowSetDisabled(EWindowHandle eWindowHandle,
								 bool bDisabled);
extern EStatus WindowSetPosition(EWindowHandle eWindowHandle,
								 int32_t s32XPos,
								 int32_t s32YPos);
extern EStatus WindowControlSetFocus(EWindowHandle eWindowHandle,
									 EHandleGeneric eControlHandle);
extern EStatus WindowSetModal(EWindowHandle eWindowHandle,
							  bool bModal);
extern EStatus WindowSetFocus(EWindowHandle eWindowHandle);
extern void WindowShutdownSetCallback(void (*Callback)(void));
extern void WindowWaitFrame(void);
extern void WindowSetMinimize(void);
extern void WindowSetMaximize(void);
extern EStatus WindowRenderStretch(uint32_t *pu32RGBACornerImage,
								   uint32_t u32CornerXSize,
								   uint32_t u32CornerYSize,
								   uint32_t *pu32RGBAEdgeImage,
								   uint32_t u32EdgeXSize,
								   uint32_t u32EdgeYSize,
								   uint32_t u32RGBAFillPixel,
								   uint32_t u32WindowStretchXSize,
								   uint32_t u32WindowStretchYSize,
								   uint32_t **ppu32RGBAWindowStretchImage);

#endif