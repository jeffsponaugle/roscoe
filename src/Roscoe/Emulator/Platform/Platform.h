#ifndef _PLATFORM_H_
#define _PLATFORM_H_

// Console debug functions
extern void PlatformFlushConsole(void);
extern EStatus PlatformDebugOut(uint8_t *pu8Data,
								uint32_t u32Count);
extern EStatus PlatformTraceOut(uint8_t *peMessage,
								uint32_t u32Length);
extern EStatus PlatformDebugConsoleInit(void);
extern void PlatformInitEarly(void);

// Post-reset assert logging functions
extern EStatus PlatformAssertRecord(char *pu8Expression,
									char *pu8Module,
									uint32_t u32Line);
extern char *PlatformGetProgramName(void);

// Get syslog info for this platform function
extern EStatus PlatformGetSyslogInfo(char **ppeBaseFilename,
									 uint32_t *pu32LogDepth,
									 bool *pbConsoleOutput);

// Sound functions
extern EStatus PlatformSoundInit(void);
extern uint32_t PlatformSoundGetSampleRate(void);
extern void PlatformSoundSetLatency(uint64_t u64LatencyMS);

// Timer/timing functions
extern void PlatformTick(void);
extern EStatus PlatformPerformanceCounterGet(uint32_t *pu32Counter);
extern EStatus PlatformPerformanceCounterGetHz(uint32_t *pu32CounterHz);

// File functions
extern char *PlatformGetBasePathname(void);
extern void PlatformGetTempBaseDirectory(char **ppeTempDirectory);

// Misc functions
extern EStatus PlatformGetUniqueSystemID(uint8_t *pu8Buffer,
										 uint32_t *pu32BuffLength);
extern void PlatformGetTimezoneOffset(int64_t *ps64Offset);
extern uint64_t PlatformGetMaxConnectionCount(void);
extern void PlatformCriticalError(char *peMessage);
extern EStatus PlatformGetAppKey(uint8_t **ppu8AppKey);
extern void PlatformGetSystemLanguage(char *peLanguage,
									  uint32_t u32LanguageBufferSize);
extern void PlatformGetTimezoneOffset(int64_t *ps64TimezoneOFfsetMilliseconds);
extern EStatus PlatformCPUGetInfo(uint32_t *pu32CPUPhysicalCores,
								  uint32_t *pu32CPUThreadedCores);
#ifdef _FASTHEAP
extern EStatus PlatformHeapGetRegions(SFastHeapRegion **ppsHeapRegions,
									  uint64_t *pu64HeapRegionCount);
#endif
extern char *PlatformGetClientName(void);
extern void PlatformGetGraphicsSettings(uint32_t *pu32PhysicalXSize,
										uint32_t *pu32PhysicalYSize,
										uint32_t *pu32VirtualXSize,
										uint32_t *pu32VirtualYSize,
										bool *pbFullScreen,
										bool *pbBorderless);
extern EStatus PlatformGetViewportInitialPosition(uint32_t *pu32XPos,
												  uint32_t *pu32YPos);

#endif
