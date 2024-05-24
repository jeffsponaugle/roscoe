#ifndef _OS_H_
#define _OS_H_

#include <time.h>

// Environmentally agnostic OS calls.
extern void OSInit(void);
extern void OSInitEarly(void);

// Wait #defines
#define	OS_WAIT_INDEFINITE			0xffffffff
#define	MAX_SEM_COUNT				(1 << 30)

// Ecosystem agnostic typedefs for various OS basic types
typedef void * SOSThreadHandle;
typedef void * SOSSemaphore;
typedef void * SOSCriticalSection;
typedef void * SOSQueue;
typedef void * SOSFile;
typedef void * SOSSocket;
typedef void * SOSSemaphoreReadWrite;

// Thread priorities - do not change their numbers!
typedef enum
{
	EOSPRIORITY_IDLE=0,					// Lowest priority setting
	EOSPRIORITY_LOW,
	EOSPRIORITY_BELOW_NORMAL,
	EOSPRIORITY_NORMAL,
	EOSPRIORITY_ABOVE_NORMAL,
	EOSPRIORITY_HIGH,
	EOSPRIORITY_REALTIME,				// Highest priority
		
	// Priority terminator (highest)
	EOSPRIORITY_COUNT
} EOSPriority;

// File attributes (taken from https://docs.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants)
#define	ATTRIB_READ_ONLY	0x01		// Read only file
#define	ATTRIB_HIDDEN		0x02		// File's hidden
#define	ATTRIB_SYSTEM		0x04		// System file
#define	ATTRIB_LFN_ENTRY	0x0f		// Long filename entry
#define	ATTRIB_DIRECTORY	0x10		// Directory entry
#define	ATTRIB_ARCHIVE		0x20		// Archive file
#define	ATTRIB_NORMAL		0x40		// Regular file

// Maximum length of a file's name
#define	MAX_FILENAME_LEN	255			// Maximum length of a file's name

// Used for findfirst/findnext
typedef struct SFileFind
{
	uint64_t u64FileSize;
	struct tm sDateTimestamp;
	uint32_t u32Attributes;
	char eFilename[MAX_FILENAME_LEN+1];

	void *pvFilesystemData;
} SFileFind;

// Types of filesystems
typedef enum
{
	EFSTYPE_UNKNOWN,
	EFSTYPE_FAT12,
	EFSTYPE_FAT16,
	EFSTYPE_FAT32,
	EFSTYPE_EXFAT,
	EFSTYPE_UNIX,
	EFSTYPE_WINDOWS,
} EFilesystemType;

typedef struct SOSEventFlagWait
{
	SOSSemaphore eEventSemaphore;
	uint64_t *pu64ValueToChange;
	uint64_t u64FlagMask;
	bool bEventFlagDestroyed;

	struct SOSEventFlagWait *psNextLink;
} SOSEventFlagWait;

typedef struct SOSEventFlag
{
	SOSCriticalSection sEventAccess;
	uint64_t u64EventFlags;

	// Linked list of threads waiting on this event flag
	SOSEventFlagWait *psWaitList;

	struct SOSEventFlag *psNextLink;
} SOSEventFlag;
 
// Thread functions
extern EStatus OSThreadCreate(char *peThreadName,
							  void *pvThreadValue,
							  void (*pvThreadEntry)(void *pvThreadValue),
							  bool bCreateSuspended,
							  SOSThreadHandle *psThreadHandle,
							  uint32_t u32StackSizeBytes,
					   		  EOSPriority ePriority);
extern EStatus OSThreadResume(SOSThreadHandle *psThreadHandle);
extern void OSThreadYield(void);
extern EStatus OSThreadDestroy(SOSThreadHandle *psThreadHandle);
extern EStatus OSThreadSetName(char *peThreadName);
extern void OSThreadSetNameprintf(char *pu8Format, ...);


// Semaphore functions
extern EStatus OSSemaphoreCreate(SOSSemaphore *psSemaphore,
							   	 uint32_t u32InitialCount,
							   	 uint32_t u32MaximumCount);
