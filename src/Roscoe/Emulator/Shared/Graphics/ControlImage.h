#ifndef _CONTROLIMAGE_H_
#define _CONTROLIMAGE_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlImageHandle;

extern EStatus ControlImageCreate(EWindowHandle eWindowHandle,
								  EControlImageHandle *peControlImageHandle,
								  int32_t s32XPos,
								  int32_t s32YPos);
extern EStatus ControlImageSetFromFile(EControlImageHandle eControlImageHandle,
									   char *peImageFilename);
extern EStatus ControlImageSetAssets(EControlImageHandle eControlImageHandle,
									 uint32_t *pu32RGBAImage,
									 uint32_t u32XSize,
									 uint32_t u32YSize,
									 bool bAutoDeallocate);
extern EStatus ControlImageInit(void);

#endif