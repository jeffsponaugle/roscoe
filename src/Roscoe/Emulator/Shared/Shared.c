#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "Shared/Shared.h"
#include "../Arch/Arch.h"
#include "Platform/Platform.h"
#include "Shared/SysEvent.h"
#include "Shared/zlib/zlib.h"
#include "Shared/SharedMisc.h"
#include "Shared/Version.h"
#include "Shared/CmdLine.h"
#include "Shared/HandlePool.h"

#define PRINT_BUFFER_SIZE (32768)

// Indicates whether or not the debug console has been initialized
static bool sg_bDebugConsoleInitialized = false;
static SOSCriticalSection sg_sDebugConsoleCriticalSection;

// Syslog logger
static SLog *sg_psSyslog;

// This is a test structure to ensure that structure packing is working properly
PACKED_BEGIN
typedef PACKED_STRUCT_START struct PACKED_STRUCT SPackStructTest
{
	uint8_t u8Dummy1;
	uint16_t u16Dummy2;
	uint32_t u32Dummy3;
} PACKED_STRUCT_STOP SPackStructTest;
PACKED_END

// This will test out various expectations of the system, such as expected
// UINTxxx sizes, etc... Note that one cannot BASSERT() nor do anything that
// relies on initialzation since it's called right after program start.
// If faults occur, spin lock so they can be caught in a debugger.
void SharedBasicCheck(void)
{
	// Ensure all basic types/sizes are working
	if (sizeof(uint8_t) != 1)
	{
		// uint8_t is not 1 byte
		for (;;)
		{
		}
	}

	if (sizeof(int8_t) != 1)
	{
		// int8_t is not 1 byte
		for (;;)
		{
		}
	}

	if (sizeof(uint16_t) != 2)
	{
		// uint16_t is not 2 bytes
		for (;;)
		{
		}
	}

	if (sizeof(int16_t) != 2)
	{
		// int16_t is not 2 bytes
		for (;;)
		{
		}
	}

	if (sizeof(uint32_t) != 4)
	{
		// uint32_t is not 4 bytes
		for (;;)
		{
		}
	}

	if (sizeof(int32_t) != 4)
	{
		// int32_t is not 4 bytes
		for (;;)
		{
		}
	}

	if (sizeof(uint64_t) != 8)
	{
		// uint64_t is not 8 bytes
		for (;;)
		{
		}
	}

	if (sizeof(int64_t) != 8)
	{
		// int64_t is not 8 bytes
		for (;;)
		{
		}
	}

	// Ensure structure packing works
	if (sizeof(SPackStructTest) != 7)
	{
		// Structure packing isn't working
		for (;;)
		{
		}
	}
}

// Called once to initialize the debug console
void DebugConsoleInit(void)
{
	EStatus eStatus;

	// Go see if the platform supports a console output
	if (PlatformDebugConsoleInit() != ESTATUS_OK)
	{
		// Platform doesn't support it
		return;
	}
	
	// Init the debug out semaphore
	eStatus = OSCriticalSectionCreate(&sg_sDebugConsoleCriticalSection);
	BASSERT(ESTATUS_OK == eStatus);

	// Ready to start dumping output
	sg_bDebugConsoleInitialized = true;
}

// When we get data in from the debug console - if any
void DebugConsoleIn(uint8_t *pu8ConsoleData,
			 		uint32_t u32Length)
{
	// Does nothing currently
}

// DebugOut console buffer. Globally protected by a semaphore.
static char sg_eDebugOutBufferForeground[16384];

// Used only by trace code - do not use elsewhere!
void DebugOutSetLock(bool bLock)
{
	if (bLock)
	{
		OSCriticalSectionEnter(sg_sDebugConsoleCriticalSection);
	}
	else
	{
		OSCriticalSectionLeave(sg_sDebugConsoleCriticalSection);
	}
}

void DebugOutInternal(bool bSkipTime,
					  const char *peProcedureName,
					  const char *pu8Format,
					  ...)
{
	va_list ap;
	char *peMessage;

	// Bail out if we haven't initialized the debug console
	if (false == sg_bDebugConsoleInitialized)
	{
		return;
	}

	va_start(ap, pu8Format);

	// Lock the debug output
	DebugOutSetLock(true);

#ifdef _RELEASE
	bSkipTime = true;
#endif

	if (false == bSkipTime)
	{
		SharedTimeToText(sg_eDebugOutBufferForeground,
						 sizeof(sg_eDebugOutBufferForeground),
						 RTCGet());
		strcat(sg_eDebugOutBufferForeground, " ");
	}
	else
	{
		sg_eDebugOutBufferForeground[0] = '\0';
	}

	if (NULL == peProcedureName)
	{
		vsnprintf(sg_eDebugOutBufferForeground + strlen(sg_eDebugOutBufferForeground), sizeof(sg_eDebugOutBufferForeground) - 1, (const char *) pu8Format, ap);
	}
	else
	{
		uint32_t u32Len;

		// Prefix the procedure name
		strcat(sg_eDebugOutBufferForeground, peProcedureName);
		strcat(sg_eDebugOutBufferForeground, ":");

		u32Len = (uint32_t) strlen(sg_eDebugOutBufferForeground);
		vsnprintf(&sg_eDebugOutBufferForeground[u32Len], sizeof(sg_eDebugOutBufferForeground) - 1 - u32Len, (const char *) pu8Format, ap);

	}

	peMessage = sg_eDebugOutBufferForeground;

	va_end(ap);
#ifdef _WIN32
	while (*peMessage)
	{
		char *peTerminator;
		EStatus eStatus;
		
		peTerminator = peMessage;
		
		// Scan until we get to the end of the string or we hit a linefeed,
		// as we will automatically add a carriage return to the output stream
		while ((*peTerminator) &&
				(*peTerminator != '\n'))
		{
			++peTerminator;
		}
		
		// Send the rest of the string to the end of the string or the linefeed
		if (peTerminator != peMessage)
		{
			eStatus = PlatformDebugOut((uint8_t *) peMessage,
									   ((uint32_t) peTerminator - (uint32_t) peMessage));
			
			DebugOutFlush();
			ERR_GOTO();
		}
		
		// Move it!
		peMessage = peTerminator;
		
		// If it's a linefeed, send cr/lf
		if (*peTerminator)
		{
			// Send CR/LF sequence
			eStatus = PlatformDebugOut((uint8_t *) "\r\n",
									   2);
			DebugOutFlush();
			ERR_GOTO();
			
			++peMessage;
		}
	}
#else
	PlatformDebugOut((uint8_t *) peMessage,
					 strlen(peMessage));
#endif

errorExit:
	// Unlock the debug output
	DebugOutSetLock(false);
}


void DebugOutFlush(void)
{
	if (false == sg_bDebugConsoleInitialized)
	{
		return;
	}
	
	PlatformFlushConsole();
}

// # Of bytes per line during a hex dump
#define	HEX_DUMP_LENGTH		16

// Dump some hex data
void DebugOutHex(uint8_t *pu8Buffer,
				 uint64_t u64Length)
{
	char eDumpBuffer[(sizeof(pu8Buffer) << 1) + 2 + (HEX_DUMP_LENGTH * 3) + 2 + 1];

	while (u64Length)
	{
		uint8_t u8BytesRemaining;
		uint32_t u32Offset;

		memset((void *) eDumpBuffer, 0, sizeof(eDumpBuffer));
		if (u64Length > HEX_DUMP_LENGTH)
		{
			u8BytesRemaining = HEX_DUMP_LENGTH;
		}
		else
		{
			u8BytesRemaining = (uint8_t) u64Length;
		}

		u64Length -= u8BytesRemaining;
		sprintf(eDumpBuffer, "%.16lx: ", (uint64_t) pu8Buffer);

		u32Offset = (uint32_t) strlen(eDumpBuffer);

		while (u8BytesRemaining)
		{
			sprintf(&eDumpBuffer[u32Offset], "%.2x ", *pu8Buffer);
			++pu8Buffer;
			--u8BytesRemaining;

			// Two hex digits and a space
			u32Offset += 3;
		}

		strcat(eDumpBuffer, "\n");
		DebugOut("%s", eDumpBuffer);
	}
}

