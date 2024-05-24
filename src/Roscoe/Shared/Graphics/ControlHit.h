#ifndef _CONTROLHIT_H_
#define _CONTROLHIT_H_

#include "Shared/HandlePool.h"

typedef EHandleGeneric EControlHitHandle;

extern EStatus ControlHitInit(void);
extern EStatus ControlHitCreate(EWindowHandle eWindowHandle,
								EControlHitHandle *peHitHandle,
								INT32 s32XPos,
								INT32 s32YPos,
								UINT32 u32XSize,
								UINT32 u32YSize);
extern EStatus ControlHitSetCallback(EControlHitHandle eHitHandle,
									 void (*HitCallback)(EControlHitHandle eHitHandle,
														 EControlCallbackReason eReason,
														 UINT32 u32ReasonData,
														 INT32 s32ControlRelativeXPos,
														 INT32 s32ControlRelativeYPos));


#endif
