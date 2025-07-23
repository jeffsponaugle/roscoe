#define _CRT_RAND_S
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <Rpc.h>
#include <DbgHelp.h>
#include <Shlwapi.h>
#include <crtdbg.h>
#include <tchar.h>
#include <psapi.h>
#include "Shared/Shared.h"
#include "../Arch.h"
#include "Platform/Platform.h"
#include "Shared/Timer.h"
#include "Shared/UtilTask.h"
#include "Shared/SysEvent.h"
#include "Shared/SHA256/sha256.h"
#include "Shared/Windows/WindowsTools.h"
#include "Shared/SharedMisc.h"

static bool sg_bAssertEntered;

// Rate of timer tick (in milliseconds)
#define	TIMER_TICK_RATE			10

// Rate of high resolution timer tick
#define	HR_TIMER_TICK_RATE		1

void ArchHardReset(void)
{
	printf("\nHard reset function called. Program exiting.\n\n");
	exit(1);
}
	
void ArchDelay(uint32_t u32Microseconds)
{
	if (u32Microseconds < 1000)
	{
		Sleep(1);
		DebugOut("<1 Microseconds sleep - fix it\n");
	}
	else
	{
		Sleep(u32Microseconds / 1000);
	}
}

void *SharedMemAlloc(uint64_t u64Size,
					 const char *peProcedure,
					 const char *peFile,
					 uint32_t u32LineNumber,
					 bool bZeroMemory)
{
	void *pvAlloc;

#ifdef _FASTHEAP
	pvAlloc = FastHeapAllocate(sg_psFastHeap,
							   u64Size,
							   peProcedure,
							   peFile,
							   u32LineNumber,
							   bZeroMemory);
#else

	// Make the compiler happy
	(void) peProcedure;
	(void) peFile;
	(void) u32LineNumber;

	pvAlloc = malloc(u64Size);

	if (pvAlloc)
	{
		if (bZeroMemory)
		{
			memset(pvAlloc, 0, u64Size);
		}
	}
#endif
	return(pvAlloc);
}


void *SharedReallocMemoryInternal(void *pvOldBlock,
								  uint64_t u64Size,
								  const char *peProcedure,
						  		  const char *peFile,
								  uint32_t u32LineNumber)
{
	void *pvNewBlock = NULL;

#ifdef _FASTHEAP
	pvNewBlock = FastHeapRealloc(sg_psFastHeap,
								 pvOldBlock,
								 u64Size,
								 peProcedure,
						  		 peFile,
								 u32LineNumber);
#else
	(void) peProcedure;
	(void) peFile;
	(void) u32LineNumber;
	
	pvNewBlock = realloc(pvOldBlock, (size_t) u64Size);
#endif

	return(pvNewBlock);
}

void SharedMemFree(void *pvBlock,
				   const char *peProcedure,
				   const char *peFile,
				   uint32_t u32LineNumber)
{
	// Do not allow NULL frees
#ifdef _FASTHEAP
	FastHeapDeallocate(sg_psFastHeap,
					   peProcedure,
					   peFile,
					   u32LineNumber,
					   pvBlock);
#else
	free(pvBlock);
#endif
}

void SharedHeapCheck(void)
{
#ifdef _FASTHEAP
	FastHeapCheckIntegrity(sg_psFastHeap);
#else
#ifndef _RELEASE
	_CrtCheckMemory();
#endif
#endif
}

#define	HEAP_DUMP_FILENAME	"./Logs/%s/Heap%u.txt"
#define	HEAP_DUMP_DEPTH		20

void SharedHeapDump(void)
{
#ifdef _FASTHEAP
	uint32_t u32Loop = HEAP_DUMP_DEPTH;
	char eTempFilename[200];
	char *peSharedServerTag = "";

#ifdef _SERVER
	peSharedServerTag = SharedServerGetTag();
#endif

	snprintf(eTempFilename, sizeof(eTempFilename) - 1, HEAP_DUMP_FILENAME, peSharedServerTag, HEAP_DUMP_DEPTH);
	_unlink(eTempFilename);									   

	while (u32Loop)
	{
		char eFilename1[1024];
		char eFilename2[1024];

		snprintf(eFilename1, sizeof(eFilename1) - 1, HEAP_DUMP_FILENAME, peSharedServerTag, u32Loop);
		snprintf(eFilename2, sizeof(eFilename2) - 1, HEAP_DUMP_FILENAME, peSharedServerTag, u32Loop - 1);
		rename(eFilename2, eFilename1);
		--u32Loop;
	}

	snprintf(eTempFilename, sizeof(eTempFilename) - 1, HEAP_DUMP_FILENAME,peSharedServerTag, 0);

	FastHeapDump(sg_psFastHeap,
				 eTempFilename);
#endif
}

