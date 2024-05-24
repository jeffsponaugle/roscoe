#ifndef _SHARED_SHAREDLOG_H_
#define _SHARED_SHAREDLOG_H_

typedef struct SLogItem
{
	char* pu8String;
	struct SLogItem* psNext;
} SLogItem;

typedef struct SLog
{
	// Options
	bool bPrintToConsole;
	uint32_t u32MaxFileCount;

	// Configuration
	char* pu8Basename;
	volatile SLogItem* psItemHead;
	volatile SLogItem* psItemTail;
	bool bTerminate;

	// Access protections
	SOSCriticalSection hListMutex;
	SOSSemaphore hActivation;
	SOSSemaphore hClosure;

} SLog;

// Open and close logging handles
extern EStatus LoggerOpen( char* pu8Basename,
						   uint32_t u32MaxFileCount,
						   bool bPrintToConsole,
						   SLog** ppsLog );
extern void LoggerClose( SLog* psLog );

// Main interface for logging strings
extern void LoggerInternal( SLog* psLog,
							bool bAddTimestamp,
							char* pu8Function,
							const char* pu8FormatString,
							... );
#define Logger(handle, formatstring, ...)			LoggerInternal(handle, false, NULL, formatstring, ##__VA_ARGS__)
#define	LoggerTime(handle, formatstring, ...)		LoggerInternal(handle, true, NULL, formatstring, ##__VA_ARGS__)

#endif // _SHARED_SHAREDLOG_H_

