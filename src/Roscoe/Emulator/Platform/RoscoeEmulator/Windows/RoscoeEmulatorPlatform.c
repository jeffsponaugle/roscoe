#include "Shared/Shared.h"
#include "Platform/Platform.h"
#include "Shared/SysEvent.h"
#include "Shared/Version.h"
#include "Arch/Arch.h"
#include "Shared/Sound/Sound.h"
#include "OS/Windows/WindowsSound.h"
#include "Platform/RoscoeEmulator/RoscoeEmulator.h"
#include <crtdbg.h>

#pragma warning(disable:4100)

// Called after the console init, console, and heap have been initialized
void PlatformInitEarly(void)
{
	// Announce yourself - Added the reference to u32CRC in the version structure
	// so the compiler doesn't truncate the version structure
	DebugOut("\nRoscoe Emulator - V%u.%.2u, build %u, Windows 64 bit\n", g_sVersionInfo.u8Major, g_sVersionInfo.u8Minor, g_sVersionInfo.u16BuildNumber);
}

// Called once per system timer tick at a rate of once per ArchGetTickRate() (ms)
void PlatformTick(void)
{
	// Does nothing
}

EStatus PlatformAssertRecord(char *pu8Expression,
							 char *pu8Module,
							 uint32_t u32Line)
{
#ifdef _DEBUG

	if (_CrtDbgReport( _CRT_ASSERT, pu8Module, u32Line, "RoscoeEmulator",
		"%s", pu8Expression) == 1)
	{
		DebugBreak();
	}

#else
	
	char eMessage[WINDOWS_MBOX_MAX];

	sprintf_s(eMessage, ARRAYSIZE(eMessage), "Assert:\n\nModule:\t%s\nLine:\t%u\n\nExpr:\t%s", pu8Module, u32Line, pu8Expression);
	MessageBoxA(NULL, eMessage, "RoscoeEmulator", MB_ICONSTOP | MB_OK);

#endif

	return(ESTATUS_LOGGED_ASSERT_UNAVAILABLE);
}

// Returns the base pathname that should prefix all file related operations.
// This is for environments that have no concept of a current working directory.
// The path returned shall include leading / if necessary - underlying code will
// not add directory separators.
// 
// If NULL is returned, then no prefix will occur.
char *PlatformGetBasePathname(void)
{
	// Whatever the current working directory is
	return("./");
}

// Called to initialize the debug console
EStatus PlatformDebugConsoleInit(void)
{
	return(ESTATUS_OK);
}

// Called when a debug message is done with programmed I/O - no interrupts
// and no buffering. Doesn't return until all data is sent.
EStatus PlatformDebugOut(UINT8 *peMessage,
						 uint32_t u32Length)
{
#ifndef _RELEASE
	HANDLE hHandle;
	DWORD u32Foo;
	char *peBuffer;

	peBuffer = calloc(u32Length + 1, 1);
	BASSERT(peBuffer);

	memcpy((void *) peBuffer, (void *) peMessage, u32Length);

	OutputDebugStringA(peBuffer);
	free(peBuffer);

	hHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteFile(hHandle,
			  peMessage,
			  u32Length,
			  &u32Foo, 
			  NULL);
#endif

	return(ESTATUS_OK);
}

// Flush the console (interrupt driven)
void PlatformFlushConsole(void)
{
	// Does nothing under Windows
}

EStatus PlatformGetSyslogInfo(char **ppeBaseFilename,
							  uint32_t *pu32LogDepth,
							  bool *pbConsoleOutput)
{
	// Make the Logs directory if it isn't there
	(void) Filemkdir("./Logs");

	*ppeBaseFilename = "Logs/RoscoeEmulator%u.txt";
	*pu32LogDepth = 10;
	*pbConsoleOutput = true;

	return(ESTATUS_OK);
}

char *PlatformGetProgramName(void)
{
	return("RoscoeEmulator");
}

// Return the physical size of the display that we want
void PlatformGetGraphicsSettings(uint32_t *pu32PhysicalXSize,
								 uint32_t *pu32PhysicalYSize,
								 uint32_t *pu32VirtualXSize,
								 uint32_t *pu32VirtualYSize,
								 bool *pbFullScreen,
								 bool *pbBorderless)
{
	// Set 4K to start with
	*pu32PhysicalXSize = 3840>>1;
	*pu32PhysicalYSize = 2160>>1;

	// And assume virtual == physical
	*pu32VirtualXSize = *pu32PhysicalXSize;
	*pu32VirtualYSize = *pu32PhysicalYSize;

	// Assume not borderless
	*pbBorderless = true;

	// Running normally
	if (CmdLineOption("-fullscreen"))
	{
		*pbFullScreen = true;
	}
	else
	{
		*pbFullScreen = false;
	}
}

EStatus PlatformGetViewportInitialPosition(uint32_t *pu32XPos,
										   uint32_t *pu32YPos)
{
	*pu32XPos = 400;
	*pu32YPos = 400;
	return(ESTATUS_OK);
}


// Set the sound latency (milliseconds)
void PlatformSoundSetLatency(UINT64 u64LatencyMS)
{
	// Not used by RoscoeEmulator
}

// Displays a critical error
void PlatformCriticalError(char *peMessage)
{
	ArchCriticalError(peMessage);
}

#define	TARGET_SAMPLE_RATE			44100
//#define	TARGET_SAMPLE_RATE			96000
#define	RENDER_BUFFER_SIZE			2048		// In samples, not bytes
#define SOUND_CHANNELS				1			// # Of channels for the audio
#define	CHANNEL_BUFFER_SIZE			(RENDER_BUFFER_SIZE / 2)	// Size of the channel buffer(s)

EStatus PlatformSoundInit(void)
{
	EStatus eStatus;

	eStatus = WindowsSoundOpen(TARGET_SAMPLE_RATE,
							   RENDER_BUFFER_SIZE,
							   SOUND_CHANNELS);
	ERR_GOTO();
							  
	// Initialize the sound engine
	eStatus = SoundInit(RENDER_BUFFER_SIZE,
						CHANNEL_BUFFER_SIZE,
						TARGET_SAMPLE_RATE);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

uint32_t PlatformSoundGetSampleRate(void)
{
	return(TARGET_SAMPLE_RATE);
}

int WINAPI WinMain(HINSTANCE eInstance,
				   HINSTANCE ePrevInstance,
				   LPSTR peCmdLine,
				   int s64CmdShow)
{
	int s64Result;

#ifndef _RELEASE
	AllocConsole();
#endif

	// Init the whole architecture. Configuration parameter not used.
	ArchInit(NULL);

	// Start the operating system
	ArchStartOS();

	s64Result = RoscoeEmulatorEntry(peCmdLine,
									NULL,
									0);

	// Shutdown the syslog
	SyslogShutdown();

	return(0);
}

