#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#if defined(__LINUX__)
#include <linux/serial.h>

#include <libgen.h>
#include "Shared/Shared.h"
#include "Shared/DCSerial.h"
#include "Shared/Timer.h"

#define INVALID_TERMIOS_HANDLE_VALUE	(-1)

// Serial timeouts
#define	SERIAL_WRITE_TIMEOUT_MS			1000
#define	SERIAL_READ_TIMEOUT_MS			1000
#define	SERIAL_SHUTDOWN_WAIT			5000

// Serial overlapped read buffers
#define	OVERLAPPED_READ_BUFFER_COUNT	5

// Enable serial logging
#define	SERIAL_LOG_ENABLE

// Macros for having serial logging
#ifdef SERIAL_LOG_ENABLE
#define SerialLog(...)	Syslog(__VA_ARGS__)
#else
#define SerialLog(...)
#endif


// States of consumer structures
typedef enum
{
	ECONSTATE_UNDEFINED,					// Not initialized/undefined

	ECONSTATE_NOT_ALLOCATED,				// Consumer slot not allocated
	ECONSTATE_PROBE,						// Awaiting port device discovery
	ECONSTATE_CONNECTED,					// Port is now connected to the device
	ECONSTATE_SHUTDOWN,						// Shutdown state
} EConsumerState;

// Linked list of consumers
typedef struct SConsumer
{
	EConsumerState eState;					// What is this conusmer's state?
	SOSCriticalSection sConsumerLock;		// Critical section/consumer lock

	char eCOMDeviceName[50];				// The COM port's device name

	// UNIX specific information
	int hSerialHandle;						// Platform serial handle

	// Used to signal a shutdown
	SOSEventFlag *psShutdownEventFlag;		// Event flag when we've shut down

	// Who registered this?
	char *peFile;							// File containing the registrar
	char *peFunction;						// Function registering
	UINT32 u32LineNumber;					// And line number
								 
	// Methods
	BOOL (*DiscoverCallback)(EDCSerialHandle eSerialHandle);
	void (*OpenCallback)(EDCSerialHandle eSerialHandle);
	void (*CloseCallback)(EDCSerialHandle eSerialHandle);
	void (*ReceiveCallback)(EDCSerialHandle eSerialHandle,
							UINT8 *pu8Data,
							UINT64 u64BytesReceived);

} SConsumer;

// Array of consumers
static SConsumer sg_sConsumers[20];
#define	MAX_CONSUMERS		(sizeof(sg_sConsumers) / sizeof(sg_sConsumers[0]))

// Used when allocating a new consumer slot
static SOSCriticalSection sg_sConsumerAllocateLock;

// Convert a serial handle to a consumer and check its range
static EStatus DCHandleToConsumer(EDCSerialHandle eSerialHandle,
								  SConsumer **ppsConsumer,
								  BOOL bCheckConnected,
								  BOOL bLockConsumer)
{
	EStatus eStatus = ESTATUS_OK;
	BOOL bLocked = FALSE;

	if ((0 == eSerialHandle) ||
		(eSerialHandle > MAX_CONSUMERS))
	{
		eStatus = ESTATUS_SERIAL_INVALID_HANDLE;
		goto errorExit;
	}

	if (bLockConsumer)
	{
		eStatus = OSCriticalSectionEnter(sg_sConsumers[eSerialHandle - 1].sConsumerLock);
		ERR_GOTO();

		bLocked = TRUE;
	}

	if (ppsConsumer)
	{
		*ppsConsumer = &sg_sConsumers[eSerialHandle - 1];
	}

	// See if we have a connection to a COM port. If not, throw an error.
	if (bCheckConnected)
	{
		if ((ECONSTATE_UNDEFINED == sg_sConsumers[eSerialHandle - 1].eState) ||
			(ECONSTATE_NOT_ALLOCATED == sg_sConsumers[eSerialHandle - 1].eState))
		{
			eStatus = ESTATUS_SERIAL_NOT_CONNECTED;
		}
		else
		{
			eStatus = ESTATUS_OK;
		}
	}

errorExit:
	if ((eStatus != ESTATUS_OK) &&
		(bLocked))
	{
		// We had an error and we're locked. Need to unlock.
		(void) OSCriticalSectionLeave(sg_sConsumers[eSerialHandle - 1].sConsumerLock);
	}

	return(eStatus);
}

