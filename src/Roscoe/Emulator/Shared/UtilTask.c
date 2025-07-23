#include "Shared/Shared.h"
#include "Shared/UtilTask.h"
#include "Arch/Arch.h"
#include "Shared/SysEvent.h"

// Uncomment to watch UtilTask do its job
// #define	WATCH_UTILTASK				1

// How many elements are in the UtilTask queue?
#define	UTILTASK_QUEUE_DEPTH		20

typedef struct SUtilTask
{
	char *peTaskName;							// Name of this task
	const char *peRegistrantFunction;			// Who registered it?
	uint32_t u32UtilTaskKey;						// UtilTask's key
	int64_t (*Task)(uint32_t u32UtilTaskKey,		// The task itself
				  int64_t s64Message);
	
	struct SUtilTask *psNextLink;				// Link to the next UtilTask
} SUtilTask;

// UtilTask
typedef struct SUtilTaskSignal
{
	SUtilTask *psTask;							// Task to signal
	int64_t s64MessageIn;							// Message to pass to signal
	
	// Callback function for non-blocking operation
	void (*CompletionCallback)(uint32_t u32UtilTaskKey,
							   int64_t s64UtilTaskResponse);
	
	// Signal semaphore for blocking operations (NULL if not)
	SOSSemaphore sSignalSemaphore;
	
	// Place to put response;
	int64_t *ps64Response;
} SUtilTaskSignal;

// Linked list of active UtilTasks
static SUtilTask *sg_psTasks;

// Our queue of tasks to be worked on
static SOSQueue sg_sUtilTaskQueue;

// Textual name of the utiltask that's currently running
static char *sg_peUtilTaskRunning = NULL;

// Looks through the UtilTask linked list of tasks and finds the task containing
// the u32UtilTaskKey.
static SUtilTask *UtilTaskFindTaskByKey(uint32_t u32UtilTaskKey)
{
	SUtilTask *psTask;
	
	psTask = sg_psTasks;
	while (psTask)
	{
		if (psTask->u32UtilTaskKey == u32UtilTaskKey)
		{
			break;
		}
		psTask = psTask->psNextLink;
	}
	
	return(psTask);
}

static EStatus UtilTaskSignalInternal(uint32_t u32UtilTaskKey,
									  int64_t s64MessageIn,
									  int64_t *ps64Response,
									  void (*CompletionCallback)(uint32_t u32UtilTaskKey,
																 int64_t s64UtilTaskResponse),
									  bool bBlockBySemaphore)
{
	EStatus eStatus;
	SUtilTask *psTask;
	SUtilTaskSignal sSignal;
	SOSSemaphore sTaskBlock = NULL;
	
	psTask = UtilTaskFindTaskByKey(u32UtilTaskKey);
	if (NULL == psTask)
	{
		eStatus = ESTATUS_UTILTASK_KEY_NOT_FOUND;
		goto errorExit;
	}
	
#ifdef WATCH_UTILTASK
	DebugOut("UtilTask: Signaling %s task\n", psTask->peTaskName);
#endif

	// psTask points to our task. Let's set up an item to stick in the queue
	memset((void *) &sSignal, 0, sizeof(sSignal));
	
	sSignal.psTask = psTask;
	sSignal.s64MessageIn = s64MessageIn;
	sSignal.ps64Response = ps64Response;
	sSignal.CompletionCallback = CompletionCallback;
	
	if (NULL == CompletionCallback)
	{
		// Do we block by semaphore?
		if (bBlockBySemaphore)
		{
			// Yes, create a temporary semaphore
			eStatus = OSSemaphoreCreate(&sTaskBlock,
										0,
										1);
			if (eStatus != ESTATUS_OK)
			{
				goto errorExit;
			}
			
			// Make a copy of the pointer to the semaphore
			sSignal.sSignalSemaphore = sTaskBlock;
		}
	}
	else
	{
		// We are doing a completion callback
	}
	
	// Drop it in the queue
	eStatus = OSQueuePut(sg_sUtilTaskQueue,
						 &sSignal,
						 0);
	if (eStatus != ESTATUS_OK)
	{
		// This is actually OK. sg_peUtilTaskRunning will either point to a
		// UtilTask name that is in flash (meaning the pointer is never stale)
		// or it'll be NULL, so grabbing a copy and using it is safe.
		char *peTaskName = sg_peUtilTaskRunning;
		
		if (peTaskName)
		{
			DebugOut("%s: Failed to signal utiltask %s because %s is running (error code 0x%.8x - %s)\n", __FUNCTION__, sSignal.psTask->peTaskName, peTaskName, eStatus, GetErrorText(eStatus));
		}
		else
		{
			DebugOut("%s: Failed to signal utiltask %s - no task is running (error code 0x%.8x - %s)\n", __FUNCTION__, sSignal.psTask->peTaskName, eStatus, GetErrorText(eStatus));
		}
		
		goto errorExit;
	}

	// Do we wait for the semaphore to be obtained? If so, do so.
	if (sTaskBlock)
	{
		eStatus = OSSemaphoreGet(sTaskBlock,
								 OS_WAIT_INDEFINITE);
	}

errorExit:
	// Get rid of the task block semaphore
	if (sTaskBlock)
	{
		(void) OSSemaphoreDestroy(&sTaskBlock);
	}
		
	return(eStatus);
}

