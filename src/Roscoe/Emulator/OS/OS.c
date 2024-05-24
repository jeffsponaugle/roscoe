#include <stdio.h>
#include <stdarg.h>
#include "Shared/Shared.h"
#include "Arch/Arch.h"
#include "OS/OSPort.h"
#include "Platform/Platform.h"
#include "Shared/SysEvent.h"

void OSInitEarly(void)
{
	OSPortInitEarly();
}

void OSInit(void)
{
   EStatus eStatus;

	// Register for pre-OS init
	eStatus = SysEventRegister(SYSEVENT_PRE_OS,
							   FilesystemInit);
	BASSERT(ESTATUS_OK == eStatus);
  
	OSPortInit();
}

// Thread functions

// Creates a thread
EStatus OSThreadCreate(char *peThreadName,
					   void *pvThreadValue,
					   void (*pvThreadEntry)(void *pvThreadValue),
					   bool bCreateSuspended,
					   SOSThreadHandle *psThreadHandle,
					   uint32_t u32StackSize,
					   EOSPriority ePriority)
{
	return(OSPortThreadCreate(peThreadName,
							  pvThreadValue,
							  pvThreadEntry,
							  bCreateSuspended,
							  psThreadHandle,
							  u32StackSize,
							  ePriority));
}

// Resumes a suspended thread
EStatus OSThreadResume(SOSThreadHandle *psThreadHandle)
{
	return(OSPortThreadResume(psThreadHandle));
}

// Causes thread to give up its timeslice for another thread
void OSThreadYield(void)
{
	if( ArchOSIsRunning() )
	{
		OSPortThreadYield();
	}
}

EStatus OSThreadSetName(char *peThreadName)
{
	return(OSPortThreadSetName(peThreadName));
}

// Sets the thread name when we're in release mode
void OSThreadSetNameprintf(char *pu8Format, ...)
{
#ifndef _RELEASE
	va_list ap;
	char eMsgBuf[512];

	va_start(ap, pu8Format);
	vsnprintf(eMsgBuf, sizeof(eMsgBuf) - 1, (const char *) pu8Format, ap);
	va_end(ap);

	OSThreadSetName(eMsgBuf);
#endif
}

// Delete a thread
EStatus OSThreadDestroy(SOSThreadHandle *psThreadHandle)
{
	return(OSPortThreadDestroy(psThreadHandle));
}

// Semaphore functions

EStatus OSSemaphoreCreate(SOSSemaphore *psSemaphore,
						  uint32_t u32InitialCount,
						  uint32_t u32MaximumCount)
{
	return(OSPortSemaphoreCreate(psSemaphore,
								 u32InitialCount,
								 u32MaximumCount));
}

EStatus OSSemaphoreGet(SOSSemaphore sSemaphore,
					   uint32_t u32TimeoutMilliseconds)
{
	return(OSPortSemaphoreGet(sSemaphore,
							  u32TimeoutMilliseconds));
}

EStatus OSSemaphorePut(SOSSemaphore sSemaphore,
					   uint32_t u32PutCount)
{
	return(OSPortSemaphorePut(sSemaphore,
							  u32PutCount));
}

EStatus OSSemaphoreGetCount(SOSSemaphore sSemaphore,
							uint32_t *pu32Count)
{
	return(OSPortSemaphoreGetCount(sSemaphore,
								   pu32Count));
}


EStatus OSSemaphoreDestroy(SOSSemaphore *psSemaphore)
{
	return(OSPortSemaphoreDestroy(psSemaphore));
}

// Critical section functions

EStatus OSCriticalSectionCreate(SOSCriticalSection *psCriticalSection)
{
	return(OSPortCriticalSectionCreate(psCriticalSection));
}

EStatus OSCriticalSectionEnter(SOSCriticalSection sCriticalSection)
{
	return(OSPortCriticalSectionEnter(sCriticalSection));
}

EStatus OSCriticalSectionLeave(SOSCriticalSection sCriticalSection)
{
	return(OSPortCriticalSectionLeave(sCriticalSection));
}

