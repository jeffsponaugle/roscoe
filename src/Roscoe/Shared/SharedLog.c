#include <stdio.h>
#include <stdarg.h>
#include "Shared/Shared.h"

#ifdef _UNIX
#include <unistd.h>
#define _unlink		unlink
#endif

static void LoggerFreeItemSafe( SLogItem** ppsItem )
{
	BASSERT( ppsItem );
	if( *ppsItem )
	{
		SafeMemFree( (*ppsItem)->pu8String );
		SafeMemFree( *ppsItem );
	}
}

static SLogItem* LoggerAllocateItem( char* pu8OutputString )
{
	SLogItem* psNew = NULL;
	char* psNewString = NULL;

	BASSERT( pu8OutputString );

	// First allocate the structure
	psNew = (SLogItem*) MemAlloc( sizeof(*psNew) );
	if( NULL == psNew )
	{
		DebugOut("Failed to allocate new logger entry\n");
		goto errorExit;
	}

	// Next allocate room for the string and fill it in
	psNewString = (char*) MemAlloc( strlen(pu8OutputString) + 1 );
	if( NULL == psNewString )
	{
		DebugOut("Failed to allocate new logger entry string\n");
		goto errorExit;
	}

	// Setup the string copy and the structure
	strcpy( psNewString, pu8OutputString );
	psNew->pu8String = psNewString;

	return( psNew );

errorExit:
	LoggerFreeItemSafe( &psNew );
	return( NULL );
}

static void LoggerFreeSafe( SLog** ppsLog )
{
	if( *ppsLog )
	{
		OSCriticalSectionEnter( (*ppsLog)->hListMutex );

		// There shouldn't be any left over items to free but check anyhow
		while( (*ppsLog)->psItemHead )
		{
			SLogItem* psNext = (SLogItem*)((*ppsLog)->psItemHead->psNext);

			LoggerFreeItemSafe( (SLogItem**)(&((*ppsLog)->psItemHead)) );

			(*ppsLog)->psItemHead = psNext;
		}

		(void) OSSemaphoreDestroy( &((*ppsLog)->hActivation) );
		(void) OSSemaphoreDestroy( &((*ppsLog)->hClosure) );
		(void) OSCriticalSectionDestroy( &((*ppsLog)->hListMutex) );

		SafeMemFree( (*ppsLog)->pu8Basename );
	}
}

static SLog* LoggerAllocate( char* pu8Basename )
{
	EStatus eStatus = ESTATUS_OK;
	SLog* psNewLog;
	UINT32 u32BasenameLength;

	psNewLog = (SLog*) MemAlloc( sizeof(*psNewLog) );
	if( NULL == psNewLog )
	{
#ifdef _SERVER
		eStatus = ESTATUS_SERVER_OUT_OF_MEMORY;
#else
		eStatus = ESTATUS_OUT_OF_MEMORY;
#endif
		goto errorExit;
	}

	// Allocate a copy of the basename with room 
	u32BasenameLength = (UINT32)strlen(pu8Basename);
	u32BasenameLength += 10 + 1;
	
	psNewLog->pu8Basename = (char*) MemAlloc( u32BasenameLength );
	if( NULL == psNewLog->pu8Basename )
	{
#ifdef _SERVER
		eStatus = ESTATUS_SERVER_OUT_OF_MEMORY;
#else
		eStatus = ESTATUS_OUT_OF_MEMORY;
#endif
		goto errorExit;
	}

	// Compose the log filename
	snprintf( psNewLog->pu8Basename, u32BasenameLength, pu8Basename, 0 );

	eStatus = OSCriticalSectionCreate( &(psNewLog->hListMutex) );
	ERR_GOTO();

	eStatus = OSSemaphoreCreate( &(psNewLog->hActivation), 0, 0x7fffffff );
	ERR_GOTO();

	eStatus = OSSemaphoreCreate( &(psNewLog->hClosure), 0, 1 );
	ERR_GOTO();

errorExit:

	if( eStatus != ESTATUS_OK )
	{
		LoggerFreeSafe( &psNewLog );
	}

	return( psNewLog );
}

static void LoggerActivate( SLog* psLog )
{
	EStatus eStatus;

	eStatus = OSSemaphorePut( psLog->hActivation, 1 );
	if( eStatus != ESTATUS_OK )
	{
		DebugOut( "Failed to activate logger thread (%u): %s\n", eStatus, GetErrorText(eStatus) );
	}
}

static void LoggerSignalClose( SLog* psLog )
{
	EStatus eStatus;

	eStatus = OSSemaphorePut( psLog->hClosure, 1 );
	if( eStatus != ESTATUS_OK )
	{
		DebugOut( "Failed to signal logger close (%u): %s\n", eStatus, GetErrorText(eStatus) );
	}
}

