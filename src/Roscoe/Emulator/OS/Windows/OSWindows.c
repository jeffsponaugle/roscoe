#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include "Shared/Shared.h"
#include "OS/OSPort.h"
#include "OS/Windows/OSWindows.h"
#include "Shared/Windows/WindowsTools.h"
#include "Shared/Version.h"
#include "Shared/SysEvent.h"
#include <DbgHelp.h>

// Allow us to watch things

// Watch thread activity
#define	WATCH_THREADS			1

// Debug output stuff
#ifdef WATCH_THREADS
#define	ThreadOut(...)	DebugOutInternal(__VA_ARGS__)
#else
#define	ThreadOut(...)
#endif

// See https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2017
// for Microsoft's guidance on how to set the thread name and where this all
// comes from.
#ifndef _RELEASE
#define	MS_VC_EXCEPTION  0x406d1388
#endif

// Useful for debugging
#pragma pack(push,8)  
typedef struct tagTHREADNAME_INFO  
{  
	DWORD dwType; // Must be 0x1000.  
	LPCSTR szName; // Pointer to name (in user addr space).  
	DWORD dwThreadID; // Thread ID (-1=caller thread).  
	DWORD dwFlags; // Reserved for future use, must be zero.  
} THREADNAME_INFO;  
#pragma pack(pop)  

// Thread structure
typedef struct SThread
{
	char *peThreadName;
	void *pvThreadValue;
	void (*pvThreadEntry)(void *pvThreadValue);
	DWORD u32ThreadID;
	bool bCreateSuspended;
	HANDLE eThread;

	// This is the thread start semaphore
	SOSSemaphore sThreadStart;

	struct SThread *psNextLink;
} SThread;

typedef struct SWindowsQueue
{
	uint32_t u32ItemSize;
	uint32_t u32ItemCount;
	void *pvData;

	// Critical section for this queue
	SOSCriticalSection sQueueCriticalSection;

	// Semaphore for # of items in the queue
	SOSSemaphore sQueueSemaphoreGet;

	// Semaphore for amount of room left in the queue
	SOSSemaphore sQueueSemaphorePut;

	// Head/tail pointers
	uint32_t u32Head;
	uint32_t u32Tail;

	// true If FIFO mode enabled (oldest data is discared if full)
	bool bFIFOMode;

	// Indicator of how many items are in the queue right now
	uint32_t u32QueueCount;
} SPOSIXQueue;


// Set false if all threads haven't been unleashed yet, otherwise true if they
// are all running
static bool sg_bOSRunning = false;

// Global semaphore for the thread access list
static SOSCriticalSection sg_sThreadListCriticalSection;

// Linked list of all created threads.
static SThread *sg_psThreads = NULL;

// Semaphore for jumping in to the main app
static SOSSemaphore sg_sAppExecuteSemaphore;

// Called when the setting init ends
static EStatus AppExecOK(uint32_t u32EventMask,
						 int64_t s64Message)
{
	EStatus eStatus;

	// Signal the sysevent engine
	eStatus = SysEventAck(u32EventMask);
	BASSERT(ESTATUS_OK == eStatus);

	// Add an app execute sempahore
	eStatus = OSSemaphorePut(sg_sAppExecuteSemaphore,
							 1);
	BASSERT(ESTATUS_OK == eStatus);

	return(ESTATUS_OK);
}

void OSPortInitEarly(void)
{
	EStatus eStatus;

	// Need this for thread list access
	eStatus = OSCriticalSectionCreate(&sg_sThreadListCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);

	eStatus = OSSemaphoreCreate(&sg_sAppExecuteSemaphore,
								0,
								1);
	BASSERT(ESTATUS_OK == eStatus);

	// Wait until the "post config init" happens, then we're ready to go.
	eStatus = SysEventRegister(SYSEVENT_SETTING_INIT_END,
							   AppExecOK);
	BASSERT(ESTATUS_OK == eStatus);
}

