#include <stdio.h>
#include "Shared/Shared.h"
#include <Setupapi.h>
#include "Shared/DCSerial.h"
#include "Shared/Timer.h"

// Serial timeouts
#define	SERIAL_WRITE_TIMEOUT_MS			1000
#define	SERIAL_READ_TIMEOUT_MS			1000
#define	SERIAL_SHUTDOWN_WAIT			5000

// Serial overlapped read buffers
#define	OVERLAPPED_READ_BUFFER_COUNT	5

// Enable serial logging
#define	SERIAL_LOG_ENABLE
//#define DCSERIAL_DEBUG

// Macros for having serial logging
#ifdef SERIAL_LOG_ENABLE
#define SerialLog(...)	Syslog(__VA_ARGS__)
#else
#define SerialLog(...)
#endif

#ifdef DCSERIAL_DEBUG
#define DCSerialDebug(...)	DebugOut(__VA_ARGS__)
#else
#define DCSerialDebug(...)
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
	bool bUnregisterPending;				// Has an unregister been requested?

	char eCOMDeviceName[50];				// The COM port's device name
	char eCOMHumanReadable[100];			// Human readable version of the name

	// Windows specific information
	HANDLE hSerialHandle;					// Windows' serial handle

	// Buffers for reads and writes
	OVERLAPPED sOverlappedWrite;			// For writes
	OVERLAPPED sOverlappedRead;				// For reads

	// Used to signal a shutdown
	SOSEventFlag *psShutdownEventFlag;		// Event flag when we've shut down

	// Who registered this?
	char *peFile;							// File containing the registrar
	char *peFunction;						// Function registering
	uint32_t u32LineNumber;					// And line number
								 
	// Methods
	bool (*DiscoverCallback)(EDCSerialHandle eSerialHandle);
	void (*OpenCallback)(EDCSerialHandle eSerialHandle);
	void (*CloseCallback)(EDCSerialHandle eSerialHandle);
	void (*ReceiveCallback)(EDCSerialHandle eSerialHandle,
							uint8_t *pu8Data,
							uint64_t u64BytesReceived);

} SConsumer;

// Array of consumers
static SConsumer sg_sConsumers[20];
#define	MAX_CONSUMERS		(sizeof(sg_sConsumers) / sizeof(sg_sConsumers[0]))

// Used when allocating a new consumer slot
static SOSCriticalSection sg_sConsumerAllocateLock;

