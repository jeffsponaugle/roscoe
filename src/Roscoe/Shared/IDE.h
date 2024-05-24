#ifndef _IDE_H_
#define _IDE_H_

extern EStatus IDEProbe(void);
extern EStatus IDEIdentify(uint8_t u8Disk,
						   volatile uint8_t *pu8IdentifySectorBase);
extern EStatus IDEReadWriteSectors(uint8_t u8Disk,
								   uint64_t u64Sector,
								   uint64_t u64SectorCount,
								   uint8_t *pu8Buffer,
								   bool bWrite);
extern EStatus IDEGetSize(uint8_t u8Disk,
						  uint64_t *pu64SectorCount);
#define	IDEReadSector(disk, sector, sector_count, buffer)	IDEReadWriteSectors(disk, sector, sector_count, buffer, false)
#define	IDEWriteSector(disk, sector, sector_count, buffer)	IDEReadWriteSectors(disk, sector, sector_count, buffer, true)

#define	IDE_REG8(csbase, reg)		*((volatile uint8_t *) (csbase + (reg << 5)))
#define	IDE_REG16(csbase, reg)		*((volatile uint16_t *) (csbase + (reg << 5)))
#define	IDE_REG32(csbase, reg)		*((volatile uint32_t *) (csbase + (reg << 5)))

// Default wait time (in milliseconds)
#define	IDE_DEFAULT_WAIT_MS		10000

// 100ms timer counter
#define	IDE_TIMEOUT_COUNTER	   	1

// IDE Registers
#define	IDE_REG_DATA				IDE_REG32(ROSCOE_IDE_1_CSA, 0)
#define	IDE_REG_ERROR				IDE_REG8(ROSCOE_IDE_1_CSA, 1)
#define	IDE_REG_SECCOUNT0			IDE_REG8(ROSCOE_IDE_1_CSA, 2)
#define	IDE_REG_LBA0  		 		IDE_REG8(ROSCOE_IDE_1_CSA, 3)
#define	IDE_REG_LBA1				IDE_REG8(ROSCOE_IDE_1_CSA, 4)
#define	IDE_REG_LBA2				IDE_REG8(ROSCOE_IDE_1_CSA, 5)
#define	IDE_REG_STATUS				IDE_REG8(ROSCOE_IDE_1_CSA, 7)
#define	IDE_REG_CMD					IDE_REG8(ROSCOE_IDE_1_CSA, 7)
#define	IDE_REG_HDDEVSEL			IDE_REG8(ROSCOE_IDE_1_CSA, 6)
#define	IDE_REG_SECCOUNT1			IDE_REG8(ROSCOE_IDE_1_CSB, 0)
#define	IDE_REG_LBA3				IDE_REG8(ROSCOE_IDE_1_CSB, 1)
#define	IDE_REG_LBA4				IDE_REG8(ROSCOE_IDE_1_CSB, 2)
#define	IDE_REG_LBA5				IDE_REG8(ROSCOE_IDE_1_CSB, 3)

// IDE Commands
#define	IDE_CMD_READ_PIO		0x20
#define	IDE_CMD_WRITE_PIO		0x30
#define	IDE_CMD_SPINDOWN		0xe0
#define	IDE_CMD_SPINUP			0xe1
#define	IDE_CMD_IDENTIFY   		0xec

// IDE Status bits
#define	IDE_STATUS_BUSY			0x80
#define IDE_STATUS_DISK_READY	0x40
#define IDE_STATUS_WRITE_FAULT	0x20
#define IDE_STATUS_SEEK_DONE	0x10
#define IDE_STATUS_DATA_READY	0x08
#define IDE_STATUS_CORRECTED	0x04
#define IDE_STATUS_INDEX		0x02
#define IDE_STATUS_ERROR		0x01

// IDE Error register
#define	IDE_ERR_ADDR_NOT_FOUND	0x01
#define	IDE_ERR_TZ_NOT_FOUND	0x02
#define	IDE_ERR_ABORTED_CMD		0x04
#define	IDE_ERR_MEDIA_CHANGE	0x08
#define	IDE_ERR_ID_NOT_FOUND	0x10
#define	IDE_ERR_MEDIA_CHANGED	0x20
#define	IDE_ERR_UNCORRECTABLE	0x40
#define	IDE_ERR_BAD_BLOCK		0x80

// Master/slave select commands (basic)
#define	IDE_MASTER				0xa0
#define	IDE_SLAVE				0xb0

#endif

