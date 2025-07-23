#ifndef _SOUND_H_
#define _SOUND_H_

#define	VOLUME_FIXED_BITS	12
#define	VOLUME_UNITY		(1 << VOLUME_FIXED_BITS)

#define	PITCH_FIXED_BITS	12
#define PITCH_UNITY			(1 << PITCH_FIXED_BITS)

#define	SOUND_GAIN_PERCENT(x)	((uint16_t) (((double) (x) / 100.0) * ((double) VOLUME_UNITY)))

// How many bits of precision of sound?
#define	SOUND_BIT_DEPTH		13

typedef enum
{
	ERENDER_OK,
	ERENDER_SILENT,
	ERENDER_EXIT
} ERenderResult;

typedef struct SSoundQueue
{
	uint64_t u64QueuedTime;		// When was this item put in the queue?
	uint16_t u16Head;				// Head pointer
	uint16_t u16Tail;				// Tail pointer
	uint16_t u16QueuedItems;		// # Of items in the queue
	uint16_t u16QueueSize;		// Queue size (# of items total)
	uint32_t u32ItemSize;			// How big is each item in the queue?

	void *pvQueueData;			// Actual queue data
} SSoundQueue;

#define	VIRTUAL_TIME_BITS	16
#define	VIRTUAL_TIME_MASK	((1 << VIRTUAL_TIME_BITS) - 1)

// Forward reference
struct SSoundChannel;

typedef struct SChannelMethod
{
	void (*Init)(uint32_t u32SampleRate,
				 void **ppvChannelData);
	ERenderResult (*Render)(struct SSoundChannel *psChannel,
							uint64_t u64BaseTimestamp,
							int32_t **pps32SampleBuffer,
						    uint16_t *pu16SampleCount);
	void (*Shutdown)(void **ppvChannelData);
} SChannelMethod;

typedef struct SSoundChannel
{
	uint16_t u16ChannelVolume;		// Channel volume
	uint16_t u16ChannelPitch;			// Channel's pitch

	bool bChannelMuted;				// true If channel is muted

	// Channel buffer info
	int16_t *ps16ChannelBuffer;
	uint16_t u16ChannelBufferSize;
	uint32_t u32ChannelBufferPos;		// (32-PITCH_FIXED_BITS).PITCH_FIXED_BITS Fixed point arithmetic

	// Channel method
	const SChannelMethod *psMethods;		// Sound channel methods

	// Method data
	void *pvMethodData;				// Pointer to method data

	struct SSoundChannel *psNextChannel;
} SSoundChannel;

typedef struct SSoundProfile
{
	int32_t *ps32MixdownBuffer;
	uint16_t u16MixdownBufferSize;

	SSoundChannel *psSoundChannels;
} SSoundProfile;

extern SSoundChannel *SoundChannelCreate(SSoundProfile *psProfile,
										 bool bAllocChannelBuffer);
extern void SoundChannelDestroy(SSoundProfile *psProfile,
								SSoundChannel *psChannel);
extern EStatus SoundInit(uint16_t u16MixdownBufferSize,
						 uint16_t u16ChannelBufferSize,
						 uint32_t u32SampleRate);
extern void SoundProfileSet(SSoundProfile *psProfile);
extern SSoundProfile *SoundProfileGet(void);
extern SSoundProfile *SoundProfileCreate(void);
extern void SoundProfileDestroy(SSoundProfile *psProfile);

extern void SoundChannelSetVolume(SSoundChannel *psChannel,
								  uint16_t u16Volume);
extern void SoundChannelSetPitch(SSoundChannel *psChannel,
								 uint16_t u16Pitch);
extern bool SoundRender(uint16_t *pu16DestBuffer,
						bool bPrimarySoundRequest);
extern SSoundQueue *SoundQueueCreate(uint16_t u16Items,
									 uint32_t u32ItemSize);
extern void SoundQueueDestroy(SSoundQueue *psQueue);
extern void *SoundQueueGetCurrent(SSoundQueue *psQueue);
extern void SoundQueueDequeue(SSoundQueue *psQueue);
extern bool SoundQueueAdd(SSoundQueue *psQueue,
						  void *pvQueuedItem);
extern void SoundSetLock(bool bLock);
extern uint64_t SoundSamplesToTimeMs(uint64_t u64Samples);

#endif	// #ifndef _SOUND_H_