#ifndef _ASMUTILS_H_
#define _ASMUTILS_H_

extern void MoveMultipleWrite(void *pvWriteAddress);
extern void MoveMultipleRead(void *pvReadAddress);
extern void IDESectorMoveRead(void *pvTargetBuffer);
extern void IDESectorMoveWrite(void *pvSourceBuffer);
extern void SRSet(uint16_t u16SRValue);
extern uint16_t SRGet(void);
extern uint16_t SPGet(void);

#endif
