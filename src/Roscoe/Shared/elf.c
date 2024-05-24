#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "Shared/Shared.h"
#include "BIOS/OS.h"
#include "Shared/elf.h"
#include "Hardware/Roscoe.h"

// NOTE: This module is not endian safe and assumes big endian. It is by no means
// a complete implementation of an ELF loader.

// Standard ELF header
static const uint8_t sg_u8ELFHeader[] = {0x7f, 0x45, 0x4c, 0x46};

// Class offset
#define	ELFCLASS_32	   			0x01		// 32 Bit
#define	ELFCLASS_64				0x02		// 64 Bit

// Data offset
#define	ELFDATA_LITTLE_ENDIAN	0x01		// Little endian
#define	ELFDATA_BIG_ENDIAN		0x02		// Big endian

// Architecture
#define	ELFARCH_68000			0x0004		// Motorola 68000

// Program header p_type values
#define	PT_LOAD					0x0001		// Loadable segment

// ELF 32 bit program info
typedef struct SELFHeaderProgram32
{
	uint32_t u32EntryPoint;			// Entry point of program
	uint32_t u32ProgramHeaderTable;	// Points to the start of program header table
	uint32_t u32SectionHeaderTable;	// Section header table
} __attribute__((packed)) SELFHeaderProgram32;

