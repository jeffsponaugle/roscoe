#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "Shared/Version.h"
#include "Shared/zlib/zlib.h"

typedef struct SVersionCodeText
{
	EVersionCode eVersionCode;
	char *peVersionCodeText;
} SVersionCodeText;

// Translate EVersionCodes to text
static const SVersionCodeText sg_sVersionCodeTable[] =
{
	{EVERSION_UNKNOWN,				"Unknown"},
	{EVERSION_OK,					"Ok"},
	{EVERSION_NOT_FOUND,			"No version signature found"},
	{EVERSION_BAD_PREFIX,			"Bad prefix"},
	{EVERSION_BAD_SUFFIX,			"Bad suffix"},
	{EVERSION_BAD_IMAGE_TYPE,		"Bad image type"},
	{EVERSION_BAD_STRUCT_CRC,		"Bad image structure CRC32"},
	{EVERSION_BAD_IMAGE_CRC,		"Bad image CRC32"},
	{EVERSION_BAD_LOAD_ADDRESS,		"Bad load address"},
	{EVERSION_BAD_ENTRY_POINT,		"Bad entry point"},
	{EVERSION_BAD_BSS_START,		"Bad start of _bss"},
	{EVERSION_BAD_END,				"Bad _end"},
	{EVERSION_BAD_STACK_TOP,		"Bad top of stack"},
	{EVERSION_BAD_IMAGE_SIZE,		"Bad image size"}
};

// Returns textual equivalent of eVersionCode
char *VersionCodeGetText(EVersionCode eVersionCode)
{
	uint8_t u8Loop;

	for (u8Loop = 0; u8Loop < (sizeof(sg_sVersionCodeTable) / sizeof(sg_sVersionCodeTable[0])); u8Loop++)
	{
		if (sg_sVersionCodeTable[u8Loop].eVersionCode == eVersionCode)
		{
			return(sg_sVersionCodeTable[u8Loop].peVersionCodeText);
		}
	}

	return("Unknown");
}

// Validates an image version structure
EVersionCode VersionValidateStructure(SImageVersion *psImageVersion)
{
	EVersionCode eVersionCode;
	uint32_t u32CRC;

	// Do we have a proper prefix?
	if (psImageVersion->u32VersionPrefix != VERSION_PREFIX)
	{
		eVersionCode = EVERSION_BAD_PREFIX;
		goto errorExit;
	}

	// And a proper suffix?
	if (psImageVersion->u32VersionSuffix != VERSION_SUFFIX)
	{
		eVersionCode = EVERSION_BAD_SUFFIX;
		goto errorExit;
	}

	// Is the image type known?
	if ((psImageVersion > EIMGTYPE_UNKNOWN) && (psImageVersion->eImageType < EIMGTYPE_COUNT))
	{
		// Image type is reasonable
	}
	else
	{
		// Image type isn't reasonable
		printf("Unreasonable image type\n");
		eVersionCode = EVERSION_BAD_IMAGE_TYPE;
		goto errorExit;
	}

	// If the bss is smaller than the load address, it's bad
	if (psImageVersion->u32BssStart < psImageVersion->u32LoadAddress)
	{
		printf("bssStart < load address\n");
		eVersionCode = EVERSION_BAD_BSS_START;
		goto errorExit;
	}

	// u32BssEnd must come after u32BssStart
	if (psImageVersion->u32BssEnd < psImageVersion->u32BssStart)
	{
//		printf("bssEnd < bssStart\n");
		eVersionCode = EVERSION_BAD_END;
		goto errorExit;
	}

	// CRC the structure. Remember... don't CRC the CRC32 field
	u32CRC = crc32(VERSION_CRC32_STRUCT_INITIAL,
				   (const unsigned char *) psImageVersion,
				   ((uint32_t) &psImageVersion->u32VersionStructCRC32) - ((uint32_t) psImageVersion));

	// And CRC everything past the u32VersionStructCRC32 member
	u32CRC = crc32(u32CRC,
				   ((const unsigned char *) &psImageVersion->u32VersionStructCRC32) + sizeof(psImageVersion->u32VersionStructCRC32),
				   ((uint32_t) (psImageVersion + 1)) - (((uint32_t) &psImageVersion->u32VersionStructCRC32) + sizeof(psImageVersion->u32VersionStructCRC32)));

	// If the CRC doesn't match, then it's bad
	if (psImageVersion->u32VersionStructCRC32 != u32CRC)
	{
//		printf("Bad CRC32\n");
		eVersionCode = EVERSION_BAD_STRUCT_CRC;
		goto errorExit;
	}

	// All good!
	eVersionCode = EVERSION_OK;

errorExit:
	return(eVersionCode);
}

// Finds a version structure within a region of memory.
EVersionCode VersionFindStructure(uint8_t *pu8Address,
								  uint32_t u32RegionSize,
								  EImageType eImageTypeSearch,
								  SImageVersion **ppsImageVersion)
{
	EVersionCode eVersionCode = EVERSION_UNKNOWN;

	// If the region is smaller than the version structure, then don't even try.
	if (u32RegionSize < sizeof(**ppsImageVersion))
	{
		goto errorExit;
	}

	// Don't scan past the area.
	u32RegionSize -= sizeof(**ppsImageVersion);

	// Make sure the incoming address is uint32_t aligned
	assert(0 == (((uint32_t) pu8Address) & 3));

	while (u32RegionSize)
	{
		if (VERSION_PREFIX == *((uint32_t *) pu8Address))
		{
			// See if this is an actual structure and it's good
			eVersionCode = VersionValidateStructure((SImageVersion *) pu8Address);
			if (EVERSION_OK == eVersionCode)
			{
				if ((EIMGTYPE_UNKNOWN == eImageTypeSearch) ||
					(eImageTypeSearch == ((SImageVersion *) pu8Address)->eImageType))
				{
					// Found it!
					*ppsImageVersion = (SImageVersion *) pu8Address;
					goto errorExit;
				}

				// Otherwise, keep searching. Might be more than one.
			}
		}

		u32RegionSize -= sizeof(uint32_t);
		pu8Address += sizeof(uint32_t);
	}

	// Couldn't find a CRC
	eVersionCode = EVERSION_NOT_FOUND;

errorExit:
	return(eVersionCode);
}

