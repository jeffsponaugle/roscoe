#include <ctype.h>
#include "Shared/Shared.h"
#include "Arch/Windows/WindowsArch.h"
#include "Shared/Windows/WindowsTools.h"
#include <errno.h>

// Translates errno->eStatus result codes

typedef struct SErrnoToEStatus
{
	int s32ErrnoValue;
	EStatus eStatus;
} SErrnoToEStatus;

static const SErrnoToEStatus sg_sErrnoEstatusTable[] =
{
	{EPERM,				ESTATUS_ERRNO_OPERATION_NOT_PERMITTED},
	{ENOENT,			ESTATUS_NO_FILE},
	{ESRCH,				ESTATUS_ERRNO_NO_SUCH_PROCESS},
	{EINTR,				ESTATUS_ERRNO_INTERRUPTED_SYSTEM_CALL},
	{EIO,				ESTATUS_ERRNO_IO_ERROR},
	{ENXIO,				ESTATUS_ERRNO_DEVICE_NOT_CONFIGURED},
	{E2BIG,				ESTATUS_ERRNO_ARGUMENT_LIST_TOO_LONG},
	{ENOEXEC,			ESTATUS_ERRNO_EXEC_FORMAT_ERROR},
	{EBADF,				ESTATUS_ERRNO_BAD_FILE_DESCRIPTOR},
	{ECHILD,			ESTATUS_ERRNO_NO_CHILD_PROCESSES},
	{EAGAIN,          	ESTATUS_ERRNO_RESOURCE_TEMPORARILY_UNAVAILABLE},
	{ENOMEM,			ESTATUS_OUT_OF_MEMORY},
	{EACCES,			ESTATUS_ERRNO_PERMISSION_DENIED},
	{EFAULT,			ESTATUS_ERRNO_BAD_ADDRESS},
	{EBUSY,				ESTATUS_ERRNO_DEVICE_BUSY},
	{EEXIST,			ESTATUS_EXIST},
	{EXDEV,				ESTATUS_ERRNO_CROSS_DEVICE_LINK},
	{ENODEV,			ESTATUS_ERRNO_OPERATION_NOT_SUPPORTED},
	{ENOTDIR,			ESTATUS_ERRNO_NOT_A_DIRECTORY},
	{EISDIR,			ESTATUS_ERRNO_IS_A_DIRECTORY},
	{ENFILE,			ESTATUS_TOO_MANY_OPEN_FILES},
	{EMFILE,			ESTATUS_TOO_MANY_OPEN_FILES},
	{ENOTTY,			ESTATUS_ERRNO_INAPPROPRIATE_IOCTL},
	{EFBIG,				ESTATUS_ERRNO_FILE_TOO_LARGE},
	{ENOSPC,			ESTATUS_ERRNO_NO_SPACE_LEFT_ON_DEVICE},
	{ESPIPE,			ESTATUS_ERRNO_ILLEGAL_SEEK},
	{EROFS,				ESTATUS_ERRNO_READ_ONLY_FILESYSTEM},
	{EMLINK,			ESTATUS_ERRNO_TOO_MANY_LINKS},
	{EPIPE,				ESTATUS_ERRNO_BROKEN_PIPE},
	{EDOM,				ESTATUS_ERRNO_NUMERICAL_ARGUMENT_OUT_OF_DOMAIN},
	{EDEADLK, 			ESTATUS_ERRNO_EDEADLK},
	{ENAMETOOLONG,     	ESTATUS_ERRNO_FILENAME_TOO_LONG},
	{ENOLCK,         	ESTATUS_ERRNO_NO_LOCKS_AVAILABLE},
	{ENOSYS,         	ESTATUS_FUNCTION_NOT_SUPPORTED},
	{ENOTEMPTY,         ESTATUS_ERRNO_DIRECTORY_NOT_EMPTY},

	// Secure CRT functions
	{EINVAL,			ESTATUS_INVALID_PARAMETER},
	{ERANGE,			ESTATUS_ERRNO_RESULTS_TOO_LARGE},
	{EILSEQ,            ESTATUS_ERRNO_ILLEGAL_BYTE_SEQUENCE},
	{STRUNCATE,			ESTATUS_ERRNO_STRUNCATE},

	// Support for compatability with older MS-C versions
	{EDEADLOCK, 		ESTATUS_ERRNO_EDEADLK},

	// POSIX supplement
	{EADDRINUSE,      	ESTATUS_ERRNO_ADDRESS_ALREADY_IN_USE},
	{EADDRNOTAVAIL,     ESTATUS_ERRNO_CANT_ASSIGN_REQUESTED_ADDRESS},
	{EAFNOSUPPORT,		ESTATUS_ERRNO_ADDRESS_FAMILY_NOT_SUPPORTED},
	{EALREADY,       	ESTATUS_ERRNO_OPERATION_ALREADY_IN_PROGRESS},
	{EBADMSG,        	ESTATUS_ERRNO_BAD_MESSAGE},
	{ECANCELED,         ESTATUS_ERRNO_OPERATION_CANCELED},
	{ECONNABORTED,    	ESTATUS_ERRNO_SOFTWARE_CAUSED_CONNECTIONABORT},
	{ECONNREFUSED,    	ESTATUS_ERRNO_CONNECTION_REFUSED},
	{ECONNRESET,        ESTATUS_ERRNO_CONNECTION_RESET_BY_PEER},
	{EDESTADDRREQ,    	ESTATUS_ERRNO_DESTIONATION_ADDRESS_REQUIRED},
	{EHOSTUNREACH,    	ESTATUS_ERRNO_NO_ROUTE_TO_HOST},
	{EIDRM,             ESTATUS_ERRNO_IDENTIFIER_REMOVED},
	{EINPROGRESS,     	ESTATUS_ERRNO_OPERATION_IN_PROGRESS},
	{EISCONN,         	ESTATUS_ERRNO_SOCKET_ALREADY_CONNECTED},
	{ELOOP,             ESTATUS_ERRNO_TOO_MANY_LEVELS_OF_SYMBOLIC_LINKS},
	{EMSGSIZE,       	ESTATUS_ERRNO_MESSAGE_TOO_LONG},
	{ENETDOWN,       	ESTATUS_ERRNO_NETWORK_IS_DOWN},
	{ENETRESET,         ESTATUS_ERRNO_NETWORK_DROPPED_CONNECTION_ON_RESET},
	{ENETUNREACH,     	ESTATUS_ERRNO_NETWORK_UNREACHABLE},
	{ENOBUFS,        	ESTATUS_ERRNO_NO_BUFFER_SPACE_AVAILABLE},
	{ENODATA, 			ESTATUS_ERRNO_ENODATA},
	{ENOLINK,        	ESTATUS_ERRNO_LINK_HAS_BEEN_SEVERED},
	{ENOMSG,            ESTATUS_ERRNO_NO_MSG_OF_DESIRED_TYPE},
	{ENOPROTOOPT,     	ESTATUS_ERRNO_PROTOCOL_NOT_AVAILABLE},
	{ENOSR, 			ESTATUS_ERRNO_ENOSR},
	{ENOSTR, 			ESTATUS_ERRNO_ENOSTR},
	{ENOTCONN,       	ESTATUS_ERRNO_SOCKET_NOT_CONNECTED},
	{ENOTRECOVERABLE,  	ESTATUS_ERRNO_STATE_NOT_RECOVERABLE},
	{ENOTSOCK,       	ESTATUS_ERRNO_SOCKET_OP_ON_NON_SOCKET},
	{ENOTSUP,        	ESTATUS_ERRNO_OPERATION_NOT_SUPPORTED},
	{EOPNOTSUPP,      	ESTATUS_ERRNO_OPERATION_NOT_SUPPORTED},
	// EOTHER
	{EOVERFLOW,         ESTATUS_ERRNO_VALUE_TOO_LARGE_TO_BE_STORED},
	{EOWNERDEAD,        ESTATUS_ERRNO_PREVIOUS_OWNER_DIED},
	{EPROTO,         	ESTATUS_ERRNO_PROTOCOL_ERROR},
	{EPROTONOSUPPORT,  	ESTATUS_ERRNO_PROTOCOL_NOT_SUPPORTED},
	{EPROTOTYPE,      	ESTATUS_ERRNO_PROTOCOL_WRONG_TYPE_FOR_SOCKET},
	{ETIME, 			ESTATUS_ERRNO_ETIME},
	{ETIMEDOUT,         ESTATUS_TIMEOUT},
	{EWOULDBLOCK, 		ESTATUS_ERRNO_RESOURCE_TEMPORARILY_UNAVAILABLE},
	{ETXTBSY,			ESTATUS_ERRNO_TEXT_FILE_BUSY},

	// End of list!
	{-1}
};

