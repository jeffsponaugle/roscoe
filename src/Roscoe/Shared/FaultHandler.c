#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <machine/endian.h>
#include "BIOS/OS.h"
#include "Shared/Shared.h"
#include "Shared/FaultHandler.h"
#include "Shared/AsmUtils.h"
#include "Hardware/Roscoe.h"
#include "Shared/Stream.h"
#include "Shared/Interrupt.h"
#include "Shared/LinkerDefines.h"

// Set to nonzero (everything) if we have a monitor default function
uint32_t g_u32MonitorDefaultSP = 0;
void (*g_pMonitorEntry)(void) = NULL;

typedef enum 
{
	EFAULT_NONE,
	EFAULT_BUS_ERROR,
	EFAULT_ADDRESS_ERROR,
	EFAULT_ILLEGAL_INSTRUCTION,
	EFAULT_DIV0,
} EFault;

// Fault codes (must match EFault order above
static const char *sg_eFaultCodes[] =
{
	"None",
	"Bus fault",
	"Address fault",
	"Illegal instruction fault",
	"Divide by 0 fault"
};

// What's the current fault, if any?
static EFault sg_eFault = EFAULT_NONE;

// Routine to dump out recorded registers
void FaultDumpRegisters(void)
{
	printf("pc=0x%.6x sr=0x%.4x\n", _pc, _sr);
	printf("a0=0x%.8x a1=0x%.8x a2=0x%.8x a3=0x%.8x\n", ar0, ar1, ar2, ar3);
	printf("a4=0x%.8x a5=0x%.8x a6=0x%.8x a7=0x%.8x\n", ar4, ar5, ar6, ar7);
	printf("d0=0x%.8x d1=0x%.8x d2=0x%.8x d3=0x%.8x\n", dr0, dr1, dr2, dr3);
	printf("d4=0x%.8x d5=0x%.8x d6=0x%.8x d7=0x%.8x\n", dr4, dr5, dr6, dr7);
}

// This quiesces the system (including interrupts) after a fault
static void FaultPrep(void)
{
	EStatus eStatus;

	// Stop interrupts in the 68K
	SRSet(0x2700);

	// Mask off all interrupts in the interrupt controller
	*((volatile uint8_t *) ROSCOE_INTC_MASK) = 0xff;
	*((volatile uint8_t *) ROSCOE_INTC_MASK2) = 0xff;

	// Set console to programmed I/O
	eStatus = StreamSetConsoleSerialInterruptMode(false);
	assert(ESTATUS_OK == eStatus);
}

// Code to dump the last fault (if any), or just return if there isn't one
void FaultDump(void)
{
	if (sg_eFault != EFAULT_NONE)
	{
		printf("\n*** %s:\n", sg_eFaultCodes[sg_eFault]);

		FaultDumpRegisters();

		while (1)
		{
		}
	}
}

static void FaultDumpStore(EFault eFault)
{
	sg_eFault = eFault;

	// Now dump the fault
	FaultDump();

	// Flush the console
	fflush(stdout);

	StreamFlush();

	// Let's see if we dispatch or 
	if (g_pMonitorEntry)
	{
		MonitorDispatch();
	}

	while (1);
}

// Bus error
void FaultBusError(void)
{
	FaultDumpStore(EFAULT_BUS_ERROR);
}

// Address error
void FaultAddressError(void)
{
	FaultDumpStore(EFAULT_ADDRESS_ERROR);
}

// Illegal instruction
void FaultIllegalInstruction(void)
{
	FaultDumpStore(EFAULT_ILLEGAL_INSTRUCTION);
}

// Divide by zero
void FaultDivByZero(void)
{
	FaultDumpStore(VECTOR_DIV0);
}

// Installs fault handlers in the appropriate interrupt vectors
void FaultHandlerInstall(void)
{
	*((volatile uint32_t *) VECTOR_BUS_ERROR) = (uint32_t) VectorFaultBusError;
	*((volatile uint32_t *) VECTOR_ADDRESS_ERROR) = (uint32_t) VectorFaultAddressError;
	*((volatile uint32_t *) VECTOR_ILLEGAL_INSTRUCTION) = (uint32_t) VectorFaultIllegalInstruction;
	*((volatile uint32_t *) VECTOR_DIV0) = (uint32_t) VectorFaultIllegalInstruction;
}

// Installs a monitor to be called at the end of a fault or when the debug
// button is pressed.
void FaultInstallMonitor(uint32_t u32DefaultSP,
						 void (*MonitorEntry)(void))
{
	EStatus eStatus;

	g_pMonitorEntry = MonitorEntry;
	g_u32MonitorDefaultSP = u32DefaultSP;

	// Disable interrupts
	InterruptDisable();

	if (NULL == MonitorEntry)
	{
		// We're removing the monitor
		eStatus = InterruptMaskSet(INTVECT_IRQL7_DEBUGGER,
								   true);
		assert(ESTATUS_OK == eStatus);
		eStatus = InterruptHook(INTVECT_IRQL7_DEBUGGER,
								NULL);
		assert(ESTATUS_OK == eStatus);
		g_u32MonitorDefaultSP = 0;
	}
	else
	{
		// We're installing the monitor
		g_u32MonitorDefaultSP = u32DefaultSP;
		eStatus = InterruptHook(INTVECT_IRQL7_DEBUGGER,
								MonitorEntry);
		assert(ESTATUS_OK == eStatus);
		eStatus = InterruptMaskSet(INTVECT_IRQL7_DEBUGGER,
								   false);
		assert(ESTATUS_OK == eStatus);
	}

	// Reenable interrupts
	InterruptEnable();

	assert(ESTATUS_OK == eStatus);
}

