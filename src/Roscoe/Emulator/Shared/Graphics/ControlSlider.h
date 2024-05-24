#ifndef _CONTROLSLIDER_H_
#define _CONTROLSLIDER_H_

typedef EHandleGeneric EControlSliderHandle;

typedef struct SControlSliderConfig
{
	uint32_t u32TrackLength;
	uint32_t *pu32RGBATrack;
	uint32_t u32XSizeTrack;
	uint32_t u32YSizeTrack;
	uint32_t *pu32RGBAThumb;
	uint32_t u32XSizeThumb;
	uint32_t u32YSizeThumb;
	bool bAutoDeallocate;
} SControlSliderConfig;

extern EStatus ControlSliderCreate(EWindowHandle eWindowHandle,
								   EControlSliderHandle *peControlSliderHandle,
								   int32_t s32XPos,
								   int32_t s32YPos);
extern EStatus ControlSliderSetAssets(EControlSliderHandle eControlSliderHandle,
								 	  SControlSliderConfig *psControlSlierConfig);
extern EStatus ControlSliderSetRange(EControlSliderHandle eControlSliderHandle,
									 int32_t s32Low,
									 int32_t s32High);
extern EStatus ControlSliderSetValue(EControlSliderHandle eControlSliderHandle,
									 int32_t s32Value);
extern EStatus ControlSliderGetValue(EControlSliderHandle eControlSliderHandle,
									 int32_t *ps32Value);
extern EStatus ControlSliderSetCallback(EControlSliderHandle eControlSliderHandle,
										void *pvUserData,
										void (*SliderCallback)(EControlSliderHandle eControlSliderHandle,
															   void *pvUserData,
															   bool bDrag,
															   bool bSet,
															   int32_t s32Value));
extern EStatus ControlSliderInit(void);


#endif
