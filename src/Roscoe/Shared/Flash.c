#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "Shared/Flash.h"
#include "Hardware/Roscoe.h"
#include "Shared/Shared.h"
#include "Shared/zlib/zlib.h"

// This is what we know of right now
#define	SST_ID			0xbfbf
#define	SST_39SF040		0xb7b7

// Pointer to active flash map
static const SFlashSegment *sg_psFlashLayout;

// Current flash/ram page register value

#ifdef _BOOTLOADER
// In the boot loader, default is read from flash, write to SRAM
static uint8_t sg_u8FlashRAMReadWrite = 0x00;
#else
// In BIOS, default is read/write to SRAM
static uint8_t sg_u8FlashRAMReadWrite = 0x02;
#endif

void FlashInit(const SFlashSegment *psFlashTable)
{
	sg_psFlashLayout = psFlashTable;
}

void FlashIdentify(void)
{
	volatile uint16_t *pu16FlashBase = (volatile uint16_t *) ROSCOE_FLASH_BOOT_BASE;
	uint16_t u16Mfg;
	uint16_t u16DeviceID;

	// Put in product and manufacturer ID read mode
	*(pu16FlashBase + 0x5555) = 0xaaaa;
	*(pu16FlashBase + 0x2aaa) = 0x5555;
	*(pu16FlashBase + 0x5555) = 0x9090;

	// Read manufacturer and device ID
	u16Mfg = *(pu16FlashBase + 0);
	u16DeviceID = *(pu16FlashBase + 1);

	// Put back operational mode
	*(pu16FlashBase + 0x5555) = 0xaaaa;
	*(pu16FlashBase + 0x2aaa) = 0x5555;
	*(pu16FlashBase + 0x5555) = 0xf0f0;

	if ((u16Mfg != SST_ID) ||
		(u16DeviceID != SST_39SF040))
	{
		printf("Unknown flash device: MfgID=0x%.4x, DeviceID=0x%.4x\n", u16Mfg, u16DeviceID);
	}
	else
	{
		printf("Detected SST 39SF040 flash device\n");
	}
}

// Reads a chunk of data out of a flash region. Returns false if the region
// isn't found.
bool FlashReadRegion(EFlashRegion eFlashRegion,
					 uint32_t u32RegionOffset,
					 uint8_t *pu8Buffer,
					 uint32_t u32ReadSize)
{
	const SFlashSegment *psFlashTable;

	psFlashTable = sg_psFlashLayout;

	while ((psFlashTable->eFlashRegion != EFLASHREGION_END) &&
		   (u32ReadSize))
	{
		if ((eFlashRegion == psFlashTable->eFlashRegion) &&
			(u32RegionOffset >= psFlashTable->u32BlockOffset) &&
			(u32RegionOffset < (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize)))
		{
			uint32_t u32ChunkSize = u32ReadSize;
			uint32_t u32Available;

			// Figure out how many bytes we have/want that are available in this offset
			u32Available = (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize) - u32RegionOffset;
			if (u32Available < u32ChunkSize)
			{
				u32ChunkSize = u32Available;
			}

			// Copy the data into the target buffer
			if (u32ChunkSize)
			{
				memcpy((void *) pu8Buffer, (void *) (psFlashTable->u32PhysicalAddress + (u32RegionOffset - psFlashTable->u32BlockOffset)), u32ChunkSize);
			}

			u32RegionOffset += u32ChunkSize;
			pu8Buffer += u32ChunkSize;
			u32ReadSize -= u32ChunkSize;
		}

		++psFlashTable;
	}

	// If we have a remaining set of read bytes, then we didn't read the region
	// properly or fully
	if (u32ReadSize)
	{
		return(false);
	}
	else
	{
		// Otherwise, we did!
		return(true);
	}
}