// This is called to init the port
void OSPortInit(void)
{
}

// Returns true if the "OS" is running, otherwise false
bool ArchOSIsRunning(void)
{
	return(sg_bOSRunning);
}

// No such thing under Windows
bool OSPortIsIdleTask(void)
{
	return(false);
}

// Resumes a suspended thread
EStatus OSPortThreadResume(SOSThreadHandle *psThreadHandle)
{
	EStatus eStatus = ESTATUS_OK;

	BASSERT(0);

	return(eStatus);
}

EStatus OSPortThreadSetName(char *pu8ThreadName)
{
#ifndef _RELEASE
	THREADNAME_INFO sInfo;

	memset((void *) &sInfo, 0, sizeof(sInfo));

	sInfo.dwType = 0x1000;
	sInfo.szName = pu8ThreadName;
	sInfo.dwThreadID = GetCurrentThreadId();
	sInfo.dwFlags = 0;

	RaiseException(MS_VC_EXCEPTION, 0, sizeof(sInfo) / sizeof(ULONG_PTR), (ULONG_PTR *) &sInfo);
	return(ESTATUS_OK);
#endif

	return(ESTATUS_FUNCTION_NOT_SUPPORTED);
}

// Causes thread to give up its timeslice for another thread
void OSPortThreadYield(void)
{
	BASSERT(0);
}

EStatus OSPortQueueCreate(SOSQueue *psQueue,
						  char *peQueueName,
						  uint32_t u32ItemSize,
						  uint32_t u32ItemCount)
{
	EStatus eStatus;
	SPOSIXQueue *psPOSIXQueue = NULL;

	// If either of these assert, then you're creating a queue with no
	// elements or elements of 0 bytes in size
	BASSERT(u32ItemSize);
	BASSERT(u32ItemCount);

	// Create the queue structure
	psPOSIXQueue = MemAlloc(sizeof(*psPOSIXQueue));
	if (NULL == psPOSIXQueue)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	psPOSIXQueue->pvData = (void *) MemAlloc(u32ItemSize * u32ItemCount);
	if (NULL == psPOSIXQueue)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	// Create its critical section
	eStatus = OSCriticalSectionCreate(&psPOSIXQueue->sQueueCriticalSection);
	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}

	psPOSIXQueue->u32ItemSize = u32ItemSize;
	psPOSIXQueue->u32ItemCount = u32ItemCount;

	// Create a semaphore for the number of items in the queue
	eStatus = OSSemaphoreCreate(&psPOSIXQueue->sQueueSemaphoreGet,
								0,
								u32ItemCount);

	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}

	// Create a semaphore for the amount of room in the queue
	eStatus = OSSemaphoreCreate(&psPOSIXQueue->sQueueSemaphorePut,
								u32ItemCount,
								u32ItemCount);

errorExit:
	if (ESTATUS_OK == eStatus)
	{
		*psQueue = (SOSQueue *) psPOSIXQueue;
	}
	else
	{	
		if (psPOSIXQueue)
		{
			if (psPOSIXQueue->pvData)
			{
				MemFree(psPOSIXQueue->pvData);
				psPOSIXQueue->pvData = NULL;
			}

			MemFree(psPOSIXQueue);
		}

	}

	return(eStatus);
}