// Find a consumer slot that isn't allocated and it will lock the consumer structure
static EStatus DCConsumerAllocate(EDCSerialHandle *peSerialHandle,
								  SConsumer **ppsConsumer)
{
	EStatus eStatus = ESTATUS_OK;
	UINT32 u32Loop;
	BOOL bLocked = FALSE;

	if (peSerialHandle)
	{
		*peSerialHandle = SERIAL_HANDLE_INVALID;
	}

	if (ppsConsumer)
	{
		*ppsConsumer = NULL;
	}

	eStatus = OSCriticalSectionEnter(sg_sConsumerAllocateLock);
	ERR_GOTO();
	bLocked = TRUE;

	for (u32Loop = 0; u32Loop < MAX_CONSUMERS; u32Loop++)
	{
		if (ECONSTATE_NOT_ALLOCATED == sg_sConsumers[u32Loop].eState)
		{
			if (peSerialHandle)
			{
				*peSerialHandle = (EDCSerialHandle) (u32Loop + 1);
			}

			if (ppsConsumer)
			{
				*ppsConsumer = &sg_sConsumers[u32Loop];
			}

			// Let the probing process include this!
			sg_sConsumers[u32Loop].eState = ECONSTATE_PROBE;
			eStatus = OSCriticalSectionEnter(sg_sConsumers[u32Loop].sConsumerLock);

			goto errorExit;
		}
	}

	// Nothing available
	eStatus = ESTATUS_SERIAL_CONSUMER_SLOTS_FULL;

errorExit:
	if (bLocked)
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = OSCriticalSectionLeave(sg_sConsumerAllocateLock);
		}
		else
		{
			(void) OSCriticalSectionLeave(sg_sConsumerAllocateLock);
		}
	}

	return(eStatus);
}

typedef struct
{
	UINT32 u32BaudRate;
	speed_t eSpeed;
} SBaudConvert;

// List of POSIX compliant speeds
static const SBaudConvert sg_sBaudRateList[] =
{
	{50, B50},
	{75, B75},  
	{110, B110},  
	{134, B134},  
	{150, B150},
	{200, B200}, 
	{300, B300}, 
	{600, B600}, 
	{1200, B1200}, 
	{1800, B1800}, 
	{2400, B2400}, 
	{4800, B4800}, 
	{9600, B9600},
	{19200, B19200},
	{38400, B38400},
	{57600, B57600},
	{115200, B115200},
	{230400, B230400},
	{460800, B460800},
};

static EStatus DCSerialGetSpeedEnum( UINT32 u32BaudRate, speed_t* peSpeed )
{
	SBaudConvert* psConvert;

	BASSERT( peSpeed );

	psConvert = (SBaudConvert*) &sg_sBaudRateList[0];
	while( psConvert < &sg_sBaudRateList[sizeof(sg_sBaudRateList)/sizeof(sg_sBaudRateList[0])] )
	{
		if( psConvert->u32BaudRate == u32BaudRate )
		{
			*peSpeed = psConvert->eSpeed;
			return(ESTATUS_OK);
		}
		psConvert++;
	}

	return( ESTATUS_SERIAL_SET_BAUDRATE_INVALID );
}



// Set the serial port's baud rate
EStatus DCSerialSetBaudRate(EDCSerialHandle eSerialHandle,
							UINT32 u32BaudRate)
{
	EStatus eStatus;
	SConsumer *psConsumer = NULL;
	struct termios sSerialParams;
	int s32Result;
	int s32SystemError;
	BOOL bLocked = FALSE;
	speed_t eRequestedSpeed;
	speed_t eInputSpeed;
	speed_t eOutputSpeed;

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 TRUE,
								 TRUE);
	ERR_GOTO();

	SerialLog("%s: Setting baud rate on '%s' to %u\n", __FUNCTION__, psConsumer->eCOMDeviceName, u32BaudRate);

	bLocked = TRUE;
	ZERO_STRUCT(sSerialParams);

	eStatus = DCSerialGetSpeedEnum( u32BaudRate, &eRequestedSpeed );
	ERR_GOTO();

	eStatus = ESTATUS_OK;

	s32Result = tcgetattr( psConsumer->hSerialHandle, &sSerialParams );
	s32SystemError = errno;

	if (0 == s32Result)
	{
		eInputSpeed = cfgetispeed( &sSerialParams );
		eOutputSpeed = cfgetospeed( &sSerialParams );

		// Got the parameters. If the parameters are not set, do it now
		if( (eInputSpeed != eRequestedSpeed) ||
			(eOutputSpeed != eRequestedSpeed) )
		{
			s32Result = cfsetispeed( &sSerialParams, eRequestedSpeed );
			BASSERT( 0 == s32Result );

			s32Result = cfsetospeed( &sSerialParams, eRequestedSpeed );
			BASSERT( 0 == s32Result );

			s32Result = tcsetattr( psConsumer->hSerialHandle, TCSANOW, &sSerialParams );
			s32SystemError = errno;

			if( s32Result )
			{
				// Error of some sort.
				SerialLog("%s: Failed to set baud rate on '%s' to %u - (%d): %s\n", __FUNCTION__, psConsumer->eCOMDeviceName, u32BaudRate, s32SystemError, strerror(s32SystemError));
				eStatus = ESTATUS_SERIAL_SET_BAUDRATE_INVALID;
			}
		}
		else
		{
			// Pass through since we're at the same baud rate
		}
	}
	else
	{
		// Some kind of error
		SerialLog("%s: Failed to get serial config on '%s' - (%d): %s\n", __FUNCTION__, psConsumer->eCOMDeviceName, s32SystemError, strerror(s32SystemError));
		eStatus = ESTATUS_SERIAL_SET_BAUDRATE_INVALID;
	}