bool FlashProgramRegion(EFlashRegion eFlashRegion,
						uint32_t u32RegionOffset,
						uint8_t *pu8Buffer,
						uint32_t u32ProgramSize)
{
	const SFlashSegment *psFlashTable;
	volatile uint16_t *pu16FlashBase = (volatile uint16_t *) ROSCOE_FLASH_BOOT_BASE;

	// Better be an even number of bytes to program
	assert(0 == (u32ProgramSize & 1));

	psFlashTable = sg_psFlashLayout;

	while ((psFlashTable->eFlashRegion != EFLASHREGION_END) &&
		   (u32ProgramSize))
	{
		if ((eFlashRegion == psFlashTable->eFlashRegion) &&
			(u32RegionOffset >= psFlashTable->u32BlockOffset) &&
			(u32RegionOffset < (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize)))
		{
			uint32_t u32ChunkSize = u32ProgramSize;
			uint32_t u32Available;
			volatile uint16_t *pu16ProgramPage;

			// Figure out how many bytes we have/want that are available in this offset
			u32Available = (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize) - u32RegionOffset;
			if (u32Available < u32ChunkSize)
			{
				u32ChunkSize = u32Available;
			}

			// Set flash read/write page
			pu16ProgramPage = (volatile uint16_t *) (psFlashTable->u32PhysicalAddress + (u32RegionOffset - psFlashTable->u32BlockOffset));

			// Program the page
			while (u32ChunkSize)
			{
				uint16_t u16PreData;
				uint16_t u16CurrentData;
				uint32_t u32Timeout = 1000000;

				// Program byte
				*(pu16FlashBase + 0x5555) = 0xaaaa;
				*(pu16FlashBase + 0x2aaa) = 0x5555;
				*(pu16FlashBase + 0x5555) = 0xa0a0;	// Program command

				// Program both chips at once
				*pu16ProgramPage = *((uint16_t *) pu8Buffer);

				// Wait until it programs properly or times out. Wait until DQ6 in each flash
				// part stops toggling.
				u16PreData = *pu16ProgramPage & 0x4040;
				while (u32Timeout)
				{
					u16CurrentData = *pu16ProgramPage & 0x4040;
					if (u16CurrentData == u16PreData)
					{
						break;
					}

					u16PreData = u16CurrentData;
				}

				if (0 == u32Timeout)
				{
					printf("Timeout while programming 0x%.6x\n", pu16ProgramPage);
					goto errorExit;
				}

				pu8Buffer += sizeof(uint16_t);
				pu16ProgramPage++;
				u32ChunkSize -= sizeof(uint16_t);
				u32ProgramSize -= sizeof(uint16_t);
				u32RegionOffset += sizeof(uint16_t);
			}
		}

		++psFlashTable;
	}

	// If we have a remaining set of bytes to program, we didn't fully program
	//  the region
errorExit:
	if (u32ProgramSize)
	{
		return(false);
	}
	else
	{
		// Otherwise, we did!
		return(true);
	}
}

uint32_t FlashCRC32Region(EFlashRegion eFlashRegion,
						  uint32_t u32CRCStart,
						  uint32_t u32RegionOffset,
						  uint32_t u32Size)
{
	const SFlashSegment *psFlashTable;

	psFlashTable = sg_psFlashLayout;

	while ((psFlashTable->eFlashRegion != EFLASHREGION_END) &&
		   (u32Size))
	{
		if ((eFlashRegion == psFlashTable->eFlashRegion) &&
			(u32RegionOffset >= psFlashTable->u32BlockOffset) &&
			(u32RegionOffset < (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize)))
		{
			uint32_t u32ChunkSize = u32Size;
			uint32_t u32Available;
			uint8_t *pu8RegionPointer;

			// Figure out how many bytes we have/want that are available in this offset
			u32Available = (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize) - u32RegionOffset;
			if (u32Available < u32ChunkSize)
			{
				u32ChunkSize = u32Available;
			}

			pu8RegionPointer = (uint8_t *) (psFlashTable->u32PhysicalAddress + (u32RegionOffset - psFlashTable->u32BlockOffset));

			// Now crc32 the data
			u32CRCStart = crc32(u32CRCStart,
								pu8RegionPointer,
								u32ChunkSize);

			// Next segment!
			u32RegionOffset += u32ChunkSize;
			u32Size -= u32ChunkSize;
		}

		++psFlashTable;
	}

	// All done

errorExit:
	return(u32CRCStart);
}