EStatus OSPortQueueGet(SOSQueue sQueue,
					   void *pvBuffer,
					   uint32_t u32TimeoutMilliseconds)
{
	EStatus eStatus = ESTATUS_OK;
	SPOSIXQueue *psPOSIXQueue = (SPOSIXQueue *) sQueue;
	uint8_t *pu8Data = NULL;

	// Park on the semaphore until we get something or we time out
	eStatus = OSSemaphoreGet(psPOSIXQueue->sQueueSemaphoreGet,
							 u32TimeoutMilliseconds);

	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}

	// Lock the critical section. We're going to adjust some stuff in the POSIX
	// structure
	OSCriticalSectionEnter(psPOSIXQueue->sQueueCriticalSection);

	// We had better have at least one item in the queue
	BASSERT(psPOSIXQueue->u32ItemCount);

	// Calculate the position of the tail pointer
	pu8Data = ((uint8_t *) psPOSIXQueue->pvData) + (psPOSIXQueue->u32Tail * psPOSIXQueue->u32ItemSize);

	// Copy the data to the target
	memcpy((void *) pvBuffer, (void *) pu8Data, psPOSIXQueue->u32ItemSize);

	// Move thte tail pointer
	psPOSIXQueue->u32Tail++;
	if (psPOSIXQueue->u32Tail >= psPOSIXQueue->u32ItemCount)
	{
		psPOSIXQueue->u32Tail = 0;
	}

	// Subtract the item count
	psPOSIXQueue->u32QueueCount--;

	// One less item in the queue, which means we need to do a put so more can be added
	eStatus = OSSemaphorePut(psPOSIXQueue->sQueueSemaphorePut,
							 1);


	// OK to unlock!
	OSCriticalSectionLeave(psPOSIXQueue->sQueueCriticalSection);

	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}

	// All good
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

EStatus OSPortQueuePut(SOSQueue sQueue,
					   void *pvBuffer,
					   uint32_t u32TimeoutMilliseconds)
{
	EStatus eStatus = ESTATUS_OK;
	SPOSIXQueue *psPOSIXQueue = (SPOSIXQueue *) sQueue;
	uint8_t *pu8Data;
	bool bLockNeeded = true;
	bool bSkipCount = false;

	// If we're in FIFO mode, we lock before we do a get
	if (psPOSIXQueue->bFIFOMode)
	{
		// Lock the queue
		OSCriticalSectionEnter(psPOSIXQueue->sQueueCriticalSection);
		bLockNeeded = false;

		// Are we out of room? If so, move the tail pointer and fall through
		if (psPOSIXQueue->u32QueueCount == psPOSIXQueue->u32ItemCount)
		{
			// We're full. Move the tail pointer.
			psPOSIXQueue->u32Tail++;
			if (psPOSIXQueue->u32Tail >= psPOSIXQueue->u32ItemCount)
			{
				psPOSIXQueue->u32Tail = 0;
			}

			// We don't adjust the semaphore if it's full
			bSkipCount = true;

			// Decrement the count, because it's incremented later (and to avoid the assert)
			--psPOSIXQueue->u32QueueCount;
		}
	}

	if (false == bSkipCount)
	{
		// Snag a put
		eStatus = OSSemaphoreGet(psPOSIXQueue->sQueueSemaphorePut,
								 u32TimeoutMilliseconds);
		if (eStatus != ESTATUS_OK)
		{
			// If we're in FIFO mode, we've locked above, so we need to unlock
			if (false == bLockNeeded)
			{
				OSCriticalSectionLeave(psPOSIXQueue->sQueueCriticalSection);
			}

			goto errorExit;
		}
	}

	if (bLockNeeded)
	{
		// Lock the queue
		OSCriticalSectionEnter(psPOSIXQueue->sQueueCriticalSection);
	}

	// Good to go! At this point the queue is locked. Figure out where the item is going to go.
	pu8Data = ((uint8_t *) psPOSIXQueue->pvData) + (psPOSIXQueue->u32Head * psPOSIXQueue->u32ItemSize);

	// Copy the data
	memcpy((void *) pu8Data, (void *) pvBuffer, psPOSIXQueue->u32ItemSize);

	// Adjust the head pointer
	psPOSIXQueue->u32Head++;
	if (psPOSIXQueue->u32Head >= psPOSIXQueue->u32ItemCount)
	{
		psPOSIXQueue->u32Head = 0;
	}

	// Make sure the queue count is still smaller than the overall item count. If not, we have a
	// logic problem somewhere, as it should not be possible.
	BASSERT(psPOSIXQueue->u32QueueCount < psPOSIXQueue->u32ItemCount);

	// Add a single item to the overall count
	++psPOSIXQueue->u32QueueCount;

	// No need to wake up the client if we've overflowed
	if (false == bSkipCount)
	{
		// Tickle the queue's semaphore so it wakes up
		eStatus = OSSemaphorePut(psPOSIXQueue->sQueueSemaphoreGet,
								 1);
	}

	// Unlock things regardless of how it turned out
	OSCriticalSectionLeave(psPOSIXQueue->sQueueCriticalSection);

	if (bSkipCount)
	{
		eStatus = ESTATUS_OS_QUEUE_OVERFLOW;
	}

errorExit:
	return(eStatus);
}

