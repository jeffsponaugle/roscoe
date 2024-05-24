#include "Shared/Shared.h"
#include "Arch/Arch.h"
#include "Shared/Timer.h"

// Linked list of timer structures

// Systick based timer
static STimer *sg_psTimerList = NULL;

// High resolution timer
static STimer *sg_psTimerHRList = NULL;

// Initializes timer subsystem (on powerup, before OS start)
void TimerInit(void)
{
	sg_psTimerList = NULL;
	sg_psTimerHRList = NULL;
}

// Called once per systick (usually every 10ms) or per high resolution timer tick (usually every 1ms)
static void TimerTickInternal(STimer *psTimer,
							  uint32_t u32TickRate)
{
	while (psTimer)
	{
		if (psTimer->bRunning)
		{
			// Step u32TickRate # of milliseconds
			if (psTimer->u32TimerMS)
			{
				psTimer->u32CounterMS += u32TickRate;
			}

			// While we've overflowed 
			while( psTimer->bRunning && (psTimer->u32CounterMS >= psTimer->u32TimerMS) )
			{
				psTimer->u32CounterMS -= psTimer->u32TimerMS;
				
				// If our reload value is 0, then it's a one-shot timer
				if (0 == psTimer->u32ReloadMS)
				{
					psTimer->bRunning = false;
					psTimer->u32CounterMS = 0;
				}
				else
				{
					// Reload the timer with the reload value and keep going
					psTimer->u32TimerMS = psTimer->u32ReloadMS;
				}

				// Timer expired. Call the callback.
				if (psTimer->Handler)
				{
					psTimer->Handler(psTimer,
									 psTimer->pvCallbackValue);
				}
			}
		}

		psTimer = psTimer->psNextLink;
	}
}

void TimerTick(void)
{
	TimerTickInternal(sg_psTimerList,
					  ArchGetTimerTickRate());
}

void TimerHRTick(void)
{
	TimerTickInternal(sg_psTimerHRList,
					  ArchGetTimerHRTickRate());
}

static STimer *TimerFindInList(STimer *psTimer,
							   STimer *psTimerToFind,
							   STimer **ppsTimerPrior)
{
	while ((psTimer) &&
		   (psTimer != psTimerToFind))
	{
		if (ppsTimerPrior)
		{
			*ppsTimerPrior = psTimer;
		}

		psTimer = psTimer->psNextLink;
	}

	// If this asserts, it likely means there is a mix of TimerHR and Timer functions mixed
	// on a given timer. Only use Timerxxxx APIs for regular resolution and TimerHRxxxxx APIs
	// for the higher resolution timers
	BASSERT(psTimer);
	return(psTimer);
}

static EStatus TimerCreateInternal(STimer **ppsTimerHead,
								   STimer **ppsTimer)
{
	EStatus eStatus = ESTATUS_OK;

	*ppsTimer = MemAlloc(sizeof(**ppsTimer));
	if (NULL == *ppsTimer)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
	}
	else
	{
		// All good
		(*ppsTimer)->psNextLink = *ppsTimerHead;
		*ppsTimerHead = *ppsTimer;
	}

	return(eStatus);
}

EStatus TimerCreate(STimer **ppsTimer)
{
	return(TimerCreateInternal(&sg_psTimerList,
		   					   ppsTimer));
}

EStatus TimerHRCreate(STimer **ppsTimer)
{
	return(TimerCreateInternal(&sg_psTimerHRList,
		   					   ppsTimer));
}

static EStatus TimerDeleteInternal(STimer **ppsTimerHead,
								   STimer *psTimer)
{
	STimer *psPrior = NULL;

	psTimer = TimerFindInList(*ppsTimerHead,
							  psTimer,
							  &psPrior);
	BASSERT(psTimer);

	// Remove the timer from the list
	if (NULL == psPrior)
	{
		// Head of list
		*ppsTimerHead = psTimer->psNextLink;
	}
	else
	{
		// Not head of list
		psPrior->psNextLink = psTimer->psNextLink;
	}

	// Now it's out of the list. Deallocate it
	MemFree(psTimer);
	return(ESTATUS_OK);
}

EStatus TimerDelete(STimer *psTimer)
{
	return(TimerDeleteInternal(&sg_psTimerList,
					   		   psTimer));
}

EStatus TimerHRDelete(STimer *psTimer)
{
	return(TimerDeleteInternal(&sg_psTimerHRList,
					   		   psTimer));
}

static EStatus TimerSetInternal(STimer *psTimerHead,
								STimer *psTimer,
								bool bStartAutomatically,
								uint32_t u32InitialMS,
								uint32_t u32ReloadMS,
								void (*ExpirationCallback)(STimer *psTimer,
														   void *pvCallbackValue),
								void *pvCallbackValue)
{
	psTimer = TimerFindInList(psTimerHead,
							  psTimer,
							  NULL);
	BASSERT(psTimer);

	// Stop the timer while we set it to other stuff
	psTimer->bRunning = false;
	psTimer->u32CounterMS = 0;

	psTimer->u32TimerMS = u32InitialMS;
	psTimer->u32ReloadMS = u32ReloadMS;
	psTimer->pvCallbackValue = pvCallbackValue;
	psTimer->Handler = ExpirationCallback;

	// Now start (or not) the timer
	psTimer->bRunning = bStartAutomatically;

	return(ESTATUS_OK);
}

