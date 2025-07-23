#include "Shared/Shared.h"
#include "Platform/Platform.h"
#include "Shared/Sound/Sound.h"

// Some globals to make it fun
static SSoundProfile *sg_psSoundProfile;

// Pending sound profile
static volatile SSoundProfile *sg_psSoundProfilePending = NULL;
static volatile bool sg_bSoundProfilePending;

// Mixdown buffer related
static int32_t *sg_ps32MixdownBuffer;
static uint16_t sg_u16MixdownBufferSize;
static int32_t *sg_ps32MixdownBufferPosition = NULL;
static uint16_t sg_u16MixdownRemaining;

// How big is the channel buffer?
static uint16_t sg_u16ChannelBufferSize;

// What's the global engine's volume setting currently?
static uint16_t sg_u16VolumeGlobal = PITCH_UNITY;

// What channel is being rendered currently?
static SSoundChannel *sg_psRenderChannel = NULL;

// Base timestamp for start of sound engine
static uint64_t sg_u64SoundBaseTimeStart = 0;

// # Of samples played since the beginning of the sound engine
static uint64_t sg_u64SamplesRendered = 0;

// Current sample rate
static uint32_t sg_u32SampleRate = 0;

// Set to true if we've initialized
static bool sg_bSoundInitialized = false;

// Critical section for sound renderer
static SOSCriticalSection sg_sSoundRenderCriticalSection;

void SoundSetLock(bool bLock)
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

uint64_t SoundSamplesToTimeMs(uint64_t u64Samples)
{
	return((uint64_t) (((double) u64Samples / (double) sg_u32SampleRate) * 1000.0));
}

bool SoundRender(uint16_t *pu16DestBuffer,
				 bool bPrimarySoundRequest)
{
	uint16_t u16Loop;
	int32_t s32Data;
	int32_t *ps32DataPtr;
	uint16_t u16MixdownRemaining;
	EStatus eStatus;
	uint64_t u64Time = sg_u64SoundBaseTimeStart + SoundSamplesToTimeMs(sg_u64SamplesRendered);

//	DebugOut("Delta = %u\n", (uint32_t) (RTCGet() - u64Time));

	if (false == sg_bSoundInitialized)
	{
		// If the sound engine isn't initialized, then just return
		return(false);
	}

	eStatus = OSCriticalSectionEnter(sg_sSoundRenderCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);

	// If we have a profile change, then change it
	if (sg_bSoundProfilePending)
	{
		// Clear the indication that we have a pending sound profile change
		sg_bSoundProfilePending = false;
		
		// Assign the new sound profile to the pending profile
		sg_psSoundProfile = (SSoundProfile *) sg_psSoundProfilePending;
		
		// NULL out any pending sound profile
		sg_psSoundProfilePending = NULL;
		
		// Clear the render channel
		sg_psRenderChannel = NULL;
		
		// Force this to be a primary sound request
		bPrimarySoundRequest = true;
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
		BASSERT(false == bPrimarySoundRequest);
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
					return(true);
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
			*pu16DestBuffer = (uint16_t) s32Data;
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
			*pu16DestBuffer = (uint16_t) s32Data;

			++pu16DestBuffer;
		}
	}

//	DebugOut("%s: Render complete exit\n", __FUNCTION__);

	sg_u64SamplesRendered += sg_u16MixdownBufferSize;

	eStatus = OSCriticalSectionLeave(sg_sSoundRenderCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);
	return(false);

renderSilence:
	while (u16Loop--)
	{
		*pu16DestBuffer = 0;
		++pu16DestBuffer;
	}

	sg_u64SamplesRendered += sg_u16MixdownBufferSize;

	eStatus = OSCriticalSectionLeave(sg_sSoundRenderCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);

	return(false);
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
								  bool bAllocChannelBuffer)
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
	BASSERT(false == sg_bSoundProfilePending);
	
	sg_psSoundProfilePending = psProfile;
	sg_bSoundProfilePending = true;
	
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
						   uint16_t u16Volume)
{
	if (NULL == psChannel)
	{
		return;
	}

	psChannel->u16ChannelVolume = u16Volume;
}

void SoundChannelSetPitch(SSoundChannel *psChannel,
						  uint16_t u16Pitch)
{
	if (NULL == psChannel)
	{
		return;
	}

	psChannel->u16ChannelPitch = u16Pitch;
}

EStatus SoundInit(uint16_t u16MixdownBufferSize,
				  uint16_t u16ChannelBufferSize,
				  uint32_t u32SampleRate)
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

	sg_bSoundInitialized = true;

errorExit:
	return(eStatus);
}

SSoundQueue *SoundQueueCreate(uint16_t u16Items,
							  uint32_t u32ItemSize)
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
	return((void *) (((uint8_t *) psQueue->pvQueueData) + (psQueue->u16Tail * psQueue->u32ItemSize)));
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

bool SoundQueueAdd(SSoundQueue *psQueue,
				   void *pvQueuedItem)
{
	uint16_t u16NewHead = psQueue->u16Head + 1;

	if (psQueue->u16QueuedItems == psQueue->u16QueueSize)
	{
		// Queue full
		return(false);
	}

	if (u16NewHead >= psQueue->u16QueueSize)
	{
		u16NewHead = 0;
	}

	memcpy((void *) (((uint8_t *) psQueue->pvQueueData) + (psQueue->u16Head * psQueue->u32ItemSize)), pvQueuedItem, psQueue->u32ItemSize);

	psQueue->u16Head = u16NewHead;
	psQueue->u16QueuedItems++;
	return(true);
}