// Process the incoming ELF state machine data
EStatus ELFProcessRXData(SELF *psELF,
						 uint8_t *pu8Buffer,
						 uint32_t u32DataLength)
{
	EStatus eStatus = ESTATUS_OK;
	SELFHeaderPrefix *psELFPrefix = (SELFHeaderPrefix *) psELF->u8ELFHeader;
	SELFHeaderProgram32 *psELFHeaderProgram32 = (SELFHeaderProgram32 *) (psELFPrefix + 1);
	SELFProgramHeader32 *psELFProgramHeader32 = (SELFProgramHeader32 *) psELF->u8ProgramHeader;

	while (u32DataLength)
	{
		switch (psELF->eELFState)
		{
			case ELFSTATE_INIT:
			{
				// Start consuming the header
				psELF->pu8DataPtr = (uint8_t *) &psELF->u8ELFHeader;

				// We start by assuming it's a 32 bit ELF header and we can change it later
				psELF->u32DataSize = sizeof(SELFHeaderPrefix) + sizeof(SELFHeaderSuffix) + sizeof(SELFHeaderProgram32);

				// Set our offset to 0 again
				psELF->u32DataOffset = 0;

				// Clear out the header
				memset((void *) psELF->u8ELFHeader, 0, sizeof(psELF->u8ELFHeader));

				// And go to header consume
				psELF->eELFState = ELFSTATE_HEADER_CONSUME;
				break;
			}

			// Consume the ELF header
			case ELFSTATE_HEADER_CONSUME:
			{
				psELF->u8ELFHeader[psELF->u32DataOffset] = *pu8Buffer;
				psELF->u32DataOffset++;
				psELF->u32ELFOffset++;
				++pu8Buffer;
				--u32DataLength;

				POST_SET(POSTCODE_ELF_START);

				// If we've consumed the prefix, then do a basic check before we continue
				if (sizeof(*psELFPrefix) == psELF->u32DataOffset)
				{
					bool bHeaderBad = false;

					// Check for ELF header
					if (memcmp((void *) psELF->u8ELFHeader,
							   (void *) sg_u8ELFHeader,
							   sizeof(sg_u8ELFHeader)) == 0)
					{
						// It's an ELF!
						printf("\nBegin ELF file transfer\n");
					}
					else
					{
						printf("\nNot an ELF file (ELF Signature incorrect)\n");
						bHeaderBad = true;
					}

					// Only check the other fields if we think this is an ELF file
					if (false == bHeaderBad)
					{
						// Check the class offset
						if (ELFCLASS_32 == psELFPrefix->u8Class)
						{
							// Cool! 32 Bit.
						}
						else
						if (ELFCLASS_64 == psELFPrefix->u8Class)
						{
							// 64 Bit, even though it's not supported, let's change our header size
							psELF->u32DataSize = sizeof(SELFHeaderPrefix) + sizeof(SELFHeaderSuffix) + sizeof(SELFHeaderProgram64);
							printf("64 Bit ELF images not supproted\n");
							bHeaderBad = true;
						}
						else
						{
							printf("Unknown class value 0x%.2x\n", psELFPrefix->u8Class);
							bHeaderBad = true;
						}

						// Check the data offset
						if (ELFDATA_BIG_ENDIAN == psELFPrefix->u8DataEndian)
						{
							// Cool - big endian
						}
						else
						{
							// Either little endian or unknown
							if (ELFDATA_LITTLE_ENDIAN == psELFPrefix->u8DataEndian)
							{
								printf("Little endian not supported\n");
							}
							else
							{
								printf("Unknown data class value 0x%.2x\n", psELFPrefix->u8DataEndian);
							}

							bHeaderBad = true;
						}

						if (psELFPrefix->u16Machine != ELFARCH_68000)
						{
							printf("Unknown CPU type - 0x%.4x - only 68000 supported\n", psELFPrefix->u16Machine);
							bHeaderBad = true;
						}
					}

					if (bHeaderBad)
					{
						goto errorExitCancel;
					}
				}

				// See if we're done or not
				if (psELF->u32DataOffset >= psELF->u32DataSize)
				{
					// Go find the program header
					psELF->eELFState = ELFSTATE_PROGRAM_HEADER_FIND;
				}

				break;
			}

			case ELFSTATE_PROGRAM_HEADER_FIND:
			{
				uint32_t u32Chunk;

				assert(psELF->u32DataOffset <= psELFHeaderProgram32->u32ProgramHeaderTable);
				u32Chunk = psELFHeaderProgram32->u32ProgramHeaderTable - psELF->u32DataOffset;

				if (0 == u32Chunk)
				{
					// We're here already! Advance 
					psELF->eELFState = ELFSTATE_PROGRAM_HEADER_CONSUME;

					psELF->pu8DataPtr = psELF->u8ProgramHeader;
					psELF->u32DataOffset = 0;
					psELF->u32DataSize = sizeof(SELFProgramHeader32);

					POST_SET(POSTCODE_ELF_HEADER);
				}
				else
				{
					if (u32Chunk > u32DataLength)
					{
						u32Chunk = u32DataLength;
					}

					// Skip over the data
					pu8Buffer += u32Chunk;
					u32DataLength -= u32Chunk;
					psELF->u32ELFOffset += u32Chunk;
				}
				break;
			}

			case ELFSTATE_PROGRAM_HEADER_CONSUME:
			{
				*psELF->pu8DataPtr = *pu8Buffer;
				++psELF->pu8DataPtr;
				++pu8Buffer;
				--u32DataLength;
				++psELF->u32DataOffset;
				++psELF->u32ELFOffset;
				if (psELF->u32DataOffset == psELF->u32DataSize)
				{
					if (psELFProgramHeader32->u32Type != PT_LOAD)
					{
						printf("Program p_type field not PT_LOAD - 0x%.4x\n", psELFProgramHeader32->u32Type);
						goto errorExitCancel;
					}

/*					printf("p_type   = 0x%.8x\n", psELFProgramHeader32->u32Type);
					printf("p_offset = 0x%.8x\n", psELFProgramHeader32->u32SegmentOffset);
					printf("p_vaddr  = 0x%.8x\n", psELFProgramHeader32->u32VirtualAddress);
					printf("p_paddr  = 0x%.8x\n", psELFProgramHeader32->u32PhysicalAddress);
					printf("p_filesz = 0x%.8x\n", psELFProgramHeader32->u32SegmentFileImage);
					printf("p_memsz  = 0x%.8x\n", psELFProgramHeader32->u32MemoryFileImage);
					printf("p_flags  = 0x%.8x\n", psELFProgramHeader32->u32SegmentFlags);
					printf("p_align  = 0x%.8x\n", psELFProgramHeader32->u32Alignment); */

					printf("Program load address of 0x%.8x for 0x%.8x bytes\n",
						   psELFProgramHeader32->u32PhysicalAddress,
						   psELFProgramHeader32->u32SegmentFileImage);

					// Copy in the execution address
					psELF->u32ExecStart = psELFHeaderProgram32->u32EntryPoint;

					// We got it! Now let's find the program itself
					psELF->eELFState = ELFSTATE_PROGRAM_FIND;
				}

				break;
			}

			case ELFSTATE_PROGRAM_FIND:
			{
				uint32_t u32Chunk;

				assert(psELF->u32ELFOffset <= psELFProgramHeader32->u32SegmentOffset);
				u32Chunk = psELFProgramHeader32->u32SegmentOffset - psELF->u32ELFOffset;

				if (0 == u32Chunk)
				{
					// We're here already! Advance 
					psELF->eELFState = ELFSTATE_PROGRAM_CONSUME;

					psELF->pu8DataPtr = (uint8_t *) psELFProgramHeader32->u32PhysicalAddress;
					psELF->u32DataOffset = 0;
					psELF->u32DataSize = psELFProgramHeader32->u32SegmentFileImage;

					POST_SET(POSTCODE_ELF_PROGRAM_HEADER);
				}
				else
				{
					if (u32Chunk > u32DataLength)
					{
						u32Chunk = u32DataLength;
					}

					// Skip over the data
					pu8Buffer += u32Chunk;
					u32DataLength -= u32Chunk;
					psELF->u32ELFOffset += u32Chunk;
				}

				break;
			}

			case ELFSTATE_PROGRAM_CONSUME:
			{
				uint32_t u32Chunk;
				uint32_t u32PhysicalAddress;

				if (psELF->bTransferToggle)
				{
					psELF->bTransferToggle = false;
					POST_SET(POSTCODE_ELF_PROGRAM_CONSUME1);
				}
				else
				{
					psELF->bTransferToggle = true;
					POST_SET(POSTCODE_ELF_PROGRAM_CONSUME2);
				}

				// Print out a xxK every 8K.
				if (0 == (psELF->u32DataOffset & 0x1fff))
				{
					printf("%uK/%uK\n", psELF->u32DataOffset >> 10, psELFProgramHeader32->u32SegmentFileImage >> 10);
				}

				u32Chunk = psELFProgramHeader32->u32SegmentFileImage - psELF->u32DataOffset;
				if (0 == u32Chunk)
				{
//					printf("Load complete!\n");
					printf("%uK/%uK\n", psELF->u32DataOffset >> 10, psELFProgramHeader32->u32SegmentFileImage >> 10);
					eStatus = ESTATUS_TRANSFER_COMPLETE;
					psELF->eELFState = ELFSTATE_INIT;
					POST_SET(POSTCODE_ELF_COMPLETE);
					goto errorExit;
				}

				// Clip the chunk size to the length of our data
				if (u32Chunk > u32DataLength)
				{
					u32Chunk = u32DataLength;
				}

				u32PhysicalAddress = psELFProgramHeader32->u32PhysicalAddress + psELF->u32DataOffset;
//				printf("Chunk size=%u, address=0x%.8x, u32DataOffset=0x%.8x\n", u32Chunk, u32PhysicalAddress, psELF->u32DataOffset);

				// If we're in the first 4K of address space, we need to put it in the *psELF structure's
				// vector table
				if (u32PhysicalAddress < sizeof(psELF->u8VectorTable))
				{
					u32Chunk = sizeof(psELF->u8VectorTable) - psELF->u32DataOffset;
					if (u32Chunk > u32DataLength)
					{
						u32Chunk = u32DataLength;
					}

//					printf(" Chunk size2=%u, address=0x%.8x, u32DataOffset=0x%.8x\n", u32Chunk, u32PhysicalAddress, psELF->u32DataOffset);
					memcpy((void *) &psELF->u8VectorTable[u32PhysicalAddress],
						   (void *) pu8Buffer,
						   u32Chunk);
				}
				else
				{
					// Just copy the data
					memcpy((void *) u32PhysicalAddress,
						   (void *) pu8Buffer,
						   u32Chunk);
				}

				// Advance our pointers
				pu8Buffer += u32Chunk;
				u32DataLength -= u32Chunk;
				psELF->u32DataOffset += u32Chunk;
				psELF->u32ELFOffset += u32Chunk;
				break;
			}

			default:
			{
				printf("Unknown ELF state - %u\n", psELF->eELFState);
				break;
			}
		}

	}

	// All good so far
	return(ESTATUS_OK);

errorExitCancel:
	eStatus = ESTATUS_CANCELED;
	psELF->eELFState = ELFSTATE_INIT;
	printf("ELF Transfer canceled\n");
	POST_SET(POSTCODE_ELF_BAD_IMAGE);

errorExit:
	return(eStatus);
}

// Resets the ELF state machine
EStatus ELFInit(SELF *psELF)
{
	memset((void *) psELF, 0, sizeof(*psELF));
	psELF->eELFState = ELFSTATE_INIT;
	return(ESTATUS_OK);
}

