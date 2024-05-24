/* Roscoe Assembly language routine support
 *
 * 68030 Calling conventions:
 *
 * d0-d1 and a0-a1 are scratch registers
 * d2-d7 and a2-a7 are assumed to be preserved by the callee
 *
 */

#include "../Hardware/Roscoe.h"
#include "../Shared/16550.h"

	.title "AsmUtils.s"

	.global	MoveMultipleWrite
	.global	MoveMultipleRead
	.global	IDESectorMoveRead
	.global IDESectorMoveWrite
	.global RTCGetTickCounts
	.global	SRSet
	.global	SRGet
	.global SPGet

/* Back up stack pointer 6 bytes - 4 for the return address and 2 for the word param of the SR value */

SRSet:
	movel	%sp, %a0
	addl	#6, %a0
	movew	(%a0), %d0
	movew	%d0, %sr
	rts

SRGet:
	movew	%sr, %d0
	rts

SPGet:
	movel	%sp, %d0
	rts

MoveMultipleWrite:
	moveml	%d2-%d7/%a2,-(%sp)
	moveml	%d0-%d7,-(%a2)
	moveml	(%sp)+,%d2-%d7/%a2
	rts

MoveMultipleRead:
	moveml	%d2-%d7/%a2,-(%sp)
	moveml	(%a2)+, %d0-%d7
	moveml	(%sp)+,%d2-%d7/%a2
	rts

/*

void IDESectorMoveRead(void *pvTargetBuffer);

*/

IDESectorMoveRead:
	moveml	%d2-%d7/%a2,-(%sp)

/* pvTargetBuffer == sp + 4 (rtn addrss) + 24 (data registers) + 4 (address registers) */

	movel	%sp, %a0
	addl	#4+24+4, %a0
	movel	(%a0), %a2

	lea	ROSCOE_IDE_1_CSA, %a0
	movel	#32, %a1

/* a0 Is our IDE_CSA address. 
   a2 Is the destination */

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)
	addal	%a1, %a2

	moveml	(%a0), %d0-%d7
	moveml	%d0-%d7,(%a2)

	moveml	(%sp)+,%d2-%d7/%a2
	rts

/*

void IDESectorMoveWrite(void *pvSourceBuffer);

*/

IDESectorMoveWrite:
	moveml	%d2-%d7,-(%sp)

/* pvSourceBuffer == sp + 4 (rtn addrss) + 24 (data registers)  */

	movel	%sp, %a0
	addl	#4+24, %a0
	movel	(%a0), %a1

	lea	ROSCOE_IDE_1_CSA, %a0

/* a0 Is our IDE_CSA address. 
   a1 Is the source */

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%a1)+, %d0-%d7
	moveml	%d0-%d7,(%a0)

	moveml	(%sp)+,%d2-%d7
	rts

/*

uint32_t RTCGetTickCounts(void);

*/

RTCGetTickCounts:
	moveml	%d2-%d4,-(%sp)

/* First find the start of an RTC interrupt */

	bsr	RTCGetPowerOnHalfSeconds

	movel	%d0, %d3
	movel	#0x0, %d4

/* d3 Now has the starting half second counter */

findEdge:
	bsr	RTCGetPowerOnHalfSeconds
	cmp	%d0, %d3
	beq	findEdge

	movel	%d0, %d3

/* d3 Has the current power on half seconds */

timeCounter:
	addil	#1, %d4
	bsr	RTCGetPowerOnHalfSeconds
	cmp	%d0, %d3
	beq	timeCounter

finishCount:

/* Return the count in %d0 */

	movel	%d4, %d0
	moveml	(%sp)+,%d2-%d4
	rts
