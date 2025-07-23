#ifndef _OS_H_
#define _OS_H_

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Return codes
typedef enum
{
	ESTATUS_OK,

	// Misc errors
	ESTATUS_OUT_OF_MEMORY,
	ESTATUS_MISSING_FUNCTION,
	ESTATUS_BAD_LENGTH,
	ESTATUS_CTRL_C,
	ESTATUS_TIMEOUT,
	ESTATUS_CANCELED,
	ESTATUS_FUNCTION_NOT_SUPPORTED,
	ESTATUS_TRANSFER_COMPLETE,
	ESTATUS_UNKNOWN_ERROR_CODE,
	ESTATUS_NO_MORE_DATA,

	// Serial port result codes
	ESTATUS_SERIAL_HANDLE_OUT_OF_RANGE,

	// Console result codes
	ESTATUS_CONSOLE_ENUM_INVALID,

	// System result codes
	ESTATUS_SYSTEM_NOT_SUPERVISOR_CPU,

	// Flash programming result codes
	ESTATUS_FLASH_PROGRAMMING_FAULT,

	// Monitor result codes
	ESTATUS_MONITOR_START_LARGER_THAN_END_ADDRESS,
	ESTATUS_MONITOR_ADDRESS_TOO_HIGH,
	ESTATUS_MONITOR_ADDRESS_EXPECTED,
	ESTATUS_MONITOR_EQUALS_EXPECTED,
	ESTATUS_MONITOR_DATA_EXPECTED,
	ESTATUS_MONITOR_DATA_SIZE_OUT_OF_RANGE,

	// Parse result codes
	ESTATUS_PARSE_INT16_EXPECTED,
	ESTATUS_PARSE_INT32_EXPECTED,
	ESTATUS_PARSE_INT64_EXPECTED,
	ESTATUS_PARSE_INT8_EXPECTED,
	ESTATUS_PARSE_NUMERIC_VALUE_EXPECTED,
	ESTATUS_PARSE_SEMICOLON_EXPECTED,
	ESTATUS_PARSE_STRING_EXPECTED,
	ESTATUS_PARSE_UINT16_EXPECTED,
	ESTATUS_PARSE_UINT32_EXPECTED,
	ESTATUS_PARSE_UINT64_EXPECTED,
	ESTATUS_PARSE_VALUE_OUT_OF_RANGE,
	ESTATUS_PARSE_ERROR,

	// Disk errors
	ESTATUS_DISK_ERROR,
	ESTATUS_DISK_WRITE_FAULT,
	ESTATUS_DISK_DATA_READY,
	ESTATUS_DISK_OUT_OF_RANGE,
	ESTATUS_DISK_NOT_PRESENT,
	ESTATUS_DISK_SECTOR_OUT_OF_RANGE,
	ESTATUS_DISK_BUFFER_NOT_ALIGNED,
	ESTATUS_DISK_SECTOR_COUNT_0,
	ESTATUS_DISK_BAD_BLOCK,
	ESTATUS_DISK_UNCORRECTABLE,

	// Protocol related
	ESTATUS_PROTOCOL_BLOCK_FAULT,
	ESTATUS_PROTOCOL_BAD_CRC,
	ESTATUS_PROTOCOL_WRONG_BLOCK,

	// Interrupt related
	ESTATUS_INTERRUPT_VECTOR_UNKNOWN,
	ESTATUS_NO_INTERRUPTS,

	// Serial port out of range
	ESTATUS_SERIAL_OUT_OF_RANGE,
	ESTATUS_SERIAL_INTERRUPT_NONFUNCTIONAL,

	// RTC related
	ESTATUS_RTC_NOT_PRESENT,

	// PTC related
	ESTATUS_PTC_NOT_PRESENT,
	ESTATUS_PTC_RATE_OUT_OF_RANGE,  
	ESTATUS_PTC_NOT_INT_CAPABLE,
	ESTATUS_PTC_CHANNEL_OUT_OF_RANGE,
	ESTATUS_PTC_CHANNEL_NONFUNCTIONAL,
	ESTATUS_PTC_INTERRUPT_NONFUNCTIONAL,

	// File related
	ESTATUS_FILE_NOT_FOUND,
	ESTATUS_NO_PATH,
	ESTATUS_INVALID_FILENAME,
	ESTATUS_FILE_EXISTS,
	ESTATUS_INVALID_OBJECT,
	ESTATUS_WRITE_PROTECTED,
	ESTATUS_DISK_INVALID,
	ESTATUS_NOT_ENABLED,
	ESTATUS_NO_FILESYSTEM,
	ESTATUS_MKFS_ABORTED,
	ESTATUS_LOCKED,
	ESTATUS_TOO_MANY_OPEN_FILES,
	ESTATUS_INVALID_PARAMETER,
	ESTATUS_ACCESS_DENIED,

	// Keyboard related
	ESTATUS_KB_FAILED_SELF_TEST,
	ESTATUS_KB_CONTROLLER_NOT_PRESENT,
	ESTATUS_KB_KEYBOARD_NOT_FOUND,
	ESTATUS_KB_MOUSE_NOT_FOUND,
	ESTATUS_KB_CONTROLLER_RESET_FAULT,
	ESTATUS_KB_IDENT_FAULT
} EStatus;

