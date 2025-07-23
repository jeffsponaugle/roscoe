#ifndef _CONTROLBUTTON_H_
#define _CONTROLBUTTON_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlButtonHandle;

// Bit set in u32ReasonData for button callback indicating a press vs. release
#define	BUTTON_PRESSED		0x80000000

typedef struct SControlButtonConfig
{
	BOOL bHitIgnoreTransparent;
	UINT32 *pu32RGBAButtonNotPressed;
	UINT32 u32XSizeNotPressed;
	UINT32 u32YSizeNotPressed;
	UINT32 *pu32RGBAButtonPressed;
	UINT32 u32XSizePressed;
	UINT32 u32YSizePressed;
	BOOL bAutoDeallocate;
} SControlButtonConfig;

extern EStatus ControlButtonInit(void);
extern EStatus ControlButtonCreate(EWindowHandle eWindowHandle,
								   EControlButtonHandle *peButtonHandle,
								   INT32 s32XPos,
								   INT32 s32YPos);
extern EStatus ControlButtonSetAssets(EControlButtonHandle eButtonHandle,
									  SControlButtonConfig *psControlButtonConfig);
extern EStatus ControlButtonSetText(EControlButtonHandle eButtonHandle,
									INT32 s32TextXOffsetNonPressed,
									INT32 s32TextYOffsetNonPressed,
									INT32 s32TextXOffsetPressed,
									INT32 s32TextYOffsetPressed,
									BOOL bCenterOrigin,
									EFontHandle eFont,
									UINT16 u16FontSize,
									char *peString,
									BOOL bSetColor,
									UINT32 u32RGBAColor);
extern EStatus ControlButtonSetCallback(EControlButtonHandle eButtonHandle,
										void (*ButtonCallback)(EControlButtonHandle eButtonHandle,
															   EControlCallbackReason eReason,
															   UINT32 u32ReasonData,
															   INT32 s32ControlRelativeXPos,
															   INT32 s32ControlRelativeYPos));
extern EStatus ControlButtonSetKeyboardTriggerable(EControlButtonHandle eButtonHandle,
												   BOOL bKeyboardTriggerable);

#endif
