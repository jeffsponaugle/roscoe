#ifndef _CONTROLLINEINPUT_H_
#define _CONTROLLINEINPUT_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlLineInputHandle;

extern EStatus ControlLineInputCreate(EWindowHandle eWindowHandle,
									  EControlLineInputHandle *peLineInputHandle,
									  INT32 s32XPos,
									  INT32 s32YPos,
									  UINT32 u32XSize,
									  UINT32 u32YSize);
extern EStatus ControlLineInputSetAssets(EControlLineInputHandle eLineInputHandle,
										 EFontHandle eFont,
										 UINT16 u16FontSize,
										 UINT32 u32RGBATextColor);
extern EStatus ControlLineInputSetText(EControlLineInputHandle eLineInputHandle,
									   char *peTextInput,
									   UINT32 u32MaxLengthChars);
extern EStatus ControlLineInputGetText(EControlLineInputHandle eLineInputHandle,
									   char *peTextBuffer,
									   UINT32 u32MaxLengthBytes,
									   UINT32 *pu32LengthInBytes,
									   UINT32 *pu32LengthInCharacters);
extern EStatus ControlLineInputSetPasswordEntry(EControlLineInputHandle eLineInputHandle,
												BOOL bPasswordEntry,
												EUnicode ePasswordChar);
extern EStatus ControlLineInputSetCallback(EControlLineInputHandle eLineInputHandle,
										   void *pvUserData,
										   void (*Callback)(EControlLineInputHandle eLineInputHandle,
															void *pvUserData,
															EControlCallbackReason eLineInputCallbackReason,
															char *peText,
															UINT32 u32CharCount,
															UINT32 u32CharSizeBytes));
extern EStatus ControlLineInputSetCursor(EControlLineInputHandle eLineInputHandle,
										 UINT32 u32BlinkRateMs,
										 INT32 s32CursorXOffset,
										 INT32 s32CursorYOffset,
										 UINT32 u32CursorXSize,
										 UINT32 u32CursorYSize,
										 UINT32 u32CursorRGBA);
extern EStatus ControlLineInputSetCursorOverride(EControlLineInputHandle eLineInputHandle,
												 BOOL bOverride);

extern EStatus ControlLineInputInit(void);

#endif