EStatus OSCriticalSectionDestroy(SOSCriticalSection *psCriticalSection)
{
	return(OSPortCriticalSectionDestroy(psCriticalSection));
}

void OSSleep(uint64_t u64Milliseconds)
{
	if (false == ArchOSIsRunning())
	{
		return;
	}
	
	if (0 == u64Milliseconds)
	{
		OSThreadYield();
		return;
	}

	OSPortSleep(u64Milliseconds);
}

EStatus OSQueueCreate(SOSQueue *psQueue,
					  char *peQueueName,
					  uint32_t u32ItemSize,
					  uint32_t u32ItemCount)
{
	return(OSPortQueueCreate(psQueue,
							 peQueueName,
							 u32ItemSize,
							 u32ItemCount));
}
EStatus OSQueueGet(SOSQueue sQueue,
				   void *pvBuffer,
				   uint32_t u32TimeoutMilliseconds)
{
	return(OSPortQueueGet(sQueue,
						  pvBuffer,
						  u32TimeoutMilliseconds));
}

EStatus OSQueuePut(SOSQueue sQueue,
				   void *pvBuffer,
				   uint32_t u32TimeoutMilliseconds)
{
	return(OSPortQueuePut(sQueue,
						  pvBuffer,
						  u32TimeoutMilliseconds));
}

EStatus OSQueueGetAvailable(SOSQueue sQueue,
							uint32_t *pu32ItemCount)
{
	return(OSPortQueueGetAvailable(sQueue,
								   pu32ItemCount));
}

EStatus OSQueueSetFIFOMode(SOSQueue sQueue,
						   bool bFIFOModeEnabled)
{
	return(OSPortQueueSetFIFOMode(sQueue,
								  bFIFOModeEnabled));
}

EStatus OSQueueClear(SOSQueue sQueue)
{
	return(OSPortQueueClear(sQueue));
}

void OSQueueDestroy(SOSQueue *psQueue)
{
	OSPortQueueDestroy(psQueue);
}

bool OSIsRunning(void)
{
	return(ArchOSIsRunning());
}

uint64_t RTCGet(void)
{
	return(ArchRTCGet());
}

EStatus OSEventFlagSet(SOSEventFlag *psEventFlag,
					   uint64_t u64FlagsToSet)
{
	EStatus eStatus = ESTATUS_OK;
	uint64_t u64FlagsFinal;
	SOSEventFlagWait *psWait = NULL;
	uint64_t u64FlagsChanged;
	SOSEventFlagWait *psPriorWait = NULL;
	bool bInterruptsEnabled = false;
	uint64_t u64FlagsToClear = 0;

	if (false == ArchIsInterrupt())
	{
		// Protect this event flag's structure
		eStatus = OSCriticalSectionEnter(psEventFlag->sEventAccess);
		ERR_GOTO();
	}

	bInterruptsEnabled = ArchInterruptsEnabled();
	ArchInterruptDisable();
	
	// Figure out what our final flags are
	u64FlagsFinal = psEventFlag->u64EventFlags | u64FlagsToSet;

	// And which changed
	u64FlagsChanged = u64FlagsFinal ^ psEventFlag->u64EventFlags;

	// If this is true, nothing has changed - just exit.
	if (0 == u64FlagsChanged)
	{
		goto errorExit;
	}

	// Update the flags' (newly modified) value
	psEventFlag->u64EventFlags = u64FlagsFinal;

	// Run through all waiting threads to see if any pertinent bits
	// should wake up another thread.
	psWait = psEventFlag->psWaitList;

	// Find any waiting threads on the affected flags and signal them
	while (psWait)
	{
		// For any bits we're waiting on...
		if (psWait->u64FlagMask & u64FlagsFinal)
		{
			// Clear the final flags that got set.
			u64FlagsToClear |= (psWait->u64FlagMask & u64FlagsFinal);

			// If we have a flag value to change, then set it
			if (psWait->pu64ValueToChange)
			{
				*psWait->pu64ValueToChange = (u64FlagsChanged & psWait->u64FlagMask);
			}

			// This removes the waiting structure 
			if (psPriorWait)
			{
				// Subsequent links
				psPriorWait->psNextLink = psWait->psNextLink;
			}
			else
			{
				// Head of the list
				psEventFlag->psWaitList = psWait->psNextLink;
			}

			// Signal the other task that there's something to do
			eStatus = OSSemaphorePut(psWait->eEventSemaphore,
									 1);
			ERR_GOTO();
		}
		else
		{
			psPriorWait = psWait;
		}

		// Next wait structure (if any)
		psWait = psWait->psNextLink;
	}

	// Clear out all pertient flags
	psEventFlag->u64EventFlags &= ~u64FlagsToClear;
	eStatus = ESTATUS_OK;

errorExit:
	if (bInterruptsEnabled)
	{
		ArchInterruptEnable();
	}
	
	if (false == ArchIsInterrupt())
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = OSCriticalSectionLeave(psEventFlag->sEventAccess);
		}
		else
		{
			(void) OSCriticalSectionLeave(psEventFlag->sEventAccess);
		}
	}

	return(eStatus);
}