errorExit:
	if (bLocked)
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = OSCriticalSectionLeave(psConsumer->sConsumerLock);
		}
		else
		{
			(void) OSCriticalSectionLeave(psConsumer->sConsumerLock);
		}
	}

	return(eStatus);
}

// Register a consumer
EStatus DCSerialRegisterInternal(char *peFile,
								 char *peFunction,
								 UINT32 u32LineNumber,
								 BOOL (*DiscoverCallback)(EDCSerialHandle eSerialHandle),
								 void (*OpenCallback)(EDCSerialHandle eSerialHandle),
								 void (*CloseCallback)(EDCSerialHandle eSerialHandle),
								 void (*ReceiveCallback)(EDCSerialHandle eSerialHandle,
														 UINT8 *pu8Data,
														 UINT64 u64BytesReceived))
{
	EStatus eStatus;
	SConsumer *psConsumer = NULL;

	// Go allocate a slot if we can
	eStatus = DCConsumerAllocate(NULL,
								 &psConsumer);
	ERR_GOTO();

	// Hook up the methods
	psConsumer->DiscoverCallback = DiscoverCallback;
	psConsumer->OpenCallback = OpenCallback;
	psConsumer->CloseCallback = CloseCallback;
	psConsumer->ReceiveCallback = ReceiveCallback;

	// Record where this came from
	psConsumer->peFile = peFile;
	psConsumer->peFunction = peFunction;
	psConsumer->u32LineNumber = u32LineNumber;

	SerialLog("%s: Registering - %s:%s(%u)\n", __FUNCTION__, peFile, peFunction, u32LineNumber);

	// Unlock this consumer
	eStatus = OSCriticalSectionLeave(psConsumer->sConsumerLock);

errorExit:
	return(eStatus);
}

// Unregister a consumer
EStatus DCSerialUnregister(BOOL (*DiscoverCallback)(EDCSerialHandle eSerialHandle),
						   void (*OpenCallback)(EDCSerialHandle eSerialHandle),
						   void (*CloseCallback)(EDCSerialHandle eSerialHandle),
						   void (*ReceiveCallback)(EDCSerialHandle eSerialHandle,
												   UINT8 *pu8Data,
												   UINT64 u64BytesReceived))
{
	EStatus eStatus = ESTATUS_OK;
	UINT32 u32Loop;
	BOOL bUnregistered = FALSE;

	// Go find which consumer this is
	for (u32Loop = 0; u32Loop < MAX_CONSUMERS; u32Loop++)
	{
		// Lock this record
		eStatus = OSCriticalSectionEnter(sg_sConsumers[u32Loop].sConsumerLock);
		ERR_GOTO();

		if ((DiscoverCallback == sg_sConsumers[u32Loop].DiscoverCallback) &&
			(OpenCallback == sg_sConsumers[u32Loop].OpenCallback) &&
			(CloseCallback == sg_sConsumers[u32Loop].CloseCallback) &&
			(ReceiveCallback == sg_sConsumers[u32Loop].ReceiveCallback))
		{
			// Found it!
			SerialLog("%s: Unregistering - %s:%s(%u)\n", __FUNCTION__, sg_sConsumers[u32Loop].peFile, sg_sConsumers[u32Loop].peFunction, sg_sConsumers[u32Loop].u32LineNumber);

			// Clear out the methods
			sg_sConsumers[u32Loop].DiscoverCallback = NULL;
			sg_sConsumers[u32Loop].OpenCallback = NULL;
			sg_sConsumers[u32Loop].CloseCallback = NULL;
			sg_sConsumers[u32Loop].ReceiveCallback = NULL;

			// Ditch the registrar info
			sg_sConsumers[u32Loop].peFile = NULL;
			sg_sConsumers[u32Loop].peFunction = NULL;
			sg_sConsumers[u32Loop].u32LineNumber = 0;

			// Clsoe the handle, set it invalid
			close(sg_sConsumers[u32Loop].hSerialHandle);
			sg_sConsumers[u32Loop].hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE; 

			// Set the consumer state to unallocated
			sg_sConsumers[u32Loop].eState = ECONSTATE_NOT_ALLOCATED;

			bUnregistered = TRUE;
		}

		// Unlock and move on
		eStatus = OSCriticalSectionLeave(sg_sConsumers[u32Loop].sConsumerLock);
		ERR_GOTO();
	}

	// If we've unregistered a consumer, then we're OK
	if (bUnregistered)
	{
		eStatus = ESTATUS_OK;
	}
	else
	{
		eStatus = ESTATUS_SERIAL_CONSUMER_NOT_FOUND;
	}

errorExit:
	return(eStatus);
}

