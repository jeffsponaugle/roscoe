#include "Shared/Shared.h"
#include "Shared/Sound/Sound.h"
#include "Shared/Sound/SoundWav.h"
#include "Platform/Platform.h"

PACKED_BEGIN
typedef PACKED_STRUCT_START struct PACKED_STRUCT SWaveFormatChunk
{
	UINT16 u16FormatTag;
	UINT16 u16Channels;
	UINT32 u32SamplesPerSecond;
	UINT32 u32BytesPerSecond;
	UINT16 u16BlockAlign;
	UINT16 u16SampleBits;
} SWaveFormatChunk;
PACKED_END

typedef enum
{
	EWAVE_SET_PITCH,
	EWAVE_SET_VOLUME,
	EWAVE_PLAY,
	EWAVE_STOP,
	EWAVE_PAUSE,
} EWaveCmd;

typedef struct SWaveCmd
{
	UINT64 u64CmdTimestamp;
	EWaveCmd eCmd;
	SSoundWav *psWave;
	UINT16 u16Volume;
	BOOL bLooped;
} SWaveCmd;

typedef enum
{
	EWAVE_NONE,
	EWAVE_PLAYING,
	EWAVE_PAUSED,
	EWAVE_RESUMING,
	EWAVE_STOPPING
} EWaveState;

typedef struct SSoundWaveChannelState
{
	EWaveState eState;

	// Playback step rate (set)
	UINT16 u16PlaybackStepSet;

	// Command queue
	SSoundQueue *psQueue;

	// Actively playing wave file
	SSoundWav *psPlayingSound;

	// Playback accumulator
	UINT16 u16PlaybackAccumulator;

	// Playback step
	UINT16 u16PlaybackStep;

	// Playback volume
	UINT16 u16PlaybackVolume;

	// Samples available
	UINT16 u16SamplesAvailable;

	// # Of samples we need to subtract from available after reading
	UINT16 u16SampleSubtractor;

	// Playback position
	UINT32 u32PlaybackSourcePosition;

	// Set to TRUE if sample is to be looped when it hits the end
	BOOL bLoop;
} SSoundWaveChannelState;

// Command queue size for wave 
#define	WAV_CMD_DEPTH	10

static ERenderResult WaveRender(SSoundChannel *psChannel,
								UINT64 u64BaseTimestamp,
								INT32 **pps32HWSampleBuffer,
								UINT16 *pu16HWSampleCount)
{
	INT32 *ps32HWSampleBuffer = *pps32HWSampleBuffer;
	UINT16 u16HWSampleCount = *pu16HWSampleCount;
	UINT64 u64SampleCount = 0;
	SSoundWaveChannelState *psState;

	psState = (SSoundWaveChannelState *) psChannel->pvMethodData;

	while (u16HWSampleCount)
	{
		SWaveCmd sCmd;
		UINT64 u64Time;
		SWaveCmd *psWaveCmdPtr = NULL;

		// Snag the virtual->real time
		u64Time = u64BaseTimestamp + SoundSamplesToTimeMs(u64SampleCount);

		// Go peek at whatever command we want done.
		psWaveCmdPtr = (SWaveCmd *) SoundQueueGetCurrent(psState->psQueue);
		if (psWaveCmdPtr)
		{
			// This means we have something inside
			if (u64Time >= psWaveCmdPtr->u64CmdTimestamp)
			{
				memcpy((void *) &sCmd, (void *) psWaveCmdPtr, sizeof(sCmd));
				SoundQueueDequeue(psState->psQueue);

				if (EWAVE_PLAY == sCmd.eCmd)
				{
					psState->bLoop = sCmd.bLooped;
					psState->psPlayingSound = sCmd.psWave;
					psState->u16PlaybackAccumulator = 0;
					psState->u16PlaybackVolume = sCmd.u16Volume;
					psState->u16SamplesAvailable = psState->psPlayingSound->u32SampleSize;
					psState->u16PlaybackStep = psState->psPlayingSound->u16SampleRatePitch;
					psState->u32PlaybackSourcePosition = 0;
					psState->eState = EWAVE_PLAYING;
				}
			}
			else
			{
				// Not time to apply this yet
			}
		}

		// If we have a sample playing, copy in appropriate data
		if (EWAVE_NONE == psState->eState)
		{
			// Not doing anything - just fall through
		}
		else
		if (EWAVE_PLAYING == psState->eState)
		{
			*ps32HWSampleBuffer += (((*((INT16 *) &psState->psPlayingSound->pu8PlatformBase[psState->psPlayingSound->u32SampleOffset + (psState->u32PlaybackSourcePosition << 1)])) * psState->u16PlaybackVolume) >> VOLUME_FIXED_BITS);
			psState->u16PlaybackAccumulator += psState->u16PlaybackStep;
			psState->u32PlaybackSourcePosition += (psState->u16PlaybackAccumulator >> PITCH_FIXED_BITS);
			psState->u16PlaybackAccumulator &= ((1 << PITCH_FIXED_BITS) - 1);

			if (psState->u32PlaybackSourcePosition >= psState->psPlayingSound->u32SampleSize)
			{
				// End of sample
				if (psState->bLoop)
				{
					psState->u32PlaybackSourcePosition -= psState->psPlayingSound->u32SampleSize;
				}
				else
				{
					// Not looping. Stop it.
					psState->eState = EWAVE_NONE;
					psState->psPlayingSound = NULL;
					psState->u32PlaybackSourcePosition = 0;
					psState->u16PlaybackAccumulator = 0;
					psState->u16PlaybackStep = 0;
				}
			}
			else
			{
				// Not at the end yet
			}
		}
		else
		{
			// WTF - bad state
			BASSERT(0);
		}


		++u64SampleCount;
		--u16HWSampleCount;
		++ps32HWSampleBuffer;
	}

	*pps32HWSampleBuffer = ps32HWSampleBuffer;
	*pu16HWSampleCount = u16HWSampleCount;

	return(ERENDER_OK);
}