void ArchHeapInit(void)
{
#ifdef _FASTHEAP
	EStatus eStatus;
	SFastHeapRegion *psHeapRegions;
	uint64_t u64HeapRegionCount;

	eStatus = PlatformHeapGetRegions(&psHeapRegions,
									 &u64HeapRegionCount);
	BASSERT(ESTATUS_OK == eStatus);

	eStatus = FastHeapCreate("Program heap",
							 &sg_psFastHeap,
							 1024*1024,
							 psHeapRegions,
							 u64HeapRegionCount);
	BASSERT(ESTATUS_OK == eStatus);
#endif
}

void ArchInterruptDisable(void)
{
	// Doesn't do anything under Windows
}

void ArchInterruptEnable(void)
{
	// Doesn't do anything under Windows
}

bool ArchIsInterrupt(void)
{
	// No concept of interrupt context under Windows
	return(false);
}

bool ArchInterruptsEnabled(void)
{
	return(true);
}

bool ArchIsDMASafe(void *pvAddress,
				   uint32_t u32Size)
{
	return(true);
}

uint64_t ArchRTCGet(void)
{
	SYSTEMTIME sSystemTime;
	struct tm sTime;

	memset((void *) &sSystemTime, 0, sizeof(sSystemTime));
	memset((void *) &sTime, 0, sizeof(sTime));

	// Go get the system time to millisecond resolution
	GetSystemTime(&sSystemTime);

	// Now convert it to time_t
	sTime.tm_year = sSystemTime.wYear - 1900;
	sTime.tm_mon = sSystemTime.wMonth - 1;
	sTime.tm_mday = sSystemTime.wDay;
	sTime.tm_hour = sSystemTime.wHour;
	sTime.tm_min = sSystemTime.wMinute;
	sTime.tm_sec = sSystemTime.wSecond;

	// # Of milliseconds since January 1st, 1970
	return((uint64_t) ((_mkgmtime(&sTime) * 1000) + sSystemTime.wMilliseconds));
}

EStatus ArchGPIOSet(uint32_t u32GPIODef,
					bool bAsserted)
{
	return(ESTATUS_OK);
}

EStatus ArchGPIOGet(uint32_t u32GPIODef,
					bool *pbAsserted)
{
	return(ESTATUS_OK);
}

// Initializes a GPIO (architecture level)
EStatus ArchGPIOInit(uint32_t u32GPIODef)
{
	return(ESTATUS_OK);
}

uint32_t ArchGetSystemTickRate(void)
{
	return(TIMER_TICK_RATE);
}

uint32_t ArchGetTimerTickRate(void)
{
	return(ArchGetSystemTickRate());
}

uint32_t ArchGetTimerHRTickRate(void)
{
	return(HR_TIMER_TICK_RATE);
}

EStatus ArchRandomNumber(uint64_t *pu64RandomNumber)
{
	EStatus eStatus = ESTATUS_OK;

	if (pu64RandomNumber)
	{
		uint32_t u32RandomNumber1;
		uint32_t u32RandomNumber2;
		errno_t eRandErr;

		eRandErr = rand_s((unsigned int *) &u32RandomNumber1);
		if (eRandErr)
		{
			eStatus = ESTATUS_RANDOM_NUMBER_NOT_VALID;
		}
		else
		{
			eRandErr = rand_s((unsigned int *) &u32RandomNumber2);
		}
		if (eRandErr)
		{
			eStatus = ESTATUS_RANDOM_NUMBER_NOT_VALID;
		}
		else
		{
			*pu64RandomNumber = (((uint64_t) u32RandomNumber2) << 32) | u32RandomNumber1;
		}
	}
	
	return(ESTATUS_OK);
}

EStatus ArchEraseSector(uint8_t u8Sector,
						uint32_t u32Address,
						uint32_t u32SectorSize)
{
	return(ESTATUS_FUNCTION_NOT_SUPPORTED);
}

EStatus ArchProgramFlash(uint8_t u8Sector,
						 uint32_t u32Address,
						 uint8_t *pu8DataSrcOriginal,
						 uint32_t u32Size)
{
	return(ESTATUS_FUNCTION_NOT_SUPPORTED);
}

EStatus ArchFlashInit(void)
{
	return(ESTATUS_FUNCTION_NOT_SUPPORTED);
}