// Calculates the overall size of a flash region. Returns false if the
// region isn't found
bool FlashRegionGetSize(EFlashRegion eFlashRegion,
 					    uint32_t *pu32RegionSize)
{
	uint32_t u32Size = 0;
	const SFlashSegment *psFlashTable;
	bool bRegionFound = false;

	psFlashTable = sg_psFlashLayout;

	while (psFlashTable->eFlashRegion != EFLASHREGION_END)
	{
		if (eFlashRegion == psFlashTable->eFlashRegion)
		{
			u32Size += psFlashTable->u32FlashPageSize;
			bRegionFound = true;
		}

		++psFlashTable;
	}

	if ((bRegionFound) && (pu32RegionSize))
	{
		*pu32RegionSize = u32Size;
	}

	return(bRegionFound);
}

// Gets a physical pointer to a logical region of flash and does whatever
// page selection is necessary for both reads and writes.
bool FlashGetRegionPointer(EFlashRegion eFlashRegion,
 						   uint32_t u32RegionOffset,
 						   uint8_t **ppu8RegionPointer)
{
	bool bRegionFound = false;
	const SFlashSegment *psFlashTable;

	psFlashTable = sg_psFlashLayout;

	while (psFlashTable->eFlashRegion != EFLASHREGION_END)
	{
		if ((eFlashRegion == psFlashTable->eFlashRegion) &&
			(u32RegionOffset >= psFlashTable->u32BlockOffset) &&
			(u32RegionOffset < (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize)))
		{
			// Found it!
			if (ppu8RegionPointer)
			{
				// Return a pointer to the appropriate block address
				*ppu8RegionPointer = (uint8_t *) (psFlashTable->u32PhysicalAddress + (u32RegionOffset - psFlashTable->u32BlockOffset));
			}

			bRegionFound = true;
			goto errorExit;
		}

		++psFlashTable;
	}

errorExit:
	return(bRegionFound);
}

// Returns true if a region is fully erased, otherwise false
bool FlashRegionCheckErased(EFlashRegion eFlashRegion)
{
	bool bRegionErased = false;
	const SFlashSegment *psFlashTable;
	volatile uint16_t *pu16FlashPtr;
	volatile uint16_t *pu16FlashBase = (volatile uint16_t *) ROSCOE_FLASH_BOOT_BASE;

	psFlashTable = sg_psFlashLayout;

	while (psFlashTable->eFlashRegion != EFLASHREGION_END)
	{
		if (eFlashRegion == psFlashTable->eFlashRegion)
		{
			volatile uint32_t *pu32DataPtr;
			uint32_t u32Count;

			pu16FlashPtr = (uint16_t *) psFlashTable->u32PhysicalAddress;

			// Run through block and see if it's fully erased - it should be all 0xffs
			// # Of uint32_ts to check
			u32Count = psFlashTable->u32FlashPageSize >> 2;
			pu32DataPtr = (uint32_t *) psFlashTable->u32PhysicalAddress;
			while (u32Count)
			{
				if (*pu32DataPtr != 0xffffffff)
				{
					bRegionErased = false;
					goto errorExit;
				}

				pu32DataPtr++;
				u32Count--;
			}

			// All good - keep going
		}

		++psFlashTable;
	}

	bRegionErased = true;

errorExit:
	return(bRegionErased);
}