EStatus OSEventFlagGet(SOSEventFlag *psEventFlag,
					   uint64_t u64FlagsToGet,
					   uint32_t u32Timeout,
					   uint64_t *pu64FlagMaskObtained)
{
	SOSEventFlagWait *psWait = NULL;
	EStatus eStatus = ESTATUS_OK;
	bool bInterruptsEnabled;
	bool bEntryMatch = false;
	bool bLocked = false;

	if (false == ArchIsInterrupt())
	{
		eStatus = OSCriticalSectionEnter(psEventFlag->sEventAccess);
		ERR_GOTO();

		bLocked = true;
	}
	
	bInterruptsEnabled = ArchInterruptsEnabled();
	ArchInterruptDisable();
	
	// If the one or more flag bits to get are already set, then return immediately
	if (u64FlagsToGet & psEventFlag->u64EventFlags)
	{
		// We have a match on entry to this routine
		bEntryMatch = true;
		if (pu64FlagMaskObtained)
		{
			*pu64FlagMaskObtained = u64FlagsToGet & psEventFlag->u64EventFlags;
		}

		// Zero all changed bits
		psEventFlag->u64EventFlags &= ~(u64FlagsToGet & psEventFlag->u64EventFlags);
	}

	if (bEntryMatch)
	{
		// We immediately match a set flag, so just return - no need to wait

		eStatus = ESTATUS_OK;
		if (bInterruptsEnabled)
		{
			ArchInterruptEnable();
		}

		if (false == ArchIsInterrupt())
		{
			eStatus = OSCriticalSectionLeave(psEventFlag->sEventAccess);
			bLocked = false;
		}

		goto errorExit;
	}
	else
	{
		SOSEventFlagWait *psWaitPrior = NULL;
		SOSEventFlagWait *psWaitPtr = NULL;
		SOSSemaphore eEventSemaphore;

		// Need a wait structure for this event flag
		MEMALLOC(psWait, sizeof(*psWait));
		psWait->u64FlagMask = u64FlagsToGet;
		psWait->pu64ValueToChange = pu64FlagMaskObtained;

		// Create a wait semaphore
		eStatus = OSSemaphoreCreate(&psWait->eEventSemaphore,
									0,
									1);
		ERR_GOTO();

		// Add this event flag wait structure to this event flag
		psWait->psNextLink = psEventFlag->psWaitList;
		psEventFlag->psWaitList = psWait;

		if (bInterruptsEnabled)
		{
			ArchInterruptEnable();
		}

		if (false == ArchIsInterrupt())
		{
			eStatus = OSCriticalSectionLeave(psEventFlag->sEventAccess);
			bLocked = false;
			ERR_GOTO();
		}

		// Store a copy of the event semaphore
		eEventSemaphore = psWait->eEventSemaphore;

		eStatus = OSSemaphoreGet(eEventSemaphore,
								 u32Timeout);

		if (false == ArchIsInterrupt())
		{
			(void) OSCriticalSectionEnter(psEventFlag->sEventAccess);
			bLocked = true;
		}

		bInterruptsEnabled = ArchInterruptsEnabled();
		ArchInterruptDisable();
		
		if (ESTATUS_OK == eStatus)
		{
			// Got it!
		}
		else
		if (ESTATUS_TIMEOUT == eStatus)
		{
			psWaitPtr = psEventFlag->psWaitList;

			// Find our wait structure in the list
			while (psWaitPtr)
			{
				if (psWaitPtr == psWait)
				{
					// We found ourselves!
					if (NULL == psWaitPrior)
					{
						// We're head of the list
						psEventFlag->psWaitList = psWait->psNextLink;
					}
					else
					{
						// Not the first of the list
						psWaitPrior->psNextLink = psWait->psNextLink;
					}

					// This will cause the loop to exit
					psWaitPtr = NULL;
				}
				else
				{
					psWaitPtr = psWaitPtr->psNextLink;
				}
			}
		}
		else
		{
			// Not an expected error
			Syslog("OSSemaphoreTimeout return code - 0x%.8x - %s\n", eStatus, GetErrorText(eStatus));
			goto errorExit;
		}

		if (bInterruptsEnabled)
		{
			ArchInterruptEnable();
		}

		if (false == ArchIsInterrupt())
		{
			OSCriticalSectionLeave(psEventFlag->sEventAccess);
			bLocked = false;
		}
		
		(void) OSSemaphoreDestroy(&psWait->eEventSemaphore);

		if (psWait->bEventFlagDestroyed)
		{
			eStatus = ESTATUS_OS_EVENT_FLAG_DELETED;
		}

		SafeMemFree(psWait);
	}

	return(eStatus);

errorExit:
	if (bInterruptsEnabled)
	{
		ArchInterruptEnable();
	}
	
	if (false == ArchIsInterrupt())
	{
		if (bLocked)
		{
			if (ESTATUS_OK == eStatus)
			{
				eStatus = OSCriticalSectionLeave(psEventFlag->sEventAccess);
			}
			else
			{
				(void) OSCriticalSectionLeave(psEventFlag->sEventAccess);
			}
		}
	}
	
	return(eStatus);
}

