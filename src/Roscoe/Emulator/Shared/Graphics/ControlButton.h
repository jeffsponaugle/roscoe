#ifndef _CONTROLBUTTON_H_
#define _CONTROLBUTTON_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlButtonHandle;

// Bit set in u32ReasonData for button callback indicating a press vs. release
#define	BUTTON_PRESSED		0x80000000

typedef struct SControlButtonConfig
{
	bool bHitIgnoreTransparent;
	uint32_t *pu32RGBAButtonNotPressed;
	uint32_t u32XSizeNotPressed;
	uint32_t u32YSizeNotPressed;
	uint32_t *pu32RGBAButtonPressed;
	uint32_t u32XSizePressed;
	uint32_t u32YSizePressed;
	bool bAutoDeallocate;
} SControlButtonConfig;

extern EStatus ControlButtonInit(void);
extern EStatus ControlButtonCreate(EWindowHandle eWindowHandle,
								   EControlButtonHandle *peButtonHandle,
								   int32_t s32XPos,
								   int32_t s32YPos);
extern EStatus ControlButtonSetAssets(EControlButtonHandle eButtonHandle,
									  SControlButtonConfig *psControlButtonConfig);
extern EStatus ControlButtonSetText(EControlButtonHandle eButtonHandle,
									int32_t s32TextXOffsetNonPressed,
									int32_t s32TextYOffsetNonPressed,
									int32_t s32TextXOffsetPressed,
									int32_t s32TextYOffsetPressed,
									bool bCenterOrigin,
									EFontHandle eFont,
									uint16_t u16FontSize,
									char *peString,
									bool bSetColor,
									uint32_t u32RGBAColor);
extern EStatus ControlButtonSetCallback(EControlButtonHandle eButtonHandle,
										void (*ButtonCallback)(EControlButtonHandle eButtonHandle,
															   EControlCallbackReason eReason,
															   uint32_t u32ReasonData,
															   int32_t s32ControlRelativeXPos,
															   int32_t s32ControlRelativeYPos));
extern EStatus ControlButtonSetKeyboardTriggerable(EControlButtonHandle eButtonHandle,
												   bool bKeyboardTriggerable);

#endif