static void WaveShutdown(void **ppvChannelData)
{
	SafeMemFree(*ppvChannelData);
}

static const SChannelMethod sg_sWaveChannelMethods =
{
	NULL,					// Init
	WaveRender,				// Render
	WaveShutdown			// Shutdown
}; 

void SoundWaveDetatch(SSoundChannel *psChannel)
{
	// This will assert if you attempt to detatch a method 
	BASSERT(psChannel->psMethods == &sg_sWaveChannelMethods);

	// Clear out the wave engine's methods
	psChannel->psMethods = NULL;

	// And free the wave file method data
	if (psChannel->pvMethodData)
	{
		SSoundWaveChannelState *psState;

		psState = (SSoundWaveChannelState *) psChannel->pvMethodData;
		if (psState)
		{
			SoundQueueDestroy(psState->psQueue);
			psState->psQueue = NULL;
		}

		MemFree(psChannel->pvMethodData);
	}

	psChannel->pvMethodData = NULL;
}

EStatus SoundWaveAttach(SSoundChannel *psChannel)
{
	SSoundWaveChannelState *psState;
	EStatus eStatus;
	
	psChannel->psMethods = &sg_sWaveChannelMethods;
	MEMALLOC(psChannel->pvMethodData, sizeof(SSoundWaveChannelState));

	psState = (SSoundWaveChannelState *) psChannel->pvMethodData;
	psState->u16PlaybackStepSet = PITCH_UNITY;
	psState->eState = EWAVE_NONE;

	psState->psQueue = SoundQueueCreate(WAV_CMD_DEPTH,
										sizeof(SWaveCmd));
	if (NULL == psState->psQueue)
	{
		eStatus = ESTATUS_OUT_OF_MEMORY;
		goto errorExit;
	}

	eStatus = ESTATUS_OK;

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		SoundWaveDetatch(psChannel);
	}

	return(eStatus);
}

void SoundWaveDestroy(SSoundWav *psWav)
{
	SafeMemFree(psWav->pu8PlatformBase);
	memset((void *) psWav, 0, sizeof(*psWav));
	MemFree(psWav);
}