// Serial port related

// Serial port flow control types
typedef enum
{
	ESERIALFLOW_NONE,
	ESERIALFLOW_CTS_RTS,
	ESERIALFLOW_XON_XOFF
} ESerialFlowControl;

// Modem control bits
#define	SERIALMODEMCTRL_DTR				0x01
#define	SERIALMODEMCTRL_RTS				0x02
#define	SERIALMODEMCTRL_OUT1   			0x04
#define	SERIALMODEMCTRL_OUT2			0x08
#define	SERIALMODEMCTRL_LOOP			0x10

// Modem status bits
#define SERIALMODEMSTAT_CTS				0x10
#define SERIALMODEMSTAT_DSR				0x20
#define SERIALMODEMSTAT_RI				0x40
#define	SERIALMODEMSTAT_DCD				0x80

extern EStatus SerialGetCount(uint8_t *pu8SerialCount);
/*extern EStatus SerialSetBaudRate(uint8_t u8SerialIndex,
							     uint32_t u32BaudRate,
							     uint32_t *pu32ActualBaudRate); */
extern EStatus SerialSetBits(uint8_t u8SerialIndex,
							 uint8_t u8DataBits,
							 uint8_t u8StopBits);
extern EStatus SerialSetFlowControl(uint8_t u8SerialIndex,
									ESerialFlowControl eFlowControl);
extern EStatus SerialFlush(uint8_t u8SerialIndex);
extern EStatus SerialClear(uint8_t u8SerialIndex);
extern EStatus SerialSetModemControl(uint8_t u8SerialIdnex,
									 uint8_t u8ModemControlFlags);
extern EStatus SerialGetLineStatus(uint8_t u8SerialIndex,
								   uint8_t *pu8LineStatus);
extern EStatus SerialSetFIFODepth(uint8_t u8SerialIndex,
								  uint8_t u8FIFODepth);
extern EStatus SerialSetBuffers(uint8_t u8SerialIndex,
								uint32_t u32TXBufferSize,
								uint8_t *pu8TXBufferBase,
								uint32_t u32RXBufferSize,
								uint8_t *pu8RXBufferBase);
extern EStatus SerialSetCallbacks(uint8_t u8SerialIndex,
								  EStatus (*TXData)(uint8_t *pu8Buffer,
													uint16_t u16TXCount),
								  EStatus (*RXData)(uint8_t *pu8Buffer,
													uint32_t u32RXCount),
								  void (*ModemStatus)(uint8_t *pu8Buffer,
													  uint8_t u8ModemStatusChangeMask,
													  uint8_t u8ModemStatus));
extern EStatus SerialWrite(uint8_t u8SerialIndex,
						   uint8_t *pu8Buffer,
						   uint16_t u16WriteCount,
						   uint16_t *pu16Written);
extern EStatus SerialRead(uint8_t u8SerialIndex,
						  uint8_t *pu8Buffer,
						  uint16_t u16ReadCount,
						  uint16_t *pu16Read);

// Console related

typedef enum
{
	EDEVICE_SERIAL,			// Serial port
	EDEVICE_VIDEO,			// Video
	EDEVICE_KEYBOARD,		// Onboard keyboard controller
} EDevice;

extern void ConsoleWrite(char *peBuffer,
						 uint32_t u32WriteLength);
extern EStatus ConsoleSetOutputDevice(EDevice eConsoleOutputDevice,
									  uint8_t u8DeviceIndex);
extern EStatus ConsoleSetInputDevice(EDevice eConsoleInputDevice,
									 uint8_t u8DeviceIndex);

// Mouse related

#define MOUSEBUTTON_LEFT		0x01
#define	MOUSEBUTTON_MIDDLE		0x02
#define	MOUSEBUTTON_RIGHT		0x04

extern EStatus MouseInit(void);
extern EStatus MouseSetCallback(void (*Callback)(uint8_t u8ButtonsPressed,
												 int8_t s8XDir,
												 int8_t s8YDir));
extern EStatus MouseGetButtonState(uint8_t *pu8ButtonsPressed);


// NVStore related
extern EStatus NVStoreGetSize(uint16_t *pu16NVStoreSize);
extern EStatus NVStoreWrite(uint16_t *pu16Buffer,
							uint16_t u16Size);
extern EStatus NVStoreRead(uint16_t *pu16Buffer,
						   uint16_t u16Size);

// Timer related
extern uint16_t TimerGetInterval(void);
extern EStatus TimerSetInterval(uint16_t u16IntervalMillisecnds,
								uint16_t *pu16IntervalActualMilliseconds);
extern EStatus TimerCreate(uint8_t *pu8TimerIndex);
extern EStatus TimerDestroy(uint8_t u8TimerIndex);
extern EStatus TimerSet(uint8_t u8TimerIndex,
						uint16_t u16TickIntervalInitial,
						uint16_t u16TickIntervalReload,
						void (*Callback)(uint8_t u8TimerIndex));