// Flush out all stored log items
static void LoggerFlush( SLog* psLog )
{
	EStatus eStatus = ESTATUS_OK;
	SLogItem* psItemHead = NULL;
	SLogItem* psItemTail = NULL;
	SOSFile hFile;

	// Grab the entire list of items to be processed
	OSCriticalSectionEnter( psLog->hListMutex );

	psItemHead = (SLogItem*)(psLog->psItemHead);
	psItemTail = (SLogItem*)(psLog->psItemTail);
	psLog->psItemHead = NULL;
	psLog->psItemTail = NULL;

	OSCriticalSectionLeave( psLog->hListMutex );

	// Sanity check the head and tail pointers obtained
	if( NULL == psItemHead )
	{
		BASSERT( NULL == psItemTail );
	}
	else
	{
		BASSERT( psItemTail );

		// Open up the log file
		eStatus = Filefopen( &hFile, psLog->pu8Basename, "a+" );

		// Failure to open the file means we should keep the log entries for later
		if( eStatus != ESTATUS_OK )
		{
			OSCriticalSectionEnter( psLog->hListMutex );

			psItemTail->psNext = (SLogItem*)(psLog->psItemHead);
			psLog->psItemHead = psItemHead;

			OSCriticalSectionLeave( psLog->hListMutex );
		}
		// Otherwise, loop over the items and append them to the file
		else
		{
			while( psItemHead )
			{
				UINT64 u64Written = 0;

				// First print it to the console
				if( psLog->bPrintToConsole )
				{
					// Make sure not to add the timestamp
					DebugOutSkipTime( psItemHead->pu8String );
				}

				// Then write to the file
				eStatus = Filefwrite( psItemHead->pu8String, 
									  strlen(psItemHead->pu8String),
									  &u64Written,
									  hFile );

				if( eStatus != ESTATUS_OK )
				{
					DebugOut( "Failed to write log entry to '%s' (%u):%s\n", 
							  psLog->pu8Basename, 
							  eStatus, 
							  GetErrorText(eStatus) );
				}
				else if( strlen(psItemHead->pu8String) != u64Written )
				{
					DebugOut( "Short write of log entry to '%s'. Expected %llu but wrote %llu\n", 
							  psLog->pu8Basename, 
							  strlen(psItemHead->pu8String), 
							  u64Written );
				}

				// Get the next item and free the head of the list
				{
					SLogItem* psItem = psItemHead;
					psItemHead = psItemHead->psNext;
					LoggerFreeItemSafe( &psItem );
				}
			}

			eStatus = Filefclose( &hFile );
			if( eStatus != ESTATUS_OK )
			{
				DebugOut( "Failed to close log file '%s'\n", psLog->pu8Basename );
			}
		}
	}
}

static void LoggerThread( void* pvThreadInfo )
{
	SLog* psLog = (SLog*) pvThreadInfo;
	EStatus eStatus;

	// Set the thread name
	(void) OSThreadSetNameprintf( "Logger: %s", psLog->pu8Basename );

	while( FALSE == psLog->bTerminate )
	{
		// Wait for activation
		eStatus = OSSemaphoreGet( psLog->hActivation, OS_WAIT_INDEFINITE );
		if( eStatus != ESTATUS_OK )
		{
			DebugOut( "Failed to wait on logger activation (%u):%s\n", eStatus, GetErrorText(eStatus) );
			goto errorExit;
		}

		// Write any items pending to disk
		LoggerFlush( psLog );
	}

errorExit:
	LoggerSignalClose( psLog );
}

static void LoggerStringOut( SLog* psLog,
							 char* pu8OutputString )
{
	SLogItem* psNew = NULL;

	// Do nothing and just return if the log structure or output string are NULL or emtpy
	if( (NULL == psLog) ||
		(NULL == pu8OutputString) ||
		(0 == strlen(pu8OutputString)) ||
		psLog->bTerminate )
	{
		return;
	}

	// Allocate and setup a new log item
	psNew = LoggerAllocateItem( pu8OutputString );
	if( NULL == psNew )
	{
		return;
	}

	OSCriticalSectionEnter(psLog->hListMutex);

	// Add to the end of the list
	if( psLog->psItemTail )
	{
		BASSERT( psLog->psItemHead );
		psLog->psItemTail->psNext = psNew;
	}
	else
	{
		BASSERT( NULL == psLog->psItemTail );
		psLog->psItemHead = psNew;
	}

	// Update the tail of the list
	psLog->psItemTail = psNew;

	OSCriticalSectionLeave(psLog->hListMutex);

	// Signal the logger thread that there's something to do
	LoggerActivate( psLog );
}