EStatus DCSerialWrite(EDCSerialHandle eSerialHandle,
					  UINT8 *pu8Data,
					  UINT64 u64BytesToWrite)
{
	EStatus eStatus = ESTATUS_OK;
	SConsumer *psConsumer = NULL;
	BOOL bLocked = FALSE;
	size_t s64DataWritten = 0;
	int s32SystemError;
	BOOL bWriteSuccess = FALSE;

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 FALSE,
								 TRUE);
	ERR_GOTO();
	bLocked = TRUE;

	// If we're not allocated or undefined, flag it as an invalid handle
	if ((ECONSTATE_UNDEFINED == psConsumer->eState) ||
		(ECONSTATE_NOT_ALLOCATED == psConsumer->eState))
	{
		eStatus = ESTATUS_SERIAL_NOT_CONNECTED;
		goto errorExit;
	}

	s64DataWritten = write( psConsumer->hSerialHandle, pu8Data, u64BytesToWrite );
	s32SystemError = errno;

	if( s64DataWritten < 0 )
	{
		Syslog("%s: Write error (%d): %s\n", __FUNCTION__, s32SystemError, strerror(s32SystemError));
		goto errorExit;
	}

	// If everything went OK, then check to see if we wrote what we thought we did,
	// and if not, return an error.
	if (u64BytesToWrite != s64DataWritten)
	{
		eStatus = ESTATUS_WRITE_TRUNCATED;
	}

errorExit:

	if (bLocked)
	{
		if (ESTATUS_OK == eStatus)
		{
			eStatus = OSCriticalSectionLeave(psConsumer->sConsumerLock);
		}
		else
		{
			(void) OSCriticalSectionLeave(psConsumer->sConsumerLock);
		}
	}

	return(eStatus);
}

EStatus DCSerialFlush(EDCSerialHandle eSerialHandle)
{
	EStatus eStatus = ESTATUS_OK;
	int s32Result;
	SConsumer *psConsumer = NULL;
	BOOL bLocked = FALSE;

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 TRUE,
								 TRUE);
	ERR_GOTO();
	bLocked = TRUE;

	// tcdrain() waits until data has been written
	// tcflush() would be incorrect because it drops all unsent data
	s32Result = tcdrain( psConsumer->hSerialHandle );
	if( s32Result )
	{
		eStatus = ESTATUS_SERIAL_FLUSH_FAULT;
	}

	errorExit:
		if (bLocked)
		{
			if (ESTATUS_OK == eStatus)
			{
				eStatus = OSCriticalSectionLeave(psConsumer->sConsumerLock);
			}
			else
			{
				(void) OSCriticalSectionLeave(psConsumer->sConsumerLock);
			}
		}

	return(eStatus);
}

static BOOL DCSerialDeviceIsSerial( char* peDeviceName )
{
	BOOL bIsSerial = FALSE;
	int hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE;
	int s32Result;
	int s32SystemError = 0;
	struct serial_struct sInfo;
	char eCOMDevicePath[512];

	ZERO_STRUCT( eCOMDevicePath );

	snprintf( eCOMDevicePath, sizeof(eCOMDevicePath)-1, "/dev/%s", peDeviceName );

	// Open up the port
	hSerialHandle = open( eCOMDevicePath, O_RDWR | O_NOCTTY | O_NONBLOCK );
	s32SystemError = errno;

	if( hSerialHandle < 0 )
	{
		hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE;
		goto errorExit;
	}

	// Try to get setial info.  This will only succeed if valid serial port
	s32Result = ioctl( hSerialHandle, TIOCGSERIAL, &sInfo );

	if( (0 == s32Result) &&
		(sInfo.type != PORT_UNKNOWN) )
	{
		bIsSerial = TRUE;
	}

	close( hSerialHandle );
	
errorExit:		
	return( bIsSerial );
}

// Linked list of ports to look at
typedef struct SCOMPort
{
	char eCOMDeviceName[50];				// The COM port's device name

	struct SCOMPort *psNextLink;
} SCOMPort;

// Deallocate a list of serial ports
static void DCSerialPortListDeallocate(SCOMPort **ppsCOMPortHead)
{
	SCOMPort *psCOMPortPtr = NULL;

	while (*ppsCOMPortHead)
	{
		psCOMPortPtr = (*ppsCOMPortHead)->psNextLink;
		SafeMemFree(*ppsCOMPortHead);
		*ppsCOMPortHead = psCOMPortPtr;
	}
}