extern EStatus OSSemaphoreGet(SOSSemaphore sSemaphore,
							  uint32_t u32TimeoutMilliseconds);
extern EStatus OSSemaphorePut(SOSSemaphore sSemaphore,
							  uint32_t u32PutCount);
extern EStatus OSSemaphoreGetCount(SOSSemaphore sSemaphore,
								   uint32_t *pu32Count);
extern EStatus OSSemaphoreDestroy(SOSSemaphore *psSemaphore);

// Event flags functions

extern EStatus OSEventFlagSet(SOSEventFlag *psEventFlag,
							  uint64_t u64FlagMask);
extern EStatus OSEventFlagGet(SOSEventFlag *psEventFlag,
							  uint64_t u64FlagMask,
							  uint32_t u32Timeout,
							  uint64_t *pu64FlagMaskObtained);
extern EStatus OSEventFlagDestroy(SOSEventFlag **ppsEventFlag);
extern EStatus OSEventFlagCreate(SOSEventFlag **ppsEventFlag,
								 uint64_t u64InitialEventFlagSetting);

// Critical section functions
extern EStatus OSCriticalSectionCreate(SOSCriticalSection *psCriticalSection);
extern EStatus OSCriticalSectionEnter(SOSCriticalSection sCriticalSection);
extern EStatus OSCriticalSectionLeave(SOSCriticalSection sCriticalSection);
extern EStatus OSCriticalSectionDestroy(SOSCriticalSection *psCriticalSection);

// Queue functions
extern EStatus OSQueueCreate(SOSQueue *psQueue,
							 char *peQueueName,
							 uint32_t u32ItemSize,
							 uint32_t u32ItemCount);
extern EStatus OSQueueGet(SOSQueue sQueue,
						  void *pvBuffer,
						  uint32_t u32TimeoutMilliseconds);
extern EStatus OSQueuePut(SOSQueue sQueue,
						  void *pvBuffer,
						  uint32_t u32TimeoutMilliseconds);
extern EStatus OSQueueGetAvailable(SOSQueue sQueue,
								   uint32_t *pu32AvailableCount);
extern EStatus OSQueueClear(SOSQueue sQueue);
extern void OSQueueDestroy(SOSQueue *psQueue);
extern EStatus OSQueueSetFIFOMode(SOSQueue sQueue,
								  bool bFIFOModeEnabled);

// Read/write semaphores
extern EStatus OSSemaphoreReadWriteCreate(SOSSemaphoreReadWrite *psOSSemaphoreReadWrite);
extern EStatus OSSemaphoreReadWriteDestroy(SOSSemaphoreReadWrite *psOSSemaphoreReadWrite);
extern EStatus OSSemaphoreReadLock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite);
extern EStatus OSSemaphoreReadUnlock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite);
extern EStatus OSSemaphoreWriteLock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite);
extern EStatus OSSemaphoreWriteUnlock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite);

// Filesystem functions
extern EStatus FilesystemInit(uint32_t u32Mask,
							  int64_t s64Message);