static const char *sg_peMonths[] =
{
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

SLog *SyslogGetLog(void)
{
	return(sg_psSyslog);
}

EStatus SyslogInit(char *peBaseFilename,
				   uint32_t u32HistoryDepth,
				   bool bConsoleOutput)
{
	EStatus eStatus;

	// If this asserts, it means it was called twice - shouldn't happen
	BASSERT(NULL == sg_psSyslog);

	eStatus = LoggerOpen(peBaseFilename,
						 u32HistoryDepth,
						 bConsoleOutput,
						 &sg_psSyslog);
	ERR_GOTO();

#ifndef _RELEASE
	Syslog("Operational Syslog started - syslog file turned over\r\n");
#endif

errorExit:
	return(eStatus);
}

void SyslogShutdown(void)
{
	if (sg_psSyslog)
	{
		LoggerClose(sg_psSyslog);
		sg_psSyslog = NULL;
	}
}

static const SStatusCodes sg_sStatusCodes[] =
{
	{ESTATUS_OK,									"OK"},

	// Filesystem errors - THESE MUST BE IN THE SAME ORDER AS ARE IN ff.h AND START +1 FROM {ESTATUS_OK!
	{ESTATUS_DISK_ERR,								"Disk error"},
	{ESTATUS_INT_ERR,								"Filesystem internal error"},
	{ESTATUS_NOT_READY,								"SD Card not ready"},
	{ESTATUS_NO_FILE,								"File not found"},
	{ESTATUS_NO_PATH,								"Path not found"},
	{ESTATUS_INVALID_NAME,							"Invalid filename"},
	{ESTATUS_DENIED,								"Access denied (likely because directory not empty)"},
	{ESTATUS_EXIST,									"File exists"},
	{ESTATUS_INVALID_OBJECT,						"Invalid object"},
	{ESTATUS_WRITE_PROTECTED,						"SD Card is write protected"},
	{ESTATUS_INVALID_DRIVE,							"Invalid drive"},
	{ESTATUS_NOT_ENABLED,							"Not enabled"},
	{ESTATUS_NO_FILESYSTEM,							"No filesystem"},
	{ESTATUS_MKFS_ABORTED,							"MKFS Aborted"},
	{ESTATUS_TIMEOUT,								"Timeout"},
	{ESTATUS_LOCKED,								"File locked"},
	{ESTATUS_NOT_ENOUGH_CORE,						"Not enough core"},
	{ESTATUS_TOO_MANY_OPEN_FILES,					"Too many open files"},
	{ESTATUS_INVALID_PARAMETER,						"Invalid filesystem parameter"},
	{ESTATUS_INVALID_FILE_MODE,						"Invalid file mode"},
	{ESTATUS_SEEK_ORIGIN_INVALID,					"Seek origin invalid"},
	{ESTATUS_FILE_INVALID_ATTRIBUTES,				"Invalid file attributes"},
	{ESTATUS_FILEFIND_ALREADY_ALLOCATED,			"FileFind already allocated"},
	{ESTATUS_UNKNOWN_FRESULT,						"Filesystem unknown result"},
	{ESTATUS_FUNCTION_NOT_SUPPORTED,				"Function not supported"},
	{ESTATUS_READ_TRUNCATED,						"File read truncated"},
	{ESTATUS_WRITE_TRUNCATED,						"File write truncated"},

	// End filesystem errors
	{ESTATUS_INVALID_CRYPT_MODE,					"Invalid cryptography action for file mode"},
	{ESTATUS_FILE_HANDLE_INVALID,					"Invalid file handle"},
	{ESTATUS_DISK_FULL,								"Disk full"},
	{ESTATUS_FILE_FULL,								"File exceeds maximum filesystem size"},
	{ESTATUS_DATA_MISCOMPARE,						"Data miscompare"},

	// Heap manager errors
	{ESTATUS_OUT_OF_MEMORY,							"Out of memory (local)"},

	// Serial port related
	{ESTATUS_SERIAL_INVALID_HANDLE,					"Invalid serial handle"},
	{ESTATUS_SERIAL_CONSUMER_SLOTS_FULL,			"Serial consumer slots full"},
	{ESTATUS_SERIAL_CONSUMER_NOT_FOUND,				"Serial consumer not found"},
	{ESTATUS_SERIAL_NOT_CONNECTED,					"Consumer not connected to serial port"},
	{ESTATUS_SERIAL_SET_BAUDRATE_INVALID,			"Setting baud rate failed"},
	{ESTATUS_SERIAL_FLUSH_FAULT,					"Fault flushing the serial port"},
	{ESTATUS_SERIAL_WRITE_EVENT_FAILED,				"Failed to create write event"},
	{ESTATUS_SERIAL_WAIT_FAULT,						"Serial write fault"},
	{ESTATUS_SERIAL_SETUP_INIT_FAILED,				"Init setup DLL failed"},

	// OS Errors
	{ESTATUS_OS_NOT_RUNNING,						"Operating system not running"},
	{ESTATUS_OS_NOT_ALLOWED_FROM_ISR,				"Function not allowed from ISR"},
	{ESTATUS_OS_SEMAPHORE_GET_FAILED,				"Semaphore get failed"},
	{ESTATUS_OS_UNKNOWN_ERROR,						"Unknown operating system error"},
	{ESTATUS_OS_SEMAPHORE_CANT_PUT,					"Semaphore put failed"},
	{ESTATUS_OS_SEMAPHORE_PUT_COUNT_NOT_SUPPORTED,	"Semaphore put count not supported"},
	{ESTATUS_OS_QUEUE_CANT_CLEAR,					"Can't clear queue"},
	{ESTATUS_OS_THREAD_PRIORITY_INVALID,			"Invalid thread priority"},
	{ESTATUS_OS_ISR_CANNOT_BLOCK,					"Cannot block in an ISR"},
	{ESTATUS_OS_SEMAPHORE_UNAVAILABLE,				"Semaphore unavailable"},
	{ESTATUS_OS_EVENT_FLAG_DELETED,					"Event flag deleted"},
	{ESTATUS_OS_CRITICAL_SECTION_CANT_CREATE,		"Can't create critical section"},
	{ESTATUS_OS_CRITICAL_SECTION_ATTRIBUTE_FAULT,	"Critical section attribute setting fault"},
	{ESTATUS_OS_SEMAPHORE_CANT_CREATE,				"Can't create semaphore"},
	{ESTATUS_OS_THREAD_CANT_CREATE,					"Can't create thread"},
	{ESTATUS_OS_QUEUE_FULL,							"OS Queue full"},
	{ESTATUS_OS_QUEUE_OVERFLOW,						"Queue overflowed"},
	{ESTATUS_OS_THREAD_DESTROY_FAILURE,				"Failed to destroy thread"},
	{ESTATUS_OS_SEMAPHORE_CANT_DESTROY,				"Can't destroy semaphore"},
	{ESTATUS_OS_NETWORK_INIT_FAILURE,				"Network layer init fault"},
	{ESTATUS_OS_END_OF_FILE,						"End of file"},

	// ZLib errors
	{ESTATUS_ZIP_FILE_INTEGRITY_PROBLEM,			"ZIP File integrity failed"},
	{ESTATUS_ZIP_INTERNAL_ERROR,					"ZIP Internal error"},
	{ESTATUS_ZIP_CHECK_CODE_FAILED,					"ZIP Check code failed"},
	{ESTATUS_ZIP_UNKNOWN_ERROR,						"Unknown ZIP error"},
	{ESTATUS_ZIP_END_OF_FILE,						"End of ZIP file"},
	{ESTATUS_ZIP_BAD_FILE,							"Bad ZIP file"},
	{ESTATUS_ZIP_PROFILE_PIN_INVALID,				"ZIP Profile pin invalid"},
	{ESTATUS_ZIP_BAD_LOAD_ADDRESS,					"Bad ZIP load adddress"},
	{ESTATUS_ZIP_WRITE_TRUNCATED,					"ZIP File write truncated"},
	{ESTATUS_ZIP_NO_MORE_FILES,						"ZIP No more files"},
	{ESTATUS_ZIP_INVALID_PARAMETER,					"Invalid ZIP parameter"},
	{ESTATUS_ZIP_CRC_MISMATCH,						"ZIP CRC Mismatch"},
	{ESTATUS_ZIP_NEED_DICT,							"ZIP File need dictionary"},
	{ESTATUS_ZIP_ERRNO,								"ZIP Filesystem error"},
	{ESTATUS_ZIP_STREAM_ERROR,						"ZIP Stream error"}, 
	{ESTATUS_ZIP_DATA_ERROR,						"ZIP Data error"},
	{ESTATUS_ZIP_BUFFER_ERROR,						"ZIP Buffer error"},
	{ESTATUS_ZIP_VERSION_ERROR,						"ZIP Version error"},

	// Syslog related
	{ESTATUS_SYSLOG_NOT_INITIALIZED,				"Syslog not initialized"},
	{ESTATUS_SYSLOG_NOT_FROM_ISR,					"Can't Syslog from ISR"},
	{ESTATUS_SYSLOG_DATA_TRUNCATED,					"Syslog data truncated"},

	// Errno related
	{ESTATUS_ERRNO_OPERATION_NOT_PERMITTED,			"Operation not permitted"},
	{ESTATUS_ERRNO_NO_SUCH_PROCESS,					"No such process"},
	{ESTATUS_ERRNO_INTERRUPTED_SYSTEM_CALL,			"Interrupted system call"},
	{ESTATUS_ERRNO_IO_ERROR,						"I/O Error"},
	{ESTATUS_ERRNO_DEVICE_NOT_CONFIGURED,			"Device not configured"},
	{ESTATUS_ERRNO_ARGUMENT_LIST_TOO_LONG,			"Argument list too long"},
	{ESTATUS_ERRNO_EXEC_FORMAT_ERROR,				"Exec format error"},
	{ESTATUS_ERRNO_BAD_FILE_DESCRIPTOR,				"Bad file descriptor"},
	{ESTATUS_ERRNO_NO_CHILD_PROCESSES,				"No child processes"},
	{ESTATUS_ERRNO_RESOURCE_DEADLOCK_AVOIDED,		"Resource deadlock avoided"},
	{ESTATUS_ERRNO_PERMISSION_DENIED,				"Permission denied"},
	{ESTATUS_ERRNO_BAD_ADDRESS,						"Bad address"},
	{ESTATUS_ERRNO_BLOCK_DEVICE_REQUIRED,			"Block device required"},
	{ESTATUS_ERRNO_DEVICE_BUSY,						"Device busy"},
	{ESTATUS_ERRNO_CROSS_DEVICE_LINK,				"Cross device link"},
	{ESTATUS_ERRNO_NOT_A_DIRECTORY,					"Not a directory"},
	{ESTATUS_ERRNO_IS_A_DIRECTORY,					"Is a directory"},
	{ESTATUS_ERRNO_INAPPROPRIATE_IOCTL, 			"Inappropriate IOCTL"},
	{ESTATUS_ERRNO_TEXT_FILE_BUSY,					"Text file busy"},
	{ESTATUS_ERRNO_FILE_TOO_LARGE,					"File too large"},
	{ESTATUS_ERRNO_NO_SPACE_LEFT_ON_DEVICE,			"No space left on device"},
	{ESTATUS_ERRNO_ILLEGAL_SEEK,					"Illegal seek"},
	{ESTATUS_ERRNO_READ_ONLY_FILESYSTEM,			"Read only filesystem"},
	{ESTATUS_ERRNO_TOO_MANY_LINKS,					"Too many links"},
	{ESTATUS_ERRNO_BROKEN_PIPE,						"Broken pipe"},
	{ESTATUS_ERRNO_NUMERICAL_ARGUMENT_OUT_OF_DOMAIN, "Numerical argument out of doman"},
	{ESTATUS_ERRNO_RESULTS_TOO_LARGE,				"Results too large"},
	{ESTATUS_ERRNO_RESOURCE_TEMPORARILY_UNAVAILABLE, "Resource temporarily unavailable"},
	{ESTATUS_ERRNO_OPERATION_IN_PROGRESS,			"Operation in progress"},
	{ESTATUS_ERRNO_OPERATION_ALREADY_IN_PROGRESS,	"Operation already in progress"},
	{ESTATUS_ERRNO_SOCKET_OP_ON_NON_SOCKET,			"Socket operation on non-socket"},
	{ESTATUS_ERRNO_DESTIONATION_ADDRESS_REQUIRED,	"Destination address requred"},
	{ESTATUS_ERRNO_MESSAGE_TOO_LONG,				"Message too long"},
	{ESTATUS_ERRNO_PROTOCOL_WRONG_TYPE_FOR_SOCKET,	"Protocol wrong type for socket"},
	{ESTATUS_ERRNO_PROTOCOL_NOT_AVAILABLE,			"Protocol not available"},
	{ESTATUS_ERRNO_PROTOCOL_NOT_SUPPORTED,			"Protocol not supported"},
	{ESTATUS_ERRNO_SOCKET_TYPE_NOT_SUPPORTED,		"Socket type not supported"},
	{ESTATUS_ERRNO_OPERATION_NOT_SUPPORTED,			"Operation not supported"},
	{ESTATUS_ERRNO_PROTOCOL_FAMILY_NOT_SUPPROTED,	"Protocol family not supported"},
	{ESTATUS_ERRNO_ADDRESS_FAMILY_NOT_SUPPORTED,	"Address family not supported"},
	{ESTATUS_ERRNO_ADDRESS_ALREADY_IN_USE,			"Address already in use"},
	{ESTATUS_ERRNO_CANT_ASSIGN_REQUESTED_ADDRESS,	"Can't assign requested address"},
	{ESTATUS_ERRNO_NETWORK_IS_DOWN,					"Network is down"},
	{ESTATUS_ERRNO_NETWORK_UNREACHABLE,				"Network is unreachable"},
	{ESTATUS_ERRNO_NETWORK_DROPPED_CONNECTION_ON_RESET, "Network dropped connection on reset"},
	{ESTATUS_ERRNO_SOFTWARE_CAUSED_CONNECTIONABORT,	"Software caused connection abort"},
	{ESTATUS_ERRNO_CONNECTION_RESET_BY_PEER,		"Connection reset by peer"},
	{ESTATUS_ERRNO_NO_BUFFER_SPACE_AVAILABLE,		"No buffer space available"},
	{ESTATUS_ERRNO_SOCKET_ALREADY_CONNECTED,		"Socket already connected"},
	{ESTATUS_ERRNO_SOCKET_NOT_CONNECTED,			"Socket not connected"},
	{ESTATUS_ERRNO_CANT_SENT_SOCKET_SHUTDOWN,		"Can't send socket shutdown"},
	{ESTATUS_ERRNO_TOO_MANY_REFERENCES_CANT_SPLICE,	"Too many references can't splice"},
	{ESTATUS_ERRNO_CONNECTION_REFUSED,				"Connection refused"},
	{ESTATUS_ERRNO_TOO_MANY_LEVELS_OF_SYMBOLIC_LINKS, "Too many levels of symbolic links"},
	{ESTATUS_ERRNO_FILENAME_TOO_LONG,				"Filename too long"},
	{ESTATUS_ERRNO_HOST_IS_DOWN,					"Host is down"},
	{ESTATUS_ERRNO_NO_ROUTE_TO_HOST,				"No route to host"},
	{ESTATUS_ERRNO_DIRECTORY_NOT_EMPTY,				"Directory not empty"},
	{ESTATUS_ERRNO_TOO_MANY_PROCESSES,				"Too many processes"},
	{ESTATUS_ERRNO_TOO_MANY_USERS,					"Too many users"},
	{ESTATUS_ERRNO_DISK_QUOTA_EXCEEDED,				"Disk quota exceeded"},
	{ESTATUS_ERRNO_STALE_NFS_FILE_HANDLE,			"Stale NFS file handle"},
	{ESTATUS_ERRNO_TOO_MANY_LEVELS_OF_REMOTE_IN_PATH, "Too many levels of remote in path"},
	{ESTATUS_ERRNO_RPC_STRUCT_IS_BAD,				"RPC Structure is bad"},
	{ESTATUS_ERRNO_RPC_VERSION_WRONG,				"RPC Version wrong"},
	{ESTATUS_ERRNO_RPC_PROGRAM_NOT_AVAILABLE,		"RPC Program not available"},
	{ESTATUS_ERRNO_PROGRAM_VERSION_WRONG,			"Program version wrong"},
	{ESTATUS_ERRNO_BAD_PROCEDURE_FOR_PROGRAM,		"Bad procedure for program"},
	{ESTATUS_ERRNO_NO_LOCKS_AVAILABLE,				"No locks available"},
	{ESTATUS_ERRNO_INAPPROPRIATE_FILE_TYPE_OR_FORMAT, "Inappropriate file type or format"},
	{ESTATUS_ERRNO_AUTHENTICATION_ERROR,			"Authentication error"},
	{ESTATUS_ERRNO_NEED_AUTHENTICATOR,				"Need authentication"},
	{ESTATUS_ERRNO_IDENTIFIER_REMOVED,				"Identifier removed"},
	{ESTATUS_ERRNO_NO_MSG_OF_DESIRED_TYPE,			"No message of desired type"},
	{ESTATUS_ERRNO_VALUE_TOO_LARGE_TO_BE_STORED,	"Value too large to be stored"},
	{ESTATUS_ERRNO_OPERATION_CANCELED,				"Operation canceled"},
	{ESTATUS_ERRNO_ILLEGAL_BYTE_SEQUENCE,			"Illegal byte sequence"},
	{ESTATUS_ERRNO_ATTRIBUTE_NOT_FOUND,				"Attribute not found"},
	{ESTATUS_ERRNO_PROGRAMMING_ERROR,				"Programming error"},
	{ESTATUS_ERRNO_BAD_MESSAGE,						"Bad message"},
	{ESTATUS_ERRNO_MUTLIHOP_ATTEMPTED,				"Multihop attempted"},
	{ESTATUS_ERRNO_LINK_HAS_BEEN_SEVERED,			"Link has been severed"},
	{ESTATUS_ERRNO_PROTOCOL_ERROR,					"Protocol error"},
	{ESTATUS_ERRNO_CAPABILITIES_INSUFFICIENT,		"Capabilities insufficient"},
	{ESTATUS_ERRNO_NOT_PERMITTED_IN_CAPABILITY_MODE, "Not permitted in capability mode"},
	{ESTATUS_ERRNO_STATE_NOT_RECOVERABLE,			"State not recoverable"},
	{ESTATUS_ERRNO_PREVIOUS_OWNER_DIED,				"Previous owner died"},
	{ESTATUS_ERRNO_MUST_BE_EQUAL_LARGEST_ERRNO,		"Must be equal larger errno"},
	{ESTATUS_ERRNO_RESTART_SYSCALL,					"Restart SysCall"},
	{ESTATUS_ERRNO_DONT_MODIFY_REGS,				"Don't modify regs"},
	{ESTATUS_ERRNO_IOCTL_NOT_HANDLED_BY_THIS_LAYER,	"IOCTL Not handled by this layer"},
	{ESTATUS_ERRNO_DO_DIRECT_IOCTL_IN_GEOM,			"Do direct IOCTL in geometry"},
	{ESTATUS_ERRNO_EDEADLK, 						"35 Resource deadlock would occur"},
	{ESTATUS_ERRNO_ENAMETOOLONG, 					"36 File name too long"},
	{ESTATUS_ERRNO_ENOLCK, 							"37 No record locks available"},
	{ESTATUS_ERRNO_ENOSYS, 							"38 Function not implemented"},
	{ESTATUS_ERRNO_ENOTEMPTY, 						"39 Directory not empty"},
	{ESTATUS_ERRNO_ELOOP, 							"40 Too many symbolic links encountered"},
	{ESTATUS_ERRNO_EWOULDBLOCK, 					"Try again"},
	{ESTATUS_ERRNO_ENOMSG, 							"42 No message of desired type"},
	{ESTATUS_ERRNO_EIDRM, 							"43 Identifier removed"},
	{ESTATUS_ERRNO_ECHRNG,							"44 Channel number out of range"},
	{ESTATUS_ERRNO_EL2NSYNC, 						"45 Level 2 not synchronized"},
	{ESTATUS_ERRNO_EL3HLT, 							"46 Level 3 halted"},
	{ESTATUS_ERRNO_EL3RST, 							"47 Level 3 reset"},
	{ESTATUS_ERRNO_ELNRNG, 							"48 Link number out of range"},
	{ESTATUS_ERRNO_EUNATCH,							"49 Protocol driver not attached"},
	{ESTATUS_ERRNO_ENOCSI, 							"50 No CSI structure available"},
	{ESTATUS_ERRNO_EL2HLT, 							"51 Level 2 halted"},
	{ESTATUS_ERRNO_EBADE, 							"52 Invalid exchange"},
	{ESTATUS_ERRNO_EBADR, 							"53 Invalid request descriptor"},
	{ESTATUS_ERRNO_EXFULL, 							"54 Exchange full"},
	{ESTATUS_ERRNO_ENOANO, 							"55 No anode"},
	{ESTATUS_ERRNO_EBADRQC, 						"56 Invalid request code"},
	{ESTATUS_ERRNO_EBADSLT, 						"57 Invalid slot"},
	{ESTATUS_ERRNO_EDEADLOCK, 						"Resource deadlock would occur"},
	{ESTATUS_ERRNO_EBFONT, 							"59 Bad font file format"},
	{ESTATUS_ERRNO_ENOSTR, 							"60 Device not a stream"},
	{ESTATUS_ERRNO_ENODATA, 						"61 No data available"},
	{ESTATUS_ERRNO_ETIME, 							"62 Timer expired"},
	{ESTATUS_ERRNO_ENOSR, 							"63 Out of streams resources"},
	{ESTATUS_ERRNO_ENONET, 							"64 Machine is not on the network"},
	{ESTATUS_ERRNO_ENOPKG, 							"65 Package not installed"},
	{ESTATUS_ERRNO_EREMOTE, 						"66 Object is remote"},
	{ESTATUS_ERRNO_ENOLINK, 						"67 Link has been severed"},
	{ESTATUS_ERRNO_EADV, 							"68 Advertise error"},
	{ESTATUS_ERRNO_ESRMNT, 							"69 Srmount error"},
	{ESTATUS_ERRNO_ECOMM, 							"70 Communication error on send"},
	{ESTATUS_ERRNO_EPROTO, 							"71 Protocol error"},
	{ESTATUS_ERRNO_EMULTIHOP, 						"72 Multihop attempted"},
	{ESTATUS_ERRNO_EDOTDOT,							"73 RFS specific error"},
	{ESTATUS_ERRNO_EBADMSG, 						"74 Not a data message"},
	{ESTATUS_ERRNO_EOVERFLOW, 						"75 Value too large for defined data type"},
	{ESTATUS_ERRNO_ENOTUNIQ, 						"76 Name not unique on network"},
	{ESTATUS_ERRNO_EBADFD, 							"77 File descriptor in bad state"},
	{ESTATUS_ERRNO_EREMCHG, 						"78 Remote address changed"},
	{ESTATUS_ERRNO_ELIBACC, 						"79 Can not access a needed shared library"},
	{ESTATUS_ERRNO_ELIBBAD, 						"80 Accessing a corrupted shared library"},
	{ESTATUS_ERRNO_ELIBSCN, 						"81 .lib section in a.out corrupted"},
	{ESTATUS_ERRNO_ELIBMAX, 						"82 Attempting to link in too many shared libraries"},
	{ESTATUS_ERRNO_ELIBEXEC, 						"83 Cannot exec a shared library directly"},
	{ESTATUS_ERRNO_EILSEQ, 							"84 Illegal byte sequence"},
	{ESTATUS_ERRNO_ERESTART, 						"85 Interrupted system call should be restarted"},
	{ESTATUS_ERRNO_ESTRPIPE,						"86 Streams pipe error"},
	{ESTATUS_ERRNO_EUSERS, 							"87 Too many users"},
	{ESTATUS_ERRNO_ENOTSOCK, 						"88 Socket operation on non-socket"},
	{ESTATUS_ERRNO_EDESTADDRREQ, 					"89 Destination address required"},
	{ESTATUS_ERRNO_EMSGSIZE, 						"90 Message too long"},
	{ESTATUS_ERRNO_EPROTOTYPE, 						"91 Protocol wrong type for socket"},
	{ESTATUS_ERRNO_ENOPROTOOPT, 					"92 Protocol not available"},
	{ESTATUS_ERRNO_EPROTONOSUPPORT, 				"93 Protocol not supported"},
	{ESTATUS_ERRNO_ESOCKTNOSUPPORT, 				"94 Socket type not supported"},
	{ESTATUS_ERRNO_EOPNOTSUPP, 						"95 Operation not supported on transport endpoint"},
	{ESTATUS_ERRNO_EPFNOSUPPORT, 					"96 Protocol family not supported"},
	{ESTATUS_ERRNO_EAFNOSUPPORT, 					"97 Address family not supported by protocol"},
	{ESTATUS_ERRNO_EADDRINUSE, 						"98 Address already in use"},
	{ESTATUS_ERRNO_EADDRNOTAVAIL, 					"99 Cannot assign requested address"},
	{ESTATUS_ERRNO_ENETDOWN, 						"100 Network is down"},
	{ESTATUS_ERRNO_ENETUNREACH, 					"101 Network is unreachable"},
	{ESTATUS_ERRNO_ENETRESET, 						"102 Network dropped connection because of reset"},
	{ESTATUS_ERRNO_ECONNABORTED, 					"103 Software caused connection abort"},
	{ESTATUS_ERRNO_ECONNRESET, 						"104 Connection reset by peer"},
	{ESTATUS_ERRNO_ENOBUFS, 						"105 No buffer space available"},
	{ESTATUS_ERRNO_EISCONN, 						"106 Transport endpoint is already connected"},
	{ESTATUS_ERRNO_ENOTCONN, 						"107 Transport endpoint is not connected"},
	{ESTATUS_ERRNO_ESHUTDOWN, 						"108  Cannot send after transport endpoint shutdown"},
	{ESTATUS_ERRNO_ETOOMANYREFS, 					"109 Too many references: cannot splice"},
	{ESTATUS_ERRNO_ETIMEDOUT, 						"110 Connection timed out"},
	{ESTATUS_ERRNO_ECONNREFUSED, 					"111 Connection refused"},
	{ESTATUS_ERRNO_EHOSTDOWN, 						"112 Host is down"},
	{ESTATUS_ERRNO_EHOSTUNREACH, 					"113 No route to host"},
	{ESTATUS_ERRNO_EALREADY, 						"114 Operation already in progress"},
	{ESTATUS_ERRNO_EINPROGRESS, 					"115 Operation now in progress"},
	{ESTATUS_ERRNO_ESTALE, 							"116 Stale NFS file handle"},
	{ESTATUS_ERRNO_EUCLEAN, 						"117 Structure needs cleaning"},
	{ESTATUS_ERRNO_ENOTNAM, 						"118 Not a XENIX named type file"},
	{ESTATUS_ERRNO_ENAVAIL, 						"119 No XENIX semaphores available"},
	{ESTATUS_ERRNO_EISNAM, 							"120 Is a named type file"},
	{ESTATUS_ERRNO_EREMOTEIO, 						"121 Remote I/O error"},
	{ESTATUS_ERRNO_EDQUOT, 							"122 Quota exceeded"},
	{ESTATUS_ERRNO_ENOMEDIUM, 						"123 No medium found"},
	{ESTATUS_ERRNO_EMEDIUMTYPE, 					"124 Wrong medium type"},
	{ESTATUS_ERRNO_ECANCELED, 						"125 Operation Canceled"},
	{ESTATUS_ERRNO_ENOKEY, 							"126 Required key not available"},
	{ESTATUS_ERRNO_EKEYEXPIRED, 					"127 Key has expired"},
	{ESTATUS_ERRNO_EKEYREVOKED, 					"128 Key has been revoked"},
	{ESTATUS_ERRNO_EKEYREJECTED, 					"129 Key was rejected by service"},
	{ESTATUS_ERRNO_EOWNERDEAD, 						"130 Owner died"},
	{ESTATUS_ERRNO_ENOTRECOVERABLE, 				"131 State not recoverable"},
	{ESTATUS_ERRNO_ERFKILL, 						"132 Unknown error"},
	{ESTATUS_ERRNO_EHWPOISON, 						"133 Unknown error"},
	{ESTATUS_ERRNO_STRUNCATE,						"80 Truncate"},
	{ESTATUS_ERRNO_UNKNOWN,							"Errno unknown"},

	// Misc administrative errors
	{ESTATUS_LANGUAGE_NOT_AVAILABLE,				"Language not available"},
	{ESTATUS_TRANSLATION_NOT_FOUND,					"Translation not found"},
	{ESTATUS_LINE_TOO_LONG,							"SMTP Line too long"},
	
	// Patient related errors
	{ESTATUS_PATIENT_DUPLICATE_NAME,				"Duplicate patient name"},
	{ESTATUS_PATIENT_GENDER_NOT_SET,				"Gender not set"},
	{ESTATUS_PATIENT_LOCAL_LOOKUP_FAULT,			"Local patient lookup fault"},
	{ESTATUS_PATIENT_DATA_UNCHANGED,				"Patient data unchanged"},
	{ESTATUS_PATIENT_CLINIC_SELECT_REQUIRED,		"Patient clinic selection required"},
	{ESTATUS_PATIENT_ATTENDING_DOCTOR_MISSING,		"Patient's attending doctor field blank"},
	{ESTATUS_PATIENT_NOT_FOUND,						"Patient not found"},
	{ESTATUS_PATIENT_MISSING,						"Patient structure missing"},
	{ESTATUS_PATIENT_ADD_UPDATE_FAILURE,			"Patient add or update failure"},
	{ESTATUS_PATIENT_REFERRED_BY_OTHER_BLANK,		"Referred by 'Other' selected but other field blank"},

	// Session errors
	{ESTATUS_UNKNOWN_COMMAND,						"Unknown command"},
	{ESTATUS_SERVER_LIST_UPDATED,					"Server list updated"},
	{ESTATUS_DATABASE_ALLOC_FAULT,					"Database handle allocation fault"},
	{ESTATUS_SERVER_DB_QUERY_FAULT,					"Server database query fault"},
	{ESTATUS_SERVER_OUT_OF_MEMORY,					"Server out of memory (server)"},
	{ESTATUS_USERNAME_PASSWORD_BAD,					"Username/Password unknown or bad"},
	{ESTATUS_USER_PERMISSIONS_FAULT,				"Insufficient user permission"},
	{ESTATUS_SESSION_ID_INVALID,					"Session ID invalid"},
	{ESTATUS_ATOM_SEQUENCE_FAULT,					"Atomizer sequence fault"},
	{ESTATUS_SESSION_NOT_FOUND,						"Session structure not found"},
	{ESTATUS_SERVER_DB_NO_MORE_DATA,				"No more data"},
	{ESTATUS_SERVER_DB_DATA_TRUNCATED,				"Row data truncated"},
	{ESTATUS_SERVER_LIST_INVALID_INDEX,				"Server list index invalid"},
	{ESTATUS_SESSION_FULL,							"No more session IDs available"},
	{ESTATUS_SERVER_IDENTITY_UNKNOWN,				"-servername provided unknown"},
	{ESTATUS_SERVER_IDENTITY_MISSING,				"Server identity missing (-servername not provided)"},
	{ESTATUS_KEY_INDEX_OUT_OF_RANGE,				"Application key index out of range"},
	{ESTATUS_DB_STATEMENT_PREPARE_FAILED,			"Database statement prepare failed"},
	{ESTATUS_DB_BIND_PARAM_FAILED,					"Database bind parameter failed"},
	{ESTATUS_DB_STATEMENT_EXEC_ERROR,				"Database statement exec error"},
	{ESTATUS_DB_BIND_RESULT_FAILURE,				"Database bind result failure"},
	{ESTATUS_DB_STATEMENT_FETCH_FAILED,				"Database statement fetch failure"},
	{ESTATUS_DB_FETCH_COLUMN_FAILURE,				"Database fetch column failure"},
	{ESTATUS_DB_HANDLE_INVALID,						"Bad database handle"},
	{ESTATUS_SERVER_GROUP_UNKNOWN,					"Unknown server group name"},
	{ESTATUS_SERVER_GROUP_NOT_SET,					"Server group not set"},

	{ESTATUS_CLINIC_NAME_MISSING,					"Clinic name missing"},
	{ESTATUS_CLINIC_NAME_DUPLICATE,					"Duplicate clinic name"},
	{ESTATUS_CLINIC_UNCHANGED,						"Clinic data unchanged"},
	{ESTATUS_CLINIC_DISABLED,						"Clinic is disabled"},
	{ESTATUS_CLINIC_GUID_MISSING,					"Clinic GUID is missing/no clinic associated"},
	{ESTATUS_CLINIC_NOT_FOUND,						"Clinic not found"},
	{ESTATUS_CLINIC_UPDATE_FAILURE,					"Clinic update failure"},
	{ESTATUS_CLINIC_ADD_FAILURE,					"Clinic add failure"},
	{ESTATUS_CLINIC_NOT_ASSOCIATED,					"Clinic not associated with user"},
	{ESTATUS_CLINIC_ACTIVE_NOT_SET,					"No active clinic set"},

	{ESTATUS_TREATMENT_BUFFER_TOO_SMALL,			"Treatment buffer invalid for request"},
	{ESTATUS_TREATMENT_STATE_INVALID,				"Current treatment state invalid for request"},
	{ESTATUS_TREATMENT_INVALID_PARAMETER,			"Invalid treatment parameter"},
	{ESTATUS_TREATMENT_INVALID_EVENT_TYPE,			"Invalid treatment parameter"},
	{ESTATUS_HOST_TIMING_INVALID_STATE,				"Invalid host timing state for request"},
	{ESTATUS_STIM_MAX_DRIVERS_REGISTERED,			"Too many drivers registered"},
	{ESTATUS_STIM_FUNCTION_NOT_SUPPORTED,			"Function not supported by driver"},
	{ESTATUS_TREATMENT_FAILED_TO_START,				"Failed to start treatment"},
	{ESTATUS_TREATMENT_FAILED_TO_PAUSE,				"Failed to pause treatment"},
	{ESTATUS_TREATMENT_FAILED_TO_RESUME,			"Failed to resume treatment"},
	{ESTATUS_TREATMENT_FAILED_TO_END,				"Failed to end treatment"},
	{ESTATUS_TREATMENT_FAILED_TO_ADD_PERIOD,		"Failed to add period"},
	{ESTATUS_TREATMENT_ENDED_ON_DEVICE_CHANGE,		"Treatment ended on device change"},
	{ESTATUS_TREATMENT_INVALID_AMPLITUDE,			"Invalid amplitude value"},
	{ESTATUS_TREATMENT_MISSING,						"Treatment data missing"},
	{ESTATUS_TREATMENT_JOB_NOT_SUPPORTED,			"Treatment job not (yet) supported"},
	{ESTATUS_TREATMENT_NO_SEGMENTS,					"Treatment has no segments"},
	{ESTATUS_TREATMENT_ADD_FAILURE,					"Failure to add treatment"},
	{ESTATUS_TREATMENT_TIMING_LENGTH_INVALID,		"Timing length not divisible by 2"},
	{ESTATUS_TREATMENT_AMPLITUDE_TIMING_MISMATCH,	"Amplitude/timing mismatch"},
	{ESTATUS_TREATMENT_AMPLITUDE_IMBALANCE,			"Amplitude length imbalance"},
	{ESTATUS_TREATMENT_LOCATION_NOT_SPECIFIED,		"Treatment location not specified"},
	{ESTATUS_TREATMENT_LOCATION_INVALID,			"Treatment location invalid"},
	{ESTATUS_TREATMENT_TYPE_NOT_SPECIFIED,			"Treatment type not specified"},
	{ESTATUS_TREATMENT_TYPE_INVALID,				"Treatment type invalid"},
	{ESTATUS_TREATMENT_LOG_MISSING,					"Treatment log structure missing"},
	{ESTATUS_TREATMENT_NOT_FOUND,					"Treatment not found"},
	{ESTATUS_TREATMENT_LOG_NO_SEGMENTS,				"No treatment log segments"},
	{ESTATUS_TREATMENT_LOG_EVENT_TIME_NOT_SET,		"Treatment log event time not set"},
	{ESTATUS_TREATMENT_EVENT_INVALID,				"Treatment event invalid"},
	{ESTATUS_TREATMENT_EVENT_UNKNOWN,				"Treatment event unknown"},
	{ESTATUS_TREATMENT_STIM_TYPE_INVALID,			"Stim type invalid"},
	{ESTATUS_TREATMENT_STIM_TYPE_UNKNOWN,			"Stim type unknown"},
	{ESTATUS_TREATMENT_TRIG_TYPE_INVALID,			"Trigger type invalid"},
	{ESTATUS_TREATMENT_TRIG_TYPE_UNKNOWN,			"Trigger type unknown"},
	{ESTATUS_TREATMENT_TIMING_METHOD_INVALID,		"Timing method invalid"},
	{ESTATUS_TREATMENT_TIMING_METHOD_UNKNOWN,		"Timing method unknown"},
	{ESTATUS_TREATMENT_MOTOR_THRESHOLD_INVALID,		"Motor threshold is invalid"},
	{ESTATUS_TREATMENT_GOAL_INTENSITY_INVALID,		"Invalid goal intensity"},
	{ESTATUS_TREATMENT_INTERTRAIN_TOO_SHORT,		"Intertrain period too short"},
	{ESTATUS_TREATMENT_TRAIN_TIME_TOO_SHORT,		"Train time too short"},
	{ESTATUS_TREATMENT_CENTER_PERIOD_OUT_OF_RANGE,	"Treatment center period out of range"},
	{ESTATUS_TREATMENT_PERIOD_STANDARD_DEV_OUT_OF_RANGE, "Period standard deviation out of range"},
	{ESTATUS_TREATMENT_NUM_OF_SD_OUT_OF_RANGE,		"Number of standard deviations out of range"},
	{ESTATUS_TREATMENT_LOCATION_TYPE_MISMATCH,		"Treatment location doesn't belong to the treatment type group"},
	{ESTATUS_TREATMENT_CONFIGURATION_INVALID,		"Configuration invalid"},
	{ESTATUS_TREATMENT_MOTOR_THRESHOLD_UNCHANGED,	"Motor threshold unchanged"},
	{ESTATUS_STIM_AMPLITUDE_SET_FAILED,				"Amplitude set failed"},
	{ESTATUS_STIM_ENABLE_FAILED,					"Enable failed"},
	{ESTATUS_STIM_TRIGGER_FAILED,					"Trigger failed"},
	{ESTATUS_TREATMENT_GEN_INTERNAL_FAULT,			"Treatment generation internal fault"},
	{ESTATUS_STIM_DEVICE_TRAIN_FAILED,				"Device train failed"},
	{ESTATUS_TREATMENT_ALREADY_INITIALIZED,			"Treatment library already initialized"},
	{ESTATUS_TREATMENT_NOT_INITIALIZED,				"Treatment library not initialized"},
	{ESTATUS_TREATMENT_INIT_IN_PROGRESS,			"Treatment library init in progress"},
	{ESTATUS_TREATMENT_DUTY_CYCLE_OUT_OF_RANGE,		"Duty cycle out of range (0-100)"},

	{ESTATUS_FONT_ID_NOT_FOUND,						"Font ID not found"},
	{ESTATUS_TREATMENT_START_LOCKED,				"Treatment start locked"},
	{ESTATUS_TREATMENT_INTENSITY_CHANGED_FROM_START,	"Treatment intensity changed from start"},
	{ESTATUS_TREATMENT_TRAIN_TOO_LONG_FOR_STIMULATOR,	"Treatment train too long for stimulator at current amplitude."},
	{ESTATUS_TREATMENT_INTERTRAIN_TOO_SHORT_FOR_STIMULATOR,	"Treatment intertrain too short for stimulator at current amplitude."},
	{ESTATUS_TREATMENT_AMPLITUDE_ABOVE_MAXIMUM,		"Amplitude above maximum"},
	{ESTATUS_TREATMENT_FREQUENCY_ABOVE_MAXIMUM,		"Frequency greater than maximum"},

	{ESTATUS_FILE_ERROR_PENDING,					"File error pending"},

	{ESTATUS_SOCKET_MAX_UDP_SOCKETS,				"Out of UDP sockets"},
	{ESTATUS_SOCKET_MAX_TCP_SOCKETS,				"Out of TCP sockets"},
	{ESTATUS_HOST_NAME_LOOKUP_FAILURE,				"Host name lookup failure"},
	{ESTATUS_REMOTE_PORT_OUT_OF_RANGE,				"Port out of range"},
	{ESTATUS_SOURCE_PORT_NOT_SUPPORTED,				"Port not supported"},
	{ESTATUS_SOCKET_NOT_READY,						"Socket not ready"},
	{ESTATUS_SSL_ALLOC,								"Can't allocate an SSL object"},
	{ESTATUS_NO_LINK,								"No link"},
	{ESTATUS_SSL_WRITE_FAULT,						"SSL write fault"},
	{ESTATUS_SSL_CONNECT,							"Unable to make an SSL connection"},
	{ESTATUS_CLIENT_REQUESTED_SHUTDOWN,				"CLient requested shutdown"},
	{ESTATUS_SSL_READ_FAULT,						"SSL read fault"},
	{ESTATUS_MALFORMED_RESPONSE,					"Malformed response structure"},
	{ESTATUS_MALFORMED_REQUEST,						"Malformed request structure"},
	{ESTATUS_NO_AVAILABLE_SERVERS,					"No available servers"},
	{ESTATUS_LOST_LINK,								"Link lost while waiting for response"},
	{ESTATUS_SOCKET_ERROR,							"Socket error"},
	{ESTATUS_INVALID_SEQUENCE_NUMBER,				"Invalid sequence number"},

	// Misc related
	{ESTATUS_BUFFER_TOO_SMALL,						"Target buffer too small"},
	{ESTATUS_ATOM_ID_SUBTYPE_UNKOWN,				"Unknown subtype"},
	{ESTATUS_ATOM_DATA_MISCOMPARE,					"Atom data miscompare"},
	{ESTATUS_ATOM_ID_UNKNOWN,						"Atom ID unknown"},
	{ESTATUS_RANDOM_NUMBER_NOT_VALID,				"Random number not valid"},
	{ESTATUS_UNKNOWN_TABLE_TYPE,					"Unknown table/data type"},
	{ESTATUS_DATE_DAY_INVALID,						"Day invalid"},
	{ESTATUS_DATE_MONTH_INVALID,					"Month invalid"},
	{ESTATUS_DATE_YEAR_INVALID,						"Year invalid"},
	{ESTATUS_TABLE_NOT_SQL,							"Not a SQL table"},
	{ESTATUS_INTERNAL_ERROR,						"Internal error"},
	{ESTATUS_DATA_TOO_SHORT,						"Data too short"},
	{ESTATUS_HASH_ALREADY_PRESENT,					"Hash already present"},
	{ESTATUS_EMPTY,									"Empty"},
	{ESTATUS_ABORTED,								"Aborted"},
	{ESTATUS_UNKNOWN_RESULT_CODE,					"Unknown result code"},
	{ESTATUS_HANDLE_INVALID,						"Handle invalid"},
	{ESTATUS_NO_MORE_HANDLE_TYPES,					"No more handle types"},
	{ESTATUS_NO_MORE_HANDLES,						"No more handles"},
	{ESTATUS_HANDLE_POOL_INVALID,					"Handle pool invalid"},
	{ESTATUS_HANDLE_POOL_MISMATCH,					"Handle pool mismatch"},
	{ESTATUS_HANDLE_NOT_ALLOCATED,					"Handle not allocated"},
	{ESTATUS_HANDLE_POOL_NO_CRITICAL_SECTION,		"Handle pool was not created with critical sections"},
	{ESTATUS_TRUNCATED,								"Data truncated"},
	{ESTATUS_CANCELLED,								"Operation cancelled"},

	// Connection related
	{ESTATUS_CONNECTION_HANDLE_INVALID,				"Connection handle invalid"},
	{ESTATUS_CONNECTION_HANDLES_FULL,				"No more connection handles"},
	{ESTATUS_CONNECTION_NOT_ALLOCATED,				"No connection handle allocated"},
	{ESTATUS_CONNECTION_INVALID_TYPE,				"Function not valid for connection type"},
	{ESTATUS_CONNECTION_ALREADY_ESTABLISHED,		"Connection already established"},
	{ESTATUS_CONNECTION_SERVICE_INVALID,			"Connection service invalid"},

	// User related
	{ESTATUS_USER_NOT_FOUND,						"User not found"},
	{ESTATUS_USER_DISABLED,							"Can't log in - User disabled"},
	{ESTATUS_USER_NOT_AUTHORIZER,					"User is not an authorizer"},
	{ESTATUS_USER_USERNAME_PRESENT,					"Username already exists"},
	{ESTATUS_USER_UPDATE_FAILURE,					"User update failure"},
	{ESTATUS_USER_MALFORMED_PASSWORD,				"Malformed password"},
	{ESTATUS_USER_PASSWORD_MISSING,					"Password missing"},
	{ESTATUS_USER_PASSWORD_SET_OTHER,				"Only logged-in user's password can be changed"},
	{ESTATUS_USER_UNCHANGED,						"User data unchanged"},
	{ESTATUS_USER_PASSWORD_MISMATCH,				"Password mismatch"},
	{ESTATUS_USER_PASSWORD_BAD_POLICY,				"Bad password policy"},
	{ESTATUS_USER_PASSWORD_BLANK,					"Password cannot be blank"},
	{ESTATUS_USER_PASSWORD_TOO_SHORT,				"Password is too short"},
	{ESTATUS_USER_MISSING,							"User data missing"},
	{ESTATUS_USER_CLINIC_ACTIVE_SELECT_REQUIRED,	"Active clinic set required"},
	{ESTATUS_USER_NOT_A_CLINICIAN,					"User not a clinician"},

	// Misc related
	{ESTATUS_GUID_MISSING,							"GUID Is missing"},
	{ESTATUS_INSTANCE_GUID_MISSING,					"Instance GUID is missing"},
	{ESTATUS_GUID_TYPE_MISMATCH,					"GUID/Data type mismatch"},
	{ESTATUS_INSTANCE_GUID_TYPE_MISMATCH,			"Instance GUID/Data type mismatch"},
	{ESTATUS_INSTANCE_IN_GUID,						"Instance GUID in a GUID position"},
	{ESTATUS_GUID_IN_INSTANCE,						"GUID in an instance GUID position"},
	{ESTATUS_DATA_MISSING,							"No incoming data"},
	{ESTATUS_GUID_NULL_CHECK_FAILED,				"Failed GUID NULL check - should not be NULL"},

	// Field errors
	{ESTATUS_FIELD_ERROR,							"Field error"},
	{ESTATUS_FIELD_BLANK,							"Field cannot be blank"},
	{ESTATUS_FIELD_INVALID,							"Field invalid"},
	{ESTATUS_FIELD_COUNTRY_NOT_SPECIFIED,			"Country not specified"},
	{ESTATUS_FIELD_COUNTRY_INVALID,					"Country selection invalid"},
	{ESTATUS_FIELD_VALUE_IS_ZERO,					"Field must be nonzero"},
	{ESTATUS_FIELD_IS_NULL,							"Field cannot be NULL"},
	{ESTATUS_FIELD_IS_NOT_NULL,						"Field must be NULL"},
	{ESTATUS_FIELD_TOO_LONG,						"String too long"},
	{ESTATUS_FIELD_MUST_BE_ZERO,					"Field must be 0"},

	// Software update errors
	{ESTATUS_UPDATE_FLAGS_UNCHANGED,				"Update flags unchanged"},
	{ESTATUE_UPDATE_COMPONENT_MISSING,				"Update component is missing"},
	{ESTATUS_UPDATE_EXEC_BUILD_ALREADY_EXISTS,		"Build/executable pair already exists in database"},
	{ESTATUS_UPDATE_WRITE_OR_VERIFY_FAULT,			"Failure to write/verify update file"},

	// File store related
	{ESTATUS_FILESTORE_FILE_TYPE_UNSPECIFIED,		"File type unspecified"},
	{ESTATUS_FILESTORE_FILE_TYPE_UNKNOWN,			"File type unknown"},
	{ESTATUS_FILESTORE_FILE_ZERO_LENGTH,			"File is 0 length"},
	{ESTATUS_FILESTORE_FILE_AMBIGUOUS,				"File type/class can't be identified definitively"},
	{ESTATUS_FILESTORE_FILE_TOO_SMALL_FOR_TYPE,		"File size is too small for type/class"},
	{ESTATUS_FILESTORE_FILE_UNEXPECTED_FILE_TYPE,	"Incorrect file class"},
	{ESTATUS_FILESTORE_FILE_DUPLICATE,				"File already present"},
	{ESTATUS_FILESTORE_FILE_USE_NOT_SPECIFIED,		"File use not specified"},
	{ESTATUS_FILESTORE_FILE_USE_INVALID,			"File use invalid"},
	{ESTATUS_FILESTORE_FILE_ASSESSMENT_USE_INVALID,	"Assessment document's file use field is not an assessment use"},
	{ESTATUS_FILESTORE_FILE_CLASS_UNKNOWN,			"File class unknown"},
	{ESTATUS_FILESTORE_FILE_USE_UNKNOWN,			"File use unknown"},
	{ESTATUS_FILESTORE_FILE_AUTODETECT_FAILED,		"File autodetect failed - format unnkown"},

	// Patient notes related
	{ESTATUS_PATIENTNOTE_FLAGS_NOT_SET,				"Patient note flags not set"},
	{ESTATUS_PATIENTNOTE_FLAGS_INVALID,				"Invalid patient note flags"},

	// Machine ID related
	{ESTATUS_MACHINE_ID_NOT_FOUND,					"Machine ID not found"},

	// EEG File related
	{ESTATUS_EEG_NOT_AN_EEG_FILE,					"Not an EEG file"},
	{ESTATUS_EEG_FILE_CORRUPT,						"EEG File corrupt (bad content)"},
	{ESTATUS_EEG_FILE_TOO_SMALL,					"EEG File too small"},
	{ESTATUS_EEG_FILE_INVALID_SAMPLE_RATE,			"EEG File Invalid sample rate"},
	{ESTATUS_EEG_CHANNEL_NOT_FOUND,					"EEG Channel not found"},
	{ESTATUS_EEG_FILE_TYPE_NOT_SUPPORTED,			"EEG File type recognized but not supported"},
	{ESTATUS_EEG_CHANNEL_OUT_OF_RANGE,				"EEG Channel out of range"},
	{ESTATUS_EEG_DEVICE_HANDLE_INVALID,				"EEG Device handle invalid"},
	{ESTATUS_EEG_HANDLES_FULL,						"No more handles for EEG devices"},

	// Service related errors
	{ESTATUS_SERVICE_HANDLE_INVALID,				"Invalid service handle"},
	{ESTATUS_SERVICE_HANDLES_FULL,					"Service handles full"},
	{ESTATUS_SERVICE_NOT_ALLOCATED,					"Service handle not allocated"},
	{ESTATUS_SERVICE_NO_MATCHES,					"No matching services available"},

	// Sound related errors
	{ESTATUS_SOUND_HARDWARE_FAULT,					"Sound hardware fault"},

	// Misc DSPish errors
	{ESTATUS_RESAMPLE_TYPE_UNKNOWN,					"Resample type unknown"},
	{ESTATUS_ALGORITHM_UNKNOWN,						"Algorithm unknown"},

	// Misc job errors
	{ESTATUS_JOB_PENDING,							"Job pending"},
	{ESTATUS_JOB_ALREADY_IN_PROGRESS,				"Job already in progress"},
	{ESTATUS_JOB_PROCESSOR_NOT_READY,				"Job processor isn't ready"},
	{ESTATUS_JOB_INVALID_STATUS,					"Job status invalid"},
	{ESTATUS_JOB_SETTING_ALREADY_PRESENT,			"Job setting already present"},
	{ESTATUS_JOB_ADD_FAILURE,						"Job add failure"},
	{ESTATUS_JOB_SETTINGS_ADD_FAILURE,				"Job settings add failure"},
	{ESTATUS_JOBS_LANGUAGE_INVALID,					"Job language invalid"},
	{ESTATUS_JOB_GUID_IS_NULL,						"Job GUID is NULL but shouldn't be"},
	{ESTATUS_JOB_GUID_IS_NOT_NULL,					"Job GUID isn't NULL but should be"},

	// Reportbot errors
	{ESTATUS_REPORTBOT_CHANNEL_MISSING,				"EEG File's channel missing"},
	{ESTATUS_REPORTBOT_STATIC_FILE_MISSING,			"Static file missing"},
	{ESTATUS_REPORTBOT_INTERNAL_CALC_ERROR,			"Internal calculation error"},
	{ESTATUS_REPORTBOT_NO_REMAINING_DATA,			"All data rejected"},

	// Report errors
	{ESTATUS_REPORT_TYPE_UNKNOWN,					"Report type unknown"},

	// Version errors
	{ESTATUS_VERSION_TAG_NOT_SPECIFIED,				"Version tag not specified"},
	{ESTATUS_VERSION_TAG_NOT_APPLICABLE,			"Version tag not applicable"},
	{ESTATUS_VERSION_TAG_NOT_FOUND,					"Version tag not found"},
	{ESTATUS_VERSION_BAD_CRC32,						"Bad CRC32"},
	{ESTATUS_VERSION_BAD_HASH,						"Bad version hash"},

	// Graphics errors
	{ESTATUS_GFX_INIT_FAULT,						"Graphics initialization failure"},
	{ESTATUS_GFX_DISPLAY_MODE_FAULT,				"Couldn't obtain display mode information"},
	{ESTATUS_GFX_FILE_FORMAT_UNKNOWN,				"Graphics file format unknown"},
	{ESTATUS_GFX_FILE_CORRUPT,						"Graphics file corrupt"},
	{ESTATUS_GFX_FILE_NOT_SUPPORTED,				"Graphics file format not supported"},

	{ESTATUS_FREETYPE_INVALID_ARGUMENT,				"Freetype: Invalid argument"},
	{ESTATUS_FREETYPE_CANT_OPEN_RESOURCE,			"Freetype: Can't open resource"},
	{ESTATUS_FREETYPE_UNKNOWN_FILE_FORMAT,          "Freetype: Unknown file format"},
	{ESTATUS_FREETYPE_INVALID_FILE_FORMAT,          "Freetype: Invalid file format"},
	{ESTATUS_FREETYPE_INVALID_VERSION,              "Freetype: Invalid version"},
	{ESTATUS_FREETYPE_LOWER_MODULE_VERSION,         "Freetype: Lower module version"},
	{ESTATUS_FREETYPE_UNIMPLEMENTED_FEATURE,        "Freetype: Unimplemented feature"},
	{ESTATUS_FREETYPE_INVALID_TABLE,                "Freetype: Invalid table"},
	{ESTATUS_FREETYPE_INVALID_OFFSET,               "Freetype: Invalid offset"},
	{ESTATUS_FREETYPE_ARRAY_TOO_LARGE,              "Freetype: Array top large"},

	{ESTATUS_FREETYPE_INVALID_GLYPH_INDEX,          "Freetype: Invalid glyph index"},
	{ESTATUS_FREETYPE_INVALID_CHARACTER_CODE,       "Freetype: Invalid character code"},
	{ESTATUS_FREETYPE_INVALID_GLYPH_FORMAT,         "Freetype: Invalid glyph format"},
	{ESTATUS_FREETYPE_CANNOT_RENDER_GLYPH,          "Freetype: Cannot render glyph"},
	{ESTATUS_FREETYPE_INVALID_OUTLINE,              "Freetype: Invalid outline"},
	{ESTATUS_FREETYPE_INVALID_COMPOSITE,            "Freetype: Invalid composite"},
	{ESTATUS_FREETYPE_TOO_MANY_HINTS,               "Freetype: Too many hints"},
	{ESTATUS_FREETYPE_INVALID_PIXEL_SIZE,           "Freetype: Invalid pixel size"},

	{ESTATUS_FREETYPE_INVALID_HANDLE,               "Freetype: Invalid handle"},
	{ESTATUS_FREETYPE_INVALID_LIBRARY_HANDLE,       "Freetype: Invalid library handle"},
	{ESTATUS_FREETYPE_INVALID_DRIVER_HANDLE,        "Freetype: Invalid driver handle"},
	{ESTATUS_FREETYPE_INVALID_FACE_HANDLE,          "Freetype: Invalid face handle"},
	{ESTATUS_FREETYPE_INVALID_SIZE_HANDLE,          "Freetype: Invalid size handle"},
	{ESTATUS_FREETYPE_INVALID_SLOT_HANDLE,          "Freetype: Invalid slot handle"},
	{ESTATUS_FREETYPE_INVALID_CHARMAP_HANDLE,       "Freetype: Invalid charmap handle"},
	{ESTATUS_FREETYPE_INVALID_CACHE_HANDLE,         "Freetype: Invalid cache handle"},
	{ESTATUS_FREETYPE_INVALID_STREAM_HANDLE,        "Freetype: Invalid stream handle"},

	{ESTATUS_FREETYPE_TOO_MANY_DRIVERS,             "Freetype: Too many drivers"},
	{ESTATUS_FREETYPE_TOO_MANY_EXTENSIONS,          "Freetype: Too many extensions"},

	{ESTATUS_FREETYPE_UNLISTED_OBJECT,              "Freetype: Unlisted object"},

	{ESTATUS_FREETYPE_CANNOT_OPEN_STREAM,           "Freetype: Cannot open stream"},
	{ESTATUS_FREETYPE_INVALID_STREAM_SEEK,          "Freetype: Invalid stream seek"},
	{ESTATUS_FREETYPE_INVALID_STREAM_SKIP,          "Freetype: Invalid stream skip"},
	{ESTATUS_FREETYPE_INVALID_STREAM_READ,          "Freetype: Invalid stream read"},
	{ESTATUS_FREETYPE_INVALID_STREAM_OPERATION,     "Freetype: Invalid stream operation"},
	{ESTATUS_FREETYPE_INVALID_FRAME_OPERATION,      "Freetype: Invalid frame operation"},
	{ESTATUS_FREETYPE_NESTED_FRAME_ACCESS,          "Freetype: Nested frame access"},
	{ESTATUS_FREETYPE_INVALID_FRAME_READ,           "Freetype: Invalid frame read"},

	{ESTATUS_FREETYPE_RASTER_UNINITIALIZED,         "Freetype: Raster uninitialized"},
	{ESTATUS_FREETYPE_RASTER_CORRUPTED,             "Freetype: Raster corrupted"},
	{ESTATUS_FREETYPE_RASTER_OVERFLOW,              "Freetype: Raster overflow"},
	{ESTATUS_FREETYPE_RASTER_NEGATIVE_HEIGHT,       "Freetype: Raster negative height"},

	{ESTATUS_FREETYPE_TOO_MANY_CACHES,              "Freetype: Too many caches"},

	{ESTATUS_FREETYPE_INVALID_OPCODE,               "Freetype: Invalid opcode"},
	{ESTATUS_FREETYPE_TOO_FEW_ARGUMENTS,            "Freetype: Too few arguments"},
	{ESTATUS_FREETYPE_STACK_OVERFLOW,               "Freetype: Stack overflow"},
	{ESTATUS_FREETYPE_CODE_OVERFLOW,                "Freetype: Code overflow"},
	{ESTATUS_FREETYPE_BAD_ARGUMENT,                 "Freetype: Bad argument"},
	{ESTATUS_FREETYPE_DIVIDE_BY_ZERO,               "Freetype: Divide by zero"},
	{ESTATUS_FREETYPE_INVALID_REFERENCE,            "Freetype: Invalid reference"},
	{ESTATUS_FREETYPE_DEBUG_OPCODE,                 "Freetype: Debug opcode"},
	{ESTATUS_FREETYPE_ENDF_IN_EXEC_STREAM,          "Freetype: Endf in exec stream"},
	{ESTATUS_FREETYPE_NESTED_DEFS,                  "Freetype: Nested defs"},
	{ESTATUS_FREETYPE_INVALID_CODERANGE,            "Freetype: Invalid coderange"},
	{ESTATUS_FREETYPE_EXECUTION_TOO_LONG,           "Freetype: Execution too long"},
	{ESTATUS_FREETYPE_TOO_MANY_FUNCTION_DEFS,       "Freetype: Too many function defs"},
	{ESTATUS_FREETYPE_TOO_MANY_INSTRUCTION_DEFS,    "Freetype: Too many instruction defs"},
	{ESTATUS_FREETYPE_TABLE_MISSING,                "Freetype: Table missing"},
	{ESTATUS_FREETYPE_HORIZ_HEADER_MISSING,         "Freetype: Horiz header missing"},
	{ESTATUS_FREETYPE_LOCATIONS_MISSING,            "Freetype: Locations missing"},
	{ESTATUS_FREETYPE_NAME_TABLE_MISSING,           "Freetype: Name table missing"},
	{ESTATUS_FREETYPE_CMAP_TABLE_MISSING,           "Freetype: CMAP table missing"},
	{ESTATUS_FREETYPE_HMTX_TABLE_MISSING,           "Freetype: HTMX table missing"},
	{ESTATUS_FREETYPE_POST_TABLE_MISSING,           "Freetype: POST table missing"},
	{ESTATUS_FREETYPE_INVALID_HORIZ_METRICS,        "Freetype: Invalid horizonal metrics"},
	{ESTATUS_FREETYPE_INVALID_CHARMAP_FORMAT,       "Freetype: Invalid charmap format"},
	{ESTATUS_FREETYPE_INVALID_PPEM,                 "Freetype: Invalid PPEM"},
	{ESTATUS_FREETYPE_INVALID_VER_METRICS,          "Freetype: Invalid ver metrics"},
	{ESTATUS_FREETYPE_COULD_NOT_FOUND_CONTEXT,      "Freetype: Could not found context"},
	{ESTATUS_FREETYPE_INVALID_POST_TABLE_FORMAT,    "Freetype: Invalid post table format"},
	{ESTATUS_FREETYPE_INVALID_POST_TABLE,           "Freetype: Invalid post table"},

	{ESTATUS_FREETYPE_SYNTAX_ERROR,                 "Freetype: Syntax error"},
	{ESTATUS_FREETYPE_STACK_UNDERFLOW,              "Freetype: Stack underflow"},
	{ESTATUS_FREETYPE_IGNORE,                       "Freetype: Ignore"},

	{ESTATUS_FREETYPE_MISSING_STARTFONT_FIELD,      "Freetype: Missing startfont field"},
	{ESTATUS_FREETYPE_MISSING_FONT_FIELD,           "Freetype: Missing font field"},
	{ESTATUS_FREETYPE_MISSING_SIZE_FIELD,           "Freetype: Missing size field"},
	{ESTATUS_FREETYPE_MISSING_CHARS_FIELD,          "Freetype: Missing chars field"},
	{ESTATUS_FREETYPE_MISSING_STARTCHAR_FIELD,      "Freetype: Missing startchar field"},
	{ESTATUS_FREETYPE_MISSING_ENCODING_FIELD,       "Freetype: Missing encoding field"},
	{ESTATUS_FREETYPE_MISSING_BBX_FIELD,            "Freetype: Missing BBX field"},
	{ESTATUS_FREETYPE_BBX_TOO_BIG,                  "Freetype: BBX too big"},
	{ESTATUS_FREETYPE_CORRUPTED_FONT_HEADER,        "Freetype: Corrupted font header"},
	{ESTATUS_FREETYPE_CORRUPTED_FONT_GLYPHS,        "Freetype: Corrupted font glyphs"},

	{ESTATUS_FREETYPE_UNKNOWN_ERROR,				"Freetype: Unknown error"},

	// Control errors
	{ESTATUS_UI_UNKNOWN_CONTROL_TYPE,				"Unknown control type"},
	{ESTATUS_UI_CONTROL_NOT_REGISTERED,				"Control not registered"},
	{ESTATUS_UI_CONTROL_NOT_HIT,					"Control not hit"},
	{ESTATUS_UI_NO_AXIS,							"2D Has no axis"},
	{ESTATUS_UI_TOO_SMALL,							"Image size too small for parameters"},
	{ESTATUS_UI_TRACK_LENGTH_EXCEEDS_TRACK_SIZE,	"Track length exceeds track size"},
	{ESTATUS_UI_ITEM_NOT_FOUND,						"Item not found"},
	{ESTATUS_UI_UNHANDLED,							"UI Event unhandled"},
	{ESTATUS_UI_IMAGE_MISSING,						"Image data missing"},

	// Parse errors
	{ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED,			"Numeric value expected"},
	{ESTATUS_PARSE_EQUALS_EXPECTED,					"Equals expected"},
	{ESTATUS_PARSE_SEMICOLON_EXPECTED,				"Semicolon expected"},
	{ESTATUS_PARSE_STRING_EXPECTED,					"String expected"},
	{ESTATUS_PARSE_COMMA_EXPECTED,					"Comma expected"},
	{ESTATUS_PARSE_UINT_EXPECTED,					"Unsigned integer expected"},
	{ESTATUS_PARSE_INT_EXPECTED,					"Signed integer epxected"},
	{ESTATUS_PARSE_UINT8_EXPECTED,					"Unsigned 8 bit integer expected"},
	{ESTATUS_PARSE_INT8_EXPECTED,					"Signed 8 bit integer expected"},
	{ESTATUS_PARSE_UINT16_EXPECTED,					"Unsigned 16 bit integer expected"},
	{ESTATUS_PARSE_INT16_EXPECTED,					"Signed 16 bit integer expected"},
	{ESTATUS_PARSE_UINT32_EXPECTED,					"Unsigned 32 bit integer expected"},
	{ESTATUS_PARSE_INT32_EXPECTED,					"Signed 32 bit integer expected"},
	{ESTATUS_PARSE_UINT64_EXPECTED,					"Unsigned 64 bit integer expected"},
	{ESTATUS_PARSE_INT64_EXPECTED,					"Signed 64 bit integer expected"},
	{ESTATUS_PARSE_FLOAT_EXPECTED,					"Floating point # expected"},
	{ESTATUS_PARSE_DOUBLE_EXPECTED,					"Double precision floating point # expected"},
	{ESTATUS_PARSE_VALUE_OUT_OF_RANGE,				"Value out of range"},
	{ESTATUS_PARSE_ERROR,							"Parse error"},

	{ESTATUS_MODP_RESULTCODE_INVALID_COMMAND,			"Invalid command"},
	{ESTATUS_MODP_RESULTCODE_INVALID_PAYLOAD_LENGTH,	"Invalid payload length"},
	{ESTATUS_MODP_RESULTCODE_BAD_SEQUENCE,				"Bad sequence number"},
	{ESTATUS_MODP_RESULTCODE_TRAIN_IN_PROGRESS,			"Train in progress"},
	{ESTATUS_MODP_RESULTCODE_DEVICE_FAULT,				"Device fault"},
	{ESTATUS_MODP_RESULTCODE_COIL_OVER_TEMPERATURE,		"Coil over temperature"},
	{ESTATUS_MODP_RESULTCODE_NO_TRAIN_SEQUENCE,			"No train sequence"},
	{ESTATUS_MODP_RESULTCODE_NO_COIL_PRESENT,			"No coil present"},
	{ESTATUS_MODP_RESULTCODE_COIL_FAULT,				"Coil fault"},
	{ESTATUS_MODP_RESULTCODE_TIMING_FIELDS_INVALID,		"Timing fields invalid"},
	{ESTATUS_MODP_RESULTCODE_UNSUPPORTED_UNKNOWN_COIL,	"Unsupported or unknown coil"},

	// Terminator
	{ESTATUS_END_OF_STATUS}
};

// Lists of languages and status codes
typedef struct SStatusLanguages
{
	ELanguage eLanguage;
	const SStatusCodes *psStatusCodeHead;
} SStatusLanguages;

// Error codes in various languages
static const struct SStatusLanguages sg_sStatusLanguages[] =
{
	{ELANG_ENGLISH,			sg_sStatusCodes},
};

// Here in case the EStatus code is unknown. 
static char sg_eErrorText[30];

// This will return the text equivalent for an EStatus code. If it's not found, it will
// return a pointer to a rendered string buffer with "Unknown EStatus - 0xxxxxxxxx". Note
// that that part of this routine is NOT thread safe, but it's highly doubtful that
// there would be two critical segments of code that would fire off two unknown error
// codes at the same time (in which case, someone needs to fix it). It will also log it
// to syslog if it's not found.
char *GetErrorTextLanguage(EStatus eStatus,
						   ELanguage eLanguage)
{
	uint32_t u32Loop;
	const SStatusCodes *psStatus = NULL;
	const SStatusCodes *psEnglish = NULL;

	for (u32Loop = 0; u32Loop < (sizeof(sg_sStatusLanguages) / sizeof(sg_sStatusLanguages[0])); u32Loop++)
	{
		if (sg_sStatusLanguages[u32Loop].eLanguage == eLanguage)
		{
			psStatus = sg_sStatusLanguages[u32Loop].psStatusCodeHead;
		}

		if (ELANG_ENGLISH == sg_sStatusLanguages[u32Loop].eLanguage)
		{
			psEnglish = sg_sStatusLanguages[u32Loop].psStatusCodeHead;
		}
	}

	// If we didn't find our language, default to English
	if (NULL == psStatus)
	{
		psStatus = psEnglish;

		// English had BETTER be supported
		BASSERT(psEnglish);
	}

	while (psStatus->eStatus != ESTATUS_END_OF_STATUS)
	{
		if (psStatus->eStatus == eStatus)
		{
			return(psStatus->peStatusText);
		}

		++psStatus;
	}
	
	snprintf(sg_eErrorText, sizeof(sg_eErrorText) - 1, "Unknown EStatus - 0x%.8x", eStatus);
	(void) Syslog("%s\n", sg_eErrorText);
	return(sg_eErrorText);
}

// Get errors in whatever the default locale is
char *GetErrorTextLocale(EStatus eStatus)
{
	ELanguage eLanguage;
	
	// Default to English
	eLanguage = ELANG_ENGLISH;

	return(GetErrorTextLanguage(eStatus,
								ELANG_ENGLISH));
}

// Always return error text in English
char *GetErrorText(EStatus eStatus)
{
	return(GetErrorTextLanguage(eStatus,
								ELANG_ENGLISH));
}

static EStatus SharedFileInfoCallback(uint32_t u32EventMask, int64_t s64Message)
{
	SysEventAck(u32EventMask);
	
	return(ESTATUS_OK);
}

static EStatus SharedPostOSInit(uint32_t u32EventMask, int64_t sMessage)
{
	EStatus eStatus = ESTATUS_OK;
	
	SysEventAck(u32EventMask);
	
	return(eStatus);
}

void SharedPreOSInit(void)
{
	EStatus eStatus;

	eStatus = SysEventRegister(SYSEVENT_FILESYSTEM_INIT,
							   SharedFileInfoCallback);
	BASSERT(ESTATUS_OK == eStatus);
}

bool IsPointerValid(void *pvAddress,
					bool bIncludeCodeRegions)
{
	return(ArchIsPointerValid(pvAddress,
							  bIncludeCodeRegions));
}

char *SharedstrdupHeap(const char *peSource,
					   const char *peModuleName,
					   const char *peProcedureName,
					   uint32_t u32LineNumber)
{
	char *peResult = NULL;

	if (NULL == peSource)
	{
		goto errorExit;
	}
	
	peResult = (char *) SharedMemAlloc(strlen(peSource) + 1,
									   peProcedureName,
									   peModuleName,
									   u32LineNumber,
									   false);
	if (NULL == peResult)
	{
		goto errorExit;
	}
	
	strcpy(peResult, peSource);
	
errorExit:
	return(peResult);
}

// Convert a UTF-8 encoded character to the 32 bit representation
EUnicode UTF8toUnicode(const char* peString)
{
	uint8_t u8Char = (uint8_t)*peString;
	uint8_t u8Mask = 0xfe;
	uint8_t u8Match = 0xfc;
	uint8_t u8Length;
	uint8_t u8Index;
	EUnicode u32Character = 0;

	// If the upper bit is zero then it's a one byte standard ASCII character
	if( 0 == (u8Char & 0x80)  )
	{
		return(u8Char);
	}

	// Find UTF8 bit pattern at the top of the byte
	u8Length = 6;
	while( u8Length >= 2 )
	{
		if( (u8Char & u8Mask) == u8Match )
		{
			break;
		}

		u8Mask <<= 1;
		u8Match <<= 1;
		u8Length--;
	}

	if( u8Length < 2 )
	{
		goto errorExit;
	}

	// Grab the most significant bits from the remainder of the first byte
	u32Character = u8Char & ~u8Mask;
	peString++;

	// Assemble the rest
	u8Index = 1;
	while( u8Index < u8Length )
	{
		u8Char = (const uint8_t)*peString;

		// Validate the continuation byte indicator bits
		if( (u8Char & 0xc0) != 0x80 )
		{
			goto errorExit;
		}

		// Grab the 6 bits and add them to the character
		u32Character <<= 6;
		u32Character |= (u8Char & ~0xc0);

		u8Index++;
		peString++;
	}

	return(u32Character);

errorExit:
	// Return zero on error for malformed character
	return('?'); 
}

// Returns the byte count of the UTF8 escaped character
uint32_t UTF8charlen(const char* peString)
{
	const uint8_t u8Char = (const uint8_t)*peString;
	uint8_t u8Mask = 0xfe;
	uint8_t u8Match = 0xfc;
	uint8_t u8Length;

	// If the upper bit is zero then it's a one byte standard ASCII character
	if( 0 == (u8Char & 0x80)  )
	{
		return(1);
	}

	// Find UTF8 bit pattern at the top of the byte
	u8Length = 6;
	while( u8Length >= 2 )
	{
		if( (u8Char & u8Mask) == u8Match )
		{
			return(u8Length);
		}

		u8Mask <<= 1;
		u8Match <<= 1;
		u8Length--;
	}

	// Return zero on error (no match for bit patterns)
	return(0); 
}

// Return the number of characters in a string
//	This works for UTF-8 encoded and regular 7-bit ASCII
uint32_t UTF8strlen(const char *peString)
{
	uint32_t u32Count = 0;

	while (*peString) 
	{
		uint32_t u32CharLen;

		u32CharLen = UTF8charlen(peString);

		if( 0 == u32CharLen )
		{
			// Malformed string!  Return character count of zero.
			return(0);
		}

		u32Count++;
		peString += u32CharLen;
	}

	return(u32Count);
}

// Returns length of Unicode charcter (in bytes) when encoded to UTF-8
uint32_t UnicodeToUTF8Len(EUnicode eUnicode)
{
	if (eUnicode < 0x80)
	{
		return(1);
	}

	if (eUnicode < 0x800)
	{
		return(2);
	}

	if (eUnicode < 0x10000)
	{
		return(3);
	}

	if (eUnicode < 0x110000)
	{
		return(4);
	}

	// No idea
	return(0);
}

// Encodes a unicode character to UTF8
uint32_t UnicodeToUTF8(char *peOutput,
					 EUnicode eUnicode)
{
	if (eUnicode < 0x80)
	{
		*(peOutput) = (char) eUnicode;
	}
	else
	if (eUnicode < 0x800)
	{
		*(peOutput) = (char) (((eUnicode >> 6) & 0x1f) | 0xc0);
		*(peOutput + 1) = (char) (((eUnicode >> 0) & 0x3f) | 0x80);
	}
	else
	if (eUnicode < 0x10000)
	{
		*(peOutput) = (char) (((eUnicode >> 12) & 0x0f) | 0xe0);
		*(peOutput + 1) = (char) (((eUnicode >>  6) & 0x3f) | 0x80);
		*(peOutput + 2) = (char) (((eUnicode >>  0) & 0x3f) | 0x80);
	}
	else
	if (eUnicode < 0x110000)
	{
		*(peOutput) = (char) (((eUnicode >> 18) & 0x07) | 0xF0);
		*(peOutput + 1) = (char) (((eUnicode >> 12) & 0x3F) | 0x80);
		*(peOutput + 2) = (char) (((eUnicode >>  6) & 0x3F) | 0x80);
		*(peOutput + 3) = (char) (((eUnicode >>  0) & 0x3F) | 0x80);
	}
	else
	{
		// No idea
	}

	return(UnicodeToUTF8Len(eUnicode));
}

void StripWhitespace(char *peStringStart)
{
	char *peString = peStringStart;
	uint32_t u32Len;

	if (NULL == peString)
	{
		return;
	}

	while ((*peString) && 
		   (isspace(*peString)))
	{
		peString++;
	}

	if (peString != peStringStart)
	{
		strcpy((char *) peStringStart, (char *) peString);
	}

	u32Len = (uint32_t) strlen(peStringStart);
	if (u32Len)
	{
		peString = peStringStart + (u32Len - 1);

		while (u32Len)
		{
			if (isspace(*peString))
			{
				u32Len--;
				--peString;
			}
			else
			{
				++peString;
				break;
			}
		}

		*peString = '\0';
	}
}

char *SharedstrncpyWhitespace(char *peSrcHead,
						   uint32_t u32MaxLen)
{
	char *peSrcPtr = peSrcHead;
	char *peStartPtr = peSrcHead;
	char *peNewString = NULL;
	uint32_t u32Count = 0;

	// First, figure out the length of the string, either capped by u32MaxLen or
	// by a 0x00
	while (u32Count < u32MaxLen)
	{
		if ('\0' == *peSrcPtr)
		{
			break;
		}

		u32Count++;
		++peSrcPtr;
	}

	u32MaxLen = u32Count;
	if (0 == u32MaxLen)
	{
		// Nothing in the string
		return(NULL);
	}

	// Now find the first non-space character
	u32Count = 0;
	while (u32Count < u32MaxLen)
	{
		if ((*peStartPtr != ' ') &&
			(*peStartPtr != '\t') &&
			(*peStartPtr != '\n') &&
			(*peStartPtr != '\r'))
		{
			// Found it
			break;
		}

		u32Count++;
		++peStartPtr;
	}

	// If this is true, it's all whitespace
	if (u32Count == u32MaxLen)
	{
		return(NULL);
	}

	u32MaxLen -= u32Count;
	u32Count = u32MaxLen;
	peSrcPtr = peStartPtr + u32Count;

	// Count backward and ditch any trailing whitespace
	while (u32Count)
	{
		--peSrcPtr;

		if ((*peSrcPtr != ' ') &&
			(*peSrcPtr != '\t') &&
			(*peSrcPtr != '\n') &&
			(*peSrcPtr != '\r'))
		{
			// Found it
			break;
		}

		--u32Count;
	}

	// This should always be nonzero. If it's not, then finding the first non-space
	// character above didn't work properly.
	BASSERT(u32Count);

	peNewString = MemAllocNoClear(u32Count + 1);
	if (NULL == peNewString)
	{
		DebugOut("Out of memory\n");
		return(NULL);
	}

	strncpy(peNewString, peStartPtr, u32Count);

	// NULL Terminate it
	*(peNewString + u32Count) = '\0';

	return(peNewString);
}

void strcat_ptr(char **ppeString, 
			   char *peStringToAdd)
{
	char *peToPtr = *ppeString;

	while (*peStringToAdd)
	{
		*peToPtr = *peStringToAdd;
		++peToPtr;
		++peStringToAdd;
	}

	*peToPtr = '\0';
	*ppeString = peToPtr;
}

// Returns the length of the destinatino path
uint64_t PathExtract(char *peSourceFilename,
				   char *peDestinationPath,
				   uint64_t u64DestinationPathBufferLength)
{
	uint64_t u64Length;
	char *pePtr = NULL;

	*peDestinationPath = '\0';

	if (NULL == peSourceFilename)
	{
		return(0);
	}

	u64Length = strlen(peSourceFilename);
	pePtr = peSourceFilename + u64Length;

	// Run backwards through the end of the string and stop when we hit
	// a /, \, or :
	while (u64Length)
	{
		--pePtr;
		if (('/' == *pePtr) ||
			('\\' == *pePtr) ||
			(':' == *pePtr))
		{
			// Found it!
			break;
		}

		u64Length--;
	}

	if (0 == u64Length)
	{
		// No path
		return(0);
	}

	if ((u64Length + 1) > u64DestinationPathBufferLength)
	{
		u64Length = u64DestinationPathBufferLength - 1;
	}
	else
	{
		// It'll fit
	}

	// All good
	strncpy(peDestinationPath, peSourceFilename, u64Length);
	*(peDestinationPath + u64Length) = '\0';
	return(u64Length);
}


void FilenameStripPath(char *peFilenameHead)
{
	char *peFilenamePtr = NULL;

	if ((NULL == peFilenameHead) ||
		('\0' == *peFilenameHead))
	{
		return;
	}

	peFilenamePtr = peFilenameHead + strlen(peFilenameHead) - 1;

	while (peFilenamePtr != peFilenameHead)
	{
		--peFilenamePtr;

		if (('/' == *peFilenamePtr) ||
			('\\' == *peFilenamePtr))
		{
			++peFilenamePtr;
			strcpy(peFilenameHead, peFilenamePtr);
			return;
		}
	}

	// No 
}

bool IsWildcardFilename(char *peWildcard)
{
	if (peWildcard)
	{
		while (*peWildcard)
		{
			if (('?' == *peWildcard) ||
				('*' == *peWildcard))
			{
				return(true);
			}

			++peWildcard;
		}
	}

	return(false);
}

static char *sg_peFilenameIllegalChars="\"*?|\\/:<>";

void FilenameValidate(char *peFilename)
{
	while (*peFilename)
	{
		// If it's in the illegal list, replace it with an underscore
		if (strchr(sg_peFilenameIllegalChars, *peFilename))
		{
			*peFilename = '_';
		}

		++peFilename;
	}
}

// This will sprintf to a buffer, and return NULL if out of memory
char *sprintfMemAlloc(char *peFormatString,
					  ...)
{
	char eString[4096];
	char *peReturnString = NULL;
	va_list sArgs;

	va_start(sArgs, peFormatString);
	vsnprintf(eString, sizeof(eString) - 1, (const char *) peFormatString, sArgs);
	va_end(sArgs);

	peReturnString = MemAllocNoClear(strlen(eString) + 1);
	if (peReturnString)
	{
		strcpy(peReturnString, eString);
	}

	return(peReturnString);
}

static SOSCriticalSection sg_sRandomNumberCriticalSection;

uint64_t SharedRandomNumber(void)
{
	uint64_t u64RandomNumber;
	EStatus eStatus;

	OSCriticalSectionEnter(sg_sRandomNumberCriticalSection);
	eStatus = ArchRandomNumber(&u64RandomNumber);
	BASSERT(ESTATUS_OK == eStatus);
	OSCriticalSectionLeave(sg_sRandomNumberCriticalSection);

	return(u64RandomNumber);
}


static uint64_t sg_u64StartTime;

void SharedTimeToText(char *peTextBuffer,
					  uint32_t u32TextBufferSize,
					  uint64_t u64Time)
{
	struct tm *psTime;
	time_t eTime;

	// Convert to time_t
	eTime = (time_t) (u64Time / 1000);

	psTime = localtime((const time_t *) &eTime);

	snprintf(peTextBuffer, 
			 (size_t) (u32TextBufferSize - 1), 
			 "%s %2d %.2d:%.2d:%.2d.%.3d",
			 sg_peMonths[psTime->tm_mon],
			 psTime->tm_mday,
			 psTime->tm_hour,
			 psTime->tm_min,
			 psTime->tm_sec,
			 (int) (u64Time % 1000));


}

void SharedDateTimeToString(uint64_t u64Timestamp,
							char *peTargetBuffer,
							uint32_t u32TargetBufferLength,
							bool bLocalTime)
{
	struct tm *psTime;
	time_t eTime;

	eTime = (time_t) (u64Timestamp / 1000);

	if (bLocalTime)
	{
		psTime = localtime((const time_t *) &eTime);
	}
	else
	{
		psTime = gmtime((const time_t *) &eTime);
	}

	snprintf(peTargetBuffer, (size_t) (u32TargetBufferLength - 1), "%.2u-%.2u-%.2u %.2u:%.2u:%.2u",
			 psTime->tm_mday,
			 psTime->tm_mon + 1,
			 psTime->tm_year + 1900,
			 psTime->tm_hour,
			 psTime->tm_min,
			 psTime->tm_sec);
}

#ifdef _SERVER
EStatus SharedGUIDCheck(SGUID *psGUID,
						SGUID *psInstanceGUID,
						ETables eTableExpected)
{
	EStatus eStatus = ESTATUS_OK;
	ETables eTable;
	bool bInstance;

	if (psGUID)
	{
		if (GUIDIsNULL(psGUID))
		{
			eStatus = ESTATUS_GUID_MISSING;
			goto errorExit;
		}

		eTable = GUIDGetInfo(psGUID,
							 NULL,
							 &bInstance);
		if (eTable != eTableExpected)
		{
			eStatus = ESTATUS_GUID_TYPE_MISMATCH;
			goto errorExit;
		}

		if (bInstance)
		{
			eStatus = ESTATUS_INSTANCE_IN_GUID;
			goto errorExit;
		}
	}
	else
	{
		// No instance GUID if there's no main GUID
		psInstanceGUID = NULL;
	}

	if (psInstanceGUID)
	{
		if (GUIDIsNULL(psInstanceGUID))
		{
			eStatus = ESTATUS_INSTANCE_GUID_MISSING;
			goto errorExit;
		}

		eTable = GUIDGetInfo(psInstanceGUID,
							 NULL,
							 &bInstance);
		if (eTable != eTableExpected)
		{
			eStatus = ESTATUS_GUID_TYPE_MISMATCH;
			goto errorExit;
		}

		if (false == bInstance)
		{
			eStatus = ESTATUS_GUID_IN_INSTANCE;
			goto errorExit;
		}
	}

errorExit:
	return(eStatus);
}
#endif

static SOSCriticalSection sg_sIDLock;
static uint32_t sg_u32IncrementingID = 1;

uint32_t SharedGetID(void)
{
	uint32_t u32ID;

	OSCriticalSectionEnter(sg_sIDLock);

	do
	{
		u32ID = sg_u32IncrementingID;
		sg_u32IncrementingID++;
	} while( 0 == u32ID );

	OSCriticalSectionLeave(sg_sIDLock);
	return(u32ID);
}

static bool sg_bShutdownFlagged = false;

// Called from the Arch layer when a program terminates or
// is being signaled to terminate
bool SharedTerminate(void)
{
	if (sg_bShutdownFlagged)
	{
		DebugOut("Still shuting down - we heard you the first time!\n");
		return(false);
	}

	sg_bShutdownFlagged = true;

#ifdef _SERVER
	// Shut down the session stuff
	SessionShutdown();

	// Shut down server<->server sync 
	ServerSyncShutdown();

	// Shut down system report
	SystemReportShutdown();
#endif

	// Shut down the syslog
	SyslogShutdown();

	return(true);
}

EStatus SharedInit(char *peProgramName,
				   const SCmdLineOption *psCommandLineOptions,
				   char *peFullCommandLineOption,
				   int argc,
				   char **argv)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32CPUCores;
	uint32_t u32ThreadedCores;
	char *peBaseFilename;
	uint32_t u32LogDepth;
	bool bConsoleOutput;

	// Make sure the basics are working the way we think they are
	SharedBasicCheck();

	// Record start time
	sg_u64StartTime = RTCGet();

	// Make the Data directory if it isn't there
	(void) Filemkdir(DATA_DIR);

	// Critical section for the random number generator
	eStatus = OSCriticalSectionCreate(&sg_sRandomNumberCriticalSection);
	ERR_GOTO();

	// Critical section for the ever-incrementing ID
	eStatus = OSCriticalSectionCreate(&sg_sIDLock);
	ERR_GOTO();

	// Init the command line option stuff
	if (argv)
	{
		// argc/argv style command line init
		eStatus = CmdLineInitArgcArgv(argc,
									  argv,
									  psCommandLineOptions,
									  peProgramName);
	}
	else
	{
		// Single command line
		eStatus = CmdLineInit(peFullCommandLineOption,
							  psCommandLineOptions,
							  peProgramName);
	}

	ERR_GOTO();

#ifdef _SERVER
	// Early session init
	eStatus = SessionInitEarly();
	ERR_GOTO();

	// Init server related stuff
	eStatus = SharedServerInit();
	ERR_GOTO();

#endif
	eStatus = PlatformGetSyslogInfo(&peBaseFilename,
									&u32LogDepth,
									&bConsoleOutput);

	if (ESTATUS_OK == eStatus)
	{
		eStatus = SyslogInit(peBaseFilename,
							 u32LogDepth,
							 bConsoleOutput);

		if (eStatus != ESTATUS_OK)
		{
			DebugOut("Failed to init Syslog - %s\n", GetErrorText(eStatus));
			BASSERT(ESTATUS_OK == eStatus);
		}

		(void) Syslog("Syslog file turned over\n");
	}
	else
	{
		DebugOut("Syslog functionality not supported on this platform\n");
	}

#ifdef _SERVER
	// Needs to come after server init so we get our tags
	eStatus = SystemReportInit();
	ERR_GOTO();

	// Init the database
	eStatus = DatabaseInit();
	ERR_GOTO();
#endif

	// Shared misc command module
	eStatus = SharedMiscInit();
	ERR_GOTO();

#ifdef _SERVER
	eStatus = SMTPLibInit();
	ERR_GOTO();

	// This handles init of the jobs handler (if launched)
	eStatus = ServerJobInit();
	ERR_GOTO();

	// And server EEG
	eStatus = ServerEEGInit();
	ERR_GOTO();
#endif

	eStatus = HandlePoolInit();
	ERR_GOTO();

	eStatus = PlatformCPUGetInfo(&u32CPUCores,
								 &u32ThreadedCores);

	if (ESTATUS_OK == eStatus)
	{
		Syslog("# Of CPU cores=%u, # of threaded CPU cores=%u\n", u32CPUCores, u32ThreadedCores);
	}
	else
	if (ESTATUS_FUNCTION_NOT_SUPPORTED == eStatus)
	{
		// It's actually OK if we can't get all the CPU info
		eStatus = ESTATUS_OK;
	}
	else
	{
		Syslog("Can't get CPU info - %s\n", GetErrorText(eStatus));
	}

#ifdef _SERVER
	eStatus = TelnetServerInitPreOS();
	ERR_GOTO();
#endif

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		Syslog("SharedInit() failed to initialize - %s\n", GetErrorText(eStatus));
	}

	return(eStatus);
}

