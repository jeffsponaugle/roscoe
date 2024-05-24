#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <machine/endian.h>
#include "BIOS/OS.h"
#include "Hardware/Roscoe.h"
#include "Shared/Shared.h"
#include "Shared/Interrupt.h"
#include "Shared/AsmUtils.h"

// Interrupt mask register memory mirror
static uint8_t sg_u8InterruptMask = 0xff;
static uint8_t sg_u8InterruptMask2 = 0xff;

typedef struct SInterruptDefinition
{
	uint8_t u8InterruptVector;	   		// Which vector is this?
	volatile uint8_t *pu8MaskAddress;	// Mask register address
	uint8_t *pu8InterruptMaskMirror;	// Interrupt mask in-memory mirror
	uint8_t u8MaskValue;				// Which bit mask to control this IRQ?
} SInterruptDefinition;

#define	VECTORDEF(x)		((volatile uint8_t *) (x))

// Table of all interrupt sources and their vectors
static const SInterruptDefinition sg_sInterrupts[] =
{
	{INTVECT_IRQL7_DEBUGGER,	VECTORDEF(ROSCOE_INTC_MASK),	&sg_u8InterruptMask,	(1 << 7)},
	{INTVECT_IRQ6A_PTC1,		VECTORDEF(ROSCOE_INTC_MASK),	&sg_u8InterruptMask,    (1 << 6)}, 
	{INTVECT_IRQ6B_PTC2,		VECTORDEF(ROSCOE_INTC_MASK),	&sg_u8InterruptMask,    (1 << 5)}, 
	{INTVECT_IRQ5A_NIC,			VECTORDEF(ROSCOE_INTC_MASK),	&sg_u8InterruptMask,    (1 << 4)}, 
	{INTVECT_IRQ5B_IDE1,		VECTORDEF(ROSCOE_INTC_MASK),	&sg_u8InterruptMask,    (1 << 3)}, 
	{INTVECT_IRQ5C_IDE2,		VECTORDEF(ROSCOE_INTC_MASK),	&sg_u8InterruptMask,    (1 << 2)},
	{INTVECT_IRQ5D_EXPANSION_I5, VECTORDEF(ROSCOE_INTC_MASK),	&sg_u8InterruptMask,    (1 << 1)},
	{INTVECT_IRQ4A_UART1,       VECTORDEF(ROSCOE_INTC_MASK),    &sg_u8InterruptMask,    (1 << 0)},
	{INTVECT_IRQ4B_UART2,		VECTORDEF(ROSCOE_INTC_MASK2),	&sg_u8InterruptMask2,	(1 << 7)},
	{INTVECT_IRQ4C_EXPANSION_T4, VECTORDEF(ROSCOE_INTC_MASK2),	&sg_u8InterruptMask2,	(1 << 6)},
	{INTVECT_IRQ3A_USB,			VECTORDEF(ROSCOE_INTC_MASK2),   &sg_u8InterruptMask2,   (1 << 5)},
	{INTVECT_IRQ3B_EXPANSION_T3, VECTORDEF(ROSCOE_INTC_MASK2),   &sg_u8InterruptMask2,   (1 << 4)},
	{INTVECT_IRQ2A_VIDEO,		VECTORDEF(ROSCOE_INTC_MASK2),   &sg_u8InterruptMask2,   (1 << 3)},
	{INTVECT_IRQ2B_EXPANSION_T2, VECTORDEF(ROSCOE_INTC_MASK2),   &sg_u8InterruptMask2,   (1 << 2)},
	{INTVECT_IRQ1A_RTC,			VECTORDEF(ROSCOE_INTC_MASK2),   &sg_u8InterruptMask2,   (1 << 1)},
	{INTVECT_IRQ1B_POWER,		VECTORDEF(ROSCOE_INTC_MASK2),   &sg_u8InterruptMask2,   (1 << 0)}
};

// Find the interrupt definition by vector
static const SInterruptDefinition *InterruptDefinitionGetByVector(uint8_t u8InterruptVector)
{
	uint8_t u8Loop;
	const SInterruptDefinition *psInterruptDefinition = sg_sInterrupts;

	for (u8Loop = 0; u8Loop < (sizeof(sg_sInterrupts) / sizeof(sg_sInterrupts[0])); u8Loop++)
	{
		if (psInterruptDefinition->u8InterruptVector == u8InterruptVector)
		{
			return(psInterruptDefinition);
		}
		++psInterruptDefinition;
	}

	return(NULL);
}

// Default handler for handling interrupts when they are enabled but no vector has been hooked
static __attribute__ ((interrupt)) void InterruptHandlerDefault(void) 
{
	POST_SET(((POST_7SEG_ALPHA_U << 8) + POST_7SEG_ALPHA_U));
}

// Install an interrupt handler for a particular vector
EStatus InterruptHook(uint8_t u8InterruptVector,
					  void (*InterruptHandler)(void))
{
	EStatus eStatus = ESTATUS_OK;
	const SInterruptDefinition *psInterruptDefinition;

	psInterruptDefinition = InterruptDefinitionGetByVector(u8InterruptVector);
	if (NULL == psInterruptDefinition)
	{
		eStatus = ESTATUS_INTERRUPT_VECTOR_UNKNOWN;
		goto errorExit;
	}

	*((uint32_t *) (psInterruptDefinition->u8InterruptVector << 2)) = (uint32_t) InterruptHandler;

errorExit:
	return(eStatus);
}

// Set/clear interrupt mask
EStatus InterruptMaskSet(uint8_t u8InterruptVector,
						 bool bMaskInterrupt)
{
	EStatus eStatus = ESTATUS_OK;
	const SInterruptDefinition *psInterruptDefinition;

	psInterruptDefinition = InterruptDefinitionGetByVector(u8InterruptVector);
	if (NULL == psInterruptDefinition)
	{
		eStatus = ESTATUS_INTERRUPT_VECTOR_UNKNOWN;
		goto errorExit;
	}

	// Need to disable interrupts since we're doing a read/modify/write
	InterruptDisable();

	// We're either masking or unmasking it. Update the mirror first.
	if (bMaskInterrupt)
	{
		*psInterruptDefinition->pu8InterruptMaskMirror |= psInterruptDefinition->u8MaskValue;
	}
	else
	{
		*psInterruptDefinition->pu8InterruptMaskMirror &= (uint8_t) (~psInterruptDefinition->u8MaskValue);
	}

	// Set the mask
	*psInterruptDefinition->pu8MaskAddress = *psInterruptDefinition->pu8InterruptMaskMirror;

	// Reenable interrupts
	InterruptEnable();

errorExit:
	return(eStatus);
}

// Initializeds interrupt subsystem
void InterruptInit(void)
{
	const SInterruptDefinition *psInterruptDefinition = sg_sInterrupts;
	uint8_t u8Loop;

	// Shut off all interrupts
	InterruptDisable();

	// Set the memory mirror for the interrupt mask registers
	sg_u8InterruptMask = 0xff;
	sg_u8InterruptMask2 = 0xff;

	// Mask all interrupts
	*VECTORDEF(ROSCOE_INTC_MASK) = 0xff;
	*VECTORDEF(ROSCOE_INTC_MASK2) = 0xff;

	// Install the default handlers in all vectors
	for (u8Loop = 0; u8Loop < (sizeof(sg_sInterrupts) / sizeof(sg_sInterrupts[0])); u8Loop++)
	{
		*((uint32_t *) (psInterruptDefinition->u8InterruptVector << 2)) = (uint32_t) InterruptHandlerDefault;
		++psInterruptDefinition;
	}

	// Enable all processor interrupts
	InterruptEnable();
}

