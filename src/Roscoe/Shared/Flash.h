#ifndef _FLASH_H_
#define _FLASH_H_

#include <stdbool.h>

#include "Shared/Version.h"

// Different flash regions
typedef enum
{
	EFLASHREGION_END = 0,
	EFLASHREGION_BOOT_LOADER,
	EFLASHREGION_OP_1,
	EFLASHREGION_OP_2
} EFlashRegion;

// Flash segment structure
typedef struct SFlashSegment
{
	EFlashRegion eFlashRegion;		// What region is this in?
	uint32_t u32BlockOffset;		// Logical offset into this region
	uint32_t u32FlashPageSize;		// How big is this flash page?
	uint32_t u32PhysicalAddress;	// Where in address space is this flash page?
} SFlashSegment;

extern void FlashInit(const SFlashSegment *psFlashTable);
extern bool FlashReadRegion(EFlashRegion eFlashRegion,
							uint32_t u32RegionOffset,
							uint8_t *pu8Buffer,
							uint32_t u32ReadSize);
extern bool FlashProgramRegion(EFlashRegion eFlashRegion,
							   uint32_t u32RegionOffset,
							   uint8_t *pu8Buffer,
							   uint32_t u32ProgramSize);
extern bool FlashRegionGetSize(EFlashRegion eFlashRegion,
							   uint32_t *pu32RegionSize);
extern bool FlashGetRegionPointer(EFlashRegion eFlashRegion,
								  uint32_t u32RegionOffset,
								  uint8_t **ppu8RegionPointer);
extern bool FlashRegionErase(EFlashRegion eFlashRegion);
extern bool FlashRegionCheckErased(EFlashRegion eFlashRegion);
extern bool FlashCopyToRAM(EFlashRegion eFlashRegion,
						   uint8_t *pu8Destination,
						   uint32_t u32RegionOffset,
						   uint32_t u32ReadSize);
extern bool FlashVerifyCopy(EFlashRegion eFlashRegion,
							uint32_t u32RegionOffset,
							uint32_t u32ReadSize,
							uint32_t *pu32FailedAddress,
							uint32_t *pu32ExpectedData,
							uint32_t *pu32GotData);
extern uint32_t FlashCountSegmentTable(const SFlashSegment *psFlashSegment);
extern void FlashIdentify(void);
extern EVersionCode FlashRegionCheck(EFlashRegion eFlashRegion,
									 EImageType eImageTypeSearch,
									 SImageVersion *psImageVersion,
									 uint32_t *pu32VersionOffset);
extern uint32_t FlashCRC32Region(EFlashRegion eFlashRegion,
								 uint32_t u32CRCStart,
								 uint32_t u32RegionOffset,
								 uint32_t u32Size);

#endif

