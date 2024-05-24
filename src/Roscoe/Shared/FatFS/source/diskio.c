/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <assert.h>
#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "Shared/Shared.h"
#include "Shared/IDE.h"

#define	DEV_IDE0_MASTER		0
#define	DEV_IDE0_SLAVE		1

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat = 0;
	EStatus eStatus;

	eStatus = IDEGetSize((uint8_t) pdrv,
						 NULL);

	if (eStatus != ESTATUS_OK)
	{
		stat = STA_NODISK;
	}

	return (stat);
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	// This does the same as disk status, since by the time we get here, the
	// disk is already initialized
	return(disk_status(pdrv));
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	EStatus eStatus;

	// Go read 1 or more sectors
	eStatus = IDEReadSector(pdrv,
							sector,
							count,
							buff);

	if (ESTATUS_OK == eStatus)
	{
		return(RES_OK);
	}
	else
	if (ESTATUS_TIMEOUT == eStatus)
	{
		return(RES_NOTRDY);
	}
	else
	{
		// Assume an error
		return(RES_ERROR);
	}
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	EStatus eStatus;

	// Go read 1 or more sectors
	eStatus = IDEWriteSector(pdrv,
							 sector,
							 count,
							 (uint8_t *) buff);

	if (ESTATUS_OK == eStatus)
	{
		return(RES_OK);
	}
	else
	if (ESTATUS_TIMEOUT == eStatus)
	{
		return(RES_NOTRDY);
	}
	else
	{
		// Assume an error
		return(RES_ERROR);
	}
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	EStatus eStatus;
	DSTATUS stat = RES_PARERR;

	if (CTRL_SYNC == cmd)
	{
		stat = RES_OK;
	}

	if (GET_SECTOR_COUNT == cmd)
	{
		eStatus = IDEGetSize((uint8_t) pdrv,
							 (uint64_t *) buff);

		stat = RES_OK;
	}

	if (GET_SECTOR_SIZE == cmd)
	{
		*((uint16_t *) buff) = 512;
		stat = RES_OK;
	}

	if (GET_BLOCK_SIZE == cmd)
	{
		// Unknown
		*((uint32_t *) buff) = 1;
		stat = RES_OK;
	}
   
	return(stat);
}