EStatus OSPortQueueClear(SOSQueue sQueue)
{
	EStatus eStatus = ESTATUS_OK;

	BASSERT(0);

	return(eStatus);
}

EStatus OSPortQueueGetAvailable(SOSQueue sQueue,
								uint32_t *pu32AvailableCount)
{
	EStatus eStatus = ESTATUS_OK;

	BASSERT(0);

	return(eStatus);
}

EStatus OSPortQueueSetFIFOMode(SOSQueue sQueue,
							   bool bFIFOModeEnabled)
{
	SPOSIXQueue *psPOSIXQueue = (SPOSIXQueue *) sQueue;

	// Lock the queue
	OSCriticalSectionEnter(psPOSIXQueue->sQueueCriticalSection);

	psPOSIXQueue->bFIFOMode = bFIFOModeEnabled;

	// Unlock the queue
	OSCriticalSectionLeave(psPOSIXQueue->sQueueCriticalSection);

	return(ESTATUS_OK);
}


static void *ThreadStub(void *pvThreadValue)
{
	SThread *psThread = (SThread *) pvThreadValue;
	EStatus eStatus;

#ifndef _RELEASE
	THREADNAME_INFO sThreadInfo;

	memset((void *) &sThreadInfo, 0, sizeof(sThreadInfo));
	sThreadInfo.dwType = 0x1000;
	sThreadInfo.szName = psThread->peThreadName;
	sThreadInfo.dwThreadID = psThread->u32ThreadID;

	// Tell MSVC what the name of this thread is
	RaiseException(MS_VC_EXCEPTION, 0, sizeof(sThreadInfo) / sizeof(ULONG_PTR), (const ULONG_PTR *) &sThreadInfo);
#endif

//	ThreadOut("ThreadStub: Start of thread '%s' - waiting for wakeup signal\n", psThread->peThreadName);

	// Wait until we get an order to get going
	eStatus = OSSemaphoreGet(psThread->sThreadStart,
							 OS_WAIT_INDEFINITE);

	// This should NEVER fail. It means we've gotten an order to start the
	// "OS".
	BASSERT(ESTATUS_OK == eStatus);

//	ThreadOut("ThreadStub: Thread '%s' released!\n", psThread->peThreadName);

	// OK, we've gotten the order to run. Go!
	psThread->pvThreadEntry(psThread->pvThreadValue);

	// All done here (if ever)
	return(NULL);
}

