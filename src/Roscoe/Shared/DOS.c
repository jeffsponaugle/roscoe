#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include "Shared/Shared.h"
#include "Hardware/Roscoe.h"
#include "Shared/DOS.h"
#include "Shared/FatFS/source/ff.h"
#include "Shared/IDE.h"
#include "Shared/Stream.h"

// How much RAM FatFS has to work with when making a new filesystem
#define		MKFS_WORK_RAM		(1024*64)

// Work area for each volume
static FATFS *sg_psFATFS[2];

EStatus DOSGetDrivePath(char *peDrivePath, size_t eMaxLength)
{
	FRESULT eFatFSErrorCode;

	*peDrivePath = '\0';
	eFatFSErrorCode	= f_getcwd(peDrivePath,
							   eMaxLength);

	if (eFatFSErrorCode != FR_OK)
	{
		printf("Error code=%u\n", eFatFSErrorCode);
	}

	return(FatFSToEStatus(eFatFSErrorCode));
}

static EStatus InitFilesystem(uint8_t u8Drive,
							  uint64_t *pu64TotalDriveSpace,
							  uint64_t *pu64TotalAvailable)
{
	EStatus eStatus;
	char eDriveString[3];
	uint8_t u8FatFSErrorCode;

	eDriveString[0] = '0' + u8Drive;
	eDriveString[1] = ':';
	eDriveString[2] = '\0';

	if (sg_psFATFS[u8Drive])
	{
		u8FatFSErrorCode = f_unmount(eDriveString);
		eStatus = FatFSToEStatus(u8FatFSErrorCode);

		free(sg_psFATFS[u8Drive]);
		sg_psFATFS[u8Drive] = NULL;
	}

	sg_psFATFS[u8Drive] = malloc(sizeof(*sg_psFATFS[u8Drive]));
	if (NULL == sg_psFATFS[u8Drive])
	{
		printf("Out of memory\n");
		goto errorExit;
	}

	u8FatFSErrorCode = f_mount(sg_psFATFS[u8Drive],
							   eDriveString,
							   1);
	eStatus = FatFSToEStatus(u8FatFSErrorCode);

	if (ESTATUS_OK == eStatus)
	{
		uint64_t u64TotalSectors;
		DWORD u32FreeClusters;

		u8FatFSErrorCode = f_getfree(eDriveString,
								     &u32FreeClusters,
									 &sg_psFATFS[u8Drive]);
		eStatus = FatFSToEStatus(u8FatFSErrorCode);

		if (ESTATUS_OK == eStatus)
		{
			if (pu64TotalDriveSpace)
			{
				*pu64TotalDriveSpace = ((uint64_t) sg_psFATFS[u8Drive]->n_fatent - 2) * ((uint64_t) sg_psFATFS[u8Drive]->csize * (uint64_t) FF_MAX_SS);
			}

			if (pu64TotalAvailable)
			{
				*pu64TotalAvailable = ((uint64_t) u32FreeClusters) * ((uint64_t) sg_psFATFS[u8Drive]->csize * (uint64_t) FF_MAX_SS);
			}
		}
		else
		{
			printf("f_getfree failed - %s\n", GetErrorText(eStatus));
		}
	}

errorExit:
	return(eStatus);
}

EStatus DOSInit(void)
{
	EStatus eStatus;
	uint8_t u8FatFSErrorCode;
	uint64_t u64TotalDriveSpace;
	uint64_t u64TotalAvailable;

	// Try mounting the master disk
	eStatus = InitFilesystem(0,
							 &u64TotalDriveSpace,
							 &u64TotalAvailable);
	if (eStatus != ESTATUS_OK)
	{
		printf("Master FS mount : %s\n", GetErrorText(eStatus));
	}
	else
	{
		printf("Master FS mount : %uK Total, %uK available\n", (uint32_t) (u64TotalDriveSpace >> 10), (uint32_t) (u64TotalAvailable >> 10));
	}

	eStatus = InitFilesystem(1,
							 &u64TotalDriveSpace,
							 &u64TotalAvailable);
	if (eStatus != ESTATUS_OK)
	{
		printf("Slave FS mount  : %s\n", GetErrorText(eStatus));
	}
	else
	{
		printf("Slave FS mount  : %uK Total, %uK available\n", (uint32_t) (u64TotalDriveSpace >> 10), (uint32_t) (u64TotalAvailable >> 10));
	}

errorExit:
	return(eStatus);
}