EStatus TimerSet(STimer *psTimer,
				 bool bStartAutomatically,
				 uint32_t u32InitialMS,
				 uint32_t u32ReloadMS,
				 void (*ExpirationCallback)(STimer *psTimer,
				 						    void *pvCallbackValue),
				 void *pvCallbackValue)
{
	return(TimerSetInternal(sg_psTimerList,
							psTimer,
							bStartAutomatically,
							u32InitialMS,
							u32ReloadMS,
							ExpirationCallback,
							pvCallbackValue));
}

EStatus TimerHRSet(STimer *psTimer,
				   bool bStartAutomatically,
				   uint32_t u32InitialMS,
				   uint32_t u32ReloadMS,
				   void (*ExpirationCallback)(STimer *psTimer,
				 							  void *pvCallbackValue),
				   void *pvCallbackValue)
{
	return(TimerSetInternal(sg_psTimerHRList,
							psTimer,
							bStartAutomatically,
							u32InitialMS,
							u32ReloadMS,
							ExpirationCallback,
							pvCallbackValue));
}

static EStatus TimerStartInternal(STimer *psTimerHead,
								  STimer *psTimer)
{
	psTimer = TimerFindInList(psTimerHead,
							  psTimer,
							  NULL);
	BASSERT(psTimer);

	psTimer->bRunning = true;

	return(ESTATUS_OK);
}

EStatus TimerStart(STimer *psTimer)
{
	return(TimerStartInternal(sg_psTimerList,
							  psTimer));
}

EStatus TimerHRStart(STimer *psTimer)
{
	return(TimerStartInternal(sg_psTimerHRList,
							  psTimer));
}

static EStatus TimerStopInternal(STimer *psTimerHead,
								 STimer *psTimer)
{
	psTimer = TimerFindInList(psTimerHead,
							  psTimer,
							  NULL);
	BASSERT(psTimer);

	psTimer->bRunning = false;

	return(ESTATUS_OK);
}

EStatus TimerStop(STimer *psTimer)
{
	return(TimerStopInternal(sg_psTimerList,
							 psTimer));
}

EStatus TimerHRStop(STimer *psTimer)
{
	return(TimerStopInternal(sg_psTimerHRList,
							 psTimer));
}

uint32_t TimerGetResolution(void)
{
	return(ArchGetTimerTickRate());
}

uint32_t TimerHRGetResolution(void)
{
	return(ArchGetTimerHRTickRate());
}

static SOSSemaphore sg_sTimerHRTestSemaphore;

static uint64_t sg_u64Timestamp = 0;

static void TimerHRExpirationCallback(STimer *psTimer,
									  void *pvCallbackValue)
{
	EStatus eStatus;
	
	// Elapsed time is now in sg_u64Timestamp
	sg_u64Timestamp = RTCGet() - sg_u64Timestamp;
	
	eStatus = OSSemaphorePut(sg_sTimerHRTestSemaphore,
							 1);
	BASSERT(ESTATUS_OK == eStatus);
}

static void TimerHRTestEntry(void *pvUser)
{
	EStatus eStatus;
	STimer *psTimer = NULL;
	uint64_t u64RandomTime = 0;
	bool bOnce = false;
	
	DebugOut("%s: Started\n", __FUNCTION__);
	
	eStatus = TimerHRCreate(&psTimer);
	BASSERT(ESTATUS_OK == eStatus);
	
	while (1)
	{
		// Let's test 1-999 msec
		u64RandomTime = SharedRandomNumber();
		
		u64RandomTime = (u64RandomTime % 999) + 1;
		
		// Let's set up the timer
		eStatus = TimerHRSet(psTimer,
						     false,
						     (uint32_t) u64RandomTime,
						     0,
						     TimerHRExpirationCallback,
						     NULL);
		BASSERT(ESTATUS_OK == eStatus);
		
		// Flag the start time
		sg_u64Timestamp = RTCGet();
		
		// Start the timer
		eStatus = TimerHRStart(psTimer);
		BASSERT(ESTATUS_OK == eStatus);
		
		// Wait for the semaphore
		eStatus = OSSemaphoreGet(sg_sTimerHRTestSemaphore,
								 OS_WAIT_INDEFINITE);
		BASSERT(ESTATUS_OK == eStatus);
		
		DebugOut("%s: Target time %3ums, measured %3ums\n", __FUNCTION__, (uint32_t) u64RandomTime, (uint32_t) sg_u64Timestamp);
		
		{
			int32_t s32Delta;
			
			s32Delta = ((int32_t) (uint32_t) u64RandomTime) - ((int32_t) sg_u64Timestamp);
			
			if ((s32Delta > -2) && (s32Delta < 2))
			{
				// Allow the delta to go crazy once - this is due to time being set
				if (bOnce)
				{
					DebugOut("%s:  FAILED - Delta is %d (%u - %u)\n", __FUNCTION__, s32Delta, (uint32_t) u64RandomTime, (uint32_t) sg_u64Timestamp);
					BASSERT((s32Delta > -2) && (s32Delta < 2));
				}
				
				bOnce = true;
			}
		}
	}
}

void TimerHRTest(void)
{
	EStatus eStatus;
	
	eStatus = OSSemaphoreCreate(&sg_sTimerHRTestSemaphore,
								0,
								1);
	BASSERT(ESTATUS_OK == eStatus);

	eStatus = OSThreadCreate("Timer test",
							 NULL,
							 TimerHRTestEntry,
							 false,
							 NULL,
							 4096,
							 EOSPRIORITY_NORMAL);
	BASSERT(ESTATUS_OK == eStatus);
}