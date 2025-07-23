#ifndef _OSPORT_H_
#define _OSPORT_H_

// Thread functions
extern EStatus OSPortThreadCreate(char *peThreadName,
								  void *pvThreadValue,
								  void (*pvThreadEntry)(void *pvThreadValue),
								  bool bCreateSuspended,
								  SOSThreadHandle *psThreadHandle,
								  uint32_t u32StackSize,
						   		  EOSPriority ePriority);
extern EStatus OSPortThreadResume(SOSThreadHandle *psThreadHandle);
extern void OSPortThreadYield(void);
extern EStatus OSPortThreadDestroy(SOSThreadHandle *psThreadHandle);
extern EStatus OSPortThreadSetName(char *peThreadName);

// Semaphore functions
extern EStatus OSPortSemaphoreCreate(SOSSemaphore *psSemaphore,
							   	 	 uint32_t u32InitialCount,
							   	 	 uint32_t u32MaximumCount);
extern EStatus OSPortSemaphoreGet(SOSSemaphore sSemaphore,
							  	  uint32_t u32TimeoutMilliseconds);
extern EStatus OSPortSemaphorePut(SOSSemaphore sSemaphore,
							  	  uint32_t u32PutCount);
extern EStatus OSPortSemaphoreGetCount(SOSSemaphore sSemaphore,
									   uint32_t *pu32SemaphoreCount);
extern EStatus OSPortSemaphoreDestroy(SOSSemaphore *psSemaphore);

// Critical section functions
extern EStatus OSPortCriticalSectionCreate(SOSCriticalSection *psCriticalSection);
extern EStatus OSPortCriticalSectionEnter(SOSCriticalSection sCriticalSection);
extern EStatus OSPortCriticalSectionLeave(SOSCriticalSection sCriticalSection);
extern EStatus OSPortCriticalSectionDestroy(SOSCriticalSection *psCriticalSection);

// Queue functions
extern EStatus OSPortQueueCreate(SOSQueue *psQueue,
								 char *peQueueName,
								 uint32_t u32ItemSize,
								 uint32_t u32ItemCount);
extern EStatus OSPortQueueGet(SOSQueue sQueue,
							  void *pvBuffer,
							  uint32_t u32TimeoutMilliseconds);
extern EStatus OSPortQueuePut(SOSQueue sQueue,
							  void *pvBuffer,
							  uint32_t u32TimeoutMilliseconds);
extern EStatus OSPortQueueGetAvailable(SOSQueue sQueue,
									   uint32_t *pu32AvailableCount);
extern EStatus OSPortQueueClear(SOSQueue sQueue);
extern void OSPortQueueDestroy(SOSQueue *psQueue);
extern EStatus OSPortQueueSetFIFOMode(SOSQueue sQueue,
									  bool bFIFOModeEnabled);

// Filesystem functions
extern EStatus PortFilesystemInit(void);
extern EStatus PortFilesystemMount(void);
extern void PortFilesystemRemount(bool bWaitForCompletion);
extern EFilesystemType PortFilesystemGetType(void);
extern EStatus PortFilefopen(SOSFile *psFile, char *peFilename, char *peFileMode);
extern EStatus PortFileOpenUniqueFile(char *pePath, char *peFilenameGenerated, uint32_t u32FilenameBufferSize, SOSFile *psOpenedFile);
extern EStatus PortFileCreateUniqueDirectory(char *pePath, char *peDirectoryGenerated, uint32_t u32DirectoryBufferSize);
extern EStatus PortFilefread(void *pvBuffer, uint64_t u64SizeToRead, uint64_t *pu64SizeRead, SOSFile psFile);
extern EStatus PortFilefwrite(void *pvBuffer, uint64_t u64SizeToWrite, uint64_t *pu64SizeWritten, SOSFile psFile);
extern EStatus PortFilefseek(SOSFile psFile, int64_t s64Offset, int32_t s32Origin);
extern EStatus PortFileftell(SOSFile psFile, uint64_t *pu64Position);
extern EStatus PortFileferror(SOSFile psFile);
extern EStatus PortFilefclose(SOSFile psFile);
extern EStatus PortFilefgets(char *peBuffer, uint64_t u64BufferSize, SOSFile sFile);
extern EStatus PortFilerename(char *peOldFilename, char *peNewFilename);
extern EStatus PortFilemkdir(char *peDirName);
extern EStatus PortFilechdir(char *peDirName);
extern EStatus PortFileunlink(char *peFilename);
extern EStatus PortFilermdir(char *peDirName);
extern EStatus PortFileGetFree(uint64_t *pu64FreeSpace);
extern EStatus PortFileSetFileTime(char *peFilename,
								   struct tm *psTime);

extern uint64_t PortFileSize(SOSFile psFile);
extern EStatus PortFileSetAttributes(char *peFilename,
							  uint32_t u32Attributes);
extern EStatus PortFileFindOpen(char *pePath,
								SFileFind **ppsFileFind);
extern EStatus PortFileFindFirst(char *pePath,
						  char *pePattern,
						  SFileFind **ppsFileFind);
extern EStatus PortFileFindReadDir(SFileFind *psFileFind);
extern EStatus PortFileFindNext(SFileFind *psFileFind);
extern EStatus PortFileFindClose(SFileFind **ppsFileFind);
extern EStatus PortFilesystemCreate(void);
extern EStatus PortFilermdirTree(char *peDirName);
extern bool PortFilefeof(SOSFile psFile);
extern EStatus PortFilefflush(SOSFile psfile);
extern EStatus PortFilefputc(uint8_t u8Data, SOSFile sFile);
extern EStatus PortFilefgetc(uint8_t *pu8Data, SOSFile sFile);
extern EStatus PortFileStat(char *peFilename,
							uint64_t *pu64Timestamp,
							uint64_t *pu64FileSize,
							uint32_t *pu32FileAttributes);

// Misc functions
extern void OSPortSleep(uint64_t u64Milliseconds);
extern void OSPortInitEarly(void);
extern void OSPortInit(void);
extern bool OSPortIsIdleTask(void);

// Time functions
extern EStatus OSPortGmtime( struct tm* psTimeDest, uint64_t u64Timestamp );
extern EStatus OSPortLocaltime( struct tm* psTimeDest, uint64_t u64Timestamp );


#endif
