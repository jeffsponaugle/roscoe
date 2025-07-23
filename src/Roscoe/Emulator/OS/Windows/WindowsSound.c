#include <stdio.h>
#include "Shared/Shared.h"
#include "Arch/Arch.h"
#include "Platform/Platform.h"
#include "Shared/Sound/Sound.h"
#include "Shared/Sound/SoundWav.h"

static uint32_t sg_u32SampleRate = 0;
static uint32_t sg_u32BufferPlayed = 0;
static bool sg_bHaltAudio = false;

// Indicates whether or not sound has been opened/running
static bool sg_bSoundOpen = false;

#define NUM_BUFFERS 4
static WAVEHDR sg_sWaveHdr[NUM_BUFFERS];
static HWAVEOUT sg_hWaveOut;

// Used for computing latency
static uint64_t sg_u64OpenTime;
static uint64_t sg_u64Latency;
static bool sg_bLatencyComputed = false;

static void CALLBACK WindowsSoundOutProc(HWAVEOUT hWaveOut,
										 UINT uMsg, 
										 DWORD dwInstance, 
										 DWORD dwParam1,
										 DWORD dwParam2)
{
	MMRESULT eResult;

	if (uMsg != WOM_DONE)
	{
		return;
	}

	if (false == sg_bLatencyComputed)
	{
		sg_u64Latency = RTCGet() - sg_u64OpenTime;
		Syslog("Audio latency - %ums\n", (uint32_t) sg_u64Latency);
		PlatformSoundSetLatency(sg_u64Latency);
		sg_bLatencyComputed = true;
	}

	++sg_u32BufferPlayed;
	if (NUM_BUFFERS == sg_u32BufferPlayed)
	{
		sg_u32BufferPlayed = 0;
	}

	if (false == sg_bHaltAudio)
	{
		eResult = waveOutWrite(hWaveOut,
							   &sg_sWaveHdr[sg_u32BufferPlayed],
							   sizeof(sg_sWaveHdr[sg_u32BufferPlayed]));

		if (eResult != MMSYSERR_NOERROR)
		{
			SyslogFunc("waveOutWrite failed=%d\n", eResult);
			goto errorExit;
		}
	}

	// Render the next block of audio
	while (SoundRender((uint16_t *) sg_sWaveHdr[sg_u32BufferPlayed].lpData,
					   true))
	{
		// Render the whole buffer
	}

errorExit:
	eResult = eResult;
}

static void WindowsSoundClose(void)
{
	uint32_t u32Loop;
	MMRESULT eResult;
	EStatus eStatus;

	if (false == sg_bSoundOpen)
	{ 
		return;
	}

	sg_bHaltAudio = true;

	// Give settline time for the audio to stop
	Sleep(NUM_BUFFERS * 50);

	eResult = waveOutReset(sg_hWaveOut);
	if (eResult != MMSYSERR_NOERROR && eResult != MMSYSERR_INVALHANDLE)
	{
		SyslogFunc("waveOutReset failed=%d\n", eResult);
		eStatus = ESTATUS_SOUND_HARDWARE_FAULT;
		goto errorExit;
	}

	eResult = waveOutClose(sg_hWaveOut);
	if (eResult != MMSYSERR_NOERROR && eResult != MMSYSERR_INVALHANDLE)
	{
		SyslogFunc("waveOutClose failed =%d\n", eResult);
		eStatus = ESTATUS_SOUND_HARDWARE_FAULT;
		goto errorExit;
	}

	for (u32Loop = 0; u32Loop < NUM_BUFFERS; u32Loop++)
	{
		if (sg_sWaveHdr[u32Loop].lpData)
		{
			free(sg_sWaveHdr[u32Loop].lpData);
			sg_sWaveHdr[u32Loop].lpData = NULL;
		}
	}

	sg_bHaltAudio = false;
	sg_bSoundOpen = false;

errorExit:
	eStatus = eStatus;
}

EStatus WindowsSoundOpen(uint32_t u32SampleRate, 
						 uint32_t u32BufferSize, 
						 uint8_t u8Channels)
{
	EStatus eStatus;
	MMRESULT eResult;
	WAVEFORMATEX sWaveFormat;
	uint32_t u32Loop;
	uint32_t u32Counter = 0;

	WindowsSoundClose();

	ZERO_STRUCT(sWaveFormat);

	sg_u32SampleRate = u32SampleRate;
	sWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	sWaveFormat.nChannels = u8Channels,
	sWaveFormat.nSamplesPerSec = u32SampleRate;
	sWaveFormat.wBitsPerSample = 16;
	sWaveFormat.nBlockAlign = sWaveFormat.nChannels * (sWaveFormat.wBitsPerSample / 8);
	sWaveFormat.cbSize = 0;
	sWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	sWaveFormat.nAvgBytesPerSec = sWaveFormat.nSamplesPerSec * sWaveFormat.nBlockAlign; 

	sg_u64OpenTime = RTCGet();

	eResult = waveOutOpen(&sg_hWaveOut,
						  0,
						  &sWaveFormat,
						  (DWORD_PTR) WindowsSoundOutProc,
						  0,
						  CALLBACK_FUNCTION);

	if (eResult != MMSYSERR_NOERROR )
	{
		SyslogFunc("waveOutOpen failed =%d\n", eResult);
		eStatus = ESTATUS_SOUND_HARDWARE_FAULT;
		goto errorExit;
	}

	// Set up a pool of playback buffers
	for (u32Loop = 0; u32Loop < NUM_BUFFERS; u32Loop++)
	{
		memset((void *) &sg_sWaveHdr[u32Loop], 0, sizeof(sg_sWaveHdr[u32Loop]));

		sg_sWaveHdr[u32Loop].dwBufferLength = u32BufferSize * (sWaveFormat.wBitsPerSample / 8) * u8Channels;
		sg_sWaveHdr[u32Loop].lpData =  (void *) malloc(sg_sWaveHdr[u32Loop].dwBufferLength);
		memset((void *) sg_sWaveHdr[u32Loop].lpData, 0, sg_sWaveHdr[u32Loop].dwBufferLength);
		sg_sWaveHdr[u32Loop].dwUser = (uint32_t) &sg_sWaveHdr[u32Loop];

		eResult = waveOutPrepareHeader(sg_hWaveOut,
									   &sg_sWaveHdr[u32Loop],
									   sizeof(sg_sWaveHdr[u32Loop]));

		if (eResult != MMSYSERR_NOERROR)
		{
			SyslogFunc("waveOutPrepareHeader failed=%d\n", eResult);
			eStatus = ESTATUS_SOUND_HARDWARE_FAULT;
			goto errorExit;
		}
	}

	sg_u32BufferPlayed = NUM_BUFFERS - 1;

	for (u32Loop = 0; u32Loop < NUM_BUFFERS; u32Loop++)
	{
		eResult = waveOutWrite(sg_hWaveOut,
							   &sg_sWaveHdr[u32Loop],
							   sizeof(sg_sWaveHdr[u32Loop]));

		if (eResult != MMSYSERR_NOERROR )
		{
			SyslogFunc("waveOutWrite failed =%d\n", eResult);
			eStatus = ESTATUS_SOUND_HARDWARE_FAULT;
			goto errorExit;
		}
	}

	sg_bSoundOpen = true;
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}