// Returns either a string or the word NULL that's \0 terminated
char *SharedStringOrNULL(char *peString)
{
	if (peString)
	{
		return(peString);
	}
	else
	{
		return("NULL");
	}
}

int Sharedstrcasecmp(const char *peString1Head, const char *peString2Head)
{
	const unsigned char *peString1 = (const unsigned char *) peString1Head;
	const unsigned char *peString2 = (const unsigned char *) peString2Head;
	unsigned char eChar1;
	unsigned char eChar2;

	if (peString1 == peString2)
	{
		// Well, of course they're equal
		return(0);
	}

	if (peString1 && (NULL == peString2))
	{
		// String 1 comes before string 2
		return(-1);
	}

	if ((NULL == peString1) && peString2)
	{
		// String 2 comes after string 1
		return(1);
	}

	while (1)
	{
		eChar1 = (unsigned char) (tolower(*peString1));
		eChar2 = (unsigned char) (tolower(*peString2));
		++peString1; 
		++peString2; 

		if ((eChar1 != eChar2) ||
			(0x00 == eChar1))
		{
			// Break out if we're at the end of the string or they don't match
			break;
		}
	}

	return(eChar1 - eChar2);
}

const char *Sharedstrcasestr(const char *peHaystack, const char *peNeedle)
{
	while (*peHaystack)
	{
		const char *peHaystackPtr;
		const char *peNeedlePtr;

		peNeedlePtr = peNeedle;
		peHaystackPtr = peHaystack;

		while ((*peNeedlePtr) &&
			   (*peHaystackPtr))
		{
			if (toupper(*peNeedlePtr) != toupper(*peHaystackPtr))
			{
				break;
			}

			++peHaystackPtr;
			++peNeedlePtr;
		}

		if ('\0' == *peNeedlePtr)
		{
			return(peHaystackPtr);
		}

		++peHaystack;
	}

	// Not found
	return(NULL);
}


