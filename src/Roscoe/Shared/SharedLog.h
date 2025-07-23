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
	BOOL bPrintToConsole;
	UINT32 u32MaxFileCount;

	// Configuration
	char* pu8Basename;
	volatile SLogItem* psItemHead;
	volatile SLogItem* psItemTail;
	BOOL bTerminate;

	// Access protections
	SOSCriticalSection hListMutex;
	SOSSemaphore hActivation;
	SOSSemaphore hClosure;

} SLog;

// Open and close logging handles
extern EStatus LoggerOpen( char* pu8Basename,
						   UINT32 u32MaxFileCount,
						   BOOL bPrintToConsole,
						   SLog** ppsLog );
extern void LoggerClose( SLog* psLog );

// Main interface for logging strings
extern void LoggerInternal( SLog* psLog,
							BOOL bAddTimestamp,
							char* pu8Function,
							const char* pu8FormatString,
							... );
#define Logger(handle, formatstring, ...)			LoggerInternal(handle, FALSE, NULL, formatstring, ##__VA_ARGS__)
#define	LoggerTime(handle, formatstring, ...)		LoggerInternal(handle, TRUE, NULL, formatstring, ##__VA_ARGS__)

#endif // _SHARED_SHAREDLOG_H_

