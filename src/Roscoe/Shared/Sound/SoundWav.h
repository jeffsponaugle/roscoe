#ifndef _SOUNDWAV_H_
#define _SOUNDWAV_H_

typedef struct SSoundWav
{
	// Pointer to the platform specific data that indicates where the real data is
	UINT8 *pu8PlatformBase;

	// Offset within resource that contains the actual sample data
	UINT32 u32SampleOffset;

	// # Of samples for this wav file
	UINT32 u32SampleSize;		

	// Sample rate correction
	UINT16 u16SampleRatePitch;
} SSoundWav;

extern SSoundWav *SoundWaveCreate(char *pu8SoundFilename);
extern void SoundWaveDestroy(SSoundWav *psWav);
extern EStatus SoundWaveAttach(SSoundChannel *psChannel);
extern EStatus SoundChannelWavePlay(SSoundChannel *psChannel,
									SSoundWav *psWave,
									UINT16 u16Volume,
									BOOL bLooped);
extern EStatus SoundChannelWavePlayImmediate(SSoundChannel *psChannel,
											 SSoundWav *psWave,
											 UINT16 u16Volume,
											 BOOL bLooped);
extern void SoundChannelWaveStop(SSoundChannel *psChannel);
extern void SoundChannelWavePause(SSoundChannel *psChannel);
extern void SoundChannelWaveSetPitch(SSoundChannel *psChannel,
									 UINT16 u16Pitch);
extern void SoundChannelWaveSetVolume(SSoundChannel *psChannel,
									  UINT16 u16Volume);

#endif	// #ifndef _SOUNDWAV_H_