EStatus ArchIntInit(void)
{
	return(ESTATUS_OK);
}

// This is a thread that runs and simulates timer ticks to the timer routine
static void CALLBACK ArchTimerTick(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	// Call timer code
	TimerTick();

	// Call platform code
	PlatformTick();
}

// This is a thread that runs and simulates timer ticks to the timer routine
static void CALLBACK ArchTimerHRTick(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	// Call timer code
	TimerHRTick();
}

static EStatus ArchTimerTickInit(void)
{
	MMRESULT sTmrResult;
	EStatus eStatus;

	sTmrResult = timeSetEvent(HR_TIMER_TICK_RATE, HR_TIMER_TICK_RATE, ArchTimerHRTick, 0, TIME_PERIODIC);
	if( NULL == (void*)sTmrResult )
	{
		eStatus = ESTATUS_OS_THREAD_CANT_CREATE;
		goto errorExit;
	}
	else
	{
		// All good
		eStatus = ESTATUS_OK;
	}

	sTmrResult = timeSetEvent(TIMER_TICK_RATE, TIMER_TICK_RATE, ArchTimerTick, 0, TIME_PERIODIC);
	if( NULL == (void*)sTmrResult )
	{
		eStatus = ESTATUS_OS_THREAD_CANT_CREATE;
		goto errorExit;
	}
	else
	{
		// All good
		eStatus = ESTATUS_OK;
	}

errorExit:
	return(eStatus);
}

// Check pointer for validity
bool ArchIsPointerValid(void *pvPointer,
						bool bIncludeCodeRegions)
{
	return(true);
}

// Algorithm from: https://docs.microsoft.com/en-us/windows/win32/perfctrs/retrieving-counter-names-and-help-text
void ArchGetUniqueSystemID(uint8_t *pu8Buffer,
						   uint32_t *pu8BufferLength)
{
	uint8_t u8CopyLength = GUID_LEN;
	uint8_t u8FinalIDBuffer[GUID_LEN];
	MY_SHA256_CTX sSHAContext;
	LSTATUS eLStatus;
	HKEY hKey;
	BYTE u8KeyData[164];	// DigitalProductId data length
	DWORD u32Length;

	// Target buffer length required
	BASSERT(pu8BufferLength);

	// Special case for "length only" query
	if (NULL == pu8Buffer)
	{
		*pu8BufferLength = u8CopyLength;
		return;
	}

	// Clip the length to the overall buffer length
	if (u8CopyLength > *pu8BufferLength)
	{
		u8CopyLength = (uint8_t) *pu8BufferLength;
	}

	ZERO_STRUCT(u8FinalIDBuffer);

	// Begin SHA256!
	MySHA256_Init(&sSHAContext);

	// Microsoft' "digital product ID" is used as the key
	eLStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
							TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), 
							REG_OPTION_NON_VOLATILE, 
							KEY_READ, 
							&hKey);
	BASSERT(ERROR_SUCCESS == eLStatus);

	ZERO_STRUCT(u8KeyData);
	u32Length = sizeof(u8KeyData);

	// Get the DigitalProductId registry key value
	eLStatus = RegQueryValueEx(hKey, 
							   TEXT("DigitalProductId"), 
							   NULL, 
							   NULL, 
							   u8KeyData, 
							   &u32Length);
	BASSERT(ERROR_SUCCESS == eLStatus);
	RegCloseKey(hKey);

	// Hash the whole key
	MySHA256_Update(&sSHAContext, u8KeyData, sizeof(u8KeyData));
	MySHA256_Final(u8FinalIDBuffer, &sSHAContext);

	// Copy out just the data that the caller wanted
	memcpy(pu8Buffer, u8FinalIDBuffer, u8CopyLength);

	// Return the length of the actual data copied
	*pu8BufferLength = u8CopyLength;
}

void PlatformGetSystemLanguage(char *peLanguageName,
							   uint32_t u32LanguageNameBufferSize)
{
	int s32Result;
	WCHAR eLanguage[200];
	uint32_t u32Loop = 0;

	s32Result = GetUserDefaultLocaleName(eLanguage,
										 sizeof(eLanguage) - 1);
	BASSERT(s32Result);

	// This comes back in ASCII/UNICODE.
	--u32LanguageNameBufferSize;
	while ((u32Loop >> 1) < u32LanguageNameBufferSize)
	{
		if ('\0' == eLanguage[u32Loop])
		{
			break;
		}

		*peLanguageName = (char) eLanguage[u32Loop];
		++peLanguageName;
		*peLanguageName = '\0';
		++u32Loop;
	}
}

