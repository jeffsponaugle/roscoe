#ifndef _VERSION_H_
#define _VERSION_H_

// Version code (validity of a particular image)
typedef enum
{
	EVERSION_TAG_NOT_SPECIFIED,				// Unknown - not set yet
	EVERSION_TAG_NOT_APPLICABLE,			// Does not apply for this type of image
	EVERSION_TAG_NOT_FOUND,					// Version tag not in image
	EVERSION_BAD_CRC32,						// Found at least one version tag but bad CRC
	EVERSION_BAD_HASH,						// Version header is good, but hash is bad
	EVERSION_OK,							// Firmware image is OK
} EVersionCode;

// CPU Types
typedef enum
{
	ECPU_UNKNOWN = 0,
	ECPU_X86,								// FreeBSD/Linux host PCs
	ECPU_CORTEX_M8,							// ARM Cortex M8 CPU
	ECPU_MAX,								// Maximum # of CPU entries

	ECPU_RESERVED1=253,
	ECPU_RESERVED2=254,
	ECPU_NOT_SPECIFIED=255
} ECPUType;

// OSArc
typedef enum
{
	ECPUARCH_UNKNOWN,
	ECPUARCH_X64_WINDOWS,
	ECPUARCH_X64_LINUX,
	ECPUARCH_X64_FREEBSD,
	ECPUARCH_MAX
} EOSArchitecture;

// Board IDs
#define	PROGRAMID_STIMULATOR_APP		0x00000001
#define	PROGRAMID_SERVER				0x00000002
#define	PROGRAMID_SWUPDATE				0x00000003
#define	PROGRAMID_ADMIN					0x00000004
#define	PROGRAMID_BUNDLE_UPDATER		0x00000005
#define	PROGRAMID_TEST					0x00000006
#define	PROGRAMID_STIMBOT				0x00000007
#define	PROGRAMID_INSPECTOR				0x00000008
#define PROGRAMID_REPORTBOT				0x00000009
#define	PROGRAMID_REPORTAL				0x0000000a
#define	PROGRAMID_STIMMAGPRO			0x0000000b
#define	PROGRAMID_STIMNEUROSOFT			0x0000000c
#define	PROGRAMID_ZAPSTIM				0x0000000d
#define	PROGRAMID_NLIB					0x0000000e
#define	PROGRAMID_EEG					0x0000000f
#define	PROGRAMID_STIMMODP				0x00000010
#define	PROGRAMID_STIMMAGSTIM			0x00000011
#define	PROGRAMID_STIMAPOLLO			0x00000012

// Must be the highest that is not undefined or anonymous
#define	PROGRAMID_MAX					PROGRAMID_STIMMODP

#define	PROGRAMID_UNDEFINED				0xffffffff
#define	PROGRAMID_ANONYMOUS				0x00000000

#ifdef _WIN32
#ifndef __packed
#define	__packed
#endif
#pragma pack(push,1)
#endif

// The type of image this is
typedef enum
{
	EIMG_UNSPECIFIED,	// Not known/unspecified
	EIMG_LOADER,		// Boot loader image
	EIMG_OP,			// Main/operational image
	EIMG_LIBRARY,		// Library image/module (may be contained within an Op or loader image)
	EIMG_MAX
} EImageType;

