#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <machine/endian.h>
#include <stdarg.h>
#include "BIOS/OS.h"
#include "Hardware/Roscoe.h"
#include "Shared/Shared.h"
#include "Shared/Interrupt.h"
#include "Shared/AsmUtils.h"
#include "Shared/16550.h"

// Interrupt mask register memory mirror
static uint8_t sg_u8InterruptMask = 0;
static uint8_t sg_u8InterruptMask2 = 0;

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

static const char *sg_peVectorList[] =
{
	"Reset SP",
	"Reset PC",
	"Bus error",
	"Address error",
	"Illegal instruction",
	"Division by zero",
	"CHK Instruction",
	"TRAPV instruction",
	"Privilege violation",
	"Trace"
	"Unimplemented instruction",
	"Unimplemented instruction",
	"Reserved",
	"Reserved",
	"Uninitialized interrupt vector",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Spurious interrupt"
};

#define	FaultOut(x, ...)			FaultOutInternal(NULL, 0, x, __VA_ARGS__)

// Dump an printf style string
static void FaultOutInternal(const char *peProcedureName,
							 uint32_t u32LineNumber,
							 const char *peFormat,
							 ...)
{
	va_list ap;
	char eString[120];
	char *peXMit;
	bool bSendCR;
	uint32_t u32Length;

	va_start(ap, peFormat);
	vsnprintf(eString, sizeof(eString) - 1, (const char *) peFormat, ap);
	va_end(ap);

	// Submit all of this to the programmed I/O.
	peXMit = eString;

	while (*peXMit)
	{
		char *peLF;

		bSendCR = false;

		// Find the next linefeed
		peLF = strstr(peXMit, "\n");
		if (peLF)
		{
			u32Length = ((uint32_t) peLF) - ((uint32_t) peXMit);
			bSendCR = true;
		}
		else
		{
			// Don't send a CR - we're just sending the rest of the string
			u32Length = strlen(peXMit);
		}

		if (u32Length)
		{
			// Send the fragment if we have one
			SerialSendPIO((S16550UART *) ROSCOE_UART_A,
						  peXMit,
						  u32Length);
		}

		if (bSendCR)
		{
			SerialSendPIO((S16550UART *) ROSCOE_UART_A,
						  "\r\n",
						  2);
			peXMit++;
		}

		peXMit += u32Length;
	}

}