void PlatformGetTimezoneOffset(int64_t *ps64TimezoneOFfsetMilliseconds)
{
	DWORD s32Result;
	TIME_ZONE_INFORMATION sTimezoneInfo;

	ZERO_STRUCT(sTimezoneInfo);

	s32Result = GetTimeZoneInformation(&sTimezoneInfo);

	*ps64TimezoneOFfsetMilliseconds = sTimezoneInfo.Bias;

	// Adjust biases if appropriate
	if (TIME_ZONE_ID_DAYLIGHT == s32Result)
	{
		*ps64TimezoneOFfsetMilliseconds += sTimezoneInfo.DaylightBias;
	}
	else
	if (TIME_ZONE_ID_STANDARD == s32Result)
	{
		*ps64TimezoneOFfsetMilliseconds += sTimezoneInfo.StandardBias;
	}

	// Adjust to be offset from GMT
	*ps64TimezoneOFfsetMilliseconds = -(*ps64TimezoneOFfsetMilliseconds);

	// Convert minutes to milliseconds
	*ps64TimezoneOFfsetMilliseconds *= (60*1000);
}

EStatus PlatformGetUniqueSystemID(uint8_t *pu8Buffer,
								  uint32_t *pu32BuffLength)
{
	ArchGetUniqueSystemID(pu8Buffer,
						  pu32BuffLength);

	return(ESTATUS_OK);
}

void ArchCriticalError(char *peMessage)
{
	MessageBoxA(NULL, peMessage, "Server", MB_ICONSTOP | MB_OK);
}

void ArchInit(void *pvArchSpecificData)
{
	EStatus eStatus;
	WSADATA wsaData;
	WORD version;
	int error;

	// Init Winsock
	version = MAKEWORD(2, 0);
	error = WSAStartup(version, &wsaData);

	// See https://www.installsetupconfig.com/win32programming/windowsocketwinsock214_7.html
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0)
	{
		// Unexpected Winsock version
		WSACleanup();
		assert(0);
	}

	// The heap needs to be initialized VERY early on.
	ArchHeapInit();

	// Init the debug console - Needs to come after the button init because it
	// changes the course of the debug console behavior
	DebugConsoleInit();

	// Initialize the SysEvent manager
	eStatus = SysEventInit();
	BASSERT(ESTATUS_OK == eStatus);

	// Now init the timer module
	TimerInit();

 	// Initialization of the OS that has no dependencies
	OSInitEarly();

	// Init the UtilTask
	eStatus = UtilTaskInit();
	BASSERT(ESTATUS_OK == eStatus);

	// Early init - after the console and RAM are up
	PlatformInitEarly();
	
	// Fire off a thread that virtualizes a timer tick
	eStatus = ArchTimerTickInit();
	BASSERT(ESTATUS_OK == eStatus);

#ifdef _FASTHEAP
	// Install a lazy deallocator
	eStatus = FastHeapInstallDeallocator(sg_psFastHeap);
	BASSERT(ESTATUS_OK == eStatus);
#endif

	// And whatever the OS needs to do to init itself. This returns - it doesn't
	// start the OS.
	OSInit();
}

// This will find pvNeedleStart for u64NeedleLength starting at pvHaystackStart for
// u64HaystackLength
void *memmem(void *pvHaystackStart,
			 uint64_t u64HaystackLength,
			 void *pvNeedleStart, 
			 uint64_t u64NeedleLength)
{
	// Can't find a needle in a hastack when the needle is bigger than the haystack
	if (u64NeedleLength > u64HaystackLength)
	{
		return(NULL);
	}

	// Can't be a 0 byte needle
	if (0 == u64NeedleLength)
	{
		return(NULL);
	}

	while (u64HaystackLength)
	{
		if (*((uint8_t *) pvHaystackStart) == *((uint8_t *) pvNeedleStart))
		{
			// Do a memcmp and see if we find it
			if (memcmp(pvHaystackStart, pvNeedleStart, (size_t) u64NeedleLength) == 0)
			{
				// Found it!
				return(pvHaystackStart);
			}
		}

		pvHaystackStart = (void *) (((uint8_t *) pvHaystackStart) + 1);
		--u64HaystackLength;
	}

	return(NULL);
}

