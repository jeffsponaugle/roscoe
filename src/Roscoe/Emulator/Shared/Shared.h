#ifndef _SHARED_H_
#define _SHARED_H_

#include "Shared/types.h"

// Must include the OS for other functions/structural definitions
#include "OS/OS.h"

#include "Shared/SharedLog.h"
#include "Shared/CmdLine.h"
#ifdef _FASTHEAP
#include "Shared/FastHeap.h"
#endif

// Data dir (for any data files needed by the servers/clients)
#define	DATA_DIR		"Data"

typedef enum
{
	ELANG_NOT_SPECIFIED,
	ELANG_START,
	ELANG_ENGLISH=ELANG_START,
	ELANG_SPANISH,
	ELANG_MANDARIN_SIMPLIFIED,
	ELANG_MANDARIN_TRADITIONAL,

	// Must always be the last enum
	ELANG_MAX
} ELanguage;


// Assert related
extern void AssertHandlerProc(char *pu8Expression, char *pu8Module, uint32_t u32Line);
#define	BASSERT(expr)		if (!(expr)) { AssertHandlerProc((char *) #expr, (char *) __FILE__, (uint32_t) __LINE__); }
#define	BASSERT_MSG(expr)	AssertHandlerProc((char *) expr, (char *) __FILE__, (uint32_t) __LINE__)

// Debug output related
extern void DebugOutInternal(bool bSkipTime,
							 const char *peProcedureName, 
							 const char *pu8Format,
							 ...);
extern void DebugOutFlush(void);
extern void DebugConsoleInit(void);
extern void DebugConsoleIn(uint8_t *pu8ConsoleData, uint32_t u32Length);
extern void DebugOutSetLock(bool bLock);
extern void DebugOutHex(uint8_t *pu8Buffer,
						uint64_t u64Length);
extern void DebugOutFuncInternal(const char *pu8Procedure, const char *pu8Format, ...);

#define	DebugOutFunc(...)		DebugOutInternal(false, __FUNCTION__, __VA_ARGS__)
#define	DebugOut(...)			DebugOutInternal(false, NULL, __VA_ARGS__)
#define	DebugOutSkipTime(...)	DebugOutInternal(true, NULL, __VA_ARGS__)

// Syslog/debugging
extern EStatus SyslogInit(char *peBaseFilename,
						  uint32_t u32HistoryDepth,
						  bool bConsoleOutput);

extern SLog *SyslogGetLog(void);

#define		Syslog(...)		LoggerInternal(SyslogGetLog(), true, NULL, ##__VA_ARGS__)
#define		SyslogFunc(...)	LoggerInternal(SyslogGetLog(), true, (char *) __FUNCTION__, ##__VA_ARGS__)

extern void SyslogShutdown(void);
extern bool SharedTerminate(void);
#define SYSLOG_SERVER_RESULT(name,code) { if ((code) != ESTATUS_OK) { Syslog("%s failed (%u): %s\n", (name), (code), GetErrorText(code)); } \
										  else { Syslog("%s ok\n", (name)); } } 

// Memory/heap related
extern void *SharedMemAlloc(uint64_t u64Size,
							const char *peProcedure,
						  	const char *peFile,
						  	uint32_t u32LineNumber,
						  	bool bZeroMemory);
extern void *SharedReallocMemoryInternal(void *pvOldBlock,
										 uint64_t u64Size,
										 const char *peProcedure,
						  				 const char *peFile,
										 uint32_t u32LineNumber);
extern void SharedMemFree(void *pvBlock,
						  const char *peProcedure,
						  const char *peFile,
						  uint32_t u32LineNumber);

#define MemAlloc(x)						SharedMemAlloc(x, (const char *) __FUNCTION__, (const char *) __FILE__, (uint32_t) __LINE__, true)
#define MemAllocNoClear(x)				SharedMemAlloc(x, (const char *) __FUNCTION__, (const char *) __FILE__, (uint32_t) __LINE__, false)
#define MemRealloc(x, y)				SharedReallocMemoryInternal(x, y, (uint8_t *) __FILE__, __LINE__)
#define MemFree(x)						SharedMemFree(x, (const char *) __FUNCTION__, (const char *) __FILE__, (uint32_t) __LINE__); x = NULL;
#define SafeMemFree(x)					if (x) {MemFree(x); x = NULL; }