// Get a list of all serial ports that aren't in use
static EStatus DCSerialGetPortList(SCOMPort **ppsCOMPortHead)
{
	EStatus eStatus = ESTATUS_OK;
	char eCOMDeviceName[50];
	SFileFind *psFileFind = NULL;
	BOOL bFileLooping = TRUE;
	SCOMPort *psCOMPortPtr = NULL;

	ZERO_STRUCT(eCOMDeviceName);

	// Iterate over tty in /dev
	eStatus = FileFindFirst("/dev",
							"tty*",
							&psFileFind);
	if (ESTATUS_OK == eStatus)
	{
		// If we've returned OK, then we ought to have a file find structure
		BASSERT(psFileFind);
	}

	// UNIX port doesn't get the first file from FileFindFirst()
	eStatus = FileFindNext(psFileFind);

	if ((eStatus != ESTATUS_OK) || ('\0' == psFileFind->eFilename[0]))
	{
		bFileLooping = FALSE;
	}

	while( bFileLooping )
	{
		//SerialLog("%s:  Testing '%s'\n", __FUNCTION__, psFileFind->eFilename);

		// Only check the name on directories
		if ((psFileFind->u32Attributes & ATTRIB_DIRECTORY) == 0)
		{
			strncpy( eCOMDeviceName, basename(psFileFind->eFilename), sizeof(eCOMDeviceName) );

			// First 3 letters need to be tty
			if (strncasecmp(eCOMDeviceName, "tty", 3) == 0)
			{
				UINT32 u32Loop;

				// Run through all active consumers and see if someone is using this one
				for (u32Loop = 0; u32Loop < MAX_CONSUMERS; u32Loop++)
				{
					// If we're not allocated, undefined, or probing, skip over it
					if ((ECONSTATE_UNDEFINED == sg_sConsumers[u32Loop].eState) ||
						(ECONSTATE_NOT_ALLOCATED == sg_sConsumers[u32Loop].eState) ||
						(ECONSTATE_PROBE == sg_sConsumers[u32Loop].eState))
					{
						// We're in a state for this consumer and we need to skip
					}
					else
					{
						// Compare the COM device name with what we've just enumerated
						if (strcasecmp(sg_sConsumers[u32Loop].eCOMDeviceName,
									 eCOMDeviceName) == 0)
						{
							// We've seen this
							break;
						}
					}
				}

				// If we have max consumers, then no one is
				if (MAX_CONSUMERS == u32Loop)
				{
					// Do a quick check to see if it's a serial port we want
					if( DCSerialDeviceIsSerial(eCOMDeviceName) )
					{
						// If this is true, we've already allocated at least one structure
						if (psCOMPortPtr)
						{
							// We have better already have a first item
							BASSERT(*ppsCOMPortHead);

							MEMALLOC(psCOMPortPtr->psNextLink, sizeof(*psCOMPortPtr));
							psCOMPortPtr = psCOMPortPtr->psNextLink;
						}
						else
						{
							// This means
							BASSERT(NULL == *ppsCOMPortHead);
							MEMALLOC(*ppsCOMPortHead, sizeof(**ppsCOMPortHead));
							psCOMPortPtr = *ppsCOMPortHead;
						}

						// psCOMPortPtr now points to new allocation!
						strncpy(psCOMPortPtr->eCOMDeviceName, eCOMDeviceName, sizeof(psCOMPortPtr->eCOMDeviceName) - 1);

						SerialLog("%s:  Discovered '%s'\n", __FUNCTION__, psCOMPortPtr->eCOMDeviceName);
					}
				}
				else
				{
					// Someone is already using this port
				}
			}
		}

		// Move on to the next file
		eStatus = FileFindNext(psFileFind);

		if ((eStatus != ESTATUS_OK) || ('\0' == psFileFind->eFilename[0]))
		{
			bFileLooping = FALSE;
		}
	}

	if( psFileFind )
	{
		eStatus = FileFindClose( &psFileFind );
		BASSERT( ESTATUS_OK == eStatus );
	}

	// Got our list!
	eStatus = ESTATUS_OK;

errorExit:

	SafeMemFree(psFileFind);

	if (eStatus != ESTATUS_OK)
	{
		SerialLog("%s: Complete - %s\n", __FUNCTION__, GetErrorText(eStatus));
	}

	return(eStatus);
}

// Serial receive thread
static void DCSerialReceiveThread(void *pvData)
{
	EDCSerialHandle eSerialHandle = (EDCSerialHandle) pvData;
	SConsumer *psConsumer;

	// Make sure it's in range
	BASSERT((eSerialHandle >= 1) && 
			(eSerialHandle <= MAX_CONSUMERS));
	psConsumer = &sg_sConsumers[eSerialHandle - 1];

	// Loop forever
	while (psConsumer->eState != ECONSTATE_SHUTDOWN)
	{
		size_t s64BytesReceived;
		UINT8 u8DataBuffer;
		BOOL bResult;
		int s32SystemError;

		// One byte at a time
		s64BytesReceived = read( psConsumer->hSerialHandle,
								 &u8DataBuffer,
								 sizeof(u8DataBuffer) );
		s32SystemError = errno;

		if( s64BytesReceived < 0 )
		{
			// PVTODO: Allow timeouts if it's an error
//                if (WAIT_TIMEOUT == u32WaitResult)
//                {
//                    // Timeout. That's OK. Just keep looping until we get something
//                }

			Syslog( "%s: Read error (%d): %s - exiting\n", __FUNCTION__, s32SystemError, strerror(s32SystemError) );
			goto errorExit;
		}

		// If we received data, send it back to the caller
		if (s64BytesReceived)
		{
			if (psConsumer->ReceiveCallback)
			{
				psConsumer->ReceiveCallback(eSerialHandle,
											&u8DataBuffer,
											s64BytesReceived);
			}
		}
	}

	SerialLog("%s: Closing due to ECONSTATE_SHUTDOWN == psConsumer->eState\n", __FUNCTION__);

errorExit:
	SerialLog("%s: Closing '%s'\n", __FUNCTION__, psConsumer->eCOMDeviceName);

	// If we have a "close" callback, then call it
	if (psConsumer->CloseCallback)
	{
		psConsumer->CloseCallback(eSerialHandle);
	}

	// Close the handle
	close(psConsumer->hSerialHandle);
	psConsumer->hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE;

	// Clear out the name of the ports
	psConsumer->eCOMDeviceName[0] = '\0';

	// Signal that we're complete
	(void) OSEventFlagSet(psConsumer->psShutdownEventFlag,
						  0x01);

	// Set the state of the port back to probe
	psConsumer->eState = ECONSTATE_PROBE;

}