EStatus ErrnoToEStatus(int s32Errno)
{
	UINT32 u32Loop;
	
	// See if we can find an equivalent errno
	for (u32Loop = 0; u32Loop < (sizeof(sg_sErrnoEstatusTable) / sizeof(sg_sErrnoEstatusTable[0])); u32Loop++)
	{
		if (s32Errno == sg_sErrnoEstatusTable[u32Loop].s32ErrnoValue)
		{
			return(sg_sErrnoEstatusTable[u32Loop].eStatus);
		}
	}

	return(ESTATUS_ERRNO_UNKNOWN);
}


// Translates Windows WSA errors->eStatus result codes

typedef struct SWindowsErrorToEStatus
{
	int s32WindowsError;
	EStatus eStatus;
} SWindowsErrorToEStatus;

#if defined(_WIN32)

// Windows errors to EStatus
static const SWindowsErrorToEStatus sg_sWindowsErrorToEStatusTable[] =
{
	{ERROR_SUCCESS,								ESTATUS_OK},
	{ERROR_FILE_NOT_FOUND,						ESTATUS_NO_FILE},
	{ERROR_OUTOFMEMORY,							ESTATUS_OUT_OF_MEMORY},
};

// WSA Errors to EStatus
static const SWindowsErrorToEStatus sg_sWindowsWSAErrorToEStatusTable[] =
{
	{WSAECONNRESET,								ESTATUS_ERRNO_CONNECTION_RESET_BY_PEER},
	{WSAETIMEDOUT,								ESTATUS_TIMEOUT},
	{WSAECONNREFUSED,							ESTATUS_ERRNO_CONNECTION_REFUSED},
};
#else
#error A definition of sg_sWindowsErrorToEStatusTable is needed for this operating system
#endif