PACKED_BEGIN
typedef PACKED_STRUCT_START struct PACKED_STRUCT SVersionInfo
{
	uint32_t u32Vers;					// Header marker
#if defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(_UNIX)
	uint8_t eImageType;				// Getting around enum == int size instead of byte
#else
	EImageType eImageType;			// What is this?
#endif
	uint8_t u8Major;					// Major version #
	uint8_t u8Minor;					// Minor version #
	uint16_t u16BuildNumber;			// Build #
	uint8_t u8SHA256[SHA256_HASH_SIZE]; // SHA256 of image (excluding the SVersionInfo structure
	uint32_t u32ImageSize;			// Total size of this image (in bytes)

#if defined(_WIN32) || defined(WIN64) || defined(_UNIX) 
	uint8_t eCPUType;
#else
	ECPUType eCPUType;				// Specific type of CPU
#endif
	
#if defined(_WIN32) || defined(WIN64) || defined(_WIN64)  || defined(_UNIX) 
	uint8_t eOSArchitecture;
#else
	EOSArchitecture eOSArchitecture;	// OS/CPU architecture
#endif
	
	uint32_t u32ProgramID;				// Target board ID for program

#if defined(_WIN64) || defined(WIN64) || defined(_WIN64)  || defined(_UNIX) 
	uint64_t u64Entry;				// Entry size is always 32 bits on target hardware
#else
	void (*const pEntry)(void);		// Entry point
#endif
	
	uint64_t u64LoadAddress;			// Address where code will be loaded

	uint64_t u64BuildTimestamp;		// time_t date/time of image build time
	uint8_t u8SecurityVersion;		// Security revision

	void *pvImageSpecificData;		// Image specific data
#if POINTER_SIZE == 4
	uint32_t u32ImageSpecificPadding;
#endif
	uint32_t u32Ion;					// Footer marker
	uint32_t u32CRC32;				// CRC32 Of this structure from start to here
} SVersionInfo;
PACKED_END

// Internal structure used for tracking library'd modules
PACKED_BEGIN
typedef PACKED_STRUCT_START struct PACKED_STRUCT SLibraryInfo
{
	uint32_t u32Libv;					// LIBV Identifier
	uint8_t u8StructureSize;			// Size of structure (packed)
#if defined(_WIN32) || defined(WIN64) || defined(_WIN64)  || defined(_UNIX)
	uint8_t eImageType;				// Getting around enum == int size instead of byte
#else
	EImageType eImageType;			// What is this?
#endif

	uint8_t u8Major;					// Major version #
	uint8_t u8Minor;					// Minor version #
	uint16_t u16BuildNumber;			// Build #
	char *peBuildTime;				// Build time string
	char *peBuildDate;				// Build date string
	
#if defined(_WIN32) || defined(WIN64) || defined(_WIN64)  || defined(_UNIX) 
	uint8_t eCPUType;
#else
	ECPUType eCPUType;				// Specific type of CPU
#endif
	
#if defined(_WIN32) || defined(WIN64) || defined(_WIN64)  || defined(_UNIX) 
	uint8_t eOSArchitecture;
#else
	EOSArchitecture eOSArchitecture;	// OS/CPU architecture
#endif

	void *pvImageSpecificData;		// Image specific data
#if POINTER_SIZE == 4
	uint32_t u32ImageSpecificPadding;
#endif
	
	uint32_t u32ersi;					// ERSI Identifier
} SLibraryInfo;
PACKED_END

#ifdef _WIN32
#pragma pack(pop)
#endif

// Version structure head and tail signatures
#define	VERSION_HEAD		((uint32_t) (0x53524556))
#define	VERSION_TAIL		((uint32_t) (0x2e4e4f49))

// Library signatures
#define	LIBRARY_HEAD		((uint32_t) (0x4c494256))
#define	LIBRARY_TAIL		((uint32_t) (0x45525349))

// Global variable containing this version structure information
extern const SVersionInfo g_sVersionInfo;

// Checks the integrity and finds a version structure within an image
extern EVersionCode FindVersionStructure(void *pvAddress,
										 int64_t s64Size,
										 SVersionInfo **ppsVersionInfo,
								  		 bool bSkipSHA256);

// Same as FindVersionStructure but only finds unstamped version structures
extern EVersionCode FindVersionStructureUnstamped(void *pvAddress,
												  int64_t s64Size,
												  SVersionInfo **ppsVersionInfo);

// Convert EVersionCode to eStatus
extern EStatus VersionCodeToEStatus(EVersionCode eCode);

#endif