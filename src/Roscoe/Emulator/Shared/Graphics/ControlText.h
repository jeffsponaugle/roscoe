#ifndef _CONTROLTEXT_H_
#define _CONTROLTEXT_H_

#include "Shared/HandlePool.h"
#include "Shared/Graphics/Font.h"

typedef EHandleGeneric EControlTextHandle;

extern EStatus ControlTextCreate(EWindowHandle eWindowHandle,
								 EControlTextHandle *peControlTextHandle,
								 int32_t s32XPos,
								 int32_t s32YPos);
extern EStatus ControlTextSet(EControlTextHandle eControlTextHandle,
							  EFontHandle eFont,
							  uint16_t u16FontSize,
							  char *peString,
							  bool bSetColor,
							  uint32_t u32RGBAColor);
extern EStatus ControlTextInit(void);

#endif
