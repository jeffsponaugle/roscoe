/*

Stream operator routines for Newlib

*/

#include <sys/stat.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include "BIOS/OS.h"
#include "Shared/16550.h"
#include "Hardware/Roscoe.h"
#include "Shared/FatFS/source/ff.h"

// Set true if serial interrupts are in operation
static bool sg_bSerialInterrupts;
static bool sg_bAutoCR = false;

// 
#define	FILE_HANDLE_BASE	20
#define	FILE_HANDLE_COUNT	20

// List of file handles
static FIL *sg_psFiles[FILE_HANDLE_COUNT];

typedef struct SFatFSToEStatus
{
	uint8_t u8FatFSErrorCode;		// FatFS error code
	EStatus eStatus;				// EStatus equivalent
	int s32ErrorNumber;				// Errno equivalent
} SFatFSToEStatus;

static const SFatFSToEStatus sg_sFatFSToEStatus[] =
{
	{FR_OK,							ESTATUS_OK,						0},
	{FR_DISK_ERR,					ESTATUS_DISK_ERROR,				EIO},
	{FR_INT_ERR,					ESTATUS_DISK_ERROR,             EIO},
	{FR_NOT_READY,					ESTATUS_DISK_NOT_PRESENT,       ENXIO},
	{FR_NO_FILE,					ESTATUS_FILE_NOT_FOUND,         ENOENT},
	{FR_NO_PATH,					ESTATUS_NO_PATH,             	ENOENT},
	{FR_INVALID_NAME,				ESTATUS_INVALID_FILENAME,       EBADF},
	{FR_DENIED,						ESTATUS_ACCESS_DENIED,          EACCES},
	{FR_EXIST,						ESTATUS_FILE_EXISTS,            EEXIST},
	{FR_INVALID_OBJECT,				ESTATUS_INVALID_OBJECT,         ENXIO},
	{FR_WRITE_PROTECTED,			ESTATUS_WRITE_PROTECTED,        EROFS},
	{FR_INVALID_DRIVE,				ESTATUS_DISK_INVALID,           ENXIO},
	{FR_NOT_ENABLED,				ESTATUS_NOT_ENABLED,            EPIPE},
	{FR_NO_FILESYSTEM,				ESTATUS_NO_FILESYSTEM,          ENODEV},
	{FR_MKFS_ABORTED,				ESTATUS_MKFS_ABORTED,           EINTR},
	{FR_TIMEOUT,					ESTATUS_TIMEOUT,             	ETIMEDOUT},
	{FR_LOCKED,						ESTATUS_LOCKED,					EDEADLK},
	{FR_NOT_ENOUGH_CORE,			ESTATUS_OUT_OF_MEMORY,          ENOMEM},
	{FR_TOO_MANY_OPEN_FILES,		ESTATUS_TOO_MANY_OPEN_FILES,    EMFILE},
	{FR_INVALID_PARAMETER,			ESTATUS_INVALID_PARAMETER,		EINVAL},
};

EStatus FatFSToEStatus(uint8_t u8FatFSErrorCode)
{
	uint8_t u8Loop;

	for (u8Loop = 0; u8Loop < (sizeof(sg_sFatFSToEStatus) / sizeof(sg_sFatFSToEStatus[0])); u8Loop++)
	{
		if (sg_sFatFSToEStatus[u8Loop].u8FatFSErrorCode == u8FatFSErrorCode)
		{
			return (sg_sFatFSToEStatus[u8Loop].eStatus);
		}
	}

	return(ESTATUS_UNKNOWN_ERROR_CODE);
}

unsigned char FatFSToErrno(uint8_t u8FatFSErrorCode)
{
	uint8_t u8Loop;

	for (u8Loop = 0; u8Loop < (sizeof(sg_sFatFSToEStatus) / sizeof(sg_sFatFSToEStatus[0])); u8Loop++)
	{
		if (sg_sFatFSToEStatus[u8Loop].u8FatFSErrorCode == u8FatFSErrorCode)
		{
			return (sg_sFatFSToEStatus[u8Loop].s32ErrorNumber);
		}
	}

	return(ESTATUS_UNKNOWN_ERROR_CODE);
}