EStatus DOSShutdown(void)
{
	EStatus eStatus;
	uint8_t u8FatFSErrorCode;

	// Try mounting the master disk
	u8FatFSErrorCode = f_unmount("0:");
	eStatus = FatFSToEStatus(u8FatFSErrorCode);
	if (eStatus != ESTATUS_OK)
	{
		printf("Master FS unmount: %s\n", GetErrorText(eStatus));
	}

	// Now the slave
	u8FatFSErrorCode = f_unmount("1:");
	eStatus = FatFSToEStatus(u8FatFSErrorCode);
	if (eStatus != ESTATUS_OK)
	{
		printf("Slave FS unmount : %s\n", GetErrorText(eStatus));
	}

errorExit:
	return(eStatus);
}

EStatus DOSFormat(SLex *psLex,
				  const struct SMonitorCommands *psMonitorCommand,
				  uint32_t *pu32Address)
{
	EStatus eStatus;
	uint8_t u8FatFSErrorCode;
	uint8_t *pu8TempSpace = NULL;
	MKFS_PARM sFSParms;

	// Unmount everything
	(void) DOSShutdown();

	printf("Making filesystem\n");

	pu8TempSpace = malloc(MKFS_WORK_RAM);
	if (NULL == pu8TempSpace)
	{
		printf("Out of memory while attempting to create a format buffer\n");
		goto errorExit;
	}

	ZERO_STRUCT(sFSParms);

	sFSParms.fmt = FM_EXFAT;

	// Let's see if they've provided a disk - either 0: or 1:
	u8FatFSErrorCode = f_mkfs("0:",
							  (const MKFS_PARM *) &sFSParms,
							  pu8TempSpace,
							  MKFS_WORK_RAM);
	free(pu8TempSpace);
	eStatus = FatFSToEStatus(u8FatFSErrorCode);

	if (eStatus != ESTATUS_OK)
	{
		printf("Result of format - %s\n", GetErrorText(eStatus));
	}
	else
	{
		// Now remount everything
		eStatus = DOSInit();
	}

errorExit:
	return(ESTATUS_OK);
}

EStatus DOSMkdir(SLex *psLex,
				 const struct SMonitorCommands *psMonitorCommand,
				 uint32_t *pu32Address)
{
	EStatus eStatus;
	uint8_t u8FatFSErrorCode;
	char *peBufferPtr = NULL;

	eStatus = LexGetBufferPosition(psLex,
								   &peBufferPtr);
	assert(ESTATUS_OK == eStatus);

	u8FatFSErrorCode = f_mkdir(peBufferPtr);
	eStatus = FatFSToEStatus(u8FatFSErrorCode);

	if (eStatus != ESTATUS_OK)
	{
		printf("mkdir failed - %s\n", GetErrorText(eStatus));
	}
	else
	{
		// All good
		printf("Directory '%s' created\n", peBufferPtr);
	}

errorExit:
	return(ESTATUS_OK);
}

EStatus DOSRmdir(SLex *psLex,
				 const struct SMonitorCommands *psMonitorCommand,
				 uint32_t *pu32Address)
{
	EStatus eStatus;
	uint8_t u8FatFSErrorCode;
	char *peBufferPtr = NULL;

	eStatus = LexGetBufferPosition(psLex,
								   &peBufferPtr);
	assert(ESTATUS_OK == eStatus);

	u8FatFSErrorCode = f_rmdir(peBufferPtr);
	eStatus = FatFSToEStatus(u8FatFSErrorCode);

	if (eStatus != ESTATUS_OK)
	{
		printf("rmdir failed - %s\n", GetErrorText(eStatus));
	}
	else
	{
		// All good
		printf("Directory '%s' removed\n", peBufferPtr);
	}

errorExit:
	return(ESTATUS_OK);
}


EStatus DOSChdir(SLex *psLex,
				 const struct SMonitorCommands *psMonitorCommand,
				 uint32_t *pu32Address)
{
	EStatus eStatus;
	uint8_t u8FatFSErrorCode;
	char *peBufferPtr = NULL;

	eStatus = LexGetBufferPosition(psLex,
								   &peBufferPtr);
	assert(ESTATUS_OK == eStatus);

	u8FatFSErrorCode = f_chdir(peBufferPtr);
	eStatus = FatFSToEStatus(u8FatFSErrorCode);

	if (eStatus != ESTATUS_OK)
	{
		printf("chdir failed - %s\n", GetErrorText(eStatus));
	}
	else
	{
		// All good
		printf("Changed to '%s'\n", peBufferPtr);
	}

errorExit:
	return(ESTATUS_OK);
}

