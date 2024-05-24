#ifndef _CONTROLHIT_H_
#define _CONTROLHIT_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlHitHandle;

extern EStatus ControlHitInit(void);
extern EStatus ControlHitCreate(EWindowHandle eWindowHandle,
								EControlHitHandle *peHitHandle,
								int32_t s32XPos,
								int32_t s32YPos,
								uint32_t u32XSize,
								uint32_t u32YSize);
extern EStatus ControlHitSetCallback(EControlHitHandle eHitHandle,
									 void (*HitCallback)(EControlHitHandle eHitHandle,
														 EControlCallbackReason eReason,
														 uint32_t u32ReasonData,
														 int32_t s32ControlRelativeXPos,
														 int32_t s32ControlRelativeYPos));


#endif
