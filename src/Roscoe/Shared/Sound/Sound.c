#include "Shared/Shared.h"
#include "Platform/Platform.h"
#include "Shared/Sound/Sound.h"

// Some globals to make it fun
static SSoundProfile *sg_psSoundProfile;

// Pending sound profile
static volatile SSoundProfile *sg_psSoundProfilePending = NULL;
static volatile BOOL sg_bSoundProfilePending;

// Mixdown buffer related
static INT32 *sg_ps32MixdownBuffer;
static UINT16 sg_u16MixdownBufferSize;
static INT32 *sg_ps32MixdownBufferPosition = NULL;
static UINT16 sg_u16MixdownRemaining;

// How big is the channel buffer?
static UINT16 sg_u16ChannelBufferSize;

// What's the global engine's volume setting currently?
static UINT16 sg_u16VolumeGlobal = PITCH_UNITY;

// What channel is being rendered currently?
static SSoundChannel *sg_psRenderChannel = NULL;

// Base timestamp for start of sound engine
static UINT64 sg_u64SoundBaseTimeStart = 0;

// # Of samples played since the beginning of the sound engine
static UINT64 sg_u64SamplesRendered = 0;

// Current sample rate
static UINT32 sg_u32SampleRate = 0;

// Set to TRUE if we've initialized
static BOOL sg_bSoundInitialized = FALSE;

// Critical section for sound renderer
static SOSCriticalSection sg_sSoundRenderCriticalSection;

void SoundSetLock(BOOL bLock)
{
	EStatus eStatus;

	if (bLock)
	{
		eStatus = OSCriticalSectionEnter(sg_sSoundRenderCriticalSection);
	}
	else
	{
		eStatus = OSCriticalSectionLeave(sg_sSoundRenderCriticalSection);
	}

	BASSERT(ESTATUS_OK == eStatus);
}

UINT64 SoundSamplesToTimeMs(UINT64 u64Samples)
{
	return((UINT64) (((double) u64Samples / (double) sg_u32SampleRate) * 1000.0));
}

BOOL SoundRender(UINT16 *pu16DestBuffer,
				 BOOL bPrimarySoundRequest)
{
	UINT16 u16Loop;
	INT32 s32Data;
	INT32 *ps32DataPtr;
	UINT16 u16MixdownRemaining;
	EStatus eStatus;
	UINT64 u64Time = sg_u64SoundBaseTimeStart + SoundSamplesToTimeMs(sg_u64SamplesRendered);

//	DebugOut("Delta = %u\n", (UINT32) (RTCGet() - u64Time));

	eStatus = OSCriticalSectionEnter(sg_sSoundRenderCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);

	// If we have a profile change, then change it
	if (sg_bSoundProfilePending)
	{
		// Clear the indication that we have a pending sound profile change
		sg_bSoundProfilePending = FALSE;
		
		// Assign the new sound profile to the pending profile
		sg_psSoundProfile = (SSoundProfile *) sg_psSoundProfilePending;
		
		// NULL out any pending sound profile
		sg_psSoundProfilePending = NULL;
		
		// Clear the render channel
		sg_psRenderChannel = NULL;
		
		// Force this to be a primary sound request
		bPrimarySoundRequest = TRUE;
	}
	
	// If there isn't a valid profile, make it silent, then silence, and return
	if (NULL == sg_psSoundProfile)
	{
		u16Loop = sg_u16MixdownBufferSize;
		
		// Clear out our buffer pointers
		sg_ps32MixdownBufferPosition = NULL;
		sg_u16MixdownRemaining = 0;
		goto renderSilence;
	}

//		DebugOut("%s: Primary sound engine request\n", __FUNCTION__);
	ps32DataPtr = sg_ps32MixdownBuffer;	
	u16MixdownRemaining = sg_u16MixdownBufferSize;

	// Better make sure these are 0/NULL
	BASSERT(NULL == sg_ps32MixdownBufferPosition);
	BASSERT(0 == sg_u16MixdownRemaining);

	// If there isn't a current render channel, then let's see if there's a render channel. If not, start over.
	if (NULL == sg_psRenderChannel)
	{
		// Pointer points to the render channel
		sg_psRenderChannel = sg_psSoundProfile->psSoundChannels;

		// Clear out the mixdown buffer
		memset((void *) sg_ps32MixdownBuffer, 0, sizeof(*sg_ps32MixdownBuffer) * sg_u16MixdownBufferSize);
	}
	else
	{
		// Fall through since we're reentering where we left off before

		// Assert if this is a primary sound request - means the rendering is getting behind
		BASSERT(FALSE == bPrimarySoundRequest);
	}

	// Now loop through the channels and render each one
	while (sg_psRenderChannel)
	{
		ERenderResult eResult;

		// If this channel has a render function, then render some audio
		if (sg_psRenderChannel->psMethods)
		{
			if (sg_psRenderChannel->psMethods->Render)
			{
				eResult = sg_psRenderChannel->psMethods->Render(sg_psRenderChannel,
																u64Time,
																&ps32DataPtr,
																&u16MixdownRemaining);

				// This means the render needs to do
				if (ERENDER_EXIT == eResult)
				{
					sg_ps32MixdownBufferPosition = ps32DataPtr;
					sg_u16MixdownRemaining = u16MixdownRemaining;
//					DebugOut("%s: Rescheduled exit\n", __FUNCTION__);
					return(TRUE);
				}
				else
				{
					// Set the mixdown pointer back to the beginning of the cumulation buffer
					ps32DataPtr = sg_ps32MixdownBuffer;

					// And the size to the final mixdown buffer
					u16MixdownRemaining = sg_u16MixdownBufferSize;
				}
			}
		}

		sg_psRenderChannel = sg_psRenderChannel->psNextChannel;
	}

	// Make sure these are zeroed out 
	sg_ps32MixdownBufferPosition = NULL;
	sg_u16MixdownRemaining = 0;

	// Final mixdown!
	u16Loop = sg_u16MixdownBufferSize;

	// If the volume is at unity, 
	if (VOLUME_UNITY == sg_u16VolumeGlobal)
	{
		while (u16Loop--)
		{
			s32Data = *ps32DataPtr;
			++ps32DataPtr;

			// Clip
			if (s32Data < -32768)
			{
				s32Data = 32768;
			}
			if (s32Data > 32767)
			{
				s32Data = 32767;
			}

			// Loss of precision is A-OK, here
			*pu16DestBuffer = (UINT16) s32Data;
			++pu16DestBuffer;
		}
	}
	else
	{
		while (u16Loop--)
		{
			s32Data = *ps32DataPtr;
			++ps32DataPtr;

			// Clip
			if (s32Data < -32768)
			{
				s32Data = 32768;
			}
			if (s32Data > 32767)
			{
				s32Data = 32767;
			}

			// Volume scale
			s32Data = (s32Data * sg_u16VolumeGlobal) >> PITCH_FIXED_BITS;

			// Loss of precision is A-OK, here
			*pu16DestBuffer = (UINT16) s32Data;

			++pu16DestBuffer;
		}
	}

//	DebugOut("%s: Render complete exit\n", __FUNCTION__);

	sg_u64SamplesRendered += sg_u16MixdownBufferSize;

	eStatus = OSCriticalSectionLeave(sg_sSoundRenderCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);
	return(FALSE);

renderSilence:
	while (u16Loop--)
	{
		*pu16DestBuffer = 0;
		++pu16DestBuffer;
	}

	sg_u64SamplesRendered += sg_u16MixdownBufferSize;

	eStatus = OSCriticalSectionLeave(sg_sSoundRenderCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);

	return(FALSE);
}