typedef struct SFileInfo
{
	char *peFilename;
	uint16_t u16Date;
	uint16_t u16Time;
	uint64_t u64FileSize;
	uint8_t u8Attribute;

	struct SFileInfo *psNextLink;
} SFileInfo;

static void DOSDestroyFileInfo(SFileInfo **ppsFileInfoHead)
{
	SFileInfo *psFileInfo;

	while (*ppsFileInfoHead)
	{
		psFileInfo = *ppsFileInfoHead;
		*ppsFileInfoHead = (*ppsFileInfoHead)->psNextLink;

		free(psFileInfo->peFilename);
		free(psFileInfo);
	}
}

static EStatus DOSAbsorbDirectory(DIR *psDir,
								  SFileInfo **ppsFileInfoHead)
{
	SFileInfo *psFileInfo = NULL;
	FILINFO sFileInfo;
	EStatus eStatus;
	uint8_t u8FatFSErrorCode;

	for (;;)
	{
		u8FatFSErrorCode = f_readdir(psDir,
									 &sFileInfo);
		eStatus = FatFSToEStatus(u8FatFSErrorCode);

		if (eStatus != ESTATUS_OK)
		{
			printf("f_readdir failed - %s\n", GetErrorText(eStatus));
			goto errorExit;
		}

		// If we're OK, but nothing in the name field, we're done
		if ('\0' == sFileInfo.fname[0])
		{
			// Done!
			break;
		}
		else
		{
			// Time to make a node out of this.
			if (psFileInfo)
			{
				psFileInfo->psNextLink = calloc(1, sizeof(*psFileInfo->psNextLink));
				if (NULL == psFileInfo->psNextLink)
				{
					eStatus = ESTATUS_OUT_OF_MEMORY;
					goto errorExit;
				}

				psFileInfo = psFileInfo->psNextLink;
			}
			else
			{
				*ppsFileInfoHead = calloc(1, sizeof(**ppsFileInfoHead));
				if (NULL == *ppsFileInfoHead)
				{
					eStatus = ESTATUS_OUT_OF_MEMORY;
					goto errorExit;
				}

				psFileInfo = *ppsFileInfoHead;
			}

			psFileInfo->peFilename = strdup(sFileInfo.fname);
			if (NULL == psFileInfo->peFilename)
			{
				eStatus = ESTATUS_OUT_OF_MEMORY;
				goto errorExit;
			}

			// Record type
			psFileInfo->u8Attribute = sFileInfo.fattrib;

			// And size
			psFileInfo->u64FileSize = sFileInfo.fsize;

			// fdate:
			//  Bits 15:9 - Year origin from 1980 (0..127)
			//  Bits  8:5 - Month (1-12)
			//  Bits  4:0 - Day   (1-31)
			//
			// ftime:
			//  Bits 15:11 - Hour (0-23)
			//  Bits 10:5  - Minute (0-59)
			//  Bits 4:0   - Second / 2 (0-29)

			psFileInfo->u16Date = sFileInfo.fdate;
			psFileInfo->u16Time = sFileInfo.ftime;
		}
	}

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		DOSDestroyFileInfo(ppsFileInfoHead);
	}
	return(eStatus);
}

#define	POSTCODE_D1           ((POST_7SEG_HEX_A << 8) + POST_7SEG_HEX_1)
#define	POSTCODE_D2           ((POST_7SEG_HEX_A << 8) + POST_7SEG_HEX_2)
#define	POSTCODE_D3           ((POST_7SEG_HEX_A << 8) + POST_7SEG_HEX_3)
#define	POSTCODE_D4           ((POST_7SEG_HEX_A << 8) + POST_7SEG_HEX_4)
#define	POSTCODE_D5           ((POST_7SEG_HEX_A << 8) + POST_7SEG_HEX_5)