#ifdef _WIN32
extern void *memmem(void *pvHaystackStart,
					uint64_t u64HaystackLength,
					void *pvNeedleStart, 
					uint64_t u64NeedleLength);
extern int strcasecmp(const char *peString1, const char *peString2);
#endif

extern int Sharedstrcasecmp(const char *peString1Head, const char *peString2Head);
extern const char *Sharedstrcasestr(const char *peHaystack, const char *peNeedle);
extern char *Sharedstrdup(char *peInputString);
extern char *stristr(register char *string, char *substring);
extern void SharedZeroMemoryFast(void *pvDataBase,
								 uint64_t u64Length);


extern void SharedHeapCheck(void);
extern void SharedHeapDump(void);
extern void SharedBasicCheck(void);

extern char *GetErrorText(EStatus eStatus);
extern char *GetErrorTextLocale(EStatus eStatus);
extern bool IsPointerValid(void *pvAddress,
						   bool bIncludeCodeRegions);
extern void SharedPreOSInit(void);

// GUIDs everywhere!
#define GUID_LEN	(32)

typedef struct SGUID
{
	uint8_t u8GUID[GUID_LEN];
	void *pvUser;
} SGUID;

// SHA256 Hash size
#define	SHA256_HASH_SIZE		(256/8)

// File hash size
#define FILE_HASH_SIZE			SHA256_HASH_SIZE

// Password hash size
#define	PASSWORD_HASH_SIZE		SHA256_HASH_SIZE

#define	NETADDR_SIZE			16			// Size of IPV6 (largest protocol)

#define SECONDS_PER_MSEC	(1000)
#define MINUTES_PER_MSEC	(SECONDS_PER_MSEC * 60)
#define HOURS_PER_MSEC		(MINUTES_PER_MSEC * 60)

typedef struct SStatusCodes
{
	EStatus eStatus;
	char *peStatusText;
} SStatusCodes;

extern void SharedTimeToText(char *peTextBuffer,
							 uint32_t u32TextBufferSize,
							 uint64_t u64Time);
extern void SharedGUIDToASCII(SGUID *psGUID,
							  char *peGUIDString,
							  uint32_t u32GUIDStringBufferLength);

// String related
extern void StripWhitespace(char *peString);
extern char *SharedstrncpyWhitespace(char *peSrcHead,
									 uint32_t u32MaxLen);
extern char *SharedstrdupHeap(const char *peString,
							  const char *peModuleName,
							  const char *peProcedureName,
							  uint32_t u32LineNumber);

extern void strcat_ptr(char **ppeString, 
					  char *peStringToAdd);
#define strdupHeap(x)		SharedstrdupHeap(x, __FILE__, __FUNCTION__, (uint32_t)__LINE__)
extern EUnicode UTF8toUnicode(const char* peString);
extern uint32_t UTF8strlen(const char *peString);
extern uint32_t UTF8charlen(const char* peString);
extern bool SharedIsStringNumeric(char *peString);
extern uint32_t UnicodeToUTF8Len(EUnicode eUnicode);
extern uint32_t UnicodeToUTF8(char *peOutput,
							EUnicode eUnicode);

extern char *sprintfMemAlloc(char *peFormatString,
							 ...);
extern uint64_t PathExtract(char *peSourceFilename,
						  char *peDestinationPath,
						  uint64_t u64DestinationPathBufferLength);

extern void FilenameStripPath(char *peFilenameHead);
extern void SharedCapitalizeAllWords(char *peString);
extern EStatus SharedEncrypt(uint8_t *pu8SourceData,
							 uint64_t u64SourceDataSize,
							 uint8_t *pu8EncryptedData,
							 uint8_t *pu8Key);
extern EStatus SharedDecrypt(uint8_t *pu8SourceData,
							 uint64_t u64SourceDataSize,
							 uint8_t *pu8DecryptedData,
							 uint8_t *pu8Key);


// Misc
extern uint64_t SharedRandomNumber(void);
extern void SharedSHA256(uint8_t *pu8Data,
						 uint64_t u64DataLength,
						 uint8_t *pu8SHA256);
extern bool IsWildcardFilename(char *peWildcard);
extern void FilenameValidate(char *peFilename);
extern char *SharedStringOrNULL(char *peString);
extern uint32_t SharedGetID(void);
extern uint32_t SharedPackDate(uint8_t u8Day, 
							 uint8_t u8Month, 
							 uint16_t u16Year);
