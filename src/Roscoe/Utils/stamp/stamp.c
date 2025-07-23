#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifndef _WIN32
#include <endian.h>
#else
#		define htobe16(x) htons(x)
#		define htole16(x) (x)
#		define be16toh(x) ntohs(x)
#		define le16toh(x) (x)
 
#		define htobe32(x) htonl(x)
#		define htole32(x) (x)
#		define be32toh(x) ntohl(x)
#		define le32toh(x) (x)
 
#		define htobe64(x) htonll(x)
#		define htole64(x) (x)
#		define be64toh(x) ntohll(x)
#		define le64toh(x) (x)

#endif
#include <time.h>
#include "Shared/Version.h"
#include "Shared/zlib/zlib.h"

// This will find a basic version structure (unstamped)
SImageVersion *VersionFindBasicStructure(uint8_t *pu8BaseAddress,
										 uint32_t u32Size,
										 uint32_t *pu32Offset)
{
	SImageVersion *psImageVersion;
	uint32_t u32Offset = 0;

	// If our size is smaller than the image version structure, then we've not
	// found it.
	if (u32Size < sizeof(SImageVersion))
	{
		psImageVersion = NULL;
		goto errorExit;
	}

	u32Size -= sizeof(*psImageVersion);
	while (u32Size >= 4)
	{
		psImageVersion = (SImageVersion *) pu8BaseAddress;

		// Do we have a proper prefix?
		if (be32toh(psImageVersion->u32VersionPrefix) != VERSION_PREFIX)
		{
			// Skip it
		}
		else
		// And a proper suffix?
		if (be32toh(psImageVersion->u32VersionSuffix) != VERSION_SUFFIX)
		{
			// Skip it
		}
		else
		// Is the image type known?
		if ((be32toh(psImageVersion->eImageType) <= EIMGTYPE_UNKNOWN) || (be32toh(psImageVersion->eImageType) >= EIMGTYPE_COUNT))
		{
			printf("Unreasonable image type\n");
			// Image type is unreasonable
		}
		else
		// Stamp fill for image?
		if (be32toh(psImageVersion->u32ImageCRC32) != STAMP_IMAGE_CRC32_FILL)
		{
			printf("CRC32 Fill not seen\n");
			// Image CRC32 fill not correct
		}
		else
		// Stamp fill for version structure?
		if (be32toh(psImageVersion->u32VersionStructCRC32) != STAMP_VERSION_CRC32_FILL)
		{
			printf("Stamp fill not seen\n");
			// Image CRC32 fill not correct
		}
		else
		{
			// Found it!
			if (pu32Offset)
			{
				*pu32Offset = u32Offset;
			}

			printf("Unstamped image found at offset %u\n", u32Offset);

			return(psImageVersion);
		}

		u32Offset += sizeof(uint32_t);
		pu8BaseAddress += sizeof(uint32_t);
		u32Size -= sizeof(uint32_t);
	}

errorExit:
	return(NULL);
}

int main(int argc, char **argv)
{
	FILE *psFile = NULL;
	uint32_t u32Size = 0;
	uint8_t *pu8Data = NULL;
	uint32_t u32VersionOffset = 0;
	uint32_t u32CRC;
	uint64_t u64Timestamp;
	SImageVersion *psImageVersion = NULL;

	if (argc < 2)
	{
		printf("Usage: stamp filename\n");
		return(1);
	}
  
	psFile = fopen(argv[1], "rb");
	if (NULL == psFile)
	{
		printf("File '%s' not found\n", argv[1]);
		return(1);
	}
	
	// Figure out how big it is
	fseek(psFile, 0, SEEK_END);
	u32Size = ftell(psFile);
	fseek(psFile, 0, SEEK_SET);
	
	pu8Data = calloc(u32Size, 1);
	if (NULL == pu8Data)
	{
		printf("Failed to malloc() %u bytes\n", u32Size);
		fclose(psFile);
		return(1);
	}

	printf("Reading in %u bytes\n", u32Size);
	fread(pu8Data, 1, u32Size, psFile);
	fclose(psFile);

	// Go find the unstamped version structure
	psImageVersion = VersionFindBasicStructure(pu8Data,
											   u32Size,
											   &u32VersionOffset);

	if (NULL == psImageVersion)
	{
		printf("Failed to find unstamped version structure\n");
		return(1);
	}

	// Found our version image! Let's do some work... Remember, everything is
	// big endian targeted.

	u64Timestamp = (uint64_t) time(0);
	psImageVersion->u64BuildTimestamp = htobe64(u64Timestamp);

	// Get the CRC of the image

	// Before the version structure
	u32CRC = crc32(VERSION_CRC32_IMAGE_INITIAL,
				   pu8Data,
				   u32VersionOffset);

	printf("After image CRC32 = %u\n", u32VersionOffset);
	// And after the version structure
	u32CRC = crc32(u32CRC,
				   pu8Data + sizeof(*psImageVersion) + u32VersionOffset,
				   u32Size - sizeof(*psImageVersion) - u32VersionOffset);

	psImageVersion->u32ImageCRC32 = htobe32(u32CRC);
	psImageVersion->u32ImageSize = htobe32(u32Size);

	// CRC the structure. Remember... don't CRC the CRC32 field
	u32CRC = crc32(VERSION_CRC32_STRUCT_INITIAL,
				   (const unsigned char *) psImageVersion,
				   ((uint32_t) &psImageVersion->u32VersionStructCRC32) - ((uint32_t) psImageVersion));

	// And CRC everything past the u32VersionStructCRC32 member
	u32CRC = crc32(u32CRC,
				   ((const unsigned char *) &psImageVersion->u32VersionStructCRC32) + sizeof(psImageVersion->u32VersionStructCRC32),
				   ((uint32_t) (psImageVersion + 1)) - (((uint32_t) &psImageVersion->u32VersionStructCRC32) + sizeof(psImageVersion->u32VersionStructCRC32)));

	psImageVersion->u32VersionStructCRC32 = htobe32(u32CRC);

	// Time to write everything back
	psFile = fopen(argv[1], "wb");
	if (NULL == psFile)
	{
		printf("Can't open '%s' for writing\n", argv[1]);
		return(1);
	}
	
	fwrite((void *) pu8Data, 1, u32Size, psFile);
	fclose(psFile);
	
	printf("Successfully stamped\n");
	return(0);
}

