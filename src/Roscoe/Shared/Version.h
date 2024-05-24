#ifndef _VERSION_H_
#define _VERSION_H_

// Image types
typedef enum
{
	EIMGTYPE_UNKNOWN = 0,
	EIMGTYPE_BOOT_LOADER,
	EIMGTYPE_OPERATIONAL,
	EIMGTYPE_FLASH_UPDATE,
	EIMGTYPE_APP,

	// Leave this at the end
	EIMGTYPE_COUNT
} EImageType;

// Version codes
typedef enum
{
	EVERSION_UNKNOWN,
	EVERSION_OK,
	EVERSION_NOT_FOUND,
	EVERSION_BAD_PREFIX,
	EVERSION_BAD_SUFFIX,
	EVERSION_BAD_IMAGE_TYPE,
	EVERSION_BAD_STRUCT_CRC,
	EVERSION_BAD_IMAGE_CRC,
	EVERSION_BAD_LOAD_ADDRESS,
	EVERSION_BAD_ENTRY_POINT,
	EVERSION_BAD_BSS_START,
	EVERSION_BAD_END,
	EVERSION_BAD_STACK_TOP,
	EVERSION_BAD_IMAGE_SIZE
} EVersionCode;

// Firmware version structure

#ifdef _WIN32
#pragma pack(push, 1)
typedef struct SImageVersion
#else
typedef struct __attribute__ ((packed)) __attribute ((aligned (4))) SImageVersion
#endif
{
	uint32_t u32VersionPrefix;

	// What type of image is this?
	EImageType eImageType;

	// Image verison #/builds
	uint32_t u32MajorMinorBuildNumber;

	// Image build timestamp (time_t - 64 bit)
	uint64_t u64BuildTimestamp;

	// Image CRC32 excluding version structure
	uint32_t u32ImageCRC32;

	// Image size (in bytes)
	uint32_t u32ImageSize;

	// Various bits of information about this image
	uint32_t u32LoadAddress;		// Load address of image
	uint32_t u32EntryPoint;			// Entry point of image
	uint32_t u32BssStart;			// Start of bss
	uint32_t u32BssEnd; 	   		// End of bss (start of heap)

	// Image specific data pointer (optional use)
	void *pvImageSpecificData;

	// This version structure's CRC32 (assuming u32VersionStrucCRC32 starts with all 0x00s)
	uint32_t u32VersionStructCRC32;
	uint32_t u32VersionSuffix;
} SImageVersion;
#ifdef _WIN32
#pragma pack(pop)
#endif

// Prefix and suffix for version structure
#define	VERSION_PREFIX	0x56455253
#define	VERSION_SUFFIX	0x494f4e2e

// Default image and version CRC32 fills so the stamp utility picks up on it
#define	STAMP_IMAGE_CRC32_FILL		0x98abcdef
#define	STAMP_VERSION_CRC32_FILL	0x12345678

// Seed value for CRC32 for version structure
#define	VERSION_CRC32_STRUCT_INITIAL		0xd82b53b7

// Seed value for CRC32 for image
#define VERSION_CRC32_IMAGE_INITIAL			0xe4a1f9f5

// External refernece to this image's image version #
extern volatile const SImageVersion g_sImageVersion;

// Version structure/data functions
extern EVersionCode VersionValidateStructure(SImageVersion *psImageVersion);
extern char *VersionCodeGetText(EVersionCode eVersionCode);
extern EVersionCode VersionFindStructure(uint8_t *pu8Address,
										 uint32_t u32RegionSize,
										 EImageType eImageTypeSearch,
										 SImageVersion **ppsImageVersion);

#endif
