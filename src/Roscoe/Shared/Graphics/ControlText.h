#ifndef _CONTROLTEXT_H_
#define _CONTROLTEXT_H_

#include "Shared/HandlePool.h"
#include "Shared/Graphics/Font.h"

typedef EHandleGeneric EControlTextHandle;

extern EStatus ControlTextCreate(EWindowHandle eWindowHandle,
								 EControlTextHandle *peControlTextHandle,
								 INT32 s32XPos,
								 INT32 s32YPos);
extern EStatus ControlTextSet(EControlTextHandle eControlTextHandle,
							  EFontHandle eFont,
							  UINT16 u16FontSize,
							  char *peString,
							  BOOL bSetColor,
							  UINT32 u32RGBAColor);
extern EStatus ControlTextInit(void);

#endif