// Creates a thread
EStatus OSPortThreadCreate(char *peThreadName,
						   void *pvThreadValue,
						   void (*pvThreadEntry)(void *pvThreadValue),
						   bool bCreateSuspended,
						   SOSThreadHandle *psThreadHandle,
						   uint32_t u32StackSize,
						   EOSPriority ePriority)
{
	EStatus eStatus = ESTATUS_OK;
	SThread *psThread = NULL;
	DWORD u32CreationFlags = 0;

	// We do not support creating a suspended thread at this time
	BASSERT(false == bCreateSuspended);

	// Create a basic thread structure
	psThread = MemAlloc(sizeof(*psThread));
	if (NULL == psThread)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	// Create a thread start semaphore (high count doesn't matter under Windows)
	eStatus = OSSemaphoreCreate(&psThread->sThreadStart,
								0,
								1);
	if (eStatus != ESTATUS_OK)
	{
		MemFree(psThread);
		goto errorExit;
	}

	// Assign the incoming parameters
	psThread->peThreadName = peThreadName;
	psThread->pvThreadValue = pvThreadValue;
	psThread->pvThreadEntry = pvThreadEntry;
	psThread->bCreateSuspended = bCreateSuspended;

	if (bCreateSuspended)
	{
		u32CreationFlags = CREATE_SUSPENDED;
	}

	// Create a Windows thread
	psThread->eThread = CreateThread(NULL,
									 0,
									 (LPTHREAD_START_ROUTINE) ThreadStub,
									 (LPVOID) psThread,
									 u32CreationFlags,
									 &psThread->u32ThreadID);

	if (NULL == psThread->eThread)
	{
		eStatus = ESTATUS_OS_THREAD_CANT_CREATE;
		MemFree(psThread);
		goto errorExit;
	}

	// Set the thread priority if non-normal
	if (EOSPRIORITY_NORMAL != ePriority )
	{
		int s32Priority;

		if (EOSPRIORITY_IDLE == ePriority)
		{
			s32Priority = THREAD_PRIORITY_IDLE;
		}
		else 
		if (EOSPRIORITY_LOW == ePriority)
		{
			s32Priority = THREAD_PRIORITY_LOWEST;
		}
		else 
		if (EOSPRIORITY_BELOW_NORMAL == ePriority)
		{
			s32Priority = THREAD_PRIORITY_BELOW_NORMAL;
		}
		else 
		if (EOSPRIORITY_ABOVE_NORMAL == ePriority)
		{
			s32Priority = THREAD_PRIORITY_ABOVE_NORMAL;
		}
		else 
		if (EOSPRIORITY_HIGH == ePriority)
		{
			s32Priority = THREAD_PRIORITY_HIGHEST;
		}
		else 
		if (EOSPRIORITY_REALTIME == ePriority)
		{
			s32Priority = THREAD_PRIORITY_TIME_CRITICAL;
		}
		else
		{
			BASSERT_MSG("Invalid priority value");
		}

		if (SetThreadPriority(psThread->eThread, s32Priority) == 0)
		{
			eStatus = ESTATUS_OS_THREAD_PRIORITY_INVALID;
			MemFree(psThread);
			goto errorExit;
		}
	}

	// If the "OS" is running and we're not creating the thread suspended,
	// launch it
	if ((ArchOSIsRunning()) && 
		(false == bCreateSuspended))
	{
		eStatus = OSSemaphorePut(psThread->sThreadStart,
								 1);
	}

	// Link this thread into the master list of threads
	eStatus = OSCriticalSectionEnter(sg_sThreadListCriticalSection);
	ERR_GOTO();

	// Tie this thread into the master list
	psThread->psNextLink = sg_psThreads;
	sg_psThreads = psThread;

	// Unlock the list
	eStatus = OSCriticalSectionLeave(sg_sThreadListCriticalSection);
	ERR_GOTO();

	if (psThreadHandle)
	{
		*psThreadHandle = (SOSThreadHandle) psThread;
	}

	// All good!
	eStatus = ESTATUS_OK;

//	ThreadOut("Successfully created thread '%s'\n", peThreadName);

errorExit:
	return(eStatus);
}

// Delete a thread
EStatus OSPortThreadDestroy(SOSThreadHandle *psThreadHandle)
{
	EStatus eStatus = ESTATUS_OK;
	SThread *psThread = NULL;
	
	if (NULL == psThreadHandle)
	{
		// This means "kill yourself". No need to do anything.
		ExitThread(0);

		// Won't get here
	}
	else
	{
		bool bResult;

		psThread = (SThread *) *psThreadHandle;

		// This means kill off another thread
		bResult = TerminateThread((HANDLE) *psThreadHandle,
								  0);

		if (false == bResult)
		{
			eStatus = ESTATUS_OS_THREAD_DESTROY_FAILURE;
		}
		else
		{
			eStatus = ESTATUS_OK;
			*psThreadHandle = NULL;
		}
	}

	return(eStatus);
}