static FIL *HandleToFile(int s32File)
{
	if ((s32File < FILE_HANDLE_BASE) ||
		((s32File - FILE_HANDLE_BASE) >= FILE_HANDLE_COUNT))
	{
		errno = EBADF;
		return(NULL);
	}

	s32File -= FILE_HANDLE_BASE;
	if (NULL == sg_psFiles[s32File])
	{
		errno = EBADF;
		return(NULL);
	}

	return(sg_psFiles[s32File]);
}

static FIL *AllocateHandleToFile(int *ps32FileHandle)
{
	for (*ps32FileHandle = 0; *ps32FileHandle < FILE_HANDLE_COUNT; *ps32FileHandle)
	{
		if (NULL == sg_psFiles[*ps32FileHandle])
		{
			sg_psFiles[*ps32FileHandle] = calloc(1, sizeof(*sg_psFiles[*ps32FileHandle]));
			if (NULL == sg_psFiles[*ps32FileHandle])
			{
				errno = ENOMEM;
				return(NULL);
			}

			*ps32FileHandle += FILE_HANDLE_BASE;
			return(sg_psFiles[(*ps32FileHandle) - FILE_HANDLE_BASE]);
		}
	}

	errno = ENOMEM;
	return(NULL);
}

int fstat(int s32File,
		  struct stat *psStat)
{
	psStat->st_mode = S_IFCHR;
	return(0);
}

int lseek(int s32File,
		  int s32Offset,
		  int s32Whence)
{
	FIL *psFile;
	FSIZE_t s64Pos;
	FRESULT eFatFSErrorCode;

	s64Pos = s32Offset;
	psFile = HandleToFile(s32File);
	if (NULL == psFile)
	{
		goto errorExit;
	}

	if (SEEK_SET == s32Whence)
	{
		// Setting an absolute offset - fall through
	}
	else
	if (SEEK_CUR == s32Whence)
	{
		// Add the current position
		s64Pos += f_tell(psFile);
	}
	else
	if (SEEK_END == s32Whence)
	{
		s64Pos += f_size(psFile);
	}

	eFatFSErrorCode = f_lseek(psFile,
							  s64Pos);

	errno = FatFSToErrno(eFatFSErrorCode);
	if (errno)
	{
		return(-1);
	}
	else
	{
		return(s64Pos);
	}

errorExit:
	return(-1);
}

int close(int s32File)
{
	FIL *psFile;
	FSIZE_t s64Pos;
	FRESULT eFatFSErrorCode;

	psFile = HandleToFile(s32File);
	if (NULL == psFile)
	{
		goto errorExit;
	}

	eFatFSErrorCode = f_close(psFile);
	free(psFile);
	sg_psFiles[s32File - FILE_HANDLE_BASE] = NULL;
	errno = FatFSToErrno(eFatFSErrorCode);
	if (errno != 0)
	{
		return(-1);
	}
	else
	{
		return(0);
	}

errorExit:
	return(-1);
}