// This takes a GUID and ASCII-izes it into peGUIDString with a max # of characters in 
void SharedGUIDToASCII(SGUID *psGUID,
					   char *peGUIDString,
					   uint32_t u32GUIDStringBufferLength)
{
	uint32_t u32GUIDBytes = sizeof(psGUID->u8GUID);
	uint8_t *pu8GUIDByte = psGUID->u8GUID;

	if (u32GUIDStringBufferLength)
	{
		--u32GUIDStringBufferLength;
		while ((u32GUIDStringBufferLength >= 2) && (u32GUIDBytes))
		{
			sprintf(peGUIDString, "%.2x", *pu8GUIDByte);
			++pu8GUIDByte;
			peGUIDString += 2;
			u32GUIDStringBufferLength -= 2;
			u32GUIDBytes--;
		}
	}
}

// Day must be 1-31
// Month must be 1-12
// Year is full year
//
// Data is packed as follows:
//
// Byte 0		- Day (0-30)
// Byte 1		- Month (0-11)
// Bytes 2&3	- Year
//

uint32_t SharedPackDate(uint8_t u8Day, 
					  uint8_t u8Month, 
					  uint16_t u16Year)
{
	// Make sure day is in range
	BASSERT((u8Day > 0) && (u8Day < 32));
	u8Day--;

	// And the month
	BASSERT(u8Month);
	BASSERT((u8Month > 0) && (u8Month < 32));
	u8Month--;

	return(u8Day | ((uint32_t) u8Month << 8) | ((uint32_t) u16Year << 16));
}

