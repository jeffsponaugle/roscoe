#ifndef _CONTROLSLIDER_H_
#define _CONTROLSLIDER_H_

typedef EHandleGeneric EControlSliderHandle;

typedef struct SControlSliderConfig
{
	UINT32 u32TrackLength;
	UINT32 *pu32RGBATrack;
	UINT32 u32XSizeTrack;
	UINT32 u32YSizeTrack;
	UINT32 *pu32RGBAThumb;
	UINT32 u32XSizeThumb;
	UINT32 u32YSizeThumb;
	BOOL bAutoDeallocate;
} SControlSliderConfig;

extern EStatus ControlSliderCreate(EWindowHandle eWindowHandle,
								   EControlSliderHandle *peControlSliderHandle,
								   INT32 s32XPos,
								   INT32 s32YPos);
extern EStatus ControlSliderSetAssets(EControlSliderHandle eControlSliderHandle,
								 	  SControlSliderConfig *psControlSlierConfig);
extern EStatus ControlSliderSetRange(EControlSliderHandle eControlSliderHandle,
									 INT32 s32Low,
									 INT32 s32High);
extern EStatus ControlSliderSetValue(EControlSliderHandle eControlSliderHandle,
									 INT32 s32Value);
extern EStatus ControlSliderGetValue(EControlSliderHandle eControlSliderHandle,
									 INT32 *ps32Value);
extern EStatus ControlSliderSetCallback(EControlSliderHandle eControlSliderHandle,
										void *pvUserData,
										void (*SliderCallback)(EControlSliderHandle eControlSliderHandle,
															   void *pvUserData,
															   BOOL bDrag,
															   BOOL bSet,
															   INT32 s32Value));
extern EStatus ControlSliderInit(void);


#endif