// Erases an entire flash region. true If successful, false if not
bool FlashRegionErase(EFlashRegion eFlashRegion)
{
	bool bRegionFound = false;
	const SFlashSegment *psFlashTable;
	volatile uint16_t *pu16FlashPtr;
	volatile uint16_t *pu16FlashBase = (volatile uint16_t *) ROSCOE_FLASH_BOOT_BASE;

	psFlashTable = sg_psFlashLayout;

	while (psFlashTable->eFlashRegion != EFLASHREGION_END)
	{
		if (eFlashRegion == psFlashTable->eFlashRegion)
		{
			volatile uint32_t *pu32DataPtr;
			uint32_t u32Count;

			// Found it! Erase the page.
			bRegionFound = true;

			// Set up the read and write registers appropriately for this region

			pu16FlashPtr = (uint16_t *) psFlashTable->u32PhysicalAddress;

			*(pu16FlashBase + 0x5555) = 0xaaaa;
			*(pu16FlashBase + 0x2aaa) = 0x5555;
			*(pu16FlashBase + 0x5555) = 0x8080;
			*(pu16FlashBase + 0x5555) = 0xaaaa;
			*(pu16FlashBase + 0x2aaa) = 0x5555;

			*(pu16FlashPtr + 0x0000) = 0x3030;

			// 25ms per the datasheet
			SharedSleep(25000);

			// Run through block and see if it's fully erased - it should be all 0xffs

			// # Of uint32_ts to check
			u32Count = psFlashTable->u32FlashPageSize >> 2;
			pu32DataPtr = (uint32_t *) psFlashTable->u32PhysicalAddress;
			while (u32Count)
			{
				if (*pu32DataPtr != 0xffffffff)
				{
					printf("Failed erasure at 0x%.6x\n", pu32DataPtr);
					bRegionFound = false;
					goto errorExit;
				}

				pu32DataPtr++;
				u32Count--;
			}

			// All good - keep going
		}

		++psFlashTable;
	}

errorExit:
	return(bRegionFound);
}

// Copies flash region to pu8Destination with region offset and read size
bool FlashCopyToRAM(EFlashRegion eFlashRegion,
					uint8_t *pu8Destination,
					uint32_t u32RegionOffset,
					uint32_t u32ReadSize)
{
	const SFlashSegment *psFlashTable;

	psFlashTable = sg_psFlashLayout;

	while ((psFlashTable->eFlashRegion != EFLASHREGION_END) &&
		   (u32ReadSize))
	{
		if ((eFlashRegion == psFlashTable->eFlashRegion) &&
			(u32RegionOffset >= psFlashTable->u32BlockOffset) &&
			(u32RegionOffset < (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize)))
		{
			uint32_t u32ChunkSize = u32ReadSize;
			uint32_t u32Available;

			// Figure out how many bytes we have/want that are available in this offset
			u32Available = (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize) - u32RegionOffset;
			if (u32Available < u32ChunkSize)
			{
				u32ChunkSize = u32Available;
			}

			// Copy the data into the target buffer
			if (u32ChunkSize)
			{
				// Copy to self. Looks weird, but is correct.
				memcpy((void *) pu8Destination,
					   (void *) (psFlashTable->u32PhysicalAddress + (u32RegionOffset - psFlashTable->u32BlockOffset)), 
					   u32ChunkSize);
			}

			u32RegionOffset += u32ChunkSize;
			pu8Destination += u32ChunkSize;
			u32ReadSize -= u32ChunkSize;
		}

		++psFlashTable;
	}

	// If we have a remaining set of read bytes, then we didn't read the region
	// properly or fully, or the incoming read size exceeds the size of the
	// region.
	if (u32ReadSize)
	{
		return(false);
	}
	else
	{
		// Otherwise, we did!
		return(true);
	}
}

// How big is our buffer for verification?
#define	VERIFY_CHUNK_SIZE			1024

static uint8_t sg_u8VerifyBuffer[VERIFY_CHUNK_SIZE];