EStatus OSEventFlagDestroy(SOSEventFlag **ppsEventFlag)
{
	EStatus eStatus = ESTATUS_OK;
	SOSEventFlagWait *psWait;
	SOSEventFlag *psEventFlag = NULL;
	bool bLocked = false;
	
	psEventFlag = *ppsEventFlag;

	if (NULL == psEventFlag)
	{
		goto errorExit;
	}

	if (false == ArchIsInterrupt())
	{
		eStatus = OSCriticalSectionEnter(psEventFlag->sEventAccess);
		ERR_GOTO();

		bLocked = true;
	}
	
	psWait = psEventFlag->psWaitList;

	// Flag any waiting threads that this wait structure is going away
	while (psWait)
	{
		EStatus eStatus;

		psWait->bEventFlagDestroyed = true;
		if (psWait->pu64ValueToChange)
		{
			*psWait->pu64ValueToChange = 0;
		}

		// Release the event on the waiting thread
		eStatus = OSSemaphorePut(psWait->eEventSemaphore,
								 1);
		ERR_GOTO();
		psWait = psWait->psNextLink;
	}

	// Nothing more waiting
	psEventFlag->psWaitList = NULL;

errorExit:
	if (false == ArchIsInterrupt())
	{
		// Only unlock the critical section for the event if it's locked
		if (bLocked)
		{
			if (ESTATUS_OK == eStatus)
			{
				eStatus = OSCriticalSectionLeave(psEventFlag->sEventAccess);
			}
			else
			{
				(void) OSCriticalSectionLeave(psEventFlag->sEventAccess);
			}
		}
	}
	
	// Critical section destroy!
	if (ESTATUS_OK == eStatus)
	{
		eStatus = OSCriticalSectionDestroy(&psEventFlag->sEventAccess);
	}
	else
	{
		(void) OSCriticalSectionDestroy(&psEventFlag->sEventAccess);
	}

	SafeMemFree(psEventFlag);

	return(eStatus);	
}

