#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <machine/endian.h>
#include "BIOS/OS.h"
#include "Shared/IDE.h"
#include "Hardware/Roscoe.h"
#include "Shared/AsmUtils.h"
#include "Shared/Shared.h"
#include "Shared/ptc.h"

// Uncomment if you want to use the optimized/more efficient sector moving from IDE
#define	FAST_SECTOR_MOVE		1

// IDE Select command bits
typedef enum
{
	EIDESTATE_NOT_PROBED,
	EIDESTATE_NOT_PRESENT,
	EIDESTATE_PRESENT
} EIDEState;

// Block mode
typedef enum
{
	EIDEBLOCK_UNKNOWN,
	EIDEBLOCK_CHS,
	EIDEBLOCK_LBA28,
	EIDEBLOCK_LBA48
} EIDEBlockMode;

// Structure containing information about the disk
typedef struct SIDEInfo
{
	EIDEState eState;					// What's the state of this disk
	char eDriveModel[40];				// Textual model of the drive
	bool bMaster;						// Is this disk the master?
	uint16_t u16DeviceType;				// Device type
	uint16_t u16Capabilities;			// Capabilities
	uint32_t u32CommandSets;			// Command sets supported
	uint64_t u64DiskSectorCount;   		// How many sectors does this disk have?
	EIDEBlockMode eBlockMode;			// What block mode does this disk support?
} SIDEDiskInfo;

// Set TRUE if we have the master disk currently selected
static bool sg_bMasterSelected = false;

// Information about the disks in this system
static SIDEDiskInfo sg_sDisks[2];

// Quick and dirty method of detecting disk presence
static EStatus IDECheckPresence(SIDEDiskInfo *psDisk)
{
	EStatus eStatus = ESTATUS_OK;

	// Check the status register and see if it's 0x00 or 0xff. Those aren't ever
	// going to be set in practice so make the quick assumption the disk isn't there
	if ((0xff == IDE_REG_STATUS) ||
		(0x00 == IDE_REG_STATUS))
	{
		eStatus = ESTATUS_DISK_NOT_PRESENT;
	}

	return(eStatus);
}

// Select the master or the slave disk
void IDESelect(bool bMaster)
{
	EStatus eStatus;

	// Select the appropriate disk
	if (bMaster)
	{
		IDE_REG_HDDEVSEL = IDE_MASTER;
	}
	else
	{
		IDE_REG_HDDEVSEL = IDE_SLAVE;
	}

	sg_bMasterSelected = bMaster;

	// Wait 1 millisecond after issuing the select command (IDE spec)
	SharedSleep(1000);
}

// Easy macros for waiting for busy only or busy and data request ready
#define		IDEWaitBusy()		IDEWait(IDE_STATUS_BUSY, 0, IDE_DEFAULT_WAIT_MS)
#define		IDEWaitBusyDRQ()   	IDEWait(IDE_STATUS_BUSY | IDE_STATUS_DATA_READY, IDE_STATUS_DATA_READY, IDE_DEFAULT_WAIT_MS)

