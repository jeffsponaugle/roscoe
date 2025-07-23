#ifndef _INTERRUPT_H_
#define _ITNERRUPT_H_

#include "Shared/AsmUtils.h"

// IPL Mask in SR register
#define	SR_IPL_BIT		8
#define	SR_IPL_MASK		0x0700

// Interrupt enable/disable macros
#define	InterruptDisable()	SRSet(SRGet() | SR_IPL_MASK);
#define InterruptEnable()	SRSet(SRGet() & ((uint16_t) ~SR_IPL_MASK));

extern void InterruptInit(void);
extern EStatus InterruptHook(uint8_t u8InterruptVector,
							 void (*InterruptHandler)(void));
extern EStatus InterruptMaskSet(uint8_t u8InterruptVector,
								bool bMaskInterrupt);


#endif