// Create a printable callstack for Windows programs
static void GenerateCallStack(char *peCallStackText, 
							  uint32_t u32Length)
{
	EStatus eStatus;
	char eCallstackStrings[4096];
	STACKFRAME64 sStackFrame;
	SYMBOL_INFO *psSymbolInfo;
	IMAGEHLP_LINE64 sLineInfo;
	IMAGEHLP_MODULE64 sModuleInfo;
	CONTEXT sStackContext;
	HANDLE eProcessHandle;
	HANDLE eThreadHandle;
	uint32_t u32SymbolInfoLength;
	uint32_t u32Displacement;
	int32_t s32ImageType;

	*peCallStackText = '\0';
	
	eProcessHandle = GetCurrentProcess();
	eThreadHandle = GetCurrentThread();

	// Create symbol info structures
	u32SymbolInfoLength = sizeof(*psSymbolInfo) + sizeof(eCallstackStrings) - 1;

	MEMALLOC(psSymbolInfo, u32SymbolInfoLength);
	
	memset((void *) psSymbolInfo, 0, (size_t) u32SymbolInfoLength);
	psSymbolInfo->SizeOfStruct = sizeof(*psSymbolInfo);
	psSymbolInfo->MaxNameLen = sizeof(eCallstackStrings);

	ZERO_STRUCT(sLineInfo);
	sModuleInfo.SizeOfStruct = sizeof(sLineInfo);

	ZERO_STRUCT(sModuleInfo);
	sModuleInfo.SizeOfStruct = sizeof(sModuleInfo);

	SymInitialize(eProcessHandle, NULL, true);

	ZERO_STRUCT(sStackContext);
	sStackContext.ContextFlags = CONTEXT_ALL;
	
	RtlCaptureContext(&sStackContext);

	ZERO_STRUCT(sStackFrame);

	s32ImageType = IMAGE_FILE_MACHINE_AMD64;
	sStackFrame.AddrPC.Offset = sStackContext.Rip;
	sStackFrame.AddrFrame.Offset = sStackContext.Rsp;
	sStackFrame.AddrStack.Offset = sStackContext.Rsp;

	sStackFrame.AddrPC.Mode = AddrModeFlat;
	sStackFrame.AddrFrame.Mode = AddrModeFlat;
	sStackFrame.AddrStack.Mode = AddrModeFlat;

	while (u32Length)
	{
		if (false == StackWalk64(s32ImageType,
								 eProcessHandle,
								 eThreadHandle,
								 &sStackFrame,
								 &sStackContext,
								 NULL,
								 SymFunctionTableAccess64,
								 SymGetModuleBase64,
								 NULL))
		{
			goto errorExit;
		}

		// Either offset or PC is 0, it means end of stack
		if ((0 == sStackFrame.AddrFrame.Offset) || 
			(0 == sStackFrame.AddrPC.Offset))
		{
			goto errorExit;
		}

		// Get the symbol from the address
		if (false == SymFromAddr(eProcessHandle, sStackFrame.AddrPC.Offset, NULL, psSymbolInfo))
		{
			goto errorExit;
		}

		// Get file/line info from the symbol's address
		if (false == SymGetLineFromAddr64(eProcessHandle, sStackFrame.AddrPC.Offset, (DWORD*) &u32Displacement, &sLineInfo))
		{
			goto errorExit;
		}

		// And finally, get the module name
		if (false == SymGetModuleInfo64(eProcessHandle, sStackFrame.AddrPC.Offset, &sModuleInfo))
		{
			goto errorExit;
		}

		// All symbol table and info obtained - append it to our call stack string
		snprintf(eCallstackStrings, sizeof(eCallstackStrings), "%s!%s (%s Line: %u)\n", 
					sModuleInfo.ModuleName, 
					PathFindFileNameA(psSymbolInfo->Name), 
					PathFindFileNameA(sLineInfo.FileName), 
					sLineInfo.LineNumber);
		strncat_s(peCallStackText, u32Length, eCallstackStrings, u32Length); 
		
		// Only if we have room in our buffer
		if (u32Length > strlen(eCallstackStrings))
		{
			u32Length -= (uint32_t) strlen(eCallstackStrings);
		}
		else
		{
			u32Length = 0;
		}
	}

errorExit:
	SafeMemFree(psSymbolInfo);
}