// Wait for a condition from the IDE disk
static EStatus IDEWait(uint8_t u8Mask,
					   uint8_t u8Equals,
					   uint32_t u32Timeout)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32InterruptCounterLast;
	uint32_t u32InterruptCounter;

	// Get the initial interrupt counter
	eStatus = PTCGetInterruptCounter(IDE_TIMEOUT_COUNTER,
									 &u32InterruptCounterLast);
	ERR_GOTO();

	for (;;)
	{
		uint8_t u8Status;

		// If what we're looking for matches, return!
		u8Status = IDE_REG_STATUS;

		if (u8Status & IDE_STATUS_ERROR)
		{
			// We have an error. Figure out what it is
			if (u8Status & IDE_STATUS_WRITE_FAULT)
			{
				eStatus = ESTATUS_DISK_WRITE_FAULT;
				goto errorExit;
			}

			// If we have a regular error, then read the error register
			if (u8Status & IDE_STATUS_ERROR)
			{
				u8Status = IDE_REG_ERROR;

				if (u8Status & IDE_ERR_BAD_BLOCK)
				{
					eStatus = ESTATUS_DISK_BAD_BLOCK;
					goto errorExit;
				}

				if (u8Status & IDE_ERR_UNCORRECTABLE)
				{
					eStatus = ESTATUS_DISK_UNCORRECTABLE;
					goto errorExit;
				}
			}

			// Unknown disk error
			eStatus = ESTATUS_DISK_ERROR;
			goto errorExit;
		}

		// If we have what we're looking for, return
		if ((u8Status & u8Mask) == u8Equals)
		{
			goto errorExit;
		}

		if (0 == u32Timeout)
		{
			eStatus = ESTATUS_TIMEOUT;
			goto errorExit;
		}

		// Get the counter's current value
		eStatus = PTCGetInterruptCounter(IDE_TIMEOUT_COUNTER,
										 &u32InterruptCounter);
		ERR_GOTO();

		// Has the tick count changed? If so, then subtract from our timeout time
		if (u32InterruptCounterLast != u32InterruptCounter)
		{
			uint32_t u32Diff;

			// It changed!
			u32Diff = (u32InterruptCounter - u32InterruptCounterLast) * (1000 / PTC_COUNTER1_HZ);

			if (u32Diff > u32Timeout)
			{
				u32Timeout = 0;
			}
			else
			{
				u32Timeout -= u32Diff;
			}

			// Record for later
			u32InterruptCounterLast = u32InterruptCounter;
		}
	}

errorExit:
	return(eStatus);
}

// Selects a block # - LBA or CHS - and # of sectors to traansfer
static void IDESectorSelect(uint8_t u8Disk,
							uint64_t u64Sector,
							uint8_t u8SectorCount)
{
	SIDEDiskInfo *psDisk;
	uint8_t u8ModeSel;
	uint16_t u16Cylinder;
	uint8_t u8Sector;
	uint8_t u8Head;

	psDisk = &sg_sDisks[u8Disk];

	if (psDisk->bMaster)
	{
		u8ModeSel = IDE_MASTER;
	}
	else 
	{
		u8ModeSel = IDE_SLAVE;
	}

	if (EIDEBLOCK_CHS == psDisk->eBlockMode)
	{
		// CHS
		u8Sector = (u64Sector % 63) + 1;
		u16Cylinder = ((u64Sector + 1) - (u8Sector)) / (16 * 63);
		u8Head = (uint8_t) (u16Cylinder / 63);
		u8ModeSel |= (u8Head & 0xf);

		IDE_REG_LBA0 = (uint8_t) u8Sector;
		IDE_REG_LBA1 = (uint8_t) (u16Cylinder >> 0);
		IDE_REG_LBA2 = (uint8_t) (u16Cylinder);

		IDE_REG_HDDEVSEL = u8ModeSel;
		return;
	}
	else
	{
		// Select LBA
		u8ModeSel |= 0x40;	// Lower 4 bits are the high 4 LBA bits and 0x40 is LBA mode

		// LBA28/48
		if (EIDEBLOCK_LBA28 == psDisk->eBlockMode)
		{
			// If LBA28, upper 4 bits of address block is in the head
			u8ModeSel |= ((u64Sector >> 24) & 0xf);	// Lower 4 bits are the high 4 LBA bits
		}
	}

	IDE_REG_HDDEVSEL = u8ModeSel;

	if (EIDEBLOCK_LBA48 == psDisk->eBlockMode)
	{
		IDE_REG_SECCOUNT1 = u8SectorCount;
		IDE_REG_LBA3 = (uint8_t) (u64Sector >> 24);
		IDE_REG_LBA4 = (uint8_t) (u64Sector >> 32);
		IDE_REG_LBA5 = (uint8_t) (u64Sector >> 40);
	}

	IDE_REG_SECCOUNT0 = u8SectorCount;
	IDE_REG_LBA0 = (uint8_t) u64Sector;
	IDE_REG_LBA1 = (uint8_t) (u64Sector >> 8);
	IDE_REG_LBA2 = (uint8_t) (u64Sector >> 16);
}