// Verifies that RAM == Flash at the given addresses/destination/offset
bool FlashVerifyCopy(EFlashRegion eFlashRegion,
					 uint32_t u32RegionOffset,
					 uint32_t u32ReadSize,
					 uint32_t *pu32FailedAddress,
					 uint32_t *pu32ExpectedData,
					 uint32_t *pu32GotData)
{
	const SFlashSegment *psFlashTable;

	psFlashTable = sg_psFlashLayout;

	while ((psFlashTable->eFlashRegion != EFLASHREGION_END) &&
		   (u32ReadSize))
	{
		if ((eFlashRegion == psFlashTable->eFlashRegion) &&
			(u32RegionOffset >= psFlashTable->u32BlockOffset) &&
			(u32RegionOffset < (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize)))
		{
			uint32_t u32ChunkSize = u32ReadSize;
			uint32_t u32Available;
			uint32_t *pu32ReadAddress;
			uint32_t *pu32VerifyAddress;
			uint32_t u32Loop;

			// Figure out how many bytes we have/want that are available in this offset
			u32Available = (psFlashTable->u32BlockOffset + psFlashTable->u32FlashPageSize) - u32RegionOffset;
			if (u32Available < u32ChunkSize)
			{
				u32ChunkSize = u32Available;
			}

			// Ensure the chunk size doesn't exceed the verify buffer size
			if (u32ChunkSize > sizeof(sg_u8VerifyBuffer))
			{
				u32ChunkSize = sizeof(sg_u8VerifyBuffer);
			}

			// Record the read address
			pu32ReadAddress = (uint32_t *) (psFlashTable->u32PhysicalAddress + (u32RegionOffset - psFlashTable->u32BlockOffset));

			// Copy data into the verify buffer
			memcpy((void *) sg_u8VerifyBuffer,
				   (void *) pu32ReadAddress, 
				   u32ChunkSize);

			// Verify that they match
			pu32VerifyAddress = (uint32_t *) sg_u8VerifyBuffer;
			for (u32Loop = 0; u32Loop < u32ChunkSize; u32Loop += sizeof(*pu32VerifyAddress))
			{
				if (*pu32VerifyAddress != *pu32ReadAddress)
				{
					// Mismatch
					*pu32FailedAddress = (uint32_t) pu32ReadAddress;
					*pu32ExpectedData = *pu32VerifyAddress;
					*pu32GotData = *pu32ReadAddress;
					goto errorExit;
				}

				pu32ReadAddress++;
				pu32VerifyAddress++;
			}

			u32RegionOffset += u32ChunkSize;
			u32ReadSize -= u32ChunkSize;
		}
		else
		{
			++psFlashTable;
		}
	}

	// If we have a remaining set of read bytes, then we didn't read the region
	// properly or fully
errorExit:
	if (u32ReadSize)
	{
		return(false);
	}
	else
	{
		// Otherwise, we did!
		return(true);
	}
}

// Counts the number of entries in a flash segment table (including the terminator)
uint32_t FlashCountSegmentTable(const SFlashSegment *psFlashSegment)
{
	uint32_t u32FlashSegmentCount = 1;	// Add 1 for the terminator

	while (psFlashSegment->eFlashRegion != EFLASHREGION_END)
	{
		++u32FlashSegmentCount;
		++psFlashSegment;
	}

	return(u32FlashSegmentCount);
}

// How big is our search buffer?
#define	SEARCH_BUFFER_SIZE		16384