static const uint8_t sg_u8DaysInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

uint8_t SharedDaysInMonth(uint8_t u8Month)
{
	BASSERT(u8Month < (sizeof(sg_u8DaysInMonth) / sizeof(sg_u8DaysInMonth[0])));
	return(sg_u8DaysInMonth[u8Month]);
}

bool SharedIsLeapYear(uint16_t u16Year)
{
	if ((u16Year % 400) == 0)
	{
		return(true);
	}
	else
	if ((u16Year % 100) == 0)
	{
		return(false);
	}
	else
	if ((u16Year % 4) == 0)
	{
		return(true);
	}

	return(false);
}

// Returns true if the system is in test mode
bool SharedIsInTestMode(void)
{
	if (CmdLineOption("-test") ||
		CmdLineOption("-localdb"))
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

EStatus SharedConsoleLineInput(char *peBuffer,
							   uint32_t u32MaxLength,
							   bool bPasswordEntry)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32Index = 0;
	uint8_t u8Key;

	*peBuffer = '\0';

	for (;;)
	{
		// Go get a key
		u8Key = (uint8_t) getch();

		if (0x03 == u8Key)
		{
			// Ctrl-C
			DebugOut("^C\n");
			eStatus = ESTATUS_ABORTED;
			break;
		}
		else
		if ('\r' == u8Key)
		{
			// Carriage return
			DebugOut("\n");
			eStatus = ESTATUS_OK;
			break;
		}
		else
		if (u8Key < ' ')
		{
			// Ignore the character
		}
		else
		if ('\b' == u8Key)
		{
			// Backspace
			if (u32Index)
			{
				// Backspace
				DebugOut("\b \b");
				--u32Index;
			}
		}
		else
		{
			// It might be valid. Let's see.
			if ((u32Index + 1) < u32MaxLength)
			{
				// Room in the buffer.
				peBuffer[u32Index++] = (char) u8Key;

				if (bPasswordEntry)
				{
					u8Key = '*';
				}

				DebugOut("%c", u8Key);
			}
			else
			{
				// No room. Ignore it.
			}
		}
	}

	// Stick a NULL at the end
	peBuffer[u32Index] = '\0';

	return(eStatus);
}

