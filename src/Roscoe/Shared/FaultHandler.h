#ifndef _FAULTHANDLER_H_
#define _FAULTHANDLER_H_

extern uint16_t _sr;
extern uint32_t _pc;
extern uint32_t dr0;
extern uint32_t dr1;
extern uint32_t dr2;
extern uint32_t dr3;
extern uint32_t dr4;
extern uint32_t dr5;
extern uint32_t dr6;
extern uint32_t dr7;
extern uint32_t ar0;
extern uint32_t ar1;
extern uint32_t ar2;
extern uint32_t ar3;
extern uint32_t ar4;
extern uint32_t ar5;
extern uint32_t ar6;
extern uint32_t ar7;

extern void VectorFaultBusError(void);
extern void VectorFaultAddressError(void);
extern void VectorFaultIllegalInstruction(void);
extern void VectorFaultDivByZero(void);
extern void FaultHandlerInstall(void);
extern void MonitorDispatch(void);
extern void FaultInstallMonitor(uint32_t u32DefaultSP,
								void (*MonitorEntry)(void));


#endif

