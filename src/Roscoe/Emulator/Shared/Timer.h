#ifndef _TIMER_H_
#define _TIMER_H_

// Timer structure
typedef struct STimer
{
	uint32_t u32CounterMS;
	uint32_t u32TimerMS;
	uint32_t u32ReloadMS;
	bool bRunning;

	void (*Handler)(struct STimer *psTimer,
					void *pvCallbackValue);
	void *pvCallbackValue;
	
	struct STimer *psNextLink;
} STimer;

// Called from the architecture code once per timer tick
extern void TimerTick(void);
extern void TimerHRTick(void);

// Called from the architecture code pre-OS to init
extern void TimerInit(void);

// Return regular and high resolution timer resolution in milliseconds
extern uint32_t TimerGetResolution(void);
extern uint32_t TimerHRGetResolution(void);

// Regular systick based timers
extern EStatus TimerCreate(STimer **ppsTimer);
extern EStatus TimerDelete(STimer *psTimer);
extern EStatus TimerSet(STimer *psTimer,
						bool bStartAutomatically,
						uint32_t u32InitialMS,
						uint32_t u32ReloadMS,
						void (*ExpirationCallback)(STimer *psTimer,
												   void *pvCallbackValue),
						void *pvCallbackValue);
extern EStatus TimerStart(STimer *psTimer);
extern EStatus TimerStop(STimer *psTimer);

// High resolution, independent timers
extern EStatus TimerHRCreate(STimer **ppsTimer);
extern EStatus TimerHRDelete(STimer *psTimer);
extern EStatus TimerHRSet(STimer *psTimer,
						  bool bStartAutomatically,
						  uint32_t u32InitialMS,
						  uint32_t u32ReloadMS,
						  void (*ExpirationCallback)(STimer *psTimer,
													 void *pvCallbackValue),
						  void *pvCallbackValue);
extern EStatus TimerHRStart(STimer *psTimer);
extern EStatus TimerHRStop(STimer *psTimer);

#endif