// This will run through all consumers that are in "probe" state and 
static EStatus DCSerialProbe(SCOMPort *psCOMPort)
{
	EStatus eStatus = ESTATUS_OK;
	UINT32 u32Loop;
	int s32Result;
	struct termios sSerialParams;
	int hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE;
	int s32SystemError = 0;

	// Run through all slots and find any consumers that are in probe state
	for (u32Loop = 0; u32Loop < MAX_CONSUMERS; u32Loop++)
	{
		// Only probe states
		if (ECONSTATE_PROBE == sg_sConsumers[u32Loop].eState)
		{
			BOOL bDiscoveryOK = TRUE;
			UINT64 u64Flags;
			char eFullFilePath[256];

			ZERO_STRUCT(sSerialParams);
			ZERO_STRUCT(eFullFilePath);

			snprintf( eFullFilePath, sizeof(eFullFilePath)-1, "/dev/%s", psCOMPort->eCOMDeviceName );
			

			int hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE;
			int s32SystemError = 0;
			// Open up the port for use by the consumer
			hSerialHandle = open( eFullFilePath, O_RDWR | O_NOCTTY );
			s32SystemError = errno;

			if( hSerialHandle < 0 )
			{
				hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE;
				goto errorExit;
			}

			// Take exclusive access of this port so that other probes won't corrupt the data
			s32Result = ioctl( hSerialHandle, TIOCEXCL, NULL );
			s32SystemError = errno;

			if( s32Result < 0 )
			{
				// Some kind of error
				SerialLog("%s: Failed to take exclusive access of '%s' - (%d): %s\n", __FUNCTION__, psCOMPort->eCOMDeviceName, s32SystemError, strerror(s32SystemError));
				goto errorExit;
			}

			s32Result = tcgetattr( hSerialHandle, &sSerialParams );
			s32SystemError = errno;

			if (s32Result)
			{
				// Some kind of error
				SerialLog("%s: Failed to get serial config on '%s' - (%d): %s\n", __FUNCTION__, psCOMPort->eCOMDeviceName, s32SystemError, strerror(s32SystemError));
				goto errorExit;
			}

			// Set serial port defaults
			// Based on the config from:
			// https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
			cfsetispeed( &sSerialParams, B115200 );
			cfsetospeed( &sSerialParams, B115200 );
			sSerialParams.c_cflag |= CREAD;		// Enable read
			sSerialParams.c_cflag &= ~CSIZE;	// Clear all size bits
			sSerialParams.c_cflag |= CS8;		// 8 bits
			sSerialParams.c_cflag &= ~PARENB;	// No parity
			sSerialParams.c_cflag &= ~CSTOPB;	// One stop bit
			sSerialParams.c_cflag &= ~CRTSCTS;	// No CTS/RTS flow control
			sSerialParams.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
			sSerialParams.c_cflag |= CLOCAL;	// Disable control lines such as carrier detect

			// Incoming data handling
			sSerialParams.c_lflag &= ~ICANON;	// Disable canonical mode
			sSerialParams.c_lflag &= ~ISIG;
			sSerialParams.c_lflag &= ~(ECHO|ECHOE|ECHONL);
			sSerialParams.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);

			// Outgoing data handling
			sSerialParams.c_oflag &= ~(OPOST|ONLCR);

			// Timeouts
			sSerialParams.c_cc[VMIN] = 0;								// When VMIN=0, VTIME is timeout from start of read
			sSerialParams.c_cc[VTIME] = SERIAL_READ_TIMEOUT_MS/100;		// Read timeout in deciseconds


			s32Result = tcsetattr( hSerialHandle, TCSANOW, &sSerialParams );
			s32SystemError = errno;

			if( s32Result )
			{
				// Error of some sort.
				SerialLog("%s: Failed to set serial config on '%s' - (%d): %s\n", __FUNCTION__, psCOMPort->eCOMDeviceName, s32SystemError, strerror(s32SystemError));
				goto errorExit;
			}

			// Copy in serial port information to be used by the consumer's discovery process
			strncpy(sg_sConsumers[u32Loop].eCOMDeviceName, psCOMPort->eCOMDeviceName, sizeof(sg_sConsumers[u32Loop].eCOMDeviceName) - 1);
			sg_sConsumers[u32Loop].hSerialHandle = hSerialHandle;

			SerialLog("%s: Probing - '%s'\n", __FUNCTION__, sg_sConsumers[u32Loop].eCOMDeviceName);

			// Wait for something to do
			u64Flags = 0;
			(void) OSEventFlagGet(sg_sConsumers[u32Loop].psShutdownEventFlag,
								  0x01,
								  0,
								  &u64Flags);

			// Fork off a listener thread
			eStatus = OSThreadCreate(sg_sConsumers[u32Loop].eCOMDeviceName,
									 (void *) (u32Loop + 1),
									 DCSerialReceiveThread,
									 FALSE,
									 NULL,
									 0,
									 EOSPRIORITY_REALTIME);
			ERR_GOTO();

			// If we have a discovery routine, call it. Otherwise, we accept it.
			if (sg_sConsumers[u32Loop].DiscoverCallback)
			{
				// Go see if the client
				SerialLog("%s: Calling discovery for %s(%u)\n", __FUNCTION__, sg_sConsumers[u32Loop].peFunction, sg_sConsumers[u32Loop].u32LineNumber);
				bDiscoveryOK = sg_sConsumers[u32Loop].DiscoverCallback((EDCSerialHandle) (u32Loop + 1));
				SerialLog("%s: Discovery result - %u\n", __FUNCTION__, bDiscoveryOK);
			}

			// If we've got the port, then transfer things
			if (bDiscoveryOK)
			{
				sg_sConsumers[u32Loop].eState = ECONSTATE_CONNECTED;

				// If we have an "open" callback, call it
				if (sg_sConsumers[u32Loop].OpenCallback)
				{
					sg_sConsumers[u32Loop].OpenCallback((EDCSerialHandle) (u32Loop + 1));
				}

				Syslog("%s: Connected - handle %u - '%s'\n", __FUNCTION__, u32Loop + 1, psCOMPort->eCOMDeviceName);

				eStatus = ESTATUS_OK;
				goto errorExit;
			}

			SerialLog("%s: Canceling '%s' probe\n", __FUNCTION__, psCOMPort->eCOMDeviceName);

			// Cancel any active serial IO so we don't pass it on to the next consumer
			(void) tcflush( hSerialHandle, TCIOFLUSH );

			// Now wait for the thread to finish/clean up

			SerialLog("%s: Waiting for listen thread cleanup flag\n", __FUNCTION__);

			// Wait for something to do
			u64Flags = 0;
			(void) OSEventFlagGet(sg_sConsumers[u32Loop].psShutdownEventFlag,
								  0x01,
								  OS_WAIT_INDEFINITE,
								  &u64Flags);

			SerialLog("%s: Cleanup flag obtained\n", __FUNCTION__);
		}
	}