// Semaphore functions
EStatus OSPortSemaphoreCreate(SOSSemaphore *psSemaphore,
							  uint32_t u32InitialCount,
							  uint32_t u32MaximumCount)
{
	EStatus eStatus = ESTATUS_OK;

	*psSemaphore = (SOSSemaphore *) CreateSemaphore(NULL,
												    (LONG) u32InitialCount,
												    (LONG) u32MaximumCount,
												    NULL);
	if (NULL == *psSemaphore)
	{
		eStatus = ESTATUS_OS_SEMAPHORE_CANT_CREATE;
	}

	return(eStatus);
}

typedef struct SWaitResponseToEStatus
{
	int s32Response;			// Windows response code
	EStatus eStatus;			// EStatus equivalent
} SWaitResponseToEStatus;

static const SWaitResponseToEStatus sg_sWaitResponseToEStatus[] =
{
	{0,					ESTATUS_OK},
	{WAIT_TIMEOUT,		ESTATUS_TIMEOUT},
	{WAIT_ABANDONED,	ESTATUS_OS_SEMAPHORE_UNAVAILABLE},
	{WAIT_FAILED,		ESTATUS_OS_SEMAPHORE_GET_FAILED},
};

static EStatus WaitResponseToEStatus(int s32Response)
{
	uint32_t u32Loop;

	for (u32Loop = 0; u32Loop < (sizeof(sg_sWaitResponseToEStatus) / sizeof(sg_sWaitResponseToEStatus[0])); u32Loop++)
	{
		if (s32Response == sg_sWaitResponseToEStatus[u32Loop].s32Response)
		{
			return(sg_sWaitResponseToEStatus[u32Loop].eStatus);
		}
	}

	return(ESTATUS_OS_UNKNOWN_ERROR);
}

EStatus OSPortSemaphoreGet(SOSSemaphore sSemaphore,
						   uint32_t u32TimeoutMilliseconds)
{
	// Wait for the semaphore

	return(WaitResponseToEStatus(WaitForSingleObject((HANDLE) sSemaphore, u32TimeoutMilliseconds)));
}