// Convert a serial handle to a consumer and check its range
static EStatus DCHandleToConsumer(EDCSerialHandle eSerialHandle,
								  SConsumer **ppsConsumer,
								  bool bCheckConnected,
								  bool bLockConsumer)
{
	EStatus eStatus = ESTATUS_OK;
	bool bLocked = false;

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

		bLocked = true;
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
	uint32_t u32Loop;
	bool bLocked = false;

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
	bLocked = true;

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

// Set the serial port's baud rate
EStatus DCSerialSetBaudRate(EDCSerialHandle eSerialHandle,
							uint32_t u32BaudRate)
{
	EStatus eStatus;
	SConsumer *psConsumer = NULL;
	DCB sSerialParams;
	bool bLocked = false;

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 true,
								 true);
	ERR_GOTO();

	SerialLog("%s: Setting baud rate on '%s'-'%s' to %u\n", __FUNCTION__, psConsumer->eCOMDeviceName, psConsumer->eCOMHumanReadable, u32BaudRate);

	bLocked = true;
	ZERO_STRUCT(sSerialParams);
	sSerialParams.DCBlength = sizeof(sSerialParams);

	eStatus = ESTATUS_OK;
	if (GetCommState(psConsumer->hSerialHandle,
					 &sSerialParams))
	{
		// Got the parameters. If we're already at the same baud rate, don't do anything
		if (u32BaudRate != sSerialParams.BaudRate)
		{
			sSerialParams.BaudRate = u32BaudRate;
			
			// Set the baud rate
			if (SetCommState(psConsumer->hSerialHandle,
							 &sSerialParams))
			{
				eStatus = ESTATUS_OK;
			}
			else
			{
				// Error of some sort.
				SerialLog("%s: Failed to set baud rate on '%s'-'%s' to %u - 0x%.8x\n", __FUNCTION__, psConsumer->eCOMDeviceName, psConsumer->eCOMHumanReadable, u32BaudRate, GetLastError());
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
		SerialLog("%s: Failed to set baud rate on '%s'-'%s' to %u - 0x%.8x\n", __FUNCTION__, psConsumer->eCOMDeviceName, psConsumer->eCOMHumanReadable, u32BaudRate, GetLastError());
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

	if (eStatus != ESTATUS_OK)
	{
		SerialLog("%s: Failed to set baud rate on '%s'-'%s' to %u - %s\n", __FUNCTION__, psConsumer->eCOMDeviceName, psConsumer->eCOMHumanReadable, u32BaudRate, GetErrorText(eStatus));
	}

	return(eStatus);
}

// Register a consumer
EStatus DCSerialRegisterInternal(char *peFile,
								 char *peFunction,
								 uint32_t u32LineNumber,
								 bool (*DiscoverCallback)(EDCSerialHandle eSerialHandle),
								 void (*OpenCallback)(EDCSerialHandle eSerialHandle),
								 void (*CloseCallback)(EDCSerialHandle eSerialHandle),
								 void (*ReceiveCallback)(EDCSerialHandle eSerialHandle,
														 uint8_t *pu8Data,
														 uint64_t u64BytesReceived))
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

static EStatus DCSerialUnregisterInternal( SConsumer* psConsumer )
{
	EStatus eStatus = ESTATUS_OK;

	// Lock this record
	eStatus = OSCriticalSectionEnter(psConsumer->sConsumerLock);
	ERR_GOTO();

	// Found it!
	SerialLog("%s: Unregistering - %s:%s(%u)\n", __FUNCTION__, psConsumer->peFile, psConsumer->peFunction, psConsumer->u32LineNumber);

	// Clear out the methods
	psConsumer->DiscoverCallback = NULL;
	psConsumer->OpenCallback = NULL;
	psConsumer->CloseCallback = NULL;
	psConsumer->ReceiveCallback = NULL;

	// Ditch the registrar info
	psConsumer->peFile = NULL;
	psConsumer->peFunction = NULL;
	psConsumer->u32LineNumber = 0;

	// Clsoe the handle, set it invalid
	CloseHandle(psConsumer->hSerialHandle);
	psConsumer->hSerialHandle = INVALID_HANDLE_VALUE; 

	// Set the consumer state to unallocated
	psConsumer->eState = ECONSTATE_NOT_ALLOCATED;

	// Indicate that unregister should be done from the probe thread
	psConsumer->bUnregisterPending = false;

	// Unlock and move on
	eStatus = OSCriticalSectionLeave(psConsumer->sConsumerLock);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

// Unregister a consumer
EStatus DCSerialUnregister(bool (*DiscoverCallback)(EDCSerialHandle eSerialHandle),
						   void (*OpenCallback)(EDCSerialHandle eSerialHandle),
						   void (*CloseCallback)(EDCSerialHandle eSerialHandle),
						   void (*ReceiveCallback)(EDCSerialHandle eSerialHandle,
												   uint8_t *pu8Data,
												   uint64_t u64BytesReceived))
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32Loop;
	bool bUnregistered = false;

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
			SerialLog("%s: Requested Unregister - %s:%s(%u)\n", __FUNCTION__, sg_sConsumers[u32Loop].peFile, sg_sConsumers[u32Loop].peFunction, sg_sConsumers[u32Loop].u32LineNumber);

			sg_sConsumers[u32Loop].bUnregisterPending = true;

			bUnregistered = true;
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
					  uint8_t *pu8Data,
					  uint64_t u64BytesToWrite)
{
	EStatus eStatus;
	SConsumer *psConsumer = NULL;
	bool bLocked = false;
	DWORD u32DataWritten = 0;
	DWORD u32WindowsError = 0;
	bool bWriteSuccess = false;

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 false,
								 true);
	ERR_GOTO();
	bLocked = true;

	// If we're not allocated or undefined, flag it as an invalid handle
	if ((ECONSTATE_UNDEFINED == psConsumer->eState) ||
		(ECONSTATE_NOT_ALLOCATED == psConsumer->eState))
	{
		eStatus = ESTATUS_SERIAL_NOT_CONNECTED;
		goto errorExit;
	}

	// Clear out the overlapped write
	ZERO_STRUCT(psConsumer->sOverlappedWrite);
	psConsumer->sOverlappedWrite.hEvent = CreateEvent(NULL,
													  true,
													  false,
													  NULL);
	if (INVALID_HANDLE_VALUE == psConsumer->sOverlappedWrite.hEvent)
	{
		eStatus = ESTATUS_SERIAL_WRITE_EVENT_FAILED;
		goto errorExit;
	}

	{
		uint32_t u32Index = 0;
		while( u32Index < u64BytesToWrite )
		{
			DCSerialDebug("Sent: 0x%02x\n", pu8Data[u32Index]);
			u32Index++;
		}
	}

	// Go write the data
	bWriteSuccess = WriteFile(psConsumer->hSerialHandle,
							  pu8Data,
							  (DWORD) u64BytesToWrite,
							  &u32DataWritten,
							  &psConsumer->sOverlappedWrite);

	if (false == bWriteSuccess)
	{
		u32WindowsError = GetLastError();
		if (ERROR_IO_PENDING == u32WindowsError)
		{
			DWORD u32WaitResult;

			// Pending operation. Keep waiting.
			u32WindowsError = 0;
			u32WaitResult = WaitForSingleObject(psConsumer->sOverlappedWrite.hEvent,
												SERIAL_WRITE_TIMEOUT_MS);

			if (WAIT_OBJECT_0 == u32WaitResult)
			{
				// Got the data!
			}
			else
			if (WAIT_TIMEOUT == u32WaitResult)
			{
				// Timeout. That's OK. Just keep looping until we get something
				eStatus = ESTATUS_TIMEOUT;
				Syslog("%s: WaitForSingleObject result - timeout - exiting\n", __FUNCTION__);
				goto errorExit;
			}
			else
			{
				Syslog("%s: WaitForSingleObject result - 0x%.8x - exiting\n", __FUNCTION__, u32WaitResult);
				goto errorExit;
			}

			// Get the result of the overlapped operation
			if (GetOverlappedResult(psConsumer->hSerialHandle,
									&psConsumer->sOverlappedWrite,
									&u32DataWritten,
									false))
			{
				// Good to go!
			}
			else
			{
				u32WindowsError = GetLastError();

				// Some other error
				Syslog("%s: Windows error 2 - 0x%.8x - exiting\n", __FUNCTION__, u32WindowsError);
				goto errorExit;
			}
				
		}
		else
		{
			Syslog("%s: Windows error 3 - 0x%.8x - exiting\n", __FUNCTION__, u32WindowsError);
			goto errorExit;
		}
	}
	else
	{
		// Success!
	}

	// If everything went OK, then check to see if we wrote what we thought we did,
	// and if not, return an error.
	if (ESTATUS_OK == eStatus)
	{
		if (u64BytesToWrite != u32DataWritten)
		{
			eStatus = ESTATUS_WRITE_TRUNCATED;
		}
	}

errorExit:
	if (psConsumer)
	{
		// Close the overlapped event handle value

	}

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
	EStatus eStatus;
	SConsumer *psConsumer = NULL;
	bool bLocked = false;

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 true,
								 true);
	ERR_GOTO();
	bLocked = true;

	if (false == FlushFileBuffers(psConsumer->hSerialHandle))
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

// Linked list of ports to look at
typedef struct SCOMPort
{
	char eCOMDeviceName[50];				// The COM port's device name
	char eCOMHumanReadable[100];			// Human readable version of the name

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
	EStatus eStatus;
	SCOMPort *psCOMPortPtr = NULL;
	int32_t s32COMEnum = 0;
	GUID *psGUIDs = NULL;
	DWORD u32GUIDCount = 0;
	HANDLE hCOMEnumerator = INVALID_HANDLE_VALUE;

	// Discovery attempt algorithm derived from these web sites
	//
	// https://blog.csdn.net/u013606170/article/details/46903713
	// http://edge.rit.edu/content/P10551/public/Source%20code/FabAtHome%20Source%20Code/enumser.cpp
	// https://www.bbsmax.com/A/VGzlyEvyJb/
	// http://www.naughter.com/enumser.html
	// https://aticleworld.com/serial-port-programming-using-win32-api/

	// Make sure we're not handed an allocated list
	BASSERT(NULL == *ppsCOMPortHead);

	// Figure out how many COM port GUIDs we have
	if (false == SetupDiClassGuidsFromName("Ports",
										   NULL,
										   0,
										   &u32GUIDCount))
	{
		if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
		{
			// All good. We are just first getting a count so this error is expected
		}
		else
		{
			SyslogFunc("Failed SetupDiClassGuidsFromName 'Ports' - Error 0x%.8x\n", GetLastError());
			eStatus = ESTATUS_SERIAL_SETUP_INIT_FAILED;
			goto errorExit;
		}
	}

	if (0 == u32GUIDCount)
	{
		SyslogFunc("No GUIDs/COM ports\n");
		eStatus = ESTATUS_OK;
		goto errorExit;
	}

	MEMALLOC(psGUIDs, sizeof(*psGUIDs) * u32GUIDCount);

	// Now we get all of the GUIDs for ports
	if (false == SetupDiClassGuidsFromName("Ports", 
										   psGUIDs, 
										   u32GUIDCount, 
										   &u32GUIDCount))
	{
		SyslogFunc("Failed SetupDiClassGuidsFromName 'Ports' GUIDs call - Error 0x%.8x\n", GetLastError());
		eStatus = ESTATUS_SERIAL_SETUP_INIT_FAILED;
		goto errorExit;
	}

	// Now set up an enumerator
	hCOMEnumerator = SetupDiGetClassDevs(psGUIDs, 
										 NULL, 
										 NULL, 
										 DIGCF_PRESENT);
	if (INVALID_HANDLE_VALUE == hCOMEnumerator)
	{
		SyslogFunc("SetupDiGetClassDevs() failed - Error 0x%.8x\n", GetLastError());
		eStatus = ESTATUS_SERIAL_SETUP_INIT_FAILED;
		goto errorExit;
	}

	for (;;)
	{
		SP_DEVINFO_DATA sCOMPort;

		ZERO_STRUCT(sCOMPort);
		sCOMPort.cbSize = sizeof(sCOMPort);

		if (SetupDiEnumDeviceInfo(hCOMEnumerator,
								  s32COMEnum,
								  &sCOMPort))
		{
			HKEY hCOMKey;
			char eCOMDeviceName[50];
			DWORD u32Size;
			DWORD u32Type;

			// We've got a port!
			hCOMKey = SetupDiOpenDevRegKey(hCOMEnumerator, 
										   &sCOMPort, 
										   DICS_FLAG_GLOBAL, 
										   0, 
										   DIREG_DEV, 
										   KEY_QUERY_VALUE);
			if (INVALID_HANDLE_VALUE == hCOMKey)
			{
				SyslogFunc("SetupDiOpenDevRegKey() failed - 0x%.8x\n", GetLastError());
				eStatus = ESTATUS_SERIAL_SETUP_INIT_FAILED;
				goto errorExit;
			}

			u32Size = sizeof(eCOMDeviceName);
			memset((void *) eCOMDeviceName, 0, sizeof(eCOMDeviceName));

			// Get the COM port name
			if (ERROR_SUCCESS == RegQueryValueEx(hCOMKey, 
												 "PortName", 
												 NULL, 
												 &u32Type, 
												 (LPBYTE) eCOMDeviceName, 
												 &u32Size))
			{
				// Must be a string
				if (REG_SZ == u32Type)
				{
					// First 3 letters need to be COM
					if (_strnicmp(eCOMDeviceName, "COM", 3) == 0)
					{
						uint32_t u32Loop;
						uint32_t u32PortNumber;

						// Yay! It's a COM port! 
						u32PortNumber = atol(&eCOMDeviceName[3]);

						// For COM10 and higher, there's a hack that needs to be done for Windows to open
						// the device name
						if (u32PortNumber >= 10)
						{
							snprintf(eCOMDeviceName, sizeof(eCOMDeviceName) - 1, "\\\\.\\COM%u", u32PortNumber);
						}

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
								if (_stricmp(sg_sConsumers[u32Loop].eCOMDeviceName,
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

							// Now get the human readable name
							u32Size = sizeof(psCOMPortPtr->eCOMHumanReadable);
							if (SetupDiGetDeviceRegistryProperty(hCOMEnumerator,
																 &sCOMPort,
																 SPDRP_DEVICEDESC,
																 &u32Type,
																 (PBYTE) psCOMPortPtr->eCOMHumanReadable,
																 u32Size,
																 &u32Size))
							{
								// We got the name! Maybe...
								if (REG_SZ == u32Type)
								{
									// It's good - it's an ASCII string so leave it as-is
								}
								else
								{
									// It's not ASCII, 
									psCOMPortPtr->eCOMHumanReadable[0] = '\0';
								}
							}
							else
							{
								// We didn't get the human readable name!
								SyslogFunc("Failed to get human readable COM port name - 0x%.8x\n", GetLastError());
							}

							//SerialLog("%s:  Discovered '%s'-'%s'\n", __FUNCTION__, psCOMPortPtr->eCOMDeviceName, psCOMPortPtr->eCOMHumanReadable);
						}
						else
						{
							// Someone is already using this port
						}
					}
				}
			}

		}
		else
		{
			// No more
			break;
		}

		// Next COM index
		s32COMEnum++;
	}

	// Got our list!
	eStatus = ESTATUS_OK;

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		SerialLog("%s: Complete - %s\n", __FUNCTION__, GetErrorText(eStatus));
	}

	(void) SetupDiDestroyDeviceInfoList(hCOMEnumerator);
	SafeMemFree(psGUIDs);
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

	// Create overlapped read buffers
	psConsumer->sOverlappedRead.hEvent = CreateEvent(NULL,
													 true,
													 false,
													 NULL);

	BASSERT(psConsumer->sOverlappedRead.hEvent != INVALID_HANDLE_VALUE);

	// Loop forever 
	while (psConsumer->eState != ECONSTATE_SHUTDOWN)
	{
		uint32_t u32BytesReceived;
		uint8_t u8DataBuffer;
		bool bResult;
		DWORD u32WindowsError;

		// Go get some data
		u32BytesReceived = 0;
		bResult = ReadFile(psConsumer->hSerialHandle,
						   &u8DataBuffer,
						   sizeof(u8DataBuffer),
						   &u32BytesReceived,
						   &psConsumer->sOverlappedRead);

		if (false == bResult)
		{
			u32WindowsError = GetLastError();
			if (ERROR_IO_PENDING == u32WindowsError)
			{
				DWORD u32WaitResult;

				// Pending operation. Keep waiting.
				u32WindowsError = 0;
				u32WaitResult = WaitForSingleObject(psConsumer->sOverlappedRead.hEvent,
													SERIAL_READ_TIMEOUT_MS);

				if (WAIT_OBJECT_0 == u32WaitResult)
				{
					// Got the data!
				}
				else
				if (WAIT_TIMEOUT == u32WaitResult)
				{
					// Timeout. That's OK. Just keep looping until we get something
				}
				else
				{
					Syslog("%s: WaitForSingleObject result - 0x%.8x - exiting\n", __FUNCTION__, u32WaitResult);
					goto errorExit;
				}

				// Get the result of the overlapped operation
				if (GetOverlappedResult(psConsumer->hSerialHandle,
										&psConsumer->sOverlappedRead,
										&u32BytesReceived,
										false))
				{
					// Good to go!
				}
				else
				{
					u32WindowsError = GetLastError();
					if (ERROR_IO_INCOMPLETE  == u32WindowsError)
					{
						// Incomplete is OK
					}
					else
					{
						// Some other error
						Syslog("%s: Windows error 2 - 0x%.8x - exiting\n", __FUNCTION__, u32WindowsError);
						goto errorExit;
					}
				}
				
			}
			else
			{
				Syslog("%s: Windows error 3 - 0x%.8x - exiting\n", __FUNCTION__, u32WindowsError);
				goto errorExit;
			}
		}
		else
		{
			// Success!
		}

		// If we received data, send it back to the caller
		if (u32BytesReceived)
		{
			if (psConsumer->ReceiveCallback && (false == psConsumer->bUnregisterPending))
			{
				psConsumer->ReceiveCallback(eSerialHandle,
											&u8DataBuffer,
											u32BytesReceived);
				DCSerialDebug("Received: 0x%02x\n", u8DataBuffer);
			}
		}
	}

	SerialLog("%s: Closing due to ECONSTATE_SHUTDOWN == psConsumer->eState\n", __FUNCTION__);

errorExit:
	SerialLog("%s: Closing '%s'-'%s'\n", __FUNCTION__, psConsumer->eCOMDeviceName, psConsumer->eCOMHumanReadable);

	// If we have a "close" callback, then call it
	if (psConsumer->CloseCallback)
	{
		psConsumer->CloseCallback(eSerialHandle);
	}

	// Get rid of all overlapped read events
	CloseHandle(psConsumer->sOverlappedRead.hEvent);
	psConsumer->sOverlappedRead.hEvent = INVALID_HANDLE_VALUE;

	// Close the handle
	CloseHandle(psConsumer->hSerialHandle);
	psConsumer->hSerialHandle = INVALID_HANDLE_VALUE;

	// Clear out the name of the ports
	psConsumer->eCOMDeviceName[0] = '\0';
	psConsumer->eCOMHumanReadable[0] = '\0';

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
	uint32_t u32Loop;
	DCB sSerialParams;
	COMMTIMEOUTS sSerialTimeout;
	HANDLE hSerialHandle = INVALID_HANDLE_VALUE;

	// Run through all slots and find any consumers that are in probe state
	for (u32Loop = 0; u32Loop < MAX_CONSUMERS; u32Loop++)
	{
		// If a consumer wants to unregister, do that now
		if( sg_sConsumers[u32Loop].bUnregisterPending )
		{
			if( ECONSTATE_CONNECTED == sg_sConsumers[u32Loop].eState )
			{
				uint64_t u64Flags;

				sg_sConsumers[u32Loop].eState = ECONSTATE_SHUTDOWN;

				// Cancel any active serial IO so we don't pass it on to the next consumer
				(void) CancelIoEx(sg_sConsumers[u32Loop].hSerialHandle, 
								  NULL);

				// Now wait for the thread to finish/clean up

				SerialLog("%s: Waiting for listen thread unregister cleanup flag\n", __FUNCTION__);

				// Wait for something to do
				u64Flags = 0;
				(void) OSEventFlagGet(sg_sConsumers[u32Loop].psShutdownEventFlag,
									  0x01,
									  OS_WAIT_INDEFINITE,
									  &u64Flags);

				SerialLog("%s: Unregister cleanup flag obtained\n", __FUNCTION__);
			}

			DCSerialUnregisterInternal( &sg_sConsumers[u32Loop] );
		}

		// Probe those in need
		if (ECONSTATE_PROBE == sg_sConsumers[u32Loop].eState)
		{
			bool bDiscoveryOK = false;
			uint64_t u64Flags;

			ZERO_STRUCT(sSerialParams);
			ZERO_STRUCT(sSerialTimeout);

			// Open up the port for use by the consumer
			hSerialHandle = CreateFile(psCOMPort->eCOMDeviceName,
									   GENERIC_READ | GENERIC_WRITE,
									   0,
									   0,
									   OPEN_EXISTING,
									   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
									   0);
			if (INVALID_HANDLE_VALUE == hSerialHandle)
			{
				if (GetLastError() != ERROR_ACCESS_DENIED)
				{
					Syslog("%s: Failed to open '%s'-'%s' - GetLastError() = 0x%.8x\n", __FUNCTION__, psCOMPort->eCOMDeviceName, psCOMPort->eCOMHumanReadable, GetLastError());
				}
				else
				{
					// Ignore access denied errors (those can happen naturally)
				}

				goto errorExit;
			}

			// Port is opened! Let's set up some reasonable defaults
			if (GetCommState(hSerialHandle,
							 &sSerialParams))
			{
				// All good
			}
			else
			{
				Syslog("%s: Failed to GetCommState() - 0x%.8x\n", __FUNCTION__, GetLastError());
				goto errorExit;
			}

			// Set serial port defaults
			sSerialParams.BaudRate = CBR_115200;
			sSerialParams.ByteSize = 8;
			sSerialParams.fAbortOnError = false;
			sSerialParams.fBinary = true;
			sSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
			sSerialParams.fDsrSensitivity = false;
			sSerialParams.fErrorChar = false;
			sSerialParams.fInX = false;
			sSerialParams.fOutxCtsFlow = false;
			sSerialParams.fOutxDsrFlow = false;
			sSerialParams.fOutX = false;
			sSerialParams.fParity = false;
			sSerialParams.fNull = false;
			sSerialParams.fRtsControl = RTS_CONTROL_ENABLE;
			sSerialParams.fTXContinueOnXoff = false;
			sSerialParams.Parity = NOPARITY;
			sSerialParams.StopBits = ONESTOPBIT;

			// Now set the params
			if (SetCommState(hSerialHandle,
							 &sSerialParams))
			{
				// All good
			}
			else
			{
				Syslog("%s: Failed to SetCommState() - 0x%.8x\n", __FUNCTION__, GetLastError());
				goto errorExit;
			}

			// Write timeout
			sSerialTimeout.WriteTotalTimeoutMultiplier = 0;
			sSerialTimeout.WriteTotalTimeoutConstant = SERIAL_WRITE_TIMEOUT_MS;

			// Read timeout
			sSerialTimeout.ReadIntervalTimeout = MAXDWORD;
			sSerialTimeout.ReadTotalTimeoutMultiplier = MAXDWORD;
			sSerialTimeout.ReadTotalTimeoutConstant = SERIAL_READ_TIMEOUT_MS;

			// Now set the port's timeouts
			if (SetCommTimeouts(hSerialHandle,
								&sSerialTimeout))
			{
				// All good
			}
			else
			{
				Syslog("%s: Failed to SetCommTimeouts() - 0x%.8x\n", __FUNCTION__, GetLastError());
				goto errorExit;
			}

			// Copy in serial port information to be used by the consumer's discovery process
			strncpy(sg_sConsumers[u32Loop].eCOMDeviceName, psCOMPort->eCOMDeviceName, sizeof(sg_sConsumers[u32Loop].eCOMDeviceName) - 1);
			strncpy(sg_sConsumers[u32Loop].eCOMHumanReadable, psCOMPort->eCOMHumanReadable, sizeof(sg_sConsumers[u32Loop].eCOMHumanReadable) - 1);
			sg_sConsumers[u32Loop].hSerialHandle = hSerialHandle;

			SerialLog("%s: Probing - '%s'-'%s'\n", __FUNCTION__, sg_sConsumers[u32Loop].eCOMDeviceName, sg_sConsumers[u32Loop].eCOMHumanReadable);

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
									 false,
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

				Syslog("%s: Connected - handle %u - '%s'-'%s'\n", __FUNCTION__, u32Loop + 1, psCOMPort->eCOMDeviceName, psCOMPort->eCOMHumanReadable);

				eStatus = ESTATUS_OK;
				goto errorExit;
			}
			else
			{
				// Flag the port for shutdown
				sg_sConsumers[u32Loop].eState = ECONSTATE_SHUTDOWN;
			}

			SerialLog("%s: Canceling '%s'-'%s' probe\n", __FUNCTION__, psCOMPort->eCOMDeviceName, psCOMPort->eCOMHumanReadable);

			// Cancel any active serial IO so we don't pass it on to the next consumer
			(void) CancelIoEx(hSerialHandle, 
							  NULL);

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
	DWORD u32WindowsError = 0;
	EStatus eStatus = ESTATUS_OK;
	SCOMPort *psCOMPortHead = NULL;
	SCOMPort *psCOMPort = NULL;

	SerialLog("%s: Started\n", __FUNCTION__);

	for (;;)
	{
		uint64_t u64Flags;

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
					Syslog("%s: DCSerialProbe of '%s'-'%s' failed - %s\n", __FUNCTION__, psCOMPort->eCOMDeviceName, psCOMPort->eCOMHumanReadable, GetErrorText(eStatus));
				}

				psCOMPort = psCOMPort->psNextLink;
			}

			DCSerialPortListDeallocate(&psCOMPortHead);

			// Fire it off at a random interval 
			eStatus = TimerSet(sg_psCOMScanTimer, 
							   true,
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

	if (u32WindowsError)
	{
		Syslog("%s: GetLastError() - 0x%.8x\n", __FUNCTION__, u32WindowsError);
	}
}

EStatus DCSerialInit(void)
{
	EStatus eStatus;
	uint32_t u32Loop;

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
		sg_sConsumers[u32Loop].hSerialHandle = INVALID_HANDLE_VALUE; 
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
					   true,
					   COM_SCAN_TIMER_MS_BASE,
					   COM_SCAN_TIMER_MS_BASE,
					   COMScanTimerCallback,
					   NULL);
	ERR_GOTO();

	// Create a serial probe thread to open/walk through ports for device discovery
	eStatus = OSThreadCreate("Serial probe",
							 NULL,
							 DCSerialProbeThread,
							 false,
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
	uint64_t u64Flags = 0;

	SerialLog("%s: Closing handle %u\n", __FUNCTION__, eSerialHandle);

	eStatus = DCHandleToConsumer(eSerialHandle,
								 &psConsumer,
								 true,
								 false);
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
	uint32_t u32Loop;

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