EStatus OSEventFlagCreate(SOSEventFlag **ppsEventFlag,
						  uint64_t u64InitialEventFlagSetting)
{
	EStatus eStatus;
	SOSEventFlag *psEventFlag = NULL;
	bool bLocked = false;

	MEMALLOC(psEventFlag, sizeof(*psEventFlag));

	// Init this event flag's critical section and set the initial flags
	psEventFlag->u64EventFlags = u64InitialEventFlagSetting;
	eStatus = OSCriticalSectionCreate(&psEventFlag->sEventAccess);
	ERR_GOTO();
		
errorExit:
	*ppsEventFlag = psEventFlag;
	return(eStatus);
}


EStatus OSgmtime( struct tm* psTimeDest, uint64_t u64Timestamp )
{
	return( OSPortGmtime(psTimeDest, u64Timestamp) );
}


EStatus OSlocaltime( struct tm* psTimeDest, uint64_t u64Timestamp )
{
	return( OSPortLocaltime(psTimeDest, u64Timestamp) );
}


bool OSIsIdleTask(void)
{
	return(OSPortIsIdleTask());
}

#define	QUEUE_TEST_ITEMS		40

void QueueTest(void)
{
	uint32_t u32Loop;
	uint32_t u32Data;
	SOSQueue sQueue;
	EStatus eStatus;

	eStatus = OSQueueCreate(&sQueue,
							"Queue test",
							sizeof(u32Loop),
							QUEUE_TEST_ITEMS);
	BASSERT(ESTATUS_OK == eStatus);

	DebugOut("Putting queue data\n");

	// Now fill up the queue with data
	for (u32Loop = 0; u32Loop < QUEUE_TEST_ITEMS; u32Loop++)
	{
		DebugOut("Putting queue data - %u\n", u32Loop);
		eStatus = OSQueuePut(sQueue,
							 &u32Loop,
						     OS_WAIT_INDEFINITE);
		BASSERT(ESTATUS_OK == eStatus);
	}

	DebugOut("Putting one more item\n");

	// Queue is now full. If we attempt to add another item, we should get a queue full error
	eStatus = OSQueuePut(sQueue,
						 &u32Loop,
						 0);
	DebugOut("eStatus=%u, %s\n", eStatus, GetErrorText(eStatus));
	BASSERT((ESTATUS_OS_QUEUE_FULL == eStatus) ||
			(ESTATUS_ERRNO_RESOURCE_TEMPORARILY_UNAVAILABLE == eStatus));

	DebugOut("Turning on FIFO mode\n");

	// Great! Now turn on FIFO mode.
	eStatus = OSQueueSetFIFOMode(sQueue,
								 true);
	BASSERT(ESTATUS_OK == eStatus);

	DebugOut("Putting in an overflow value of %u\n", u32Loop);

	// Now we add another item. It should be OK!
	eStatus = OSQueuePut(sQueue,
						 &u32Loop,
						 0);
	BASSERT(ESTATUS_OK == eStatus);

	DebugOut("Getting a queue item\n");

	// Now get the first item in the list. The value should be 1, because the 0th value got
	// bumped out
	eStatus = OSQueueGet(sQueue,
						 (void *) &u32Data,
						 OS_WAIT_INDEFINITE);
	BASSERT(ESTATUS_OK == eStatus);
	BASSERT(u32Data == 1);

	eStatus = OSQueueGet(sQueue,
						 (void *) &u32Data,
						 OS_WAIT_INDEFINITE);
	BASSERT(ESTATUS_OK == eStatus);
	BASSERT(u32Data == 2);

	eStatus = OSQueueGet(sQueue,
						 (void *) &u32Data,
						 OS_WAIT_INDEFINITE);
	BASSERT(ESTATUS_OK == eStatus);
	BASSERT(u32Data == 3);

	DebugOut("Complete!");
}
