#ifndef _CONTROLIMAGE_H_
#define _CONTROLIMAGE_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlImageHandle;

extern EStatus ControlImageCreate(EWindowHandle eWindowHandle,
								  EControlImageHandle *peControlImageHandle,
								  INT32 s32XPos,
								  INT32 s32YPos);
extern EStatus ControlImageSetFromFile(EControlImageHandle eControlImageHandle,
									   char *peImageFilename);
extern EStatus ControlImageSetAssets(EControlImageHandle eControlImageHandle,
									 UINT32 *pu32RGBAImage,
									 UINT32 u32XSize,
									 UINT32 u32YSize,
									 BOOL bAutoDeallocate);
extern EStatus ControlImageInit(void);

#endif