extern EStatus TimerStat(uint8_t u8TimerIndex);
extern EStatus TimerStop(uint8_t u8TimerIndex);

// System related
extern uint8_t SystemGetSlotID(void);
extern EStatus SystemSetResetState(uint8_t u8SlotBits);

// Disk related
typedef struct SDiskInfo
{
	char *peDiskName;
	uint64_t u64SectorCount;
	uint16_t u16SectorSize;
} SDiskInfo;

extern EStatus DiskGetCount(uint8_t *pu8DiskCount);
extern EStatus DiskGetInfo(uint8_t u8DiskIndex,
						   SDiskInfo *psDiskInfo);
extern EStatus DiskRead(uint64_t u64SectorNumber,
						uint8_t *pu8Buffer,
						uint32_t u32SectorsToRead);
extern EStatus DiskWrite(uint64_t u64SectorNumber,
						 uint8_t *pu8Buffer,
						 uint32_t u32SectorsToWrite);



// Filesystem related
typedef void * SFile;

// File attributes (taken from https://docs.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants)
#define	ATTRIB_READ_ONLY	0x01		// Read only file
#define	ATTRIB_HIDDEN		0x02		// File's hidden
#define	ATTRIB_SYSTEM		0x04		// System file
#define	ATTRIB_LFN_ENTRY	0x0f		// Long filename entry
#define	ATTRIB_DIRECTORY	0x10		// Directory entry
#define	ATTRIB_ARCHIVE		0x20		// Archive file
#define	ATTRIB_NORMAL		0x40		// Regular file

// Maximum length of a file's name
#define	MAX_FILENAME_LEN	255			// Maximum length of a file's name

// Used for findfirst/findnext
typedef struct SFileFind
{
	uint64_t u64FileSize;
	struct tm sDateTimestamp;
	uint32_t u32Attributes;
	char eFilename[MAX_FILENAME_LEN+1];

	void *pvFilesystemData;
} SFileFind;

extern EStatus Filefopen(SFile *psFile,
						 char *peFilename,
						 char *peFileMode);
extern EStatus FileOpenUniqueFile(char *pePath, 
								  char *peFilenameGenerated, 
								  uint32_t u32FilenameBufferSize, 
								  SFile sOpenedFile);
extern EStatus FileCreateUniqueDirectory(char *pePath, 
										 char *peDirectoryGenerated, 
										 uint32_t u32DirectoryBufferSize);
extern EStatus Filefread(void *pvBuffer, 
						 uint64_t u64SizeToRead, 
						 uint64_t *pu64SizeRead, 
						 SFile sFile);
extern EStatus Filefwrite(void *pvBuffer, 
						  uint64_t u64SizeToWrite, 
						  uint64_t *pu64SizeWritten, 
						  SFile sFile);
extern EStatus Filefseek(SFile sFile, 
						 int64_t s64Offset, 
						 int8_t s8Origin);
extern EStatus Fileftell(SFile sFile, 
						 uint64_t *pu64Position);
extern EStatus Fileferror(SFile sFile);
extern EStatus Filefclose(SFile sFile);
extern EStatus Filefgets(char *peBuffer, 
						 uint64_t u64BufferSize, 
						 SFile sFile);
extern EStatus Filerename(char *peOldFilename, 
						  char *peNewFilename);
extern EStatus Filemkdir(char *peDirName);
extern EStatus Filechdir(char *peDirName);
extern EStatus Fileunlink(char *peFilename);
extern EStatus Filermdir(char *peDirName);
extern EStatus FileGetFree(uint64_t *pu64FreeSpace);
extern EStatus FileSetFileTime(char *peFilename,
							   struct tm *psTime);
extern uint64_t FileSize(SFile sFile);
extern EStatus FileSetAttributes(char *peFilename,
								 uint32_t u32Attributes);
extern EStatus FileFindOpen(char *pePath,
							SFileFind **ppsFileFind);
extern EStatus FileFindFirst(char *pePath,
							 char *pePattern,
							 SFileFind **ppsFileFind);
extern EStatus FileFindReadDir(SFileFind *psFileFind);
extern EStatus FileFindNext(SFileFind *psFileFind);
extern EStatus FileFindClose(SFileFind **ppsFileFind);
extern EStatus FilesystemCreate(void);
extern EStatus FilermdirTree(char *peDirName);
extern bool Filefeof(SFile sFile);
extern EStatus Filefflush(SFile sfile);
extern EStatus Filefputc(uint8_t u8Data, 
						 SFile sFile);
extern EStatus Filefgetc(uint8_t *pu8Data, 
						 SFile sFile);
extern EStatus FileStat(char *peFilename,
						uint64_t *pu64Timestamp,
						uint64_t *pu64FileSize,
						uint32_t *pu32FileAttributes);

// IPC related

#endif	// #ifndef _OS_H_