// Handles app level assertions
void AssertHandlerProc(char *pu8Expression,
					   char *pu8Module,
					   uint32_t u32Line)
{
	// Only handle first assert, to prevent infinite reentry
	if( false == sg_bAssertEntered )
	{
		char u8CallStack[4096];
		
		sg_bAssertEntered = true;

		// If we're in debug mode, give a slight delay so the logging threads can
		// catch up
#ifndef _RELEASE
		OSSleep(1000);
#endif

		ArchInterruptDisable();

		// Construct the call stack
		GenerateCallStack(u8CallStack, sizeof(u8CallStack));

		DebugOut("\r\n\r\nAssert:\r\nModule : %s\r\nLine   : %u\r\nExpr   : %s\r\n\r\nCallstack:\r\n%s\r\n", pu8Module, u32Line, pu8Expression, u8CallStack);
		Syslog("\r\n\r\nAssert:\r\nModule : %s\r\nLine   : %u\r\nExpr   : %s\r\n\r\nCallstack:\r\n%s\r\n", pu8Module, u32Line, pu8Expression, u8CallStack);

		SharedHeapCheck();
	
		// Give the platform code an opportunity to save off the various bits of assert
		// info for logging upon restart
		PlatformAssertRecord(pu8Expression,
							 pu8Module,
							 u32Line);
	}

	// Slight delay to allow other threads to finish
	OSSleep(1000);
	exit(1);
}



// This will return ESTATUS_OK if the provided program is running, or ESTATUS_NO_FILE if it's not running.
// All other errors are internal issues.
EStatus ArchIsProgramRunning(char *peExecutableFilenameSubstring, 
							 uint32_t *pu32Matches)
{
	EStatus eStatus = ESTATUS_OK;
	DWORD u32Processes[4096];
	DWORD u32ProcessCount;
	uint32_t u32Loop;
	bool bMatched = false;

	if (pu32Matches)
	{
		*pu32Matches = 0;
	}

	// Find out how many processes there are
	if (false == EnumProcesses(u32Processes,
							   sizeof(u32Processes),
							   &u32ProcessCount))
	{
		Syslog("Failed EnumProcesses()\n");
		eStatus = ESTATUS_OS_UNKNOWN_ERROR;
		goto errorExit;
	}

	// Turn u32ProcessCount into a count rather than the size returned
	u32ProcessCount /= sizeof(u32Processes[0]);

	Syslog("*** START\n");

	for (u32Loop = 0; u32Loop < u32ProcessCount; u32Loop++)
	{
		WCHAR eProcessNameTChar[MAX_PATH];
		HANDLE hProcess;

		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
							   PROCESS_VM_READ,
							   false, 
							   u32Processes[u32Loop]);
		if (hProcess)
		{
			HMODULE hModule;
			DWORD u32Count;

			if (EnumProcessModules(hProcess,
								   &hModule,
								   sizeof(hModule),
								   &u32Count))
			{
				int s64Result;
				char eProcessName[MAX_PATH + 1];

				GetModuleBaseNameW(hProcess,
								   hModule,
								   eProcessNameTChar,
								   sizeof(eProcessNameTChar) / sizeof(eProcessNameTChar[0]));
				memset((void *) eProcessName, 0, sizeof(eProcessName));
				s64Result = WideCharToMultiByte(CP_UTF8,
												0,
												eProcessNameTChar,
												(int) wcslen(eProcessNameTChar),
												eProcessName,
												sizeof(eProcessName) - 1,
												NULL,
												NULL);

				if (0 == s64Result)
				{
					DWORD dwError;

					dwError = GetLastError();
					Syslog("Failed WideCharToMultiByte() - dwError=%.8x\n", dwError);
					eStatus = ESTATUS_OS_UNKNOWN_ERROR;
					goto errorExit;
				}
				else
				{
					// Substring match?
//					Syslog("  %s - %u\n", eProcessName, u32Processes[u32Loop]);
					if (Sharedstrcasestr(eProcessName,
										 peExecutableFilenameSubstring))
					{
						// Matched! It's running!
						eStatus = ESTATUS_OK;
						bMatched = true;

						if (pu32Matches)
						{
							++(*pu32Matches);
						}
					}
				}
			}
			else
			{
				Syslog("Failed EnumProcessModules() on ID %u\n", u32Processes[u32Loop]);
			}
		}
		else
		{
			// This can happen on processes that are supervisor/administrator permissioned and not necessarily
			// indicative of a failure.

//			Syslog("Failed OpenProcess() on ID %u\n", u32Processes[u32Loop]);
		}

		CloseHandle(hProcess);
	}

	if (bMatched)
	{
		eStatus = ESTATUS_OK;
	}
	else
	{
		eStatus = ESTATUS_NO_FILE;
	}

errorExit:
	Syslog("*** END\n");
	return(eStatus);
}