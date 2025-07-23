#ifndef _SOUNDWAV_H_
#define _SOUNDWAV_H_

typedef struct SSoundWav
{
	// Pointer to the platform specific data that indicates where the real data is
	uint8_t *pu8PlatformBase;

	// Offset within resource that contains the actual sample data
	uint32_t u32SampleOffset;

	// # Of samples for this wav file
	uint32_t u32SampleSize;		

	// Sample rate correction
	uint16_t u16SampleRatePitch;
} SSoundWav;

extern SSoundWav *SoundWaveCreate(char *pu8SoundFilename);
extern void SoundWaveDestroy(SSoundWav *psWav);
extern EStatus SoundWaveAttach(SSoundChannel *psChannel);
extern EStatus SoundChannelWavePlay(SSoundChannel *psChannel,
									SSoundWav *psWave,
									uint16_t u16Volume,
									bool bLooped);
extern EStatus SoundChannelWavePlayImmediate(SSoundChannel *psChannel,
											 SSoundWav *psWave,
											 uint16_t u16Volume,
											 bool bLooped);
extern void SoundChannelWaveStop(SSoundChannel *psChannel);
extern void SoundChannelWavePause(SSoundChannel *psChannel);
extern void SoundChannelWaveSetPitch(SSoundChannel *psChannel,
									 uint16_t u16Pitch);
extern void SoundChannelWaveSetVolume(SSoundChannel *psChannel,
									  uint16_t u16Volume);

#endif	// #ifndef _SOUNDWAV_H_