EStatus OSPortSemaphorePut(SOSSemaphore sSemaphore,
						   uint32_t u32PutCount)
{
	EStatus eStatus = ESTATUS_OK;
	bool bResult;

	// Put count can't be 0
	BASSERT(u32PutCount);

	while (u32PutCount)
	{
		bResult = ReleaseSemaphore((HANDLE) sSemaphore,
								   1,
								   NULL);

		if (false == bResult)
		{
			eStatus = ESTATUS_OS_SEMAPHORE_PUT_COUNT_NOT_SUPPORTED;
			goto errorExit;
		}

		--u32PutCount;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

EStatus OSPortSemaphoreGetCount(SOSSemaphore sSemaphore,
								uint32_t *pu32SemaphoreCount)
{
	EStatus eStatus = ESTATUS_OK;

	BASSERT(0);

	return(eStatus);
}

EStatus OSPortSemaphoreDestroy(SOSSemaphore *psSemaphore)
{
	EStatus eStatus = ESTATUS_OK;

	if (NULL == *psSemaphore)
	{
		eStatus = ESTATUS_OS_SEMAPHORE_CANT_DESTROY;
		goto errorExit;
	}

	if (false == CloseHandle((HANDLE) *psSemaphore))
	{
		eStatus = ESTATUS_OS_SEMAPHORE_CANT_DESTROY;
	}
	else
	{
		// All good
		*psSemaphore = NULL;
	}

errorExit:
	return(eStatus);
}

EStatus OSPortCriticalSectionCreate(SOSCriticalSection *psCriticalSection)
{
	*psCriticalSection = (SOSCriticalSection *) malloc(sizeof(CRITICAL_SECTION));
	memset((void *) *psCriticalSection, 0, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((LPCRITICAL_SECTION) *psCriticalSection);
	return(ESTATUS_OK);
}

EStatus OSPortCriticalSectionEnter(SOSCriticalSection sCriticalSection)
{
	EnterCriticalSection((LPCRITICAL_SECTION) sCriticalSection);
	return(ESTATUS_OK);
}

EStatus OSPortCriticalSectionLeave(SOSCriticalSection sCriticalSection)
{
	LeaveCriticalSection((LPCRITICAL_SECTION) sCriticalSection);
	return(ESTATUS_OK);
}

EStatus OSPortCriticalSectionDestroy(SOSCriticalSection *psCriticalSection)
{
	DeleteCriticalSection((LPCRITICAL_SECTION) *psCriticalSection);
	free(*psCriticalSection);
	*psCriticalSection = NULL;
	return(ESTATUS_OK);
}

void OSPortQueueDestroy(SOSQueue *psQueue)
{	
	SPOSIXQueue *psQueueDestroy = (SPOSIXQueue *) *psQueue;

	(void) OSSemaphoreDestroy(&psQueueDestroy->sQueueSemaphorePut);
	(void) OSCriticalSectionDestroy(&psQueueDestroy->sQueueCriticalSection);
	MemFree(psQueueDestroy->pvData);
	MemFree(psQueueDestroy);

	*psQueue = NULL;
}

void OSPortSleep(uint64_t u64Milliseconds)
{
	Sleep((DWORD) u64Milliseconds);
}

typedef bool (WINAPI *LPFN_GLPI)(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, 
    PDWORD);

// Helper function to count set bits in the processor mask.
static DWORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
    DWORD i;
    
    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}

EStatus PlatformCPUGetInfo(uint32_t *pu32CPUPhysicalCores,
						   uint32_t *pu32CPUThreadedCores)
{
	LPFN_GLPI glpi;
    bool done = false;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
    DWORD returnLength = 0;
    DWORD logicalProcessorCount = 0;
    DWORD numaNodeCount = 0;
    DWORD processorCoreCount = 0;
    DWORD processorL1CacheCount = 0;
    DWORD processorL2CacheCount = 0;
    DWORD processorL3CacheCount = 0;
    DWORD processorPackageCount = 0;
    DWORD byteOffset = 0;
    PCACHE_DESCRIPTOR Cache;

	if (pu32CPUPhysicalCores)
	{
		*pu32CPUPhysicalCores = 0;
	}

	if (pu32CPUThreadedCores)
	{
		*pu32CPUThreadedCores = 0;
	}

    glpi = (LPFN_GLPI) GetProcAddress(
                            GetModuleHandle(TEXT("kernel32")),
                            "GetLogicalProcessorInformation");
    if (NULL == glpi) 
    {
        Syslog("\nGetLogicalProcessorInformation is not supported.\n");
        return (1);
    }

    while (!done)
    {
        DWORD rc = glpi(buffer, &returnLength);

        if (false == rc) 
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
            {
                if (buffer) 
                    free(buffer);

                buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
                        returnLength);

                if (NULL == buffer) 
                {
                    Syslog("\nError: Allocation failure\n");
                    return (2);
                }
            } 
            else 
            {
                Syslog("\nError %d\n", GetLastError());
                return (3);
            }
        } 
        else
        {
            done = true;
        }
    }

    ptr = buffer;

    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) 
    {
        switch (ptr->Relationship) 
        {
        case RelationNumaNode:
            // Non-NUMA systems report a single record of this type.
            numaNodeCount++;
            break;

        case RelationProcessorCore:
            processorCoreCount++;

            // A hyperthreaded core supplies more than one logical processor.
            logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
            break;

        case RelationCache:
            // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
            Cache = &ptr->Cache;
            if (Cache->Level == 1)
            {
                processorL1CacheCount++;
            }
            else if (Cache->Level == 2)
            {
                processorL2CacheCount++;
            }
            else if (Cache->Level == 3)
            {
                processorL3CacheCount++;
            }
            break;

        case RelationProcessorPackage:
            // Logical processors share a physical package.
            processorPackageCount++;
            break;

        default:
            Syslog("\nError: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n");
            break;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }

	if (pu32CPUThreadedCores)
	{
		*pu32CPUThreadedCores = (uint32_t) logicalProcessorCount;
	}

	if (pu32CPUPhysicalCores)
	{
		*pu32CPUPhysicalCores = (uint32_t) processorCoreCount;
	}

	return(ESTATUS_OK);
}


