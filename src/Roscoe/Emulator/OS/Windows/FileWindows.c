#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <direct.h>
#include "Shared/Shared.h"
#include "Shared/zlib/zlib.h"
#include "Shared/zlib/zutil.h"
#include "Shared/UtilTask.h"
#include "../OSPort.h"
#include "Arch/Arch.h"
#include "Platform/Platform.h"
#include "Shared/Windows/WindowsTools.h"
#include "Shared/SysEvent.h"

// UtilTask key for filesystem remount
#define		UTILTASK_FILESYSTEM_REMOUNT_KEY		0x6b8cae82

static void FilesystemUnmount(void)
{
	// Does nothing under Windows
}

EFilesystemType PortFilesystemGetType(void)
{
	return(EFSTYPE_WINDOWS);
}

EStatus PortFilesystemMount(void)
{
	// Doesn't do anything under Windows
	return(ESTATUS_OK);
}

EStatus PortFilefopen(SOSFile *psFile, char *peFilename, char *peFileMode)
{
	EStatus eStatus;
	FILE *psPOSIXFile;
	WCHAR eFilenameWide[MAX_PATH];
	WCHAR eFileModeWide[10];
	int32_t s32Result;

	s32Result = MultiByteToWideChar(CP_UTF8,
									0,
									peFilename,
									-1,
									eFilenameWide,
									ARRAYSIZE(eFilenameWide));

	if (0 == s32Result)
	{
		eStatus = ESTATUS_INVALID_NAME;
		goto errorExit;
	}

	s32Result = MultiByteToWideChar(CP_UTF8,
									0,
									peFileMode,
									-1,
									eFileModeWide,
									ARRAYSIZE(eFileModeWide));

	if (0 == s32Result)
	{
		eStatus = ESTATUS_INVALID_FILE_MODE;
		goto errorExit;
	}

	psPOSIXFile = _wfopen(eFilenameWide, eFileModeWide);
	if (NULL == psPOSIXFile)
	{
		eStatus = WindowsErrorsToEStatus(errno);
		goto errorExit;
	}

	*psFile = (SOSFile) psPOSIXFile;
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// This will generate a unique filename, place it in peFilenameGenerated, and
// open a file for writing.
EStatus PortFileOpenUniqueFile(char *pePath, char *peFilenameGenerated, uint32_t u32FilenameBufferSize, SOSFile *psOpenedFile)
{
	EStatus eStatus;
	char eFullFilename[250];
	
	// We don't care about the buffer being larger than 9 total bytes
	if (u32FilenameBufferSize > 9)
	{
		u32FilenameBufferSize = 9;
	}
	
	while (1)
	{
		uint32_t u32Loop;
		
		// Randomize a filename
		for (u32Loop = 0; u32Loop < u32FilenameBufferSize - 1; u32Loop++)
		{
			uint64_t u64RandomNumber;
			
			// Go get a random number
			u64RandomNumber = SharedRandomNumber();
			
			// We are generating an (up to 8) character random filename that doesn't exist
			peFilenameGenerated[u32Loop] = (char) ('A' + (u64RandomNumber % 26));;
		}
		
		// NULL Terminate it
		peFilenameGenerated[u32Loop] = '\0';
		
		// Fully qualify the path		
		snprintf(eFullFilename, sizeof(eFullFilename) - 1, "%s/%s", pePath, peFilenameGenerated);
		
		// First see if it exists
		eStatus = PortFilefopen(psOpenedFile,
							   eFullFilename,
							   "rb");
		if (ESTATUS_NO_FILE == eStatus)
		{
			// Good to go!
			eStatus = PortFilefopen(psOpenedFile,
								   eFullFilename,
								   "wb");
			
			// Regardless of the outcome, just exit
			break;
		}
		else
		if (ESTATUS_OK == eStatus)
		{
			// Already there. Close it and try another
			(void) PortFilefclose(*psOpenedFile);
		}
		else
		{
			// Errored out for some other reason
			break;
		}
	}

	return(eStatus);
}

EStatus PortFileCreateUniqueDirectory(char *pePath, char *peDirectoryGenerated, uint32_t u32DirectoryBufferSize)
{
	EStatus eStatus = ESTATUS_OK;
	char eFullDirname[250];
	char eFilename[10];

	while (1)
	{
		uint32_t u32Loop;

		// Randomize a filename
		for (u32Loop = 0; u32Loop < sizeof(eFilename) - 1; u32Loop++)
		{
			uint32_t u32RandomNumber;

			// Go get a random number
			u32RandomNumber = (uint32_t) SharedRandomNumber();

			// We are generating an (up to 8) character random filename that doesn't exist
			eFilename[u32Loop] = (char) ('A' + (u32RandomNumber % 26));;
		}

		// NULL Terminate it
		eFilename[u32Loop] = '\0';

		// Fully qualify the path		
		snprintf(eFullDirname, sizeof(eFullDirname) - 1, "%s/%s", pePath, eFilename);

		// Attempt to create it
		if (_mkdir(eFullDirname) == -1)
		{
			int32_t s32Errno = errno;

			if (EEXIST == s32Errno)
			{
				// Already there. Try another
			}
			else
			{
				// Failed - need to convert errno->EStatus
				eStatus = ErrnoToEStatus(errno);
				break;
			}
		}
		else
		{
			// It's either all or nothing.  We don't want a partial path, ever.
			if( u32DirectoryBufferSize < strlen(eFullDirname)+1 )
			{
				eStatus = ESTATUS_BUFFER_TOO_SMALL;
			}
			else
			{
				strcpy( peDirectoryGenerated, eFullDirname );
			}

			// Good to go!
			break;
		}
	}

	return(eStatus);
}

EStatus PortFilefread(void *pvBuffer, uint64_t u64SizeToRead, uint64_t *pu64SizeRead, SOSFile psFile)
{
	// fread Doesn't distinguish between an actual error. That's what Filefeof() and Fileferror() are for
	*pu64SizeRead = (uint64_t) fread(pvBuffer, 1, (size_t) u64SizeToRead, (FILE *) psFile);
	
	// Always OK. Kinda messed up, POSIX, but hey, it is what it is.
	return(ESTATUS_OK);
}

EStatus PortFilefwrite(void *pvBuffer, uint64_t u64SizeToWrite, uint64_t *pu64SizeWritten, SOSFile psFile)
{
	// fread Doesn't distinguish between an actual error. That's what Filefeof() and Fileferror() are for
	if (pu64SizeWritten)
	{
		*pu64SizeWritten = (uint64_t) fwrite(pvBuffer, 1, (size_t) u64SizeToWrite, (FILE *) psFile);
	}
	else
	{
		// We don't care about the result code
		(void) fwrite(pvBuffer, 1, (size_t) u64SizeToWrite, (FILE *) psFile);
	}

	// Always OK. Kinda messed up, POSIX, but hey, it is what it is.
	return(ESTATUS_OK);
}

EStatus PortFilefputc(uint8_t u8Data, SOSFile sFile)
{
	EStatus eStatus;
	int s32Result;

	s32Result = fputc((int) u8Data, (FILE *) sFile);
	if (EOF == s32Result)
	{
		eStatus = ESTATUS_DISK_FULL;
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFilefgetc(uint8_t *pu8Data, SOSFile sFile)
{
	EStatus eStatus;
	int s32Result;

	s32Result = fgetc((FILE *) sFile);

	if (s32Result >= 0)
	{
		*pu8Data = (uint8_t) s32Result;
		eStatus = ESTATUS_OK;
	}
	else
	{
		// Error of some sort
		eStatus = WindowsErrorsToEStatus(errno);
	}

	return(eStatus);
}

EStatus PortFilefseek(SOSFile psFile, int64_t s64Offset, int32_t s32Origin)
{
	EStatus eStatus;
	int eResult;

	eResult = fseek((FILE *) psFile, (long)s64Offset, (int) s32Origin);

	if (-1 == eResult)
	{
		// Error of some sort
		eStatus = WindowsErrorsToEStatus(errno);
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFileftell(SOSFile psFile, uint64_t *pu64Position)
{
	EStatus eStatus;
	int eResult;

	eResult = ftell((FILE *) psFile);

	if (-1 == eResult)
	{
		// Error of some sort
		eStatus = WindowsErrorsToEStatus(errno);
	}
	else
	{
		*pu64Position = (uint64_t) eResult;
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFileferror(SOSFile psFile)
{
	EStatus eStatus;

	if (ferror((FILE *) psFile))
	{
		// We have a stream error.
		eStatus = ESTATUS_FILE_ERROR_PENDING;
	}
	else
	{
		// No error
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFilefclose(SOSFile psFile)
{
	EStatus eStatus;
	int eResult;

	eResult = fclose((FILE *) psFile);

	if (0 == eResult)
	{
		eStatus = ESTATUS_OK;
	}
	else
	{
		// Error of some sort
		eStatus = WindowsErrorsToEStatus(errno);
	}

	return(eStatus);
}

EStatus PortFilefgets(char *peBuffer, uint64_t u64BufferSize, SOSFile sFile)
{
	EStatus eStatus;
	char *peResult;

	peResult = fgets(peBuffer, (int) u64BufferSize, (FILE *) sFile);

	if (NULL == peResult)
	{
		if (feof((FILE *) sFile))
		{
			eStatus = ESTATUS_OS_END_OF_FILE;	
		}
		else
		{
			eStatus = WindowsErrorsToEStatus(errno);
		}
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFilerename(char *peOldFilename, char *peNewFilename)
{
	EStatus eStatus;

	if (rename(peOldFilename, peNewFilename) == -1)
	{
		// Failed - need to convert errno->EStatus
		eStatus = WindowsErrorsToEStatus(errno);
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFilemkdir(char *peDirNameIncoming)
{
	EStatus eStatus;
	char *peDirNameTemp = NULL;
	char *peDirName = NULL;

	MEMALLOC(peDirNameTemp, strlen(peDirNameIncoming) + 1);

	strcpy(peDirNameTemp, peDirNameIncoming);

	peDirName = peDirNameTemp;
	while (*peDirName)
	{
		while ((*peDirName) && 
			   (*peDirName != '/')   &&
			   (*peDirName != '\\'))
		{
			++peDirName;
		}

		if (*peDirName)
		{
			*peDirName = '\0';

			if (strcmp(peDirNameTemp, ".") == 0)
			{
				// Don't do anything since that's a "here"
			}
			else
			{
				if (_mkdir(peDirNameTemp) == -1)
				{
					if (EEXIST == errno)
					{
						// Already there
						eStatus = ESTATUS_OK;
					}
					else
					{
						// Failed - need to convert errno->EStatus
						eStatus = WindowsErrorsToEStatus(errno);
						goto errorExit;
					}
				}
				else
				{
					eStatus = ESTATUS_OK;
				}
			}

			*peDirName = '/';
			++peDirName;
		}
	}

	if (_mkdir(peDirNameIncoming) == -1)
	{
		if (EEXIST == errno)
		{
			// Already there
			eStatus = ESTATUS_EXIST;
		}
		else
		{
			// Failed - need to convert errno->EStatus
			eStatus = WindowsErrorsToEStatus(errno);
		}
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

errorExit:
	SafeMemFree(peDirNameTemp);
	return(eStatus);
}

EStatus PortFilechdir(char *peDirName)
{
	BASSERT(0);
	return(ESTATUS_OK);
}

EStatus PortFileunlink(char *peFilename)
{
	EStatus eStatus;

	if (_unlink(peFilename) == -1)
	{
		// Failed - need to convert errno->EStatus
		eStatus = WindowsErrorsToEStatus(errno);
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFilermdir(char *peDirName)
{
	EStatus eStatus;

	if (_rmdir(peDirName) == -1)
	{
		eStatus = WindowsErrorsToEStatus(errno);
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

	return(eStatus);
}

EStatus PortFileGetFree(uint64_t *pu64FreeSpace)
{
	BASSERT(0);
	return(ESTATUS_OK);
}

EStatus PortFileSetFileTime(char *peFilename,
						struct tm *psTime)
{
	BASSERT(0);
	return(ESTATUS_OK);
}

uint64_t PortFileSize(SOSFile psFile)
{
	size_t s64CurrentPosition;
	size_t s64Size;

	// Record the current position
	s64CurrentPosition = ftell((FILE *) psFile);

	// Seek to the end of the file
	(void) fseek((FILE *) psFile, 0, SEEK_END);

	// Get the size
	s64Size = ftell((FILE *) psFile);

	// Now seek back
	(void) fseek((FILE *) psFile, (long) s64CurrentPosition, SEEK_SET);

	return((uint64_t) s64Size);
}

EStatus PortFileStat(char *peFilename,
					 uint64_t *pu64Timestamp,
					 uint64_t *pu64FileSize,
					 uint32_t *pu32FileAttributes)
{
	EStatus eStatus;
	HANDLE hFile;
	FILETIME sFileTime;
	WCHAR eFilenameWide[MAX_PATH];
	int32_t s32Result;

	BASSERT(NULL == pu64FileSize);
	BASSERT(NULL == pu32FileAttributes);

	s32Result = MultiByteToWideChar(CP_UTF8,
									0,
									peFilename,
									-1,
									eFilenameWide,
									ARRAYSIZE(eFilenameWide));

	if (0 == s32Result)
	{
		eStatus = ESTATUS_INVALID_NAME;
		goto errorExit;
	}								

	hFile = CreateFileW(eFilenameWide,
					    GENERIC_READ, 
					    FILE_SHARE_READ,  
					    NULL,  
					    OPEN_EXISTING,  
					    FILE_ATTRIBUTE_NORMAL, 
					    NULL);

	if (INVALID_HANDLE_VALUE == hFile)
	{
		eStatus = ESTATUS_NO_FILE;
		goto errorExit;
	}

	if (GetFileTime(hFile,
					&sFileTime,
					NULL,
					NULL))
	{
		*pu64Timestamp = ((((uint64_t) sFileTime.dwHighDateTime << 32) | sFileTime.dwLowDateTime) / 10000) - 11644473600000;
		eStatus = ESTATUS_OK;
	}
	else
	{
		eStatus = ESTATUS_INVALID_PARAMETER;
	}

	// 116444736000000000

errorExit:
	(void) CloseHandle(hFile);
	return(eStatus);
}

EStatus PortFileSetAttributes(char *peFilename,
							  uint32_t u32Attributes)
{
	BASSERT(0);
	return(ESTATUS_OK);
}

#define	MAX_WILDCARD_LEN	256

typedef struct SFileFindInternal
{
	HANDLE hFindFile;
	WIN32_FIND_DATAW sFindData;
	char *peBasePathname;
	char eWildcard[MAX_WILDCARD_LEN];
	bool bFirstFind;
	uint32_t u32LastAttributes;
	time_t eModificationTime;
	uint64_t u64FileSize;
} SFileFindInternal;

static EStatus PortFileFindNextInternal(SFileFind *psFileFind)
{
	EStatus eStatus = ESTATUS_OK;
	SFileFindInternal *psFileInfo = NULL;
	SYSTEMTIME sSystemTime;
	bool bResult;
	int s64Result;

	psFileInfo = (SFileFindInternal *) psFileFind->pvFilesystemData;
	memset((void *) psFileInfo->sFindData.cFileName, 0, sizeof(psFileInfo->sFindData.cFileName));

	// If we've not gotten a first, then we need to open things up
	if (false == psFileInfo->bFirstFind)
	{
		WCHAR u16FullPath[MAX_PATH];

		memset((void *) u16FullPath, 0, sizeof(u16FullPath));

		// Convert UTF8->WCHAR
		s64Result = MultiByteToWideChar(CP_UTF8,
										0,
										(LPCSTR) psFileInfo->peBasePathname,
										(int) strlen(psFileInfo->peBasePathname),
										u16FullPath,
										sizeof(u16FullPath) - 2);

		// See if we made it
		if (s64Result > 0)
		{
			// All good
		}
		else
		{
			// Failed somehow.
			eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
			goto errorExit;
		}

		// Append the file pattern if it was specified
		if (psFileInfo->eWildcard[0])
		{
			WCHAR u16Wildcard[MAX_PATH];

			memset((void*) u16Wildcard, 0, sizeof(u16Wildcard));
			
			s64Result = MultiByteToWideChar(CP_UTF8,
											0,
											psFileInfo->eWildcard,
											(int) strlen(psFileInfo->eWildcard),
											u16Wildcard,
											sizeof(u16FullPath) - 2);
											
			if (s64Result > 0)
			{
				wcscat_s(u16FullPath, ARRAYSIZE(u16FullPath), L"/");
				wcscat_s(u16FullPath, ARRAYSIZE(u16FullPath), u16Wildcard);
			}
		}

		psFileInfo->bFirstFind = true;
		psFileInfo->hFindFile = FindFirstFileW(u16FullPath,
											   &psFileInfo->sFindData);
		if (INVALID_HANDLE_VALUE == psFileInfo->hFindFile)
		{
			// Nothing left
			psFileFind->eFilename[0] = '\0';
			goto errorExit;
		}
	}
	else
	{
		// Next file
		bResult = FindNextFileW(psFileInfo->hFindFile,
								&psFileInfo->sFindData);
		if (false == bResult)
		{
			// Nothing left
			psFileFind->eFilename[0] = '\0';
			goto errorExit;
		}
	}

	// Get the attributes
	psFileFind->u32Attributes = 0;

	if (psFileInfo->sFindData.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
	{
		psFileFind->u32Attributes |= ATTRIB_READ_ONLY;
	}

	if (psFileInfo->sFindData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
	{
		psFileFind->u32Attributes |= ATTRIB_HIDDEN;
	}

	if (psFileInfo->sFindData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
	{
		psFileFind->u32Attributes |= ATTRIB_SYSTEM;
	}

	if (psFileInfo->sFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		psFileFind->u32Attributes |= ATTRIB_DIRECTORY;
	}

	if (psFileInfo->sFindData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
	{
		psFileFind->u32Attributes |= ATTRIB_ARCHIVE;
	}

	if (psFileInfo->sFindData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
	{
		psFileFind->u32Attributes |= ATTRIB_NORMAL;
	}

	// Copy in the file size
	psFileFind->u64FileSize = (uint64_t) ((uint64_t) psFileInfo->sFindData.nFileSizeHigh << 32) | psFileInfo->sFindData.nFileSizeLow;

	// Now the date/timestamp
	bResult = FileTimeToSystemTime(&psFileInfo->sFindData.ftLastWriteTime,
									&sSystemTime);
	BASSERT(bResult);

	memset((void *) &psFileFind->sDateTimestamp, 0, sizeof(psFileFind->sDateTimestamp));

	psFileFind->sDateTimestamp.tm_sec = sSystemTime.wSecond;
	psFileFind->sDateTimestamp.tm_min = sSystemTime.wMinute;
	psFileFind->sDateTimestamp.tm_hour = sSystemTime.wHour;
	psFileFind->sDateTimestamp.tm_mday = sSystemTime.wDay;
	psFileFind->sDateTimestamp.tm_mon = sSystemTime.wMonth;
	psFileFind->sDateTimestamp.tm_year = sSystemTime.wYear - 1900;

	// Convert from WCHAR to UTF8
	memset((void *) psFileFind->eFilename, 0, sizeof(psFileFind->eFilename));
	s64Result = (int) wcslen(psFileInfo->sFindData.cFileName);

	s64Result = WideCharToMultiByte(CP_UTF8,
									0,
									psFileInfo->sFindData.cFileName,
									s64Result,
									psFileFind->eFilename,
									sizeof(psFileFind->eFilename),
									NULL,
									NULL);

	if (0 == s64Result)
	{
		DWORD dwError;

		dwError = GetLastError();
		Syslog("dwError=%.8x\n", dwError);
		eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
		goto errorExit;
	}
	else
	{
		// All good!
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

static EStatus PortFileFindInternal(char *pePath,
									char *pePattern,
									SFileFind **ppsFileFind)
{
	EStatus eStatus = ESTATUS_OK;
	SFileFindInternal *psFileInfo = NULL;

	if (*ppsFileFind)
	{
		eStatus = ESTATUS_FILEFIND_ALREADY_ALLOCATED;
		goto errorExit;
	}
	
	// Allocate a file find structure
	MEMALLOC(*ppsFileFind, sizeof(**ppsFileFind));

	// Now a filesystem info structure
	MEMALLOC((*ppsFileFind)->pvFilesystemData, sizeof(*psFileInfo));
	psFileInfo = (SFileFindInternal *) (*ppsFileFind)->pvFilesystemData;

	// Copy the path
	psFileInfo->peBasePathname = strdupHeap(pePath);
	if (NULL == psFileInfo->peBasePathname)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	// Copy in the wildcard for later. Assuming there is one
	if (pePattern)
	{
		strncpy(psFileInfo->eWildcard, pePattern, sizeof(psFileInfo->eWildcard) - 1);
	}

	// All good
	eStatus = ESTATUS_OK;

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		SafeMemFree(*ppsFileFind);
	}

	return(eStatus);
}

EStatus PortFileFindFirst(char *pePath,
						  char *pePattern,
						  SFileFind **ppsFileFind)
{
	EStatus eStatus;

	eStatus = PortFileFindInternal(pePath,
								   pePattern,
								   ppsFileFind);
	ERR_GOTO();

	// Implied find
	eStatus = PortFileFindNext(*ppsFileFind);

errorExit:
	return(eStatus);
}

EStatus PortFileFindOpen(char *pePath,
						 SFileFind **ppsFileFind)
{
	return(PortFileFindInternal(pePath,
								NULL,
								ppsFileFind));
}

EStatus PortFileFindReadDir(SFileFind *psFileFind)
{
	return(PortFileFindNextInternal(psFileFind));
}

EStatus PortFileFindNext(SFileFind *psFileFind)
{
	return(PortFileFindNextInternal(psFileFind));
}

EStatus PortFileFindClose(SFileFind **ppsFileFind)
{
	EStatus eStatus = ESTATUS_OK;
	SFileFindInternal *psFileInfo = NULL;

	if (*ppsFileFind)
	{
		// We have a file find structure
		psFileInfo = (SFileFindInternal *) (*ppsFileFind)->pvFilesystemData;
		if (psFileInfo)
		{
			(void) FindClose(psFileInfo->hFindFile);

			SafeMemFree(psFileInfo->peBasePathname);

			// Get rid of the master structure
			SafeMemFree((*ppsFileFind)->pvFilesystemData);
		}

		// Get rid of our structure
		MemFree(*ppsFileFind);
	}

	return(eStatus);
}

// Set true if Syslog has been initialized
static bool sg_bSyslogInitialized = false;

static int64_t FilesystemRemountTask(uint32_t u32UtilTaskKey,
							   	   int64_t s64Message)
{
	EStatus eStatus;
	bool bResult;
//	bool bInitFailureOK = false;
//	char *peConfigFilename = NULL;
	
	if (sg_bSyslogInitialized)
	{
		// Dismount the filesystem (if it's mounted)
		(void) Syslog("Filesystem dismounted\n");
	}
	
	// Send out a SysEvent signal
	eStatus = SysEventSignal(SYSEVENT_FILESYSTEM_SHUTDOWN,
							 0);
	BASSERT(ESTATUS_OK == eStatus);
	
	// Unmount it
	FilesystemUnmount();

	// Time to mount the filesystem
	eStatus = FilesystemMount();
	BASSERT(ESTATUS_OK == eStatus);
	
	// Signal everyone that the filesystem is up
	eStatus = SysEventSignal(SYSEVENT_FILESYSTEM_INIT,
							 0);
	
	BASSERT(ESTATUS_OK == eStatus);
	
	eStatus = SysEventSignal(SYSEVENT_SETTING_INIT_START,
							 0);
	BASSERT(ESTATUS_OK == eStatus);

	// Doesn't init anything - for now
	bResult = true;

	eStatus = SysEventSignal(SYSEVENT_SETTING_INIT_END,
							 (int64_t) bResult);
	BASSERT(ESTATUS_OK == eStatus);
	
	return(0);
}

void PortFilesystemRemount(bool bWaitForCompletion)
{
	EStatus eStatus;
	
	// Now signal it, because we do want it to run
	if (false == bWaitForCompletion)
	{
		eStatus = UtilTaskSignal(UTILTASK_FILESYSTEM_REMOUNT_KEY,
								 0,
								 NULL,
								 NULL);
	}
	else
	{
		eStatus = UtilTaskSignalBlocking(UTILTASK_FILESYSTEM_REMOUNT_KEY,
										 0,
										 NULL);
	}
	
//	DebugOut("UtilTaskSignalBlocking result = %s, %d\n", GetErrorText(eStatus), eStatus);
		
	BASSERT(ESTATUS_OK == eStatus);
}

// Called before the OS initializes and runs
EStatus PortFilesystemInit(void)
{
	EStatus eStatus;
	
	// Register a UtilTask for remounting the filesystem
	eStatus = UtilTaskRegister("Filesystem remount",
							   UTILTASK_FILESYSTEM_REMOUNT_KEY,
							   FilesystemRemountTask);
	BASSERT(ESTATUS_OK == eStatus);
	
	// Now acknowledge the pre OS event
	eStatus = SysEventAck(SYSEVENT_PRE_OS);
	BASSERT(ESTATUS_OK == eStatus);
	
	// Signal a filesystem mount/remount
	FilesystemRemount(false);
	
	return(eStatus);
}

// feof() equivalent
bool PortFilefeof(SOSFile psFile)
{
	if (feof((FILE *) psFile))
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

EStatus PortFilermdirTree(char *peDirName)
{
	EStatus eStatus = ESTATUS_OK;
	char* pu8Cmd;
	int32_t s32Result;

	BASSERT( peDirName );

	pu8Cmd = sprintfMemAlloc("rmdir /Q /S \"%s\"", peDirName);
	if( NULL == pu8Cmd )
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	s32Result = system( pu8Cmd );
	if( -1 == s32Result )
	{
		eStatus = ErrnoToEStatus(errno);
		goto errorExit;
	}
	else if( s32Result > 0 )
	{
		eStatus = ESTATUS_UNKNOWN_RESULT_CODE;
	}

errorExit:
	SafeMemFree(pu8Cmd);
	return(eStatus);
}

EStatus PortFilefflush(SOSFile psFile)
{
	EStatus eStatus;
	int eResult;

	eResult = fflush((FILE *) psFile);

	if (0 == eResult)
	{
		eStatus = ESTATUS_OK;
	}
	else
	{
		eStatus = WindowsErrorsToEStatus(errno);
	}

	return(eStatus);
}

EStatus PortFilesystemCreate(void)
{
	return(ESTATUS_OK);
}