EStatus UtilTaskSignal(uint32_t u32UtilTaskKey,
					   int64_t s64MessageIn,
					   int64_t *ps64UtilTaskResponse,
					   void (*CompletionCallback)(uint32_t u32UtilTaskKey,
												  int64_t s64UtilTaskResponse))
{
	return(UtilTaskSignalInternal(u32UtilTaskKey,
								  s64MessageIn,
								  ps64UtilTaskResponse,
								  CompletionCallback,
								  false));
}

EStatus UtilTaskSignalBlocking(uint32_t u32UtilTaskKey,
				 			   int64_t s64MessageIn,
							   int64_t *ps64UtilTaskResponse)
{
	return(UtilTaskSignalInternal(u32UtilTaskKey,
								  s64MessageIn,
								  ps64UtilTaskResponse,
								  NULL,
								  true));
}

// Registers a UtilTask for signaling
EStatus UtilTaskRegisterInternal(char *peTaskName,
								 const char *peRegistrantFunction,
								 uint32_t u32UtilTaskKey,
								 int64_t (*Task)(uint32_t u32UtilTaskKey,
											   int64_t s64Message))
{
	EStatus eStatus;
	SUtilTask *psTask;
	
	// See if this UtilTask's key already exists
	psTask = UtilTaskFindTaskByKey(u32UtilTaskKey);
	if (psTask)
	{
		eStatus = ESTATUS_UTILTASK_KEY_IN_USE;
		goto errorExit;
	}
	
	psTask = MemAlloc(sizeof(*psTask));
	if (NULL == psTask)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}
	
	psTask->Task = Task;
	psTask->u32UtilTaskKey = u32UtilTaskKey;
	psTask->peTaskName = peTaskName;
	psTask->peRegistrantFunction = peRegistrantFunction;
	
	// First in the list
	psTask->psNextLink = sg_psTasks;
	sg_psTasks = psTask;
	
	// All good!
	eStatus = ESTATUS_OK;
	
errorExit:
	return(eStatus);
}

static void UtilTaskProcess(void *pvValue)
{
	EStatus eStatus;
	
	// Here to make the compiler happy
	(void) pvValue;
	
	// Post-OS broadcast
	(void) SysEventSignal(SYSEVENT_POST_OS,
						  0);
	 
	while (1)
	{
		SUtilTaskSignal sSignal;
		
		// Wait for something to do
		eStatus = OSQueueGet(sg_sUtilTaskQueue,
							 &sSignal,
							 OS_WAIT_INDEFINITE);
		if (ESTATUS_OK == eStatus)
		{
			int64_t s64Reply;
			
			// Got something to do! sSignal contains the detail of the task to go do
			
			sg_peUtilTaskRunning = sSignal.psTask->peTaskName;
			
#ifdef WATCH_UTILTASK
			DebugOut("UtilTask: Calling %s task (registed by %s)\n", sSignal.psTask->peTaskName, sSignal.psTask->peRegistrantFunction);
#endif
			
			// Call the task at hand
			s64Reply = sSignal.psTask->Task(sSignal.psTask->u32UtilTaskKey,
									  		sSignal.s64MessageIn);
			
			// All done. Let's see what we do.
			if (sSignal.CompletionCallback)
			{
				// Completion callback. Do so.
				sSignal.CompletionCallback(sSignal.psTask->u32UtilTaskKey,
										   s64Reply);
			}
			else
			if (sSignal.sSignalSemaphore)
			{
				EStatus eStatus;
				
				// Record the response message
				if (sSignal.ps64Response)
				{
					*sSignal.ps64Response = s64Reply;
				}
				
				// Wake up the blocked caller.
				eStatus = OSSemaphorePut(sSignal.sSignalSemaphore,
										 1);
				
				if (eStatus != ESTATUS_OK)
				{
					Syslog("%s: OSSemaphorePut Returned 0x%.8x unexpectedly - %s\n", __FUNCTION__, eStatus, GetErrorText(eStatus));
				}
			}
			else
			{
				// No need to acknowledge. This is a "fire and forget" UtilTask.
			}

			sg_peUtilTaskRunning = NULL;

#ifdef WATCH_UTILTASK
			DebugOut("UtilTask: Finished calling %s task (registed by %s)\n", sSignal.psTask->peTaskName, sSignal.psTask->peRegistrantFunction);
#endif
		}
		else
		{
			Syslog("%s: OSQueueGet Returned 0x%.8x unexpectedly - %s\n", __FUNCTION__, eStatus, GetErrorText(eStatus));
		}
	}
}

EStatus UtilTaskInit(void)
{
	EStatus eStatus;
	
	// Create a queue for UtilTask processing
	eStatus = OSQueueCreate(&sg_sUtilTaskQueue,
							"UtilTask",
							sizeof(SUtilTaskSignal),
							UTILTASK_QUEUE_DEPTH);
	if (eStatus != ESTATUS_OK)
	{
		goto errorExit;
	}
	
	// Now create a thread to handle the UtilTasks
	eStatus = OSThreadCreate("UtilTask",
							 NULL,
							 UtilTaskProcess,
							 false,
							 NULL,
							 7592,
							 EOSPRIORITY_NORMAL);
	
errorExit:
	return(eStatus);
}