// Returns true if the string is numeric
bool SharedIsStringNumeric(char *peString)
{
	if (NULL == peString)
	{
		return(false);
	}

	while (*peString)
	{
		if ((*peString < '0') || (*peString > '9'))
		{
			return(false);
		}

		++peString;
	}

	return(true);
}

char *Sharedstrdup(char *peInputString)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32Length;
	char *peString;

	if (NULL == peInputString)
	{
		return(NULL);
	}

	u32Length = (uint32_t) strlen(peInputString);
	MEMALLOC_NO_CLEAR(peString, u32Length + 1);
	memcpy((void *) peString, (void *) peInputString, u32Length);
	peString[u32Length] = '\0';

errorExit:
	return(peString);
}

// Clears memory as fast as possible
void SharedZeroMemoryFast(void *pvDataBase,
						  uint64_t u64Length)
{
	if (NULL == pvDataBase)
	{
		return;
	}

#ifdef _WIN32
	ZeroMemory(pvDataBase,
			   u64Length);
#else
	if (1 == u64Length)
	{
		*((uint8_t *) pvDataBase) = 0;
	}
	else
	if (2 == u64Length)
	{
		*((uint16_t *) pvDataBase) = 0;
	}
	else
	if (4 == u64Length)
	{
		*((uint32_t *) pvDataBase) = 0;
	}
	else
	if (8 == u64Length)
	{
		*((uint64_t *) pvDataBase) = 0;
	}
	else
	{
		memset(pvDataBase, 0, u64Length);
	}
#endif
}

#ifdef _WIN32
int strcasecmp(const char *peString1, const char *peString2)
{
	int8_t s8Result;
	const char *pePtr1 = peString1;
	const char *pePtr2 = peString2;

	// If the string pointers are the same, just return
	if (pePtr1 == pePtr2)
	{
		return(0);
	}

	while ((s8Result = toupper(*pePtr1) - toupper(*pePtr2)) == 0)
	{
		if ('\0' == *pePtr1)
		{
			break;
		}
		++pePtr1;
		++pePtr2;
	}

	return(s8Result);
}
#endif