EStatus OSPortGmtime( struct tm* psTimeDest, uint64_t u64Timestamp )
{
	errno_t s32Error;

	s32Error = gmtime_s( psTimeDest, &u64Timestamp );
	if (s32Error )
	{
		return(ErrnoToEStatus(s32Error));
	}

	return(ESTATUS_OK);
}


EStatus OSPortLocaltime( struct tm* psTimeDest, uint64_t u64Timestamp )
{
	errno_t s32Error;

	s32Error = localtime_s( psTimeDest, &u64Timestamp );
	if (s32Error )
	{
		return(ErrnoToEStatus(s32Error));
	}

	return(ESTATUS_OK);
}

EStatus OSSemaphoreReadWriteCreate(SOSSemaphoreReadWrite *psOSSemaphoreReadWrite)
{
	EStatus eStatus = ESTATUS_OK;

	MEMALLOC(*psOSSemaphoreReadWrite, sizeof(PSRWLOCK));

	InitializeSRWLock((PSRWLOCK) *psOSSemaphoreReadWrite);

errorExit:
	return(eStatus);
}

EStatus OSSemaphoreReadWriteDestroy(SOSSemaphoreReadWrite *psOSSemaphoreReadWrite)
{
	SafeMemFree(psOSSemaphoreReadWrite);
	return(ESTATUS_OK);
}

EStatus OSSemaphoreReadLock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite)
{
	AcquireSRWLockShared((PSRWLOCK) sOSSemaphoreReadWrite);
	return(ESTATUS_OK);
}

EStatus OSSemaphoreReadUnlock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite)
{
	ReleaseSRWLockShared((PSRWLOCK) sOSSemaphoreReadWrite);
	return(ESTATUS_OK);
}

EStatus OSSemaphoreWriteLock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite)
{
	AcquireSRWLockExclusive((PSRWLOCK) sOSSemaphoreReadWrite);
	return(ESTATUS_OK);
}

EStatus OSSemaphoreWriteUnlock(SOSSemaphoreReadWrite sOSSemaphoreReadWrite)
{
	ReleaseSRWLockExclusive((PSRWLOCK) sOSSemaphoreReadWrite);
	return(ESTATUS_OK);
}

void ArchStartOS(void)
{
	SThread *psThread;
	EStatus eStatus;

	sg_bOSRunning = true;

	// Pre-OS broadcast - start things off!
	(void) SysEventSignal(SYSEVENT_PRE_OS,
						  0);

	// Run through all threads and get them going!
	OSCriticalSectionEnter(sg_sThreadListCriticalSection);

	psThread = sg_psThreads;
	while (psThread)
	{
		// Launch the waiting threads!
		eStatus = OSSemaphorePut(psThread->sThreadStart,
								 1);
		BASSERT(ESTATUS_OK == eStatus);

		psThread = psThread->psNextLink;
	}


	// All done!
	OSCriticalSectionLeave(sg_sThreadListCriticalSection);

	// Set the name of this thread
	eStatus = OSThreadSetName("WinMain");
	BASSERT((ESTATUS_OK == eStatus) ||
			(ESTATUS_FUNCTION_NOT_SUPPORTED == eStatus));

	// Wait until everything has settled
	eStatus = OSSemaphoreGet(sg_sAppExecuteSemaphore,
							 OS_WAIT_INDEFINITE);
	BASSERT(ESTATUS_OK == eStatus);
}
