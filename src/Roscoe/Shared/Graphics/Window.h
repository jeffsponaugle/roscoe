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
extern void WindowUpdated(void);
extern void WindowSetFrameLock(BOOL bLock);
extern BOOL WindowScancodeGetState(SDL_Scancode eScancode);
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
extern void WindowGetVirtualSize(UINT32 *pu32XSize,
								 UINT32 *pu32YSize);
extern BOOL WindowHandleIsValid(EWindowHandle eWindowHandle);
extern EStatus WindowCreate(EWindowHandle *peWindowHandle,
							INT32 s32XPos,
							INT32 s32YPos,
							UINT32 u32XSize,
							UINT32 u32YSize);
extern EStatus WindowDestroy(EWindowHandle *peWindowHandle);
extern EStatus WindowSetImageFromFile(EWindowHandle eWindowHandle,
									  char *peWindowImageFilename);
extern EStatus WindowSetImage(EWindowHandle eWindowHandle,
							  UINT32 *pu32RGBA,
							  UINT32 u32XSize,
							  UINT32 u32YSize,
							  BOOL bAutoDeallocate);
extern EStatus WindowSetVisible(EWindowHandle eWindowHandle,
								BOOL bVisible);
extern EStatus WindowSetDisabled(EWindowHandle eWindowHandle,
								 BOOL bDisabled);
extern EStatus WindowSetPosition(EWindowHandle eWindowHandle,
								 INT32 s32XPos,
								 INT32 s32YPos);
extern EStatus WindowControlSetFocus(EWindowHandle eWindowHandle,
									 EHandleGeneric eControlHandle);
extern EStatus WindowSetModal(EWindowHandle eWindowHandle,
							  BOOL bModal);
extern EStatus WindowSetFocus(EWindowHandle eWindowHandle);
extern void WindowShutdownSetCallback(void (*Callback)(void));
extern void WindowWaitFrame(void);
extern void WindowSetMinimize(void);
extern void WindowSetMaximize(void);
extern EStatus WindowRenderStretch(UINT32 *pu32RGBACornerImage,
								   UINT32 u32CornerXSize,
								   UINT32 u32CornerYSize,
								   UINT32 *pu32RGBAEdgeImage,
								   UINT32 u32EdgeXSize,
								   UINT32 u32EdgeYSize,
								   UINT32 u32RGBAFillPixel,
								   UINT32 u32WindowStretchXSize,
								   UINT32 u32WindowStretchYSize,
								   UINT32 **ppu32RGBAWindowStretchImage);

#endif