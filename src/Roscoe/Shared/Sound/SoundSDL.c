#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/CmdLine.h"
#include "Shared/Sound/Sound.h"
#include "Shared/Sound/SoundSDL.h"

// For SDL audio
#include "Shared/libsdl/libsdlsrc/include/SDL.h"

// Callback for all audio processing
static void SoundSDLCallback(void *pvUserData,
							 Uint8 *pu8Stream,
							 int len)
{
	SoundRender((UINT16 *) pu8Stream,
				TRUE);
}

// Initializes SDL's sound engine
EStatus SoundSDLInit(UINT32 u32TargetSampleRate,
					 UINT32 u32RenderBufferSize,
					 UINT32 u32NumberOfChannels)
{
	SDL_AudioSpec sAudioSpec;
	EStatus eStatus;
	int s32Loop;
	SDL_AudioDeviceID ePlaybackID = 0;
	UINT32 u32Index = 0;

	ZERO_STRUCT(sAudioSpec);

	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		Syslog("SDL Audio could not initialize! SDL Error: %s\n", SDL_GetError());
		eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
		goto errorExit;
    }
	else
	{
		Syslog("SDL Audio initialized\n");
	}

	// Dump a list of all audio devices to the log

	Syslog("SDL Audio init %uhz, %u channel(s). Device availability:\n", u32TargetSampleRate, u32NumberOfChannels);

	// Run through all audio devices
	for (s32Loop = 0; s32Loop < SDL_GetNumAudioDevices(0); s32Loop++)
	{
		Syslog(" %d: %s\n", s32Loop, SDL_GetAudioDeviceName(s32Loop,
															0));	// Playback devices only
	}

	// u32Index is the device we're opening for output sound
	if (CmdLineOption("-soundindex"))
	{
		u32Index = (UINT32) atol(CmdLineOptionValue("-soundindex"));
		if (u32Index >= SDL_GetNumAudioDevices(0))
		{
			Syslog(" -soundindex index %u provided - beyond available sound devices - disabling audio\n");
			eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
			goto errorExit;
		}
		else
		{
			Syslog(" -soundindex of %u provided\n", u32Index);
		}
	}

	// Log which device we've chosen
	Syslog("Sound using device '%s'\n", SDL_GetAudioDeviceName(u32Index,
															   0));

	// Open up the output device for what we want
	sAudioSpec.freq = u32TargetSampleRate;
	sAudioSpec.format = AUDIO_S16LSB;
	sAudioSpec.channels = u32NumberOfChannels;
	sAudioSpec.samples = u32RenderBufferSize;
	sAudioSpec.callback = SoundSDLCallback;

	Syslog("Audio request: Sample rate=%u, channels=%u, render buffer size=%u\n", u32TargetSampleRate, u32NumberOfChannels, u32RenderBufferSize);

	// Go get what we want
	ePlaybackID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(u32Index,
															 TRUE),
									  0,									// Playback only
									  &sAudioSpec,							// Incoming spec
									  &sAudioSpec,							// What we wound up getting
									  0);									// No changes allowed

	if (0 == ePlaybackID)
	{
		// Error of some sort
		Syslog("Failed to open audio device - %s\n", SDL_GetError());
		eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
	}
	else
	{
		// Fire it up - unpause
		SDL_PauseAudioDevice(ePlaybackID,
							 0);

		Syslog("Audio granted: Sample rate=%u, channels=%u, render buffer size=%u\n", u32TargetSampleRate, u32NumberOfChannels, u32RenderBufferSize);
		Syslog("Audio started\n");
		eStatus = ESTATUS_OK;
	}

errorExit:
	return(eStatus);
}

