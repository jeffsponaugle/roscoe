#include "Shared/Shared.h"
#include "Shared/SysEvent.h"

// SysEvent timeout in milliseconds - the amount of time we wait for all other SysEvent
// consumers to signal the SysEvent module
#define	SYSEVENT_TIMEOUT			(10*1000)

// Uncomment to watch sysevent deployment/acknowledgement
// #define	WATCH_SYSEVENTS				1

// Structure for converting mask to a textual name
typedef struct SSysEvents
{
	uint32_t u32Mask;						// Bitmask for this sysevent
	char *peSysEventName;				// Textual name for this sysevent
} SSysEvents;

#ifdef WATCH_SYSEVENTS
static const SSysEvents sg_sEventNames[] =
{
	{SYSEVENT_PRE_OS,					"SYSEVENT_PRE_OS"},
	{SYSEVENT_POST_OS,					"SYSEVENT_POST_OS"},
	{SYSEVENT_FILESYSTEM_INIT,			"SYSEVENT_FILESYSTEM_INIT"},
	{SYSEVENT_FILESYSTEM_SHUTDOWN,		"SYSEVENT_FILESYSTEM_SHUTDOWN"},
	{SYSEVENT_SETTING_INIT_START,		"SYSEVENT_SETTING_INIT_START"},
	{SYSEVENT_SETTING_INIT_END,			"SYSEVENT_SETTING_INIT_END"},

	// End of list
	{0, NULL}
};
#endif

// One of (many) system event callbacks
typedef struct SSysEventCallbacks
{
	const char *peRegistrantFunction;
	EStatus (*SysEventCallback)(uint32_t u32EventMask,
								int64_t s64Message);
	struct SSysEventCallbacks *psNextLink;
} SSysEventCallbacks;

// SysEvent structure - one for each type of SysEvent
typedef struct SSysEventTable
{
	// # Of (total) consumers for this SysEvent	
	uint8_t u8ConsumerCount;					
	
	// Event esmaphore for signalling (under the OS)
	SOSSemaphore sEvent;
	
	// Event count for signalling (outside the OS)
	uint8_t u8EventResponseCount;						
		
	// Link to all consumers of this SysEvent
	SSysEventCallbacks *psConsumers;
} SSysEventTable;

#ifdef WATCH_SYSEVENTS
static void SysEventNameDebugOut(uint32_t u32Mask)
{
	const SSysEvents *psEvent = sg_sEventNames;
	
	while (psEvent->peSysEventName)
	{
		if (psEvent->u32Mask == u32Mask)
		{
			DebugOut("%s", psEvent->peSysEventName);
			return;
		}
		
		++psEvent;
	}
	
	DebugOut("Unknown SysEvent Mask - 0x%.8x\n", u32Mask);
}
#endif

// SysEvent table - all consumers, callbacks, etc...
static SSysEventTable sg_sSysEventTable[SYSEVENT_TYPE_COUNT];

// Register a caller for a given system event mask
EStatus SysEventRegisterInternal(uint32_t u32EventMask,
								 const char *peRegistrantFunction,
								 EStatus (*SysEventCallback)(uint32_t u32EventMask,
															 int64_t s64Message))
{
	EStatus eStatus;
	uint8_t u8Loop;
	SSysEventTable *psEvent = sg_sSysEventTable;
	SSysEventCallbacks *psCallback = NULL;

#ifdef WATCH_SYSEVENTS
	DebugOut("%s: Register event:\n  ");
	SysEventNameDebugOut(u32EventMask);
	DebugOut(" from %s\n", peRegistrantFunction);
#endif
	
	// Run through all of the bit fields and register the callback for
	// any and all SysEvents that it's requesting
	for (u8Loop = 0; u8Loop < (sizeof(sg_sSysEventTable) / sizeof(sg_sSysEventTable[0])); u8Loop++)
	{
		if (u32EventMask & 1)
		{
			// If we have no consumers, create a semaphore for it
			if (0 == psEvent->u8ConsumerCount)
			{
				// Init a semaphore. The count doesn't matter since it's 0.
				eStatus = OSSemaphoreCreate(&psEvent->sEvent,
											0,
											0xffff);
				if (eStatus != ESTATUS_OK)
				{
					goto errorExit;
				}
			}
			
			// Create a callback structure
			psCallback = MemAlloc(sizeof(*psCallback));
			if (NULL == psCallback)
			{
				eStatus = ESTATUS_OUT_OF_MEMORY;
				goto errorExit;
			}
			
			// Link it in to our list of consumers
			psCallback->SysEventCallback = SysEventCallback;
			psCallback->psNextLink = psEvent->psConsumers;
			psCallback->peRegistrantFunction = peRegistrantFunction;
			psEvent->psConsumers = psCallback;

			// Increase the consumer count for this SysEvent
			++psEvent->u8ConsumerCount;
		}
		else
		{
			// Bit is a 0 - move to the next SysEvent
		}
				
		// Next event
		u32EventMask >>= 1;
		++psEvent;
	}
	
	// All good
	eStatus = ESTATUS_OK;
	
errorExit:
	return(eStatus);
}