extern uint8_t SharedDaysInMonth(uint8_t u8Month);
extern bool SharedIsLeapYear(uint16_t u16Year);
extern bool SharedIsInTestMode(void);
extern void SharedDateTimeToString(uint64_t u64Timestamp,
								   char *peTargetBuffer,
								   uint32_t u32TargetBufferLength,
								   bool bLocalTime);
extern EStatus SharedConsoleLineInput(char *peBuffer,
									  uint32_t u32MaxLength,
									  bool bPasswordEntry);

// Misc directories/filenames
#define	UPDATE_BUNDLE_DOWNLOADS		"Data/Bundles"

#ifdef _SERVER
#define	OOM_ESTATUS		ESTATUS_SERVER_OUT_OF_MEMORY
#else
#define	OOM_ESTATUS		ESTATUS_OUT_OF_MEMORY
#endif

// Very common error handling macros
#define	ERR_GOTO()				if (eStatus != ESTATUS_OK) { goto errorExit; }
#define ERR_SYSLOG(x)			if (eStatus != ESTATUS_OK) { SyslogFunc("%s - code 0%.8x (%s)\n", x, eStatus, GetErrorText(eStatus)); }
#define ERR_SYSLOG_GOTO(x)		if (eStatus != ESTATUS_OK) { SyslogFunc("%s - code 0%.8x (%s)\n", x, eStatus, GetErrorText(eStatus));  goto errorExit; }
#define ERR_SYSLOG_GOTO_ERR(err, x)	if (eStatus != ESTATUS_OK) { if ((err) && (*(&err))) \
																{\
																	SyslogFunc("%s - code 0%.8x (%s)\n", x, eStatus, *(&err)); \
																}\
																else\
																{\
																	SyslogFunc("%s - code 0%.8x (%s)\n", x, eStatus, GetErrorText(eStatus)); \
																}\
																goto errorExit; \
														   }
#define ERR_NULL_OOM(x)			if (NULL == x) {eStatus = OOM_ESTATUS; goto errorExit;}
#define	ZERO_STRUCT(x)			memset((void *) &x, 0, sizeof(x));
#define	EStatusResultConditional(statement)	if (ESTATUS_OK == eStatus) \
											{ \
												eStatus = statement;\
											} \
											else \
											{ \
												(void) statement; \
											}
	

#define	MEMALLOC(x, y)			x = MemAlloc(y); if (NULL == x) {eStatus = OOM_ESTATUS; goto errorExit;}
#define	MEMALLOC_NO_CLEAR(x, y)	x = MemAllocNoClear(y); if (NULL == x) {eStatus = OOM_ESTATUS; goto errorExit;}
#define	MEMSTRDUP(x, y)			x = Sharedstrdup(y); if (NULL == x)  {eStatus = OOM_ESTATUS; goto errorExit;}

#define INVERT_BOOL(x) ((false == (x)) ? true : false)

// GUIDCompare returns true when x == y
#define GUIDCompare(x, y)		((*(uint64_t*)&((x)->u8GUID[0]) == *(uint64_t*)&((y)->u8GUID[0])) && \
								 (*(uint64_t*)&((x)->u8GUID[8]) == *(uint64_t*)&((y)->u8GUID[8])) && \
								 (*(uint64_t*)&((x)->u8GUID[16]) == *(uint64_t*)&((y)->u8GUID[16])) && \
								 (*(uint64_t*)&((x)->u8GUID[24]) == *(uint64_t*)&((y)->u8GUID[24])))
// Check if the GUID is all zero
#define GUIDIsNULLRaw(x)		((0 == *(uint64_t*)&((x)[0])) && (0 == *(uint64_t*)&((x)[8])) && \
								 (0 == *(uint64_t*)&((x)[16])) && (0 == *(uint64_t*)&((x)[24])))
#define GUIDIsNULL(x)			(GUIDIsNULLRaw((x)->u8GUID))

#define	GUIDCopy(dest, src)		memcpy((void *) (dest)->u8GUID, (void *) (src)->u8GUID, sizeof((dest)->u8GUID));

#define	GUIDCopyUserData(dest, src)	(dest)->pvUser = (src)->pvUser;

#define	GUIDZero(dest)			memset((void *) (dest), 0, sizeof(*(dest)));

extern EStatus SharedInit(char *peProgramName,
						  const SCmdLineOption *psCommandLineOptions,
						  char *peFullCommandLineOption,
						  int argc,
						  char **argv);

#endif