// Handler for all faults
static void Fault(uint8_t u8Vector,
				  uint32_t u32StackPointer,
				  bool bGroupB)
{
	uint8_t *pu8Digits;
	uint32_t *pu32DWORDPtr = NULL;
	uint16_t *pu16WORDPtr = NULL;
	uint32_t u32D1;
	uint32_t u32D2;

	*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;

	// Set the memory mirror for the interrupt mask registers
	sg_u8InterruptMask = 0;
	sg_u8InterruptMask2 = 0;

	// Shut off all interrupts in the hardware
	*((uint8_t *) ROSCOE_INTC_MASK) = 0;
	*((uint8_t *) ROSCOE_INTC_MASK2) = 0;

	// Set the POST LEDs including decimal points but adjust the pointer to point
	// to flash.
	pu8Digits = (uint8_t *) (((uint32_t) sg_u8LEDHex) - ROSCOE_BOOT_BASE_SRAM);

	// Fetch the hex->7 segment from flash and set the POST code
#define	POST_HEX_FLASH(x) POST_SET(((pu8Digits[((x) >> 4) & 0x0f] & POST_7SEG_DP) << 8) | pu8Digits[(x) & 0x0f] & POST_7SEG_DP)
	POST_HEX_FLASH(u8Vector);

	if (u8Vector < (sizeof(sg_peVectorList) / sizeof(sg_peVectorList[0])))
	{
		FaultOut("\n%s - Vector 0x%.2x\n", sg_peVectorList[u8Vector], u8Vector);
	}
	else
	if ((u8Vector >= 25) && (u8Vector <= 31))
	{
		FaultOut("\nLevel %u interrupt autovector - Vector 0x%.2x\n", u8Vector - 25, u8Vector);
	}
	else
	if ((u8Vector >= 32) && (u8Vector <= 47))
	{
		FaultOut("\nTrap #%u - Vector 0x%.2x\n", u8Vector - 32, u8Vector);
	}
	else
	{
		FaultOut("\nUser vector 0x%.2x\n", u8Vector);
	}

	// Stack pointer should look like this (from top to bottom

	// +4 = Saved d2
	// +0 = Saved d1

	pu32DWORDPtr = (uint32_t *) u32StackPointer;
	u32D1 = *pu32DWORDPtr;
	pu32DWORDPtr++;
	u32D2 = *pu32DWORDPtr;
	pu32DWORDPtr++;
	pu16WORDPtr = (uint16_t *) pu32DWORDPtr;

	// Figure out which frame this is
	if (false == bGroupB)
	{
		uint16_t u16Fault;

		// Group A

		FaultOut("Fault WORD=0x%.4x Access=0x%.4x%.4x, IR=0x%.4x, SR=0x%.4x, PC=0x%.4x%.4x\n",
				 pu16WORDPtr[0],
				 pu16WORDPtr[1], pu16WORDPtr[2],	// Access address
				 pu16WORDPtr[3],					// IR
				 pu16WORDPtr[4],					// SR
				 pu16WORDPtr[5], pu16WORDPtr[6]);	// PC
	}
	else
	{
		// Group B
		FaultOut("PC=0x%.4x%.4x SR=0x%.4x\n", pu16WORDPtr[1], pu16WORDPtr[2], *pu16WORDPtr);
	}

	// Move saved-off registers needed during vectoring into their proper positions
	pu32DWORDPtr = ((uint32_t *) (ROSCOE_STICKY_RAM_BASE + STICKYRAM_OFFSET_FAULT_REGS));
	pu32DWORDPtr[1] = u32D1;				// Original D1
	pu32DWORDPtr[15] = pu32DWORDPtr[2];		// A7=D2
	pu32DWORDPtr[2] = u32D2;				// Original D2

	// Dump all registers
	FaultOut("d0=0x%.8x d1=0x%.8x d2=0x%.8x d3=0x%.8x\n", pu32DWORDPtr[0], pu32DWORDPtr[1], pu32DWORDPtr[2], pu32DWORDPtr[3]);
	FaultOut("d4=0x%.8x d5=0x%.8x d6=0x%.8x d7=0x%.8x\n", pu32DWORDPtr[4], pu32DWORDPtr[5], pu32DWORDPtr[6], pu32DWORDPtr[7]);
	FaultOut("a0=0x%.8x a1=0x%.8x a2=0x%.8x a3=0x%.8x\n", pu32DWORDPtr[8], pu32DWORDPtr[9], pu32DWORDPtr[10], pu32DWORDPtr[11]);
	FaultOut("a4=0x%.8x a5=0x%.8x a6=0x%.8x a7=0x%.8x\n", pu32DWORDPtr[12], pu32DWORDPtr[13], pu32DWORDPtr[14], pu32DWORDPtr[15]);

	// Loop forever
	while (1);
}


// Fault handler entry points
void InterruptFaultGroupA(uint32_t u32Vector,
						  uint32_t u32StackPointer)
{
	Fault(u32Vector,
		  u32StackPointer,
		  false);
}

void InterruptFaultGroupB(uint32_t u32Vector,
						  uint32_t u32StackPointer)
{
	Fault(u32Vector,
		  u32StackPointer,
		  true);
}

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

#ifdef _NO_INTS
	// Don't update the vector
#else
	*((uint32_t *) ((psInterruptDefinition->u8InterruptVector << 2) + ROSCOE_BOOT_BASE_SRAM)) = (uint32_t) InterruptHandler;
#endif

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

#ifdef _NO_INTS
	// Go through the motions, but don't enable the interrupt
#else
	// We're either masking or unmasking it. Update the mirror first.
	if (bMaskInterrupt)
	{
		*psInterruptDefinition->pu8InterruptMaskMirror &= (uint8_t) (~psInterruptDefinition->u8MaskValue);
	}
	else
	{
		*psInterruptDefinition->pu8InterruptMaskMirror |= psInterruptDefinition->u8MaskValue;
	}

	// Set the mask
	*psInterruptDefinition->pu8MaskAddress = *psInterruptDefinition->pu8InterruptMaskMirror;
#endif

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
	sg_u8InterruptMask = 0;
	sg_u8InterruptMask2 = 0;

	// Mask all interrupts
	*VECTORDEF(ROSCOE_INTC_MASK) = 0;
	*VECTORDEF(ROSCOE_INTC_MASK2) = 0;

	// Enable all processor interrupts
	InterruptEnable();
}