int write(int s32File,
		  char *pu8Buffer,
		  int s32Count)
{
	FRESULT eFatFSErrorCode;
	FIL *psFile;

	psFile = HandleToFile(s32File);
	if (psFile)
	{
		int s32Written;

		eFatFSErrorCode = f_write(psFile,
								  (void *) pu8Buffer,
								  s32Count,
								  &s32Written);

		errno = FatFSToErrno(eFatFSErrorCode);
		if (errno != 0)
		{
			return(-1);
		}
		else
		{
			return(s32Written);
		}
	}

	if (sg_bSerialInterrupts)
	{
		if (sg_bAutoCR)
		{
			// Add an automatic carriage return
			while (s32Count)
			{
				char *peString2;
				uint16_t u16Count;

				peString2 = pu8Buffer;
				while (s32Count && *peString2 != '\n')
				{
					++peString2;
					s32Count--;
				}

				u16Count = ((uint32_t) peString2) - ((uint32_t) pu8Buffer);

				if (u16Count)
				{
					(void) SerialTransmitDataAll(0,
												 pu8Buffer,
												 u16Count,
												 NULL);
				}

				pu8Buffer = peString2;
				if ('\n' == *peString2)
				{
					(void) SerialTransmitDataAll(0,
												 "\r\n",
												 sizeof(uint8_t) + sizeof(uint8_t),
												 NULL);
					++pu8Buffer;
					s32Count--;
				}
			}

			assert(0 == s32Count);
		}
		else
		{
			// Else don't
			(void) SerialTransmitDataAll(0,
										 pu8Buffer,
										 (uint16_t) s32Count,
										 NULL);
		}
	}
	else
	{
		if (sg_bAutoCR)
		{
			// Add an automatic carriage return
			while (s32Count)
			{
				char *peString2;
				uint16_t u16Count;

				peString2 = pu8Buffer;
				while (s32Count && *peString2 != '\n')
				{
					++peString2;
					s32Count--;
				}

				u16Count = ((uint32_t) peString2) - ((uint32_t) pu8Buffer);

				if (u16Count)
				{
					(void) SerialSendPIO((S16550UART *) ROSCOE_UART_A,
										 pu8Buffer,
										 u16Count);
				}

				pu8Buffer = peString2;
				if ('\n' == *peString2)
				{
					(void) SerialSendPIO((S16550UART *) ROSCOE_UART_A,
										 "\r\n",
										 sizeof(uint8_t) + sizeof(uint8_t));
					++pu8Buffer;
					s32Count--;
				}
			}

			assert(0 == s32Count);
		}
		else
		{
			SerialSendPIO((S16550UART *) ROSCOE_UART_A,
						  (uint8_t *) pu8Buffer,
						  (uint32_t) s32Count);
		}
	}

	return(s32Count);
}

int isatty(int s32File)
{
	if ((s32File < FILE_HANDLE_BASE) ||
		((s32File - FILE_HANDLE_BASE) >= FILE_HANDLE_COUNT))
	{
		return(0);
	}

	return(1);
}

int read(int s32File,
		 char *pu8Buffer,
		 int s32Count)
{
	int s32Read = 0;

	FRESULT eFatFSErrorCode;
	FIL *psFile;

	psFile = HandleToFile(s32File);
	if (psFile)
	{
		int s32Read;

		eFatFSErrorCode = f_read(psFile,
								 (void *) pu8Buffer,
								 s32Count,
								 &s32Read);

		errno = FatFSToErrno(eFatFSErrorCode);
		if (errno != 0)
		{
			return(-1);
		}
		else
		{
			return(s32Read);
		}
	}

	if (sg_bSerialInterrupts)
	{
		while (s32Count)
		{
			EStatus eStatus;
			uint16_t u16ReceivedDataCount = 0;

			eStatus = SerialReceiveData(0,
										pu8Buffer,
										(uint16_t) s32Count,
										&u16ReceivedDataCount);
			
			if (ESTATUS_OK == eStatus)
			{
				pu8Buffer += u16ReceivedDataCount;
				s32Count -= u16ReceivedDataCount;
				s32Read += u16ReceivedDataCount;
			}
			else
			{
				assert(0);
			}
		}
	}
	else
	{
		while (s32Count)
		{
			*pu8Buffer = SerialReceiveWaitPIO((S16550UART*) ROSCOE_UART_A);

			++pu8Buffer;
			++s32Read;
			s32Count--;
		}
	}

	return(s32Read);
}