errorExit:
	return(eStatus);
}

// COM port event flags
static SOSEventFlag *sg_psCOMEvent;

// How often do we fire the COM scan timer
#define	COM_SCAN_TIMER_MS_BASE		1000
#define	COM_SCAN_TIMER_MS_RANDOM	1500

// Periodic COM scan timer
static STimer *sg_psCOMScanTimer;

// Events for our COM port scanner
#define		SERIALEVENT_SCAN		(1 << 0)
#define		SERIALEVENT_EXIT		(1 << 1)

#define		SERIALEVENT_ALL	(SERIALEVENT_SCAN | SERIALEVENT_EXIT)

static void COMScanTimerCallback(STimer *psTimer,
								 void *pvCallbackValue)
{
	EStatus eStatus;

	eStatus = OSEventFlagSet(sg_psCOMEvent,
							 SERIALEVENT_SCAN);
	BASSERT(ESTATUS_OK == eStatus);
}

// This routine will constantly open/scan through the COM ports available on this 
// machine.
static void DCSerialProbeThread(void *pvData)
{
	EStatus eStatus = ESTATUS_OK;
	int s32SystemError = 0;
	SCOMPort *psCOMPortHead = NULL;
	SCOMPort *psCOMPort = NULL;

	SerialLog("%s: Started\n", __FUNCTION__);

	for (;;)
	{
		UINT64 u64Flags;

		// Wait for something to do
		u64Flags = 0;
		eStatus = OSEventFlagGet(sg_psCOMEvent,
								 SERIALEVENT_ALL,
								 OS_WAIT_INDEFINITE,
								 &u64Flags);
		ERR_GOTO();

		// Shall we exit?
		if (u64Flags & SERIALEVENT_EXIT)
		{
			Syslog("%s: Signaled an exit\n", __FUNCTION__);
			eStatus = ESTATUS_OK;
			break;
		}

		// Time to scan?
		if (u64Flags & SERIALEVENT_SCAN)
		{
			SCOMPort *psCOMPort;

			// Go get a list of ports to probe
			eStatus = DCSerialGetPortList(&psCOMPortHead);
			ERR_GOTO();

			// Run through all ports
			psCOMPort = psCOMPortHead;
			while (psCOMPort)
			{
				eStatus = DCSerialProbe(psCOMPort);
				if (eStatus != ESTATUS_OK)
				{
					Syslog("%s: DCSerialProbe of '%s' failed - %s\n", __FUNCTION__, psCOMPort->eCOMDeviceName, GetErrorText(eStatus));
				}

				psCOMPort = psCOMPort->psNextLink;
			}

			DCSerialPortListDeallocate(&psCOMPortHead);

			// Fire it off at a random interval
			eStatus = TimerSet(sg_psCOMScanTimer,
							   TRUE,
							   COM_SCAN_TIMER_MS_BASE + (SharedRandomNumber() % COM_SCAN_TIMER_MS_RANDOM),
							   0,
							   COMScanTimerCallback,
							   NULL);
			BASSERT(ESTATUS_OK == eStatus);
		}
	}

errorExit:
	SerialLog("%s: Shutting down\n", __FUNCTION__);

	if (eStatus != ESTATUS_OK)
	{
		Syslog("%s: eStatus=0x%.8x (%s)\n", __FUNCTION__, eStatus, GetErrorText(eStatus));
	}

	if (s32SystemError)
	{
		Syslog("%s: strerror() - (%d): %s\n", __FUNCTION__, s32SystemError, strerror(s32SystemError));
	}
}