void LoggerInternal( SLog* psLog,
					 BOOL bAddTimestamp,
					 char* pu8Function,
					 const char* pu8FormatString,
					 ... )
{
	char u8Buffer[8192];
	char* pu8Buffer = &u8Buffer[0];
	UINT32 u32Remaining;

	// Do nothing and just return if the log structure is NULL
	if( (NULL == psLog) || psLog->bTerminate )
	{
		return;
	}

	// Get the total remaining space in the buffer
	u32Remaining = sizeof(u8Buffer);

	// Pre-terminate the buffer
	*pu8Buffer = 0;

	// Add timestamp at the start of the buffer if desired
	if( bAddTimestamp )
	{
		char u8Time[64];
		SharedTimeToText( &u8Time[0], sizeof(u8Time), RTCGet() );

		strcat(pu8Buffer, &u8Time[0]);
		pu8Buffer += strlen(&u8Time[0]);
		u32Remaining -= (UINT32)strlen(&u8Time[0]);

		strcat(pu8Buffer, " ");
		pu8Buffer += 1;
		u32Remaining -= 1;
	}

	if( pu8Function )
	{
		strcat(pu8Buffer, pu8Function);
		pu8Buffer += strlen(pu8Function);
		u32Remaining -= (UINT32)strlen(pu8Function);

		strcat(pu8Buffer, ": ");
		pu8Buffer += 2;
		u32Remaining -= 2;
	}

	// Add the rest of the string
	{
		va_list sArgs;

		va_start( sArgs, pu8FormatString );

		vsnprintf( pu8Buffer, u32Remaining, pu8FormatString, sArgs );

		va_end( sArgs );
	}

	// Now pass the whole string down to be logged
	LoggerStringOut( psLog, u8Buffer );
}

EStatus LoggerOpen( char* pu8Basename,
					UINT32 u32MaxFileCount,
					BOOL bPrintToConsole,
					SLog** ppsLog )

{
	EStatus eStatus;
	SLog* psNewLog;
	UINT32 u32Index;

	psNewLog = LoggerAllocate( pu8Basename );
	if( NULL == psNewLog )
	{
#ifdef _SERVER
		eStatus = ESTATUS_SERVER_OUT_OF_MEMORY;
#else
		eStatus = ESTATUS_OUT_OF_MEMORY;
#endif
		goto errorExit;
	}

	psNewLog->bPrintToConsole = bPrintToConsole;
	psNewLog->u32MaxFileCount = u32MaxFileCount;

	// Get rid of any log file that would exceed the maximum file count when shifted
	{
		char u8Delete[1024];

		snprintf( &u8Delete[0], 
				  sizeof(u8Delete), 
				  pu8Basename, 
				  u32MaxFileCount );

		_unlink( &u8Delete[0] );
	}

	// Shift all files up one index
	u32Index = 0;
	while( u32Index < u32MaxFileCount )
	{
		char u8Src[1024];
		char u8Dst[1024];

		// Prepare source filename
		snprintf( &u8Src[0], 
				  sizeof(u8Src), 
				  pu8Basename, 
				  u32MaxFileCount - (u32Index+1) );

		// Prepare destination filename
		snprintf( &u8Dst[0], 
				  sizeof(u8Dst), 
				  pu8Basename, 
				  u32MaxFileCount - u32Index );

		rename( &u8Src[0], &u8Dst[0] );

		u32Index++;
	}

	// Start a logger thread for this instance
	eStatus = OSThreadCreate( "Logger", (void*)psNewLog, LoggerThread, FALSE, NULL, 0, EOSPRIORITY_NORMAL );
	ERR_GOTO();

	if( ppsLog )
	{
		*ppsLog = psNewLog;
	}

errorExit:

	if( eStatus != ESTATUS_OK )
	{
		LoggerFreeSafe( &psNewLog );
	}

	return( eStatus );
}

void LoggerClose( SLog* psLog )
{
	EStatus eStatus;

	psLog->bPrintToConsole = FALSE;

	// Compose and send a final message to the log
	{
		char u8Buffer[1024];
		strcpy( &u8Buffer[0], "Log terminated: " );
		strcat( &u8Buffer[0], psLog->pu8Basename );
		strcat( &u8Buffer[0], "\n" );
		LoggerStringOut( psLog, &u8Buffer[0] );
	}

	// Activate the service thread and then wait for exit
	psLog->bTerminate = TRUE;
	LoggerActivate( psLog );

	eStatus = OSSemaphoreGet( psLog->hClosure, OS_WAIT_INDEFINITE );
	if( eStatus != ESTATUS_OK )
	{
		DebugOut( "Failed to wait for logger termination (%u): %s\n", eStatus, GetErrorText(eStatus) );
	}

	LoggerFreeSafe( &psLog );
}

