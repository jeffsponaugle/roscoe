#include "../Hardware/Roscoe.h"
#include "../Shared/16550.h"

	.title "FaultHandlerAsm.s"
	.global	_sr, _pc, dr0, dr1, dr2, dr3, dr4, dr5, dr6, dr7
	.global ar0, ar1, ar2, ar3, ar4, ar5, ar6, ar7
	.global	VectorFaultBusError
	.global	VectorFaultAddressError
	.global	VectorFaultIllegalInstruction
	.global VectorFaultDivByZero
	.global	MonitorDispatch

/* Register variable storage */

	.data

_sr:	.word	0
_pc:	.long	0x00000000
dr0:	.long	0x00000000
dr1:	.long	0x00000000
dr2:	.long	0x00000000
dr3:	.long	0x00000000
dr4:	.long	0x00000000
dr5:	.long	0x00000000
dr6:	.long	0x00000000
dr7:	.long	0x00000000
ar0:	.long	0x00000000
ar1:	.long	0x00000000
ar2:	.long	0x00000000
ar3:	.long	0x00000000
ar4:	.long	0x00000000
ar5:	.long	0x00000000
ar6:	.long	0x00000000
ar7:	.long	0x00000000

	.text

/* Value of SR when dealing with a fault */
ssri:	.word	0x2700

VectorFaultBusError:
	movem.l #0xffff,dr0
        move.l  %usp,%a6
        move.l  %a6,ar7
        move.w  %a7@(8),_sr
        move.l  %a7@(0x0a),_pc
        pea     FaultBusError
        move.w  ssri,-(%a7)
	rte

VectorFaultAddressError:
	movem.l #0xffff,dr0
        move.l  %usp,%a6
        move.l  %a6,ar7
        move.w  %a7@(8),_sr
        move.l  %a7@(0x0a),_pc
        pea     FaultAddressError
        move.w  ssri,-(%a7)
	rte

VectorFaultIllegalInstruction:
	movem.l #0xffff,dr0
        move.l  %usp,%a6
        move.l  %a6,ar7
        move.w  %a7@(0),_sr
        move.l  %a7@(2),_pc
        pea     FaultIllegalInstruction
        move.w  ssri,-(%a7)
	rte

VectorFaultDivByZero:
	movem.l #0xffff,dr0
        move.l  %usp,%a6
        move.l  %a6,ar7
        move.w  %a7@(0),_sr
        move.l  %a7@(2),_pc
        pea     FaultDivByZero
        move.w  ssri,-(%a7)
	rte

/* This will restore the stack pointer from g_u32MonitorDefaultSP and dispatch to
   the address in g_pMonitorEntry */

MonitorDispatch:
	move.l	g_u32MonitorDefaultSP, %a7
	move.l	%a7, %a6

	/* Pull out the target address into %d0 */
	move.l	g_pMonitorEntry, %d0

	/* Stick target address on the stack and "return" to it */
	movem.l	%d0, -(%a7)
	rts

