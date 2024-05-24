#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "Shared/types.h"
#include "Shared/Shared.h"
#include "Shared/Version.h"
#include "Shared/SHA256/sha256.h"
#include "Shared/zlib/zlib.h"

// Version code to 
typedef struct SVersionCodeToEStatus
{
	EVersionCode eCode;
	EStatus eStatus;
} SVersionCodeToEStatus;

// Translate version codes to eStatus
static const SVersionCodeToEStatus sg_sVersionToEStatus[] =
{
	{EVERSION_OK,						ESTATUS_OK},
	{EVERSION_TAG_NOT_SPECIFIED,		ESTATUS_VERSION_TAG_NOT_SPECIFIED},
	{EVERSION_TAG_NOT_APPLICABLE,		ESTATUS_VERSION_TAG_NOT_APPLICABLE},
	{EVERSION_TAG_NOT_FOUND,			ESTATUS_VERSION_TAG_NOT_FOUND},
	{EVERSION_BAD_CRC32,				ESTATUS_VERSION_BAD_CRC32},
	{EVERSION_BAD_HASH,					ESTATUS_VERSION_BAD_HASH},
};

EStatus VersionCodeToEStatus(EVersionCode eCode)
{
	uint32_t u32Loop;

	for (u32Loop = 0; u32Loop < (sizeof(sg_sVersionToEStatus) / sizeof(sg_sVersionToEStatus[0])); u32Loop++)
	{
		if (eCode == sg_sVersionToEStatus[u32Loop].eCode)
		{
			return(sg_sVersionToEStatus[u32Loop].eStatus);
		}
	}

	// This should've been handled by the table above
	BASSERT(0);
	return(ESTATUS_UNKNOWN_RESULT_CODE);
}

// This routine checks the version structure's SHA256 against the overall image
static EVersionCode VersionCheckSHA256(void *pvAddress,
									   uint64_t u64Offset,
									   SVersionInfo *psVersion)
{
	MY_SHA256_CTX sSHA256Ctx;
	uint8_t u8SHA256[SHA256_HASH_SIZE];

	// Initialize the context
	ZERO_STRUCT(sSHA256Ctx);
	MySHA256_Init(&sSHA256Ctx);

	// SHA256 All data up to the version structure
	MySHA256_Update(&sSHA256Ctx,
					(const void *) pvAddress,
					(size_t) u64Offset);
					
	// Now SHA256 everything after the version structure to the end of the image
	if (psVersion->u32ImageSize > (u64Offset + sizeof(*psVersion)))
	{
		MySHA256_Update(&sSHA256Ctx,
						(const void *) (u64Offset + sizeof(*psVersion) + ((uint8_t *) pvAddress)),
						(size_t) (psVersion->u32ImageSize - (u64Offset + sizeof(*psVersion))));
	}
													
	// Finalize the SHA256 that we've calculated
	MySHA256_Final(u8SHA256,  &sSHA256Ctx);
					
	// If they match, it's good to go!
	if (memcmp(u8SHA256, psVersion->u8SHA256, sizeof(u8SHA256)) == 0)
	{
		return(EVERSION_OK);
	}
	else
	{
		// SHA256 Failure on the image. Assume a bad hash unless we find another one
		// that's correct.
		return(EVERSION_BAD_HASH);
	}
}

EVersionCode FindVersionStructure(void *pvAddress,
								  int64_t s64Size,
								  SVersionInfo **ppsVersionInfo,
								  bool bSkipSHA256)
{
	SVersionInfo *psVersion;
	uint8_t *pu8Data = (uint8_t *) pvAddress;
	EVersionCode eCode = EVERSION_TAG_NOT_FOUND;
	uint64_t u64Offset = 0;

	// If the incoming image is too small, no way we'll find this version info
	if (s64Size < (sizeof(*psVersion) + 1))
	{
		eCode = EVERSION_TAG_NOT_FOUND;
		goto errorExit;
	}

	// Adjust for the size of the version structure
	s64Size -= sizeof(*psVersion);

	while (s64Size > 0)
	{
		psVersion = (SVersionInfo *) pu8Data;
		
		// Look for our ASCII signatures for our structure marker
		if ((VERSION_HEAD == psVersion->u32Vers) && (VERSION_TAIL == psVersion->u32Ion))
		{
			// Check calculated CRC against structure's CRC - they had better match
			if (*((uint32_t *) (pu8Data + offsetof(SVersionInfo, u32CRC32))) == crc32(0,
								  pu8Data,
								  sizeof(*psVersion) - sizeof(psVersion->u32CRC32)))
			{
				// Matching CRC! Go check the SHA256 if we want to			
				if (false == bSkipSHA256)
				{
					// Go see if the SHA256 is OK
					eCode = VersionCheckSHA256(pvAddress,
											   u64Offset,
											   psVersion);
					if (EVERSION_OK == eCode)
					{
						goto errorExit;
					}
				}
				else
				{
					// CRC32 Passed - not checking SHA256
					eCode = EVERSION_OK;
					goto errorExit;
				}
			}
			else
			{
				// Tag found but CRC failed - keep looking - likely will appear more than once
				eCode = EVERSION_BAD_CRC32;
			} 
		}
		
		s64Size -= sizeof(uint32_t);
		pu8Data += sizeof(uint32_t);
		u64Offset += sizeof(uint32_t);
	}

	// Couldn't find the tag. ;-(
	eCode = EVERSION_TAG_NOT_FOUND;
	psVersion = NULL;

errorExit:
	// If the caller wants the version info structure address, then return it
	if (ppsVersionInfo)
	{
		*ppsVersionInfo = psVersion;
	}
			
	return(eCode);
}

EVersionCode FindVersionStructureUnstamped(void *pvAddress,
										   int64_t s64Size,
										   SVersionInfo **ppsVersionInfo)
{
	SVersionInfo *psVersion = NULL;
	uint8_t *pu8Data = NULL;
	EVersionCode eCode = EVERSION_TAG_NOT_FOUND;
	uint32_t u32Offset = 0;
	
	// If the incoming image is too small, no way we'll find this version info
	if (s64Size < (sizeof(*psVersion) + 1))
	{
		eCode = EVERSION_TAG_NOT_FOUND;
		goto errorExit;
	}
	
	s64Size -= (sizeof(*psVersion) - 1);
	pu8Data = (uint8_t *) pvAddress;
	while (s64Size > 0)
	{
		psVersion = (SVersionInfo *) pu8Data;

		if (VERSION_HEAD == psVersion->u32Vers)
		{
			psVersion = psVersion;
		}

		// Look for version tag
		if ((VERSION_HEAD == psVersion->u32Vers) &&
			(VERSION_TAIL == psVersion->u32Ion))
		{
			// We found the VERS ION tag. Let's see if the SHA256 is the correct - 1-32
			// and if the CRC is 0
			if (0 == psVersion->u32CRC32)
			{
				uint8_t u8Loop;

				for (u8Loop = 1; u8Loop < 33; u8Loop++)
				{
					if (psVersion->u8SHA256[u8Loop - 1] != u8Loop)
					{
						break;
					}
				}

				if (33 == u8Loop)
				{
					if (ppsVersionInfo)
					{
						*ppsVersionInfo = psVersion;
					}

					eCode = EVERSION_OK;
					goto errorExit;
				}
				else
				{
					// Not found. Keep looking.
				}
			}
		}

		--s64Size;
		pu8Data++;
	}

	// Couldn't find the tag. ;-(
	eCode = EVERSION_TAG_NOT_FOUND;

errorExit:
	return(eCode);
}