// This routine will perform the following tasks:
//
// * Determines if the flash region has some content
// * If it does, looks for the version structure within it.
// * If the version structure fails integrity, the region gets erased
//
EVersionCode FlashRegionCheck(EFlashRegion eFlashRegion,
							  EImageType eImageTypeSearch,
							  SImageVersion *psImageVersionCopy,
							  uint32_t *pu32VersionOffset)
{
	EVersionCode eVersionCode = EVERSION_UNKNOWN;
	uint32_t u32FlashSignature[2];
	uint32_t *pu32FlashPointer;
	uint32_t u32FlashRegionSize;
	uint8_t *pu8SearchBufferOriginal = NULL;
	uint8_t *pu8SearchBuffer = NULL;
	uint32_t u32Offset = 0;
	bool bResult;

	// See if the first 8 bytes of the region are all 0xffs or not
	bResult = FlashReadRegion(eFlashRegion,
							  0,
							  (uint8_t *) u32FlashSignature,
							  sizeof(u32FlashSignature));

	if (false == bResult)
	{
		printf("Failed to ReadRegion() - %u\n", eFlashRegion);
		exit(0);
	}

	// If the first 8 bytes (PC and initial stack pointer) are 0xff, 
	// then there's nothing here - just return
	if ((0xffffffff == u32FlashSignature[0]) &&
		(0xffffffff == u32FlashSignature[1]))
	{
		eVersionCode = EVERSION_BAD_STRUCT_CRC;
		goto errorExit;
	}

	// There's something there. Let's run through the region and see if we can
	// find the version structure.

	bResult = FlashRegionGetSize(eFlashRegion,
								 &u32FlashRegionSize);
	if (false == bResult)
	{
		printf("Failed region GetSize() - %u\n", eFlashRegion);
		exit(0);
	}

	// Allocate a search buffer
	pu8SearchBufferOriginal = malloc(SEARCH_BUFFER_SIZE + sizeof(uint32_t));
	if (NULL == pu8SearchBufferOriginal)
	{
		printf("Out of memory when allocating image search buffer\n");
		exit(0);
	}

	// Align our search buffer on 4 byte boundaries, otherwise comparisons won't work
	// properly
	pu8SearchBuffer = (uint8_t *) (((uint32_t) (pu8SearchBufferOriginal + 3)) & ~0x3);

	memset((void *) pu8SearchBuffer, 0xff, SEARCH_BUFFER_SIZE);
	eVersionCode = EVERSION_BAD_STRUCT_CRC;

	// Read in a block at a time so we can search everything
	while (u32Offset < u32FlashRegionSize)
	{
		SImageVersion *psImageVersion = NULL;

		// Move the data over half a buffer size
		memcpy((void *) pu8SearchBuffer,
			   (void *) (pu8SearchBuffer + (SEARCH_BUFFER_SIZE >> 1)),
			   SEARCH_BUFFER_SIZE >> 1);

		// Now read in the next chunk of data
		bResult = FlashReadRegion(eFlashRegion,
								  u32Offset,
								  (uint8_t *) (pu8SearchBuffer + (SEARCH_BUFFER_SIZE >> 1)),
								  SEARCH_BUFFER_SIZE >> 1);
		if (false == bResult)
		{
			printf("Failed to FlashReadRegion() - sliding window offset %u\n", u32Offset);
			exit(0);
		}

		// See if we can find a version structure
		eVersionCode = VersionFindStructure(pu8SearchBuffer,
											SEARCH_BUFFER_SIZE,
											eImageTypeSearch,
											&psImageVersion);
		if ((EVERSION_OK == eVersionCode) &&
			(EIMGTYPE_OPERATIONAL == psImageVersion->eImageType))
		{
			// Found it! Figure out what our actual offset is.
			u32Offset += (((((uint32_t) psImageVersion) - ((uint32_t) pu8SearchBuffer)) - (SEARCH_BUFFER_SIZE >> 1)));

			// u32Offset contains the position of the version info structure
			if (psImageVersionCopy)
			{
				memcpy((void *) psImageVersionCopy, (void *) psImageVersion, sizeof(*psImageVersionCopy));
			}

			// If they want the offset, get it
			if (pu32VersionOffset)
			{
				*pu32VersionOffset = u32Offset;
			}

			goto errorExit;
		}

		u32Offset += (SEARCH_BUFFER_SIZE >> 1);
	}

errorExit:
	if (pu8SearchBufferOriginal)
	{
		free(pu8SearchBufferOriginal);
	}

	return (eVersionCode);
}


