#ifndef _SBRK_H_
#define _SBRK_H_

extern void SBRKHeapReset(void);
extern void SBRKGetInfo(uint32_t *pu32HeapEnd,
						uint32_t *pu32LowestStack,
						uint32_t *pu32StackPointer);

#endif