// The following are required for assert() to work properly
int getpid(void)
{
	return(1);
}

int kill(int pid,
		 int sig)
{
	errno = EINVAL;
	return(-1);
}

EStatus StreamFlush(void)
{
	EStatus eStatus;

	if (sg_bSerialInterrupts)
	{

	}
	else
	{
		eStatus = SerialFlush(0);
	}

	return(eStatus);
}

void StreamSetAutoCR(bool bAutoCR)
{
	sg_bAutoCR = bAutoCR;
}

EStatus StreamSetConsoleSerialInterruptMode(bool bInterruptMode)
{
	EStatus eStatus = ESTATUS_OK;

	if (bInterruptMode)
	{
		// Interrupt mode for the UARTs
		eStatus = SerialInterruptInit();
		ERR_GOTO();

		// We're interrupt mode!
	}
	else
	{
		eStatus = SerialFlush(0);
		ERR_GOTO();

		// Run programmed I/O
		eStatus = SerialInterruptShutdown();
		ERR_GOTO();

		// No longer interrupt mode
	}

	// Set serial interrupts (or not)
	sg_bSerialInterrupts = bInterruptMode;

errorExit:
	return(eStatus);
}

typedef struct SOpenModeToFatFSMode 
{
	uint16_t u16OpenMode;
	uint16_t u16FatFSMode;
} SOpenModeToFatFSMode;

#define	FLAGS_MASK		(_FCREAT | _FTRUNC | _FEXCL | _FWRITE | _FREAD)

static const SOpenModeToFatFSMode sg_sOpenModeToFatFSMode[] =
{
	{0,										FA_READ},									// r
	{_FWRITE,								FA_READ | FA_WRITE},						// r+
	{_FCREAT + _FTRUNC + _FREAD,			FA_CREATE_ALWAYS | FA_WRITE},				// w
	{_FCREAT + _FTRUNC + _FWRITE,			FA_CREATE_ALWAYS | FA_WRITE | FA_READ},		// w+
	{_FCREAT + _FAPPEND + _FREAD,			FA_OPEN_APPEND | FA_WRITE},					// a
	{_FCREAT + _FAPPEND + _FWRITE,			FA_OPEN_APPEND | FA_WRITE | FA_READ},		// a+
	{_FCREAT + _FTRUNC + _FEXCL + _FREAD,	FA_CREATE_NEW | FA_WRITE},					// wx
	{_FCREAT + _FTRUNC + _FEXCL + _FWRITE,	FA_CREATE_NEW | FA_WRITE | FA_READ},		// w+x
};

int open(const char *peFilename, int flags, ...)
{
	FIL *psFile;
	FSIZE_t s64Pos;
	FRESULT eFatFSErrorCode;
	int s64Handle;
	uint8_t u8Loop;

	for (u8Loop = 0; u8Loop < (sizeof(sg_sOpenModeToFatFSMode) / sizeof(sg_sOpenModeToFatFSMode[0])); u8Loop++)
	{
		if ((flags & FLAGS_MASK) == sg_sOpenModeToFatFSMode[u8Loop].u16OpenMode)
		{
			flags = sg_sOpenModeToFatFSMode[u8Loop].u16FatFSMode;
			break;
		}
	}

	// If we don't recognize our flags, then don't do it
	if ((sizeof(sg_sOpenModeToFatFSMode) / sizeof(sg_sOpenModeToFatFSMode[0])) == u8Loop)
	{
		errno = EINVAL;
		goto errorExit;
	}

	psFile = AllocateHandleToFile(&s64Handle);
	if (NULL == psFile)
	{
		goto errorExit;
	}

	eFatFSErrorCode = f_open(psFile,
							 peFilename,
							 flags);
	errno = FatFSToErrno(eFatFSErrorCode);
	if (errno != 0)
	{
		return(-1);
	}
	else
	{
		return(s64Handle);
	}

errorExit:
	return(-1);
}