EStatus DOSDir(SLex *psLex,
			   const struct SMonitorCommands *psMonitorCommand,
			   uint32_t *pu32Address)
{
	EStatus eStatus = ESTATUS_OK;
	DIR sDir;
	char eCWD[256];
	uint8_t u8FatFSErrorCode;
	bool bDirOpened = false;
	SFileInfo *psFileInfo = NULL;
	SFileInfo *psFileInfoHead = NULL;
	uint32_t u32TotalFiles = 0;
	uint32_t u32TotalDirs = 0;
	uint64_t u64TotalSize = 0;
	char eSize[25];
	uint64_t u64TotalSectors;
	DWORD u32FreeClusters;

	ZERO_STRUCT(sDir);

	// Get our current working directory
	eStatus = DOSGetDrivePath(eCWD,
							  sizeof(eCWD) - 1);
	ERR_GOTO();

	// Open up the dir
	u8FatFSErrorCode = f_opendir(&sDir,
								 eCWD);
	eStatus = FatFSToEStatus(u8FatFSErrorCode);
	ERR_GOTO();

	bDirOpened = true;

	// Go absorb all the files
	eStatus = DOSAbsorbDirectory(&sDir,
								 &psFileInfoHead);
	ERR_GOTO();

	psFileInfo = psFileInfoHead;

	// Display the directory
	while (psFileInfo)
	{
		if (psFileInfo->u8Attribute & AM_DIR)
		{
			POST_SET(POSTCODE_D1);
			printf("%.4u-%.2u-%.2u %.2u:%.2u:%.2u %-20s %s\n",
				   ((psFileInfo->u16Date >> 9) + 1980), (psFileInfo->u16Date >> 5) & 0x0f, psFileInfo->u16Date & 0x1f,
				   (psFileInfo->u16Time >> 11), (psFileInfo->u16Time >> 5) & 0x3f, (psFileInfo->u16Time & 0x1f) << 1,
				   "<DIR>",
				   psFileInfo->peFilename);
			POST_SET(POSTCODE_D4);
			if (u32TotalDirs != 0xffffffff)
			{
				u32TotalDirs++;
			}
		}
		else
		{
			POST_SET(POSTCODE_D2);
			UINT64ToASCII(psFileInfo->u64FileSize,
						  eSize,
						  sizeof(eSize) - 1,
						  false,
						  true);

			printf("%.4u-%.2u-%.2u %.2u:%.2u:%.2u %20s %s\n",
				   ((psFileInfo->u16Date >> 9) + 1980), (psFileInfo->u16Date >> 5) & 0x0f, psFileInfo->u16Date & 0x1f,
				   (psFileInfo->u16Time >> 11), (psFileInfo->u16Time >> 5) & 0x3f, (psFileInfo->u16Time & 0x1f) << 1,
				   eSize,
				   psFileInfo->peFilename);
			if (u32TotalFiles != 0xffffffff)
			{
				u32TotalFiles++;
			}
			POST_SET(POSTCODE_D5);

			u64TotalSize += psFileInfo->u64FileSize;
		}

		POST_SET(POSTCODE_D3);
		psFileInfo = psFileInfo->psNextLink;
	}

	// x File(s)                     x bytes
	UINT64ToASCII(u64TotalSize,
				  eSize,
				  sizeof(eSize) - 1,
				  false,
				  true);

	printf("       %9u File(s)  %20s bytes\n",
		   u32TotalFiles,
		   eSize);

	// x Dir(s)       xxx bytes free
	u8FatFSErrorCode = f_getfree("0:",
								 &u32FreeClusters,
								 &sg_psFATFS[0]);
	eStatus = FatFSToEStatus(u8FatFSErrorCode);

	if (ESTATUS_OK == eStatus)
	{
		UINT64ToASCII((((uint64_t) u32FreeClusters) * ((uint64_t) sg_psFATFS[0]->csize * (uint64_t) FF_MAX_SS)),
					  eSize,
					  sizeof(eSize) - 1,
					  false,
					  true);

		printf("       %9u Dir(s)  %20s bytes free\n",
			   u32TotalDirs,
			   eSize);
	}
	else
	{
		printf("       %9u Dir(s)  Failed to get free space - %s\n", u32TotalDirs, GetErrorText(eStatus));
	}

errorExit:
	if (bDirOpened)
	{
		// Close the directory
		u8FatFSErrorCode = f_closedir(&sDir);
		if (ESTATUS_OK == eStatus)
		{
			eStatus = FatFSToEStatus(u8FatFSErrorCode);
		}
	}

	DOSDestroyFileInfo(&psFileInfoHead);
	if (eStatus != ESTATUS_OK)
	{
		printf("Dir failed - %s\n", GetErrorText(eStatus));
	}

	return(ESTATUS_OK);
}

EStatus DOSDelete(SLex *psLex,
				  const SMonitorCommands *psMonitorCommand,
				  uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	char *peBufferPtr = NULL;

	eStatus = LexGetBufferPosition(psLex,
								   &peBufferPtr);
	assert(ESTATUS_OK == eStatus);

	if (0 == f_unlink(peBufferPtr))
	{
		printf("File '%s' deleted\n", peBufferPtr);
	}
	else
	{
		printf("Failed to delete file '%s'\n");
	}

errorExit:
	return(ESTATUS_OK);
}