void SoundChannelDestroy(SSoundProfile *psProfile,
						 SSoundChannel *psChannelReference)
{
	SSoundChannel *psPrior = NULL;
	SSoundChannel *psChannel = psProfile->psSoundChannels;

	while ((psChannel) &&
		   (psChannel != psChannelReference))
	{
		psPrior = psChannel;
		psChannel = psChannel->psNextChannel;
	}

	BASSERT(psChannel);

	// First, unlink the channel from the rendering list
	if (NULL == psPrior)
	{
		BASSERT(psChannelReference == psProfile->psSoundChannels);
		psProfile->psSoundChannels = psChannelReference->psNextChannel;
	}
	else
	{
		psPrior->psNextChannel = psChannelReference->psNextChannel;
	}
	
	// Unlinked from the channel list. Now go kill off anything connected to that channel
	if (psChannel->psMethods)
	{
		if (psChannel->psMethods->Shutdown)
		{
			psChannel->psMethods->Shutdown(&psChannel->pvMethodData);
			psChannel->pvMethodData = NULL;
		}
	}

	// If there's a buffer allocated, deallocate it
	if (psChannel->ps16ChannelBuffer)
	{
		MemFree(psChannel->ps16ChannelBuffer);
		psChannel->ps16ChannelBuffer = NULL;
	}

	memset((void *) psChannel, 0, sizeof(*psChannel));
	MemFree(psChannel);
}

SSoundChannel *SoundChannelCreate(SSoundProfile *psProfile,
								  BOOL bAllocChannelBuffer)
{
	SSoundChannel *psChannel;

	psChannel = MemAlloc(sizeof(*psChannel));
	BASSERT(psChannel);

	// Provide some sample buffers - but only if asked for
	if (bAllocChannelBuffer)
	{
		psChannel->ps16ChannelBuffer = MemAlloc(sizeof(*psChannel->ps16ChannelBuffer) * sg_u16ChannelBufferSize);
		BASSERT(psChannel->ps16ChannelBuffer);
		psChannel->u16ChannelBufferSize = sg_u16ChannelBufferSize;
	}

	// This will force a rerender the next time it comes around
	psChannel->u32ChannelBufferPos = (sg_u16ChannelBufferSize << PITCH_FIXED_BITS);

	// Attach this channel to the sound profile
	psChannel->psNextChannel = psProfile->psSoundChannels;
	psProfile->psSoundChannels = psChannel;

	return(psChannel);
}

