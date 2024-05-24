#ifndef _SOUND_H_
#define _SOUND_H_

#define	VOLUME_FIXED_BITS	12
#define	VOLUME_UNITY		(1 << VOLUME_FIXED_BITS)

#define	PITCH_FIXED_BITS	12
#define PITCH_UNITY			(1 << PITCH_FIXED_BITS)

#define	SOUND_GAIN_PERCENT(x)	((UINT16) (((double) (x) / 100.0) * ((double) VOLUME_UNITY)))

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
	UINT64 u64QueuedTime;		// When was this item put in the queue?
	UINT16 u16Head;				// Head pointer
	UINT16 u16Tail;				// Tail pointer
	UINT16 u16QueuedItems;		// # Of items in the queue
	UINT16 u16QueueSize;		// Queue size (# of items total)
	UINT32 u32ItemSize;			// How big is each item in the queue?

	void *pvQueueData;			// Actual queue data
} SSoundQueue;

#define	VIRTUAL_TIME_BITS	16
#define	VIRTUAL_TIME_MASK	((1 << VIRTUAL_TIME_BITS) - 1)

// Forward reference
struct SSoundChannel;

typedef struct SChannelMethod
{
	void (*Init)(UINT32 u32SampleRate,
				 void **ppvChannelData);
	ERenderResult (*Render)(struct SSoundChannel *psChannel,
							UINT64 u64BaseTimestamp,
							INT32 **pps32SampleBuffer,
						    UINT16 *pu16SampleCount);
	void (*Shutdown)(void **ppvChannelData);
} SChannelMethod;

typedef struct SSoundChannel
{
	UINT16 u16ChannelVolume;		// Channel volume
	UINT16 u16ChannelPitch;			// Channel's pitch

	BOOL bChannelMuted;				// TRUE If channel is muted

	// Channel buffer info
	INT16 *ps16ChannelBuffer;
	UINT16 u16ChannelBufferSize;
	UINT32 u32ChannelBufferPos;		// (32-PITCH_FIXED_BITS).PITCH_FIXED_BITS Fixed point arithmetic

	// Channel method
	const SChannelMethod *psMethods;		// Sound channel methods

	// Method data
	void *pvMethodData;				// Pointer to method data

	struct SSoundChannel *psNextChannel;
} SSoundChannel;

typedef struct SSoundProfile
{
	INT32 *ps32MixdownBuffer;
	UINT16 u16MixdownBufferSize;

	SSoundChannel *psSoundChannels;
} SSoundProfile;

extern SSoundChannel *SoundChannelCreate(SSoundProfile *psProfile,
										 BOOL bAllocChannelBuffer);
extern void SoundChannelDestroy(SSoundProfile *psProfile,
								SSoundChannel *psChannel);
extern EStatus SoundInit(UINT16 u16MixdownBufferSize,
						 UINT16 u16ChannelBufferSize,
						 UINT32 u32SampleRate);
extern void SoundProfileSet(SSoundProfile *psProfile);
extern SSoundProfile *SoundProfileGet(void);
extern SSoundProfile *SoundProfileCreate(void);
extern void SoundProfileDestroy(SSoundProfile *psProfile);

extern void SoundChannelSetVolume(SSoundChannel *psChannel,
								  UINT16 u16Volume);
extern void SoundChannelSetPitch(SSoundChannel *psChannel,
								 UINT16 u16Pitch);
extern BOOL SoundRender(UINT16 *pu16DestBuffer,
						BOOL bPrimarySoundRequest);
extern SSoundQueue *SoundQueueCreate(UINT16 u16Items,
									 UINT32 u32ItemSize);
extern void SoundQueueDestroy(SSoundQueue *psQueue);
extern void *SoundQueueGetCurrent(SSoundQueue *psQueue);
extern void SoundQueueDequeue(SSoundQueue *psQueue);
extern BOOL SoundQueueAdd(SSoundQueue *psQueue,
						  void *pvQueuedItem);
extern void SoundSetLock(BOOL bLock);
extern UINT64 SoundSamplesToTimeMs(UINT64 u64Samples);

#endif	// #ifndef _SOUND_H_