EStatus DCSerialInit(void)
{
	EStatus eStatus;
	UINT32 u32Loop;

	// Init the consumer array
	memset((void *) sg_sConsumers, 0, sizeof(sg_sConsumers));

	// Init the components of the consumer array
	for (u32Loop = 0; u32Loop < MAX_CONSUMERS; u32Loop++)
	{
		eStatus = OSCriticalSectionCreate(&sg_sConsumers[u32Loop].sConsumerLock);
		ERR_GOTO();

		eStatus = OSEventFlagCreate(&sg_sConsumers[u32Loop].psShutdownEventFlag,
									0);
		ERR_GOTO();

		sg_sConsumers[u32Loop].eState = ECONSTATE_NOT_ALLOCATED;
		sg_sConsumers[u32Loop].hSerialHandle = INVALID_TERMIOS_HANDLE_VALUE; 
	}

	// Consumer allocation lock
	eStatus = OSCriticalSectionCreate(&sg_sConsumerAllocateLock);
	ERR_GOTO();

	// Create a COM monitor event flag
	eStatus = OSEventFlagCreate(&sg_psCOMEvent,
								0);
	ERR_GOTO();

	// Create a periodic timer to wake up the COM port scanner
	eStatus = TimerCreate(&sg_psCOMScanTimer);
	ERR_GOTO();

	eStatus = TimerSet(sg_psCOMScanTimer, 
					   TRUE,
					   COM_SCAN_TIMER_MS_BASE,
					   COM_SCAN_TIMER_MS_BASE,
					   COMScanTimerCallback,
					   NULL);
	ERR_GOTO();

	// Create a serial probe thread to open/walk through ports for device discovery
	eStatus = OSThreadCreate("Serial probe",
							 NULL,
							 DCSerialProbeThread,
							 FALSE,
							 NULL,
							 0,
							 EOSPRIORITY_NORMAL);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

// Close a port
EStatus DCSerialClose(EDCSerialHandle eSerialHandle)
{
	EStatus eStatus;
	SConsumer *psConsumer;
	UINT64 u64Flags = 0;

	SerialLog("%s: Closing handle %u\n", __FUNCTION__, eSerialHandle);

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 TRUE,
								 FALSE);
	ERR_GOTO();

	// Flag the port for shutdown
	psConsumer->eState = ECONSTATE_SHUTDOWN;

	// Wait for something to do
	u64Flags = 0;
	eStatus = OSEventFlagGet(psConsumer->psShutdownEventFlag,
							 0x01,
							 SERIAL_SHUTDOWN_WAIT,
							 &u64Flags);


errorExit:
	SerialLog("%s: Closing handle %u - %s\n", __FUNCTION__, eSerialHandle, GetErrorText(eStatus));
	return(eStatus);
}

void DCSerialShutdown(void)
{
	EStatus eStatus;
	UINT32 u32Loop;

	SerialLog("%s: Serial shutdown called\n", __FUNCTION__);

	// Shut down the probe thread
	eStatus = OSEventFlagSet(sg_psCOMEvent,
							 SERIALEVENT_EXIT);
	BASSERT(ESTATUS_OK == eStatus);

	for (u32Loop = 0; u32Loop < MAX_CONSUMERS; u32Loop++)
	{
		(void) DCSerialClose((EDCSerialHandle) (u32Loop + 1));
		sg_sConsumers[u32Loop].DiscoverCallback = NULL;
		sg_sConsumers[u32Loop].OpenCallback = NULL;
		sg_sConsumers[u32Loop].CloseCallback = NULL;
		sg_sConsumers[u32Loop].ReceiveCallback = NULL;
		sg_sConsumers[u32Loop].eState = ECONSTATE_NOT_ALLOCATED;
	}

	SerialLog("%s: Serial shutdown complete\n", __FUNCTION__);
}

#endif

