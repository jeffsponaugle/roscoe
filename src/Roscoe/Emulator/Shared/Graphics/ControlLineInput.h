#ifndef _CONTROLLINEINPUT_H_
#define _CONTROLLINEINPUT_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlLineInputHandle;

extern EStatus ControlLineInputCreate(EWindowHandle eWindowHandle,
									  EControlLineInputHandle *peLineInputHandle,
									  int32_t s32XPos,
									  int32_t s32YPos,
									  uint32_t u32XSize,
									  uint32_t u32YSize);
extern EStatus ControlLineInputSetAssets(EControlLineInputHandle eLineInputHandle,
										 EFontHandle eFont,
										 uint16_t u16FontSize,
										 uint32_t u32RGBATextColor);
extern EStatus ControlLineInputSetText(EControlLineInputHandle eLineInputHandle,
									   char *peTextInput,
									   uint32_t u32MaxLengthChars);
extern EStatus ControlLineInputGetText(EControlLineInputHandle eLineInputHandle,
									   char *peTextBuffer,
									   uint32_t u32MaxLengthBytes,
									   uint32_t *pu32LengthInBytes,
									   uint32_t *pu32LengthInCharacters);
extern EStatus ControlLineInputSetPasswordEntry(EControlLineInputHandle eLineInputHandle,
												bool bPasswordEntry,
												EUnicode ePasswordChar);
extern EStatus ControlLineInputSetCallback(EControlLineInputHandle eLineInputHandle,
										   void *pvUserData,
										   void (*Callback)(EControlLineInputHandle eLineInputHandle,
															void *pvUserData,
															EControlCallbackReason eLineInputCallbackReason,
															char *peText,
															uint32_t u32CharCount,
															uint32_t u32CharSizeBytes));
extern EStatus ControlLineInputSetCursor(EControlLineInputHandle eLineInputHandle,
										 uint32_t u32BlinkRateMs,
										 int32_t s32CursorXOffset,
										 int32_t s32CursorYOffset,
										 uint32_t u32CursorXSize,
										 uint32_t u32CursorYSize,
										 uint32_t u32CursorRGBA);
extern EStatus ControlLineInputSetCursorOverride(EControlLineInputHandle eLineInputHandle,
												 bool bOverride);

extern EStatus ControlLineInputInit(void);

#endif