// Return the disk identify data
static EStatus IDEIdentifyInternal(volatile uint8_t *pu8IdentifyStructure)
{
	EStatus eStatus;
	uint8_t u8Status;
	uint16_t u16Count = 128;
	uint32_t u32Timeout = 100000;
	uint16_t *pu16Data;

	eStatus = IDEWaitBusy();
	ERR_GOTO();

	// Send the identify command
	IDE_REG_CMD = IDE_CMD_IDENTIFY;

	// Wait 1ms after issuing the identify command
	SharedSleep(1000);

	// Now look at the resulting status
	eStatus = IDEWaitBusyDRQ();
	ERR_GOTO();

#ifdef FAST_SECTOR_MOVE
	IDESectorMoveRead((void *) pu8IdentifyStructure);
#else
	// Now read 256 16 bit words
	while (u16Count)
	{
		*((uint32_t *) pu8IdentifyStructure) = IDE_REG_DATA;
		pu8IdentifyStructure += sizeof(uint32_t);
		u16Count--;
	}

	// Adjust the pointer becuase we swap it back later
	pu8IdentifyStructure -= 0x200;
#endif

	// flip all the uint16_ts around since we're wired up little endian
	// and this is a big endian machine
	u16Count = 256;
	pu16Data = (uint16_t *) pu8IdentifyStructure;
	while (u16Count)
	{
		*pu16Data = __bswap16(*pu16Data);
		++pu16Data;
		u16Count--;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Get # of sectors for this disk
EStatus IDEGetSize(uint8_t u8Disk,
				   uint64_t *pu64SectorCount)
{
	EStatus eStatus;

	// Check to see if the disk is present
	if (u8Disk >= (sizeof(sg_sDisks) / sizeof(sg_sDisks[0])))
	{
		eStatus = ESTATUS_DISK_OUT_OF_RANGE;
		goto errorExit;
	}

	// And see if the disk is there
	if (sg_sDisks[u8Disk].eState != EIDESTATE_PRESENT)
	{
		eStatus = ESTATUS_DISK_NOT_PRESENT;
		goto errorExit;
	}

	if (pu64SectorCount)
	{
		*pu64SectorCount = sg_sDisks[u8Disk].u64DiskSectorCount;
	}
	
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Checks that the disk, sector #s and count are in range, and the buffer is
// uint32_t aligned
static EStatus IDECheckRanges(uint8_t u8Disk,
							  uint64_t u64Sector,
							  uint64_t u64SectorCount,
							  uint8_t *pu8Buffer)
{
	EStatus eStatus;

	// Check to see if the disk is present
	if (u8Disk >= (sizeof(sg_sDisks) / sizeof(sg_sDisks[0])))
	{
		eStatus = ESTATUS_DISK_OUT_OF_RANGE;
		goto errorExit;
	}

	// And see if the disk is there
	if (sg_sDisks[u8Disk].eState != EIDESTATE_PRESENT)
	{
		eStatus = ESTATUS_DISK_NOT_PRESENT;
		goto errorExit;
	}

	// And see if the sector reads are out of range
	if ((u64Sector + u64SectorCount) > sg_sDisks[u8Disk].u64DiskSectorCount)
	{
		eStatus = ESTATUS_DISK_SECTOR_OUT_OF_RANGE;
		goto errorExit;
	}

	// Can't be no sectors!
	if (0 == u64SectorCount)
	{
		eStatus = ESTATUS_DISK_SECTOR_COUNT_0;
		goto errorExit;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

EStatus IDEReadWriteSectors(uint8_t u8Disk,
							uint64_t u64Sector,
							uint64_t u64SectorCount,
							uint8_t *pu8Buffer,
							bool bWrite)
{
	EStatus eStatus;
	uint32_t u32Loop;
	bool bUnaligned = false;

	eStatus = IDECheckRanges(u8Disk,
							 u64Sector,
							 u64SectorCount,
							 pu8Buffer);
	ERR_GOTO();

	// If not 4 byte aligned, use the older/slower transfer method
	if (((uint32_t) pu8Buffer) & 3)
	{
		bUnaligned = true;
	}

	while (u64SectorCount)
	{
		uint8_t u8SectorChunk;
		uint8_t u8SectorCounter;

		if (u64SectorCount > 255)
		{
			u8SectorChunk = 255;
		}
		else
		{
			u8SectorChunk = (uint8_t) u64SectorCount;
		}

		u8SectorCounter = u8SectorChunk;

		// # Of 16 bit quantities to move
		u32Loop = (u8SectorChunk << 8);

		// Wait for the disk to be idle
		eStatus = IDEWaitBusy();
		ERR_GOTO();

		// Go select the sector
		IDESectorSelect(u8Disk,
						u64Sector,
						u8SectorChunk);

		if (bWrite)
		{
			// Now issue the command to write
			IDE_REG_CMD = IDE_CMD_WRITE_PIO;

			while (u8SectorCounter)
			{
				// Wait until the disk is ready to accept the data
				eStatus = IDEWaitBusyDRQ();
				ERR_GOTO();

				if (bUnaligned)
				{
					IDESectorMoveWrite((void *) pu8Buffer);
					pu8Buffer += (1 << 9);
				}
				else
				{
					u32Loop = 128;
					while (u32Loop)
					{
						IDE_REG_DATA = *((uint32_t *) pu8Buffer);
						pu8Buffer += sizeof(uint32_t);
						u32Loop--;
					}
				}

				--u8SectorCounter;
			}
		}
		else
		{
			// Now issue the command to read
			IDE_REG_CMD = IDE_CMD_READ_PIO;

			while (u8SectorCounter)
			{
				// Wait until we have data
				eStatus = IDEWaitBusyDRQ();
				ERR_GOTO();

				if (bUnaligned)
				{
					u32Loop = 128;
					while (u32Loop)
					{
						*((uint32_t *) pu8Buffer) = IDE_REG_DATA;
						pu8Buffer += sizeof(uint32_t);
						u32Loop--;
					}
				}
				else
				{
					IDESectorMoveRead((void *) pu8Buffer);
					pu8Buffer += (1 << 9);
				}

				--u8SectorCounter;
			}
		}

		u64SectorCount -= u8SectorChunk;
		u64Sector += u8SectorChunk;
	}

	// Got the sector(s)!
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Returns identification structure of the selected disk
EStatus IDEIdentify(uint8_t u8Disk,
					volatile uint8_t *pu8IdentifySectorBase)
{
	EStatus eStatus;

	// Don't actually get the size. Just use it to determine if the disk index is
	// in range or not
	eStatus = IDEGetSize(u8Disk,
						 NULL);
	ERR_GOTO();

	// Select the appropriate disk
	IDESelect(sg_sDisks[u8Disk].bMaster);

	// Now the identify command
	eStatus = IDEIdentifyInternal(pu8IdentifySectorBase);

errorExit:
	return(eStatus);
}

// Identify structure offsets
#define	IDE_DEVICETYPE			0
#define	IDE_IDENT_MODEL			54
#define	IDE_CAPABILITIES		98
#define	IDE_MAX_LBA				120
#define	IDE_COMMANDSETS			164
#define	IDE_MAX_LBA_EXT			200

static EStatus IDEProbeDisk(SIDEDiskInfo *psDisk,
							bool bMaster)
{
	EStatus eStatus;
	uint8_t u8Loop;
	volatile uint8_t *pu8IdentifyInfo = NULL;
	volatile uint16_t *pu16IdentifyInfo;
	char *peDisk = "Master";
	char *peSectorAccess = NULL;

	ZERO_STRUCT(*psDisk);
	psDisk->eState = EIDESTATE_NOT_PROBED;

	if (false == bMaster)
	{
		peDisk = "Slave ";
	}

	// 512 Byte buffer for identify stuff
	pu8IdentifyInfo = malloc(512);
	pu16IdentifyInfo = (uint16_t *) pu8IdentifyInfo;
	if (NULL == pu8IdentifyInfo)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	// Select the disk
	IDESelect(bMaster);

	psDisk->bMaster = bMaster;

	eStatus = IDECheckPresence(psDisk);
	if (ESTATUS_OK == eStatus)
	{
		// Only probe if we've discovered a disk
		eStatus = IDEIdentifyInternal(pu8IdentifyInfo);
	}

	if (ESTATUS_OK == eStatus)
	{
		uint8_t u8Loop;
		uint8_t u8LastNonSpace;

		psDisk->eState = EIDESTATE_PRESENT;

		memcpy((void *) psDisk->eDriveModel,
			   (void *) (pu8IdentifyInfo + IDE_IDENT_MODEL),
			   sizeof(psDisk->eDriveModel));

		// Zero terminate it
		psDisk->eDriveModel[sizeof(psDisk->eDriveModel) - 1] = '\0';

		// Get rid of trailing spaces
		u8LastNonSpace = 0;
		for (u8Loop = 0; u8Loop < sizeof(psDisk->eDriveModel); u8Loop++)
		{
			if ('\0' == psDisk->eDriveModel[u8Loop])
			{
				// End of string
				break;
			}

			if (psDisk->eDriveModel[u8Loop] != ' ')
			{
				u8LastNonSpace = u8Loop;
			}
		}

		// Pull out various things we might care about
		psDisk->u16DeviceType = *((volatile uint16_t *) (pu8IdentifyInfo + IDE_DEVICETYPE));
		psDisk->u16Capabilities = *((volatile uint16_t *) (pu8IdentifyInfo + IDE_CAPABILITIES));
		psDisk->u32CommandSets = (pu16IdentifyInfo[IDE_COMMANDSETS >> 1]) |
								 ((pu16IdentifyInfo[(IDE_COMMANDSETS + 2) >> 1]) << 16);

		// Zero terminate the last significant character
		psDisk->eDriveModel[u8LastNonSpace + 1] = '\0';

		if (psDisk->u16Capabilities & 0x200)
		{
			// Supports LBA
			// Figure out if we're 48 or 28 bit addressing
			if (psDisk->u32CommandSets & (1 << 26))
			{
				// 48 Bit LBA
				psDisk->u64DiskSectorCount = (pu16IdentifyInfo[IDE_MAX_LBA_EXT >> 1]) |
											 ((pu16IdentifyInfo[(IDE_MAX_LBA_EXT + 2) >> 1]) << 16);
				psDisk->eBlockMode = EIDEBLOCK_LBA48;
				peSectorAccess = "LBA48";
			}
			else
			{
				// 28 Bit LBA
				psDisk->u64DiskSectorCount = (pu16IdentifyInfo[IDE_MAX_LBA >> 1]) |
											 ((pu16IdentifyInfo[(IDE_MAX_LBA + 2) >> 1]) << 16);
				peSectorAccess = "LBA28";
				psDisk->eBlockMode = EIDEBLOCK_LBA28;
			}
		}
		else
		{
			// CHS Only
			peSectorAccess = "CHS";
			psDisk->eBlockMode = EIDEBLOCK_CHS;
		}

		// Convert # of 512 byte sectors to megabytes
		printf("IDE %s         : %s - %uMB (%s)\n", peDisk, psDisk->eDriveModel, (uint32_t) (psDisk->u64DiskSectorCount >> 11), peSectorAccess);
	}
	else
	{
		printf("IDE %s         : Not present\n", peDisk);
		psDisk->eState = EIDESTATE_NOT_PRESENT;
	}

errorExit:
	if (pu8IdentifyInfo)
	{
		free((char *) pu8IdentifyInfo);
	}

	return (eStatus);
}

// Probe disks
EStatus IDEProbe(void)
{
	EStatus eStatus;

	ZERO_STRUCT(sg_sDisks);

	// Look for the master
	eStatus = IDEProbeDisk(&sg_sDisks[0],
						   true);

	// And the slave
	eStatus = IDEProbeDisk(&sg_sDisks[1],
						   false);

	return(ESTATUS_OK);
}

