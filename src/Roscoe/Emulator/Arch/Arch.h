#ifndef _ARCH_H_
#define _ARCH_H_

#if defined(STM32F439xx)
#include "Arch/F439/F439.h"
#elif defined(STM32H743xx)
#include "Arch/H743/H743.h"
#elif defined(__FREEBSD__)
#include "Arch/UNIX/UNIX.h"
#elif defined(__LINUX__)
#include "Arch/UNIX/UNIX.h"
#elif defined(ANDROID)
#include "Arch/UNIX/UNIX.h"
#elif defined(_WIN32)
#include "Arch/Windows/WindowsArch.h"
#else
#error "Architecture not defined in Arch.h"
#endif

// Architecture initialization (usually clocks, timers, etc...)
extern void ArchInit(void *pvArchSpecificData);

// Start the operating system
extern void ArchStartOS(void);

// Interrupt control and related
extern void ArchInterruptEnable(void);
extern void ArchInterruptDisable(void);
extern bool ArchIsInterrupt(void);
extern bool ArchInterruptsEnabled(void);

// System tick related
extern void ArchDelay(uint32_t u32Microseconds);
extern void ArchTick(void);
extern uint32_t ArchGetSystemTickRate(void);
extern uint64_t ArchGetTotalTicks(void);

// Userland timer APIs (both regular and high resolution timer tick rates)
// Result is in milliseconds.
extern uint32_t ArchGetTimerTickRate(void);
extern uint32_t ArchGetTimerHRTickRate(void);
extern EStatus ArchPerformanceCounterGet(uint32_t *pu32Counter);
extern EStatus ArchPerformanceCounterGetHz(uint32_t *pu32CounterHz);

// Serial ports
extern EStatus ArchSerialInit(void);

// Forward reference so we don't have to include a ton of stuff
struct SSerialConfig;
extern EStatus ArchSerialConfig(uint8_t u8SerialIndex,
								struct SSerialConfig *psSerialConfig);
extern EStatus ArchSerialSend(uint8_t u8SerialIndex,
							  char *peData,
							  uint32_t u32Count,
							  uint32_t u32Timeout);
extern EStatus ArchSerialReceive(uint8_t u8SerialIndex,
								 char *peDataBuffer,
								 uint32_t *pu32BytesReceived,
								 uint32_t u32Timeout);
extern EStatus ArchSerialSendProgrammedIO(uint8_t u8LogicalIndex,
										  char *peData,
										  uint32_t u32Count);
extern EStatus ArchSerialSendFlush(uint8_t u8SerialIndex);
extern void ArchSerialOSStart(void);


// RTC
extern EStatus ArchRTCInit(bool bInternalClock);
extern uint64_t ArchRTCGet(void);

// Operating system interfacing
extern bool ArchOSIsRunning(void);

// Misc
extern EStatus ArchRandomNumber(uint64_t *pu64RandomNumber);
extern void ArchHardReset(void);
extern bool ArchIsPointerValid(void *pvAddress,
							   bool bIncludeCodeRegions);
extern void ArchGetUniqueSystemID(uint8_t *pu8Buffer,
								  uint32_t *pu8BufferLength);
extern void ArchCriticalError(char *peMessage);

extern EStatus ArchIsProgramRunning(char *peExecutableFilenameSubstring, uint32_t *pu32Matches);

#endif