void SoundProfileSet(SSoundProfile *psProfile)
{
	// Make sure nothing else is pending (it shouldn't be)
	BASSERT(FALSE == sg_bSoundProfilePending);
	
	sg_psSoundProfilePending = psProfile;
	sg_bSoundProfilePending = TRUE;
	
	// Spin lock until the sound profile is no longer pending
	while (sg_bSoundProfilePending);
}

SSoundProfile *SoundProfileCreate(void)
{
	SSoundProfile *psProfile;

	psProfile = MemAlloc(sizeof(*psProfile));
	BASSERT(psProfile);
	return(psProfile);
}

SSoundProfile *SoundProfileGet(void)
{
	return(sg_psSoundProfile);
}

void SoundProfileDestroy(SSoundProfile *psProfile)
{
	// If it's the active profile, then set it to NULL while we're working

	if (sg_psSoundProfile == psProfile)
	{
		// Set our sound profile to NULL
		SoundProfileSet(NULL);
	}

	// Rip through all of the channels and delete the profile
	while (psProfile->psSoundChannels)
	{
		SoundChannelDestroy(psProfile,
							psProfile->psSoundChannels);
	}

	memset((void *) psProfile, 0, sizeof(*psProfile));
	MemFree(psProfile);
}

void SoundChannelSetVolume(SSoundChannel *psChannel,
						   UINT16 u16Volume)
{
	psChannel->u16ChannelVolume = u16Volume;
}

void SoundChannelSetPitch(SSoundChannel *psChannel,
						  UINT16 u16Pitch)
{
	psChannel->u16ChannelPitch = u16Pitch;
}

EStatus SoundInit(UINT16 u16MixdownBufferSize,
				  UINT16 u16ChannelBufferSize,
				  UINT32 u32SampleRate)
{
	EStatus eStatus = ESTATUS_OK;

	eStatus = OSCriticalSectionCreate(&sg_sSoundRenderCriticalSection);
	ERR_GOTO();

	// Better be NULL!
	BASSERT(NULL == sg_ps32MixdownBuffer);

	// And the channel buffer size
	sg_u16ChannelBufferSize = u16ChannelBufferSize;

	// Create a mixdown buffer
	sg_u16MixdownBufferSize = u16MixdownBufferSize;
	MEMALLOC(sg_ps32MixdownBuffer, sg_u16MixdownBufferSize * sizeof(*sg_ps32MixdownBuffer));

	// Record the sample rate
	sg_u32SampleRate = u32SampleRate;

	eStatus = ESTATUS_OK;

	// Get the start of the sound engine's init
	sg_u64SoundBaseTimeStart = RTCGet();

	sg_bSoundInitialized = TRUE;

errorExit:
	return(eStatus);
}

SSoundQueue *SoundQueueCreate(UINT16 u16Items,
							  UINT32 u32ItemSize)
{
	SSoundQueue *psQueue;

	psQueue = MemAlloc(sizeof(*psQueue));
	BASSERT(psQueue);
	psQueue->pvQueueData = MemAlloc(u16Items * u32ItemSize);
	BASSERT(psQueue->pvQueueData);

	psQueue->u32ItemSize = u32ItemSize;
	psQueue->u16QueueSize = u16Items;

	return(psQueue);
}

void SoundQueueDestroy(SSoundQueue *psQueue)
{
	SafeMemFree(psQueue->pvQueueData);
	MemFree(psQueue);
}

void *SoundQueueGetCurrent(SSoundQueue *psQueue)
{
	// If the queue isn't present, just return
	if (NULL == psQueue)
	{
		return(NULL);
	}

	// Nothing in the queue if this is true
	if (0 == psQueue->u16QueuedItems)
	{
		return(NULL);
	}

	// Return the tail
	return((void *) (((UINT8 *) psQueue->pvQueueData) + (psQueue->u16Tail * psQueue->u32ItemSize)));
}

void SoundQueueDequeue(SSoundQueue *psQueue)
{
	if (0 == psQueue->u16QueuedItems)
	{
		// Nothing in the queue. Return.
		return;
	}

	++psQueue->u16Tail;
	if (psQueue->u16Tail >= psQueue->u16QueueSize)
	{
		psQueue->u16Tail = 0;
	}

	psQueue->u16QueuedItems--;
}

BOOL SoundQueueAdd(SSoundQueue *psQueue,
				   void *pvQueuedItem)
{
	UINT16 u16NewHead = psQueue->u16Head + 1;

	if (psQueue->u16QueuedItems == psQueue->u16QueueSize)
	{
		// Queue full
		return(FALSE);
	}

	if (u16NewHead >= psQueue->u16QueueSize)
	{
		u16NewHead = 0;
	}

	memcpy((void *) (((UINT8 *) psQueue->pvQueueData) + (psQueue->u16Head * psQueue->u32ItemSize)), pvQueuedItem, psQueue->u32ItemSize);

	psQueue->u16Head = u16NewHead;
	psQueue->u16QueuedItems++;
	return(TRUE);
}
