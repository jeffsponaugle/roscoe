#ifndef _SHARED_H_
#define _SHARED_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <WinDef.h>
#endif

#include "Shared/Version.h"
#include "BIOS/OS.h"

extern const uint8_t sg_u8LEDHex[];
#define	POST_HEX(x) POST_SET((sg_u8LEDHex[((x) >> 4) & 0x0f] << 8) | sg_u8LEDHex[(x) & 0x0f])

extern void FlashTableCopy(SImageVersion *psBootLoaderInfo,
						   bool bAddFlashOffset);
extern void SharedSleep(volatile uint32_t u32Microseconds);
extern EStatus DumpHex(uint32_t u32Offset,
					   uint32_t u32BytesToDump,
					   uint16_t u16LinesToDump,
					   uint8_t *pu8DataPtr,
					   uint32_t *pu32FinalAddress,
					   bool bRepeatDataSilence,
					   bool (*TerminateCallback)(void));
extern const char *GetErrorText(EStatus eStatus);
extern void UINT64ToASCII(uint64_t u64Value,
						  char *peBuffer,
						  uint32_t u32BufferSize,
						  bool bLeadingZeroes,
						  bool bThousandsCommas);

#endif