extern EStatus FilesystemMount(void);
extern void FilesystemRemount(bool bWaitForMount);
extern EFilesystemType FilesystemGetType(void);
extern EStatus Filefopen(SOSFile *psFile, char *peFilename, char *peFileMode);
extern EStatus FileOpenEncrypted(SOSFile *psFile, char *peFilename, char *peFileMode);
extern EStatus FileOpenUniqueFile(char *pePath, char *peFilenameGenerated, uint32_t u32FilenameBufferSize, SOSFile *psOpenedFile);
extern EStatus FileOpenUniqueFileEncrypted(char *pePath, char *peFilenameGenerated, uint32_t u32FilenameBufferSize, SOSFile *psOpenedFile);
extern EStatus FileCreateUniqueDirectory(char *pePath, char *peDirectoryGenerated, uint32_t u32DirectoryBufferSize);
extern EStatus Filefread(void *pvBuffer, uint64_t u64SizeToRead, uint64_t *pu64SizeRead, SOSFile psFile);
extern EStatus Filefwrite(void *pvBuffer, uint64_t u64SizeToWrite, uint64_t *pu64SizeWritten, SOSFile psFile);
extern EStatus Filefseek(SOSFile psFile, int64_t s64Offset, int32_t s32Origin);
extern EStatus Fileftell(SOSFile psFile, uint64_t *pu64Position);
extern EStatus Fileferror(SOSFile psFile);
extern EStatus Filefclose(SOSFile *psFile);
extern EStatus Filefgets(char *peStringBuffer, uint64_t u64MaxSize, SOSFile sFile);
extern EStatus Filerename(char *peOldFilename, char *peNewFilename);
extern EStatus Filemkdir(char *peDirName);
extern EStatus Filechdir(char *peDirName);
extern EStatus Fileunlink(char *peFilename);
extern EStatus Filermdir(char *peDirName);
extern EStatus FileGetFree(uint64_t *pu64FreeSpace);
extern EStatus FileSetFileTime(char *peFilename,
							   struct tm *psTime);
extern uint64_t FileSize(SOSFile psFile);
extern EStatus FileSetAttributes(char *peFilename,
								 uint32_t u32Attributes);
extern EStatus FileFindOpen(char *pePath,
							SFileFind **ppsFileFind);
extern EStatus FileFindFirst(char *pePath,
					  		 char *pePattern,
							 SFileFind **ppsFileFind);
extern EStatus FileFindReadDir(SFileFind *psFileFind);
extern EStatus FileFindNext(SFileFind *psFileFind);
extern EStatus FileFindClose(SFileFind **ppsFileFind);
extern EStatus FilesystemCreate(void);
extern EStatus FilermdirTree(char *peDirName);
extern bool Filefeof(SOSFile psFile);
extern EStatus FileTestEncryption(void);
extern EStatus Filefprintf(SOSFile sFile,
						   const char *peFormat,
						   ...);
extern EStatus FilefprintfCSV(SOSFile sFile,
							  const char *peFormat,
							  ...);
extern EStatus FileIsValid(SOSFile sFile);
extern EStatus FileTestWrite(void);
extern EStatus FileCopy(char *peSourceFile,					
						char *peDestinationFile);
extern EStatus FileMove(char *peSourceFile,					
						char *peDestinationFile);
extern EStatus Filefflush(SOSFile psFile);
extern EStatus FileLoad(char *peFilename,
						uint8_t **ppu8FileData,
						uint64_t *pu64FileLength,
						uint64_t u64AdditionalAlloc);
extern EStatus FileLoadEncrypted(char *peFilename,
								 uint8_t **ppu8FileData,
								 uint64_t *pu64FileLength);
extern EStatus Filefputc(uint8_t u8Data, SOSFile sFile);
extern EStatus Filefgetc(uint8_t *pu8Data, SOSFile sFile);
extern EStatus FileWriteFromMemoryAndVerify(char *peFilename,
											uint8_t *pu8FileData,
											uint64_t u64FileSize);
extern EStatus FileWriteFromMemory(char *peFilename,
								   uint8_t *pu8FileData,
								   uint64_t u64FileSize);
extern EStatus FileStat(char *peFilename,
						uint64_t *pu64Timestamp,
						uint64_t *pu64FileSize,
						uint32_t *pu32FileAttributes);

// RTC
extern uint64_t RTCGet(void);

// Misc
extern void OSSleep(uint64_t u64Milliseconds);
extern bool OSIsRunning(void);
extern bool OSIsIdleTask(void);

// Used for port # when the system can or should pick one, or if it's not relevant
#define	PORT_NOT_SPECIFIED		0xffffffff

// Used for all functions below - it means "use the reasonable default timeout
// inside the wifi module for this function"
#define	NETWORK_TIMEOUT_DEFAULT			0

// Time
extern EStatus OSgmtime( struct tm* psTimeDest, uint64_t u64Timestamp );
extern EStatus OSlocaltime( struct tm* psTimeDest, uint64_t u64Timestamp );

#endif