SSoundWav *SoundWaveCreate(char *peSoundFilename)
{
	EStatus eStatus;
	UINT64 u64WaveFileSize;
	SSoundWav *psWav = NULL;
	UINT8 u8Tag[4];
	UINT64 u64Offset = 0;
	UINT64 u64Length = 0;
	UINT32 u32Length = 0;
	SWaveFormatChunk sFormat;

	psWav = MemAlloc(sizeof(*psWav));
	if (NULL == psWav)
	{
		goto errorExit;
	}

	eStatus = FileLoad(peSoundFilename,
					   (UINT8 **) &psWav->pu8PlatformBase,
					   &u64WaveFileSize);

	ERR_GOTO();
					   
	if (u64WaveFileSize < 4)
	{
		DebugOutFunc("Malformed wave file (%u bytes in size?)\n", u64WaveFileSize);
	}

	// First 4 bytes must be RIFF
	memcpy((void *) u8Tag, &psWav->pu8PlatformBase[u64Offset], sizeof(u8Tag));

	// Skip over RIFF and the file's size - we don't care about it
	u64Offset += sizeof(UINT32) + sizeof(UINT32);

	// Is it RIFF?
	if (memcmp((void *) "RIFF", u8Tag, sizeof(u8Tag)))
	{
		DebugOutFunc("File '%s' not a wave file (no RIFF tag)\n", peSoundFilename);
		goto errorExit;
	}

	// Next 4 bytes need to be WAVE.
	memcpy((void *) u8Tag, &psWav->pu8PlatformBase[u64Offset], sizeof(u8Tag));
	u64Offset += sizeof(u8Tag);

	// Is it WAVE?
	if (memcmp((void *) "WAVE", u8Tag, sizeof(u8Tag)))
	{
		DebugOutFunc("File '%s' not a wave file (no WAVE tag)\n", peSoundFilename);
		goto errorExit;
	}

	// It's WAVE. Let's skip through the header and look for the 'fmt ' tag
	while (1)
	{
		// Read the tag
		memcpy((void *) u8Tag, &psWav->pu8PlatformBase[u64Offset], sizeof(u8Tag));
		u64Offset += sizeof(u8Tag);

		// Now the length of the tag
		memcpy((void *) &u32Length, &psWav->pu8PlatformBase[u64Offset], sizeof(u32Length));

		// Skip past the actual length
		u64Offset += sizeof(u32Length);

		// If it's a "fmt ", then bail out! We've found our format structure
		if (memcmp(u8Tag, "fmt ", sizeof(u8Tag)) == 0)
		{
			break;
		}

		// Skip past this tag
		u64Offset += u32Length;
	}

	// We've got our format tag.
	memcpy((void *) &sFormat, (void *) &psWav->pu8PlatformBase[u64Offset], sizeof(sFormat));

	// Skip over the format structure if there's extra data
	u64Offset += sizeof(sFormat);

	// Validate the format data
	if (sFormat.u16Channels != 1)
	{
		DebugOut("%s: Expected 1 channels, got %u\n", __FUNCTION__, sFormat.u16Channels);
		goto errorExit;
	}

	if (sFormat.u16SampleBits != 0x10)
	{
		DebugOut("%s: Expected 16 bit sample sizes, got %u\n", __FUNCTION__, sFormat.u16SampleBits);
		goto errorExit;
	}

	// Time to find the data tag
	while (1)
	{
		// Read the tag
		memcpy((void *) u8Tag, &psWav->pu8PlatformBase[u64Offset], sizeof(u8Tag));
		u64Offset += sizeof(u8Tag);

		// Now the length of the tag
		memcpy((void *) &u32Length, &psWav->pu8PlatformBase[u64Offset], sizeof(u32Length));

		// Skip past the actual length
		u64Offset += sizeof(u32Length);

		// If it's a "data", then bail out! We've found our data area
		if (memcmp(u8Tag, "data", sizeof(u8Tag)) == 0)
		{
			break;
		}

		// Skip past this tag
		u64Offset +=  u32Length;
	}

	// Fill in our wave structure
	
	// Point to our sample data offset (in bytes)
	psWav->u32SampleOffset = (UINT32) u64Offset;

	// Fill in our sample size (# of samples, not bytes)
	psWav->u32SampleSize = u32Length / (sFormat.u16SampleBits >> 3);

	// Fill in the sample rate pitch step value
	psWav->u16SampleRatePitch = (UINT16) (((UINT32) sFormat.u32SamplesPerSecond << PITCH_FIXED_BITS) / PlatformSoundGetSampleRate());

	return(psWav);

errorExit:
	SoundWaveDestroy(psWav);
	return(NULL);
}

static EStatus SoundChannelWavePlayInternal(SSoundChannel *psChannel,
											SSoundWav *psWave,
											UINT16 u16Volume,
											BOOL bLooped,
											UINT64 u64Timestamp)
{
	EStatus eStatus;
	SSoundWaveChannelState *psState = (SSoundWaveChannelState *) psChannel->pvMethodData;
	SWaveCmd sWaveCmd;

	ZERO_STRUCT(sWaveCmd);

	sWaveCmd.eCmd = EWAVE_PLAY;
	sWaveCmd.psWave = psWave;
	sWaveCmd.u16Volume = u16Volume;
	sWaveCmd.bLooped = bLooped;

	// Make sure this is a wave method
	BASSERT(psChannel->psMethods == &sg_sWaveChannelMethods);
	SoundSetLock(TRUE);

	sWaveCmd.u64CmdTimestamp = u64Timestamp;
	if (FALSE == SoundQueueAdd(psState->psQueue,
							   &sWaveCmd))
	{
		eStatus = ESTATUS_OS_QUEUE_FULL;
	}
	else
	{
		eStatus = ESTATUS_OK;
	}

	SoundSetLock(FALSE);

	return(eStatus);
}

EStatus SoundChannelWavePlay(SSoundChannel *psChannel,
							 SSoundWav *psWave,
							 UINT16 u16Volume,
							 BOOL bLooped)
{
	return(SoundChannelWavePlayInternal(psChannel,
										psWave,
										u16Volume,
										bLooped,
										RTCGet()));
}

EStatus SoundChannelWavePlayImmediate(SSoundChannel *psChannel,
									  SSoundWav *psWave,
									  UINT16 u16Volume,
									  BOOL bLooped)
{
	return(SoundChannelWavePlayInternal(psChannel,
										psWave,
										u16Volume,
										bLooped,
										0));
}

void SoundChannelWaveStop(SSoundChannel *psChannel)
{

}

void SoundChannelWavePause(SSoundChannel *psChannel)
{

}

void SoundChannelWaveSetPitch(SSoundChannel *psChannel,
							  UINT16 u16Pitch)
{

}

void SoundChannelWaveSetVolume(SSoundChannel *psChannel,
							   UINT16 u16Volume)
{

}