// Signal a SysEvent to all consumers
EStatus SysEventSignal(uint32_t u32EventMaskOriginal,
					   int64_t s64Message)
{
	EStatus eStatus = ESTATUS_OK;
	EStatus eStatus2 = ESTATUS_OK;
	SSysEventTable *psEvent = sg_sSysEventTable;
	uint8_t u8SignalNumber = 0;
	uint32_t u32EventMask = u32EventMaskOriginal;

#ifdef WATCH_SYSEVENTS
	DebugOut("%s: Signalling ", __FUNCTION__);
	SysEventNameDebugOut(u32EventMask);
	DebugOut(":\n");
#endif
	
	// Run through all events that we're signalling and wait for the consumers to respond.
	while (u32EventMask)
	{
		// Only signal the event if we have consumers for it
		if ((u32EventMask & 1) &&
			(psEvent->u8ConsumerCount))
		{
			SSysEventCallbacks *psConsumers;
			
			psConsumers = psEvent->psConsumers;
			
			// Zero out the response count from the consumers in case it's not running
			// under the OS
			psEvent->u8EventResponseCount = 0;

			// Notify all consumers
			while (psConsumers)
			{
				if (psConsumers->SysEventCallback)
				{
#ifdef WATCH_SYSEVENTS
					DebugOut("Calling function (registered by %s)\n", psConsumers->peRegistrantFunction);
#endif
					(void) psConsumers->SysEventCallback(1 << u8SignalNumber,
														 s64Message);
#ifdef WATCH_SYSEVENTS
					DebugOut("Done calling function (registered by %s)\n", psConsumers->peRegistrantFunction);
#endif
				}
				
				psConsumers = psConsumers->psNextLink;
			}
			
			// If the OS is running, then consume as many semaphores as as we have
			// consumers for this event
			if (OSIsRunning())
			{
				uint8_t u8Loop;
				
				for (u8Loop = 0; u8Loop < psEvent->u8ConsumerCount; u8Loop++)
				{
					eStatus = OSSemaphoreGet(psEvent->sEvent,
											 SYSEVENT_TIMEOUT);
					if (ESTATUS_OK == eStatus)
					{
						// Good to go. Next!
					}
					else
					if (ESTATUS_TIMEOUT == eStatus)
					{
						// This means we're missing a signal and it's a timeout.
						DebugOut("%s: Timeout for signal to mask 0x%.8x\r\n", __FUNCTION__, (1 << u8SignalNumber));
						
						// Indicate that we're missing at least one callback
						eStatus2 = ESTATUS_SYSEVENT_MISSING_SIGNAL;
					}
					else
					{
						// Unknown error - this shouldn't happen
						BASSERT(0);
					}
				}
			}
			else
			{
				// When we're not running 
				if (psEvent->u8EventResponseCount != psEvent->u8ConsumerCount)
				{
					DebugOut("%s: Lack of signal for mask 0x%.8x - Expected %u, got %u\r\n", __FUNCTION__, (1 << u8SignalNumber), psEvent->u8ConsumerCount, psEvent->u8EventResponseCount);
					
					// Indicate that we're missing at least one callback
					eStatus2 = ESTATUS_SYSEVENT_MISSING_SIGNAL;
				}
			}
		}
		
		// Next event
		++psEvent;
		u32EventMask >>= 1;
		u8SignalNumber++;
	}
	
#ifdef WATCH_SYSEVENTS
	DebugOut("%s: Signalling ", __FUNCTION__);
	SysEventNameDebugOut(u32EventMaskOriginal);

	if (eStatus2 != ESTATUS_OK)
	{
		DebugOut(" complete - missed signal\n");
	}
	else
	{
		DebugOut(" complete - OK\n", __FUNCTION__);
	}
#endif
	
	return(eStatus2);
}

// Acknwoledge a SysEvent
EStatus SysEventAck(uint32_t u32Mask)
{
	EStatus eStatus = ESTATUS_OK;
	EStatus eStatus2 = ESTATUS_OK;
	SSysEventTable *psEvent = sg_sSysEventTable;
	uint8_t u8SignalNumber = 0;
	
	while (u32Mask)
	{
		if ((u32Mask & 1) &&
			(psEvent->u8ConsumerCount))
		{
#ifdef WATCH_SYSEVENTS
			DebugOut("%s: Acknowledging ", __FUNCTION__);
			SysEventNameDebugOut(1 << u8SignalNumber);
			DebugOut("\n");
#endif
			
			if (OSIsRunning())
			{
				// Tickle the OS's semaphore
				eStatus = OSSemaphorePut(psEvent->sEvent,
										 1);
				if (ESTATUS_OK == eStatus)
				{
					// Good to go. Next!
				}
				else
				{
					// Record the error and move on - ensure it gets returned
					// to the caller letting them know that ACKing wasn't
					// possible
					eStatus2 = eStatus;
				}
			}
			else
			{
				// OS Isn't running - just 
				++psEvent->u8EventResponseCount;
			}
		}
		
		// Next event
		++psEvent;
		u32Mask >>= 1;
		u8SignalNumber++;
	}
	
	return(eStatus2);
}

// Initialize the SysEvent
EStatus SysEventInit(void)
{
	// Clear out the SysEvent structure
	memset((void *) sg_sSysEventTable, 0, sizeof(sg_sSysEventTable));

	return(ESTATUS_OK);
}