// Handles Windows WSA errors
EStatus WindowsWSAErrorsToEStatus(int s32WindowsWSAError)
{
	UINT32 u32Loop;
	
	// See if we can find an equivalent Windows error to EStatus
	for (u32Loop = 0; u32Loop < (sizeof(sg_sWindowsWSAErrorToEStatusTable) / sizeof(sg_sWindowsWSAErrorToEStatusTable[0])); u32Loop++)
	{
		if (s32WindowsWSAError == sg_sWindowsWSAErrorToEStatusTable[u32Loop].s32WindowsError)
		{
			return(sg_sWindowsWSAErrorToEStatusTable[u32Loop].eStatus);
		}
	}

	Syslog("Couldn't find WSA error - 0x%.8x\n", s32WindowsWSAError);
	return(ESTATUS_ERRNO_UNKNOWN);
}

// Handles Windows errors
EStatus WindowsErrorsToEStatus(int s32WindowsError)
{
	UINT32 u32Loop;
	
	// See if we can find an equivalent Windows error to EStatus
	for (u32Loop = 0; u32Loop < (sizeof(sg_sWindowsErrorToEStatusTable) / sizeof(sg_sWindowsErrorToEStatusTable[0])); u32Loop++)
	{
		if (s32WindowsError == sg_sWindowsErrorToEStatusTable[u32Loop].s32WindowsError)
		{
			return(sg_sWindowsErrorToEStatusTable[u32Loop].eStatus);
		}
	}

	Syslog("Couldn't find Windows error - 0x%.8x\n", s32WindowsError);
	return(ESTATUS_ERRNO_UNKNOWN);
}
