#ifndef _PLATFORM_H_
#define _PLATFORM_H_

// Console debug functions
extern void PlatformFlushConsole(void);
extern EStatus PlatformDebugOut(UINT8 *pu8Data,
								UINT32 u32Count);
extern EStatus PlatformTraceOut(UINT8 *peMessage,
								UINT32 u32Length);
extern EStatus PlatformDebugConsoleInit(void);
extern void PlatformInitEarly(void);

// Post-reset assert logging functions
extern EStatus PlatformAssertRecord(char *pu8Expression,
									char *pu8Module,
									UINT32 u32Line);
extern char *PlatformGetProgramName(void);

// Get syslog info for this platform function
extern EStatus PlatformGetSyslogInfo(char **ppeBaseFilename,
									 UINT32 *pu32LogDepth,
									 BOOL *pbConsoleOutput);

// Sound functions
extern EStatus PlatformSoundInit(void);
extern UINT32 PlatformSoundGetSampleRate(void);
extern void PlatformSoundSetLatency(UINT64 u64LatencyMS);

// Timer/timing functions
extern void PlatformTick(void);
extern EStatus PlatformPerformanceCounterGet(UINT32 *pu32Counter);
extern EStatus PlatformPerformanceCounterGetHz(UINT32 *pu32CounterHz);

// File functions
extern char *PlatformGetBasePathname(void);
extern void PlatformGetTempBaseDirectory(char **ppeTempDirectory);

// Misc functions
extern EStatus PlatformGetUniqueSystemID(UINT8 *pu8Buffer,
										 UINT32 *pu32BuffLength);
extern void PlatformGetTimezoneOffset(INT64 *ps64Offset);
extern UINT64 PlatformGetMaxConnectionCount(void);
extern void PlatformCriticalError(char *peMessage);
extern EStatus PlatformGetAppKey(UINT8 **ppu8AppKey);
extern void PlatformGetSystemLanguage(char *peLanguage,
									  UINT32 u32LanguageBufferSize);
extern void PlatformGetTimezoneOffset(INT64 *ps64TimezoneOFfsetMilliseconds);
extern EStatus PlatformCPUGetInfo(UINT32 *pu32CPUPhysicalCores,
								  UINT32 *pu32CPUThreadedCores);
#ifdef _FASTHEAP
extern EStatus PlatformHeapGetRegions(SFastHeapRegion **ppsHeapRegions,
									  UINT64 *pu64HeapRegionCount);
#endif
extern char *PlatformGetClientName(void);
extern void PlatformGetGraphicsSettings(UINT32 *pu32PhysicalXSize,
										UINT32 *pu32PhysicalYSize,
										UINT32 *pu32VirtualXSize,
										UINT32 *pu32VirtualYSize,
										BOOL *pbFullScreen,
										BOOL *pbBorderless);
extern EStatus PlatformGetViewportInitialPosition(UINT32 *pu32XPos,
												  UINT32 *pu32YPos);

#endif
