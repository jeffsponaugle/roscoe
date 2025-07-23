#ifndef _ELF_H_
#define _ELF_H_

typedef enum
{
	ELFSTATE_INIT,
	ELFSTATE_HEADER_CONSUME,
	ELFSTATE_PROGRAM_HEADER_FIND,
	ELFSTATE_PROGRAM_HEADER_CONSUME,
	ELFSTATE_PROGRAM_FIND,
	ELFSTATE_PROGRAM_CONSUME
} EELFState;

// ELF header prefix
typedef struct SELFHeaderPrefix
{
	uint8_t u8ELFMagicNumber[4];	// 7f 45 4c 46
	uint8_t u8Class;				// 1=32 bit, 2=64 bit
	uint8_t u8DataEndian;			// 1=Big endian, 2=Little endian
	uint8_t u8Version;				// Set to 1 for original/current version of ELF
	uint8_t u8OSABI;				// Operating system ABI
	uint8_t u8ABIVersion;			// ABI Version
	uint8_t u8Pad0[7];				// Padding
	uint16_t u16Type;				// Object file type
	uint16_t u16Machine;			// Instruction set architecture
	uint32_t u32ELFVersion;			// ELF Version
} __attribute__((packed)) SELFHeaderPrefix;

// ELF 64 bit program info
typedef struct SELFHeaderProgram64
{
	uint64_t u64EntryPoint;			// Entry point of program
	uint64_t u64ProgramHeaderTable;	// Points to the start of program header table
	uint64_t u64SectionHeaderTable;	// Section header table
} __attribute__((packed)) SELFHeaderProgram64;

// ELF header suffix
typedef struct SELFHeaderSuffix
{
	uint32_t u32Flags;				// File flags
	uint16_t u16HeaderSize;			// Header size (in bytes)
	uint16_t u16PHentSize;			// Program header table entry size
	uint16_t u16PNum;				// # Of entries in Program header table entry
	uint16_t u16SHentSize;			// Section header table entry size
	uint16_t u16SHNum;				// # Of entries in the section header table
	uint16_t u16SectionNameIndex;	// Which section header table contains the names of the sections?
} __attribute__((packed)) SELFHeaderSuffix;

// Program header (32 bit)
typedef struct SELFProgramHeader32
{
	uint32_t u32Type;				// Type of program
	uint32_t u32SegmentOffset;		// Program offset
	uint32_t u32VirtualAddress;		// Where is this program located virtually?
	uint32_t u32PhysicalAddress;	// Where is this program located physically?
	uint32_t u32SegmentFileImage;	// How many bytes is this segment in the file?
	uint32_t u32MemoryFileImage;	// How many bytes is this segment in memory?
	uint32_t u32SegmentFlags;		// Segment dependent flags
	uint32_t u32Alignment;			// Alignment
} __attribute__((packed)) SELFProgramHeader32;

// Program header (64 bit)
typedef struct SELFProgramHeader64
{
	uint32_t u32Type;				// Type of program
	uint32_t u32Flags;				// Segment dependent flags
	uint32_t u64SegmentOffset;		// Program offset
	uint32_t u64VirtualAddress;		// Where is this program located virtually?
	uint32_t u64PhysicalAddress;	// Where is this program located physically?
	uint32_t u64SegmentFileImage;	// How many bytes is this segment in the file?
	uint32_t u64MemoryFileImage;	// How many bytes is this segment in memory?
	uint32_t u64SegmentFlags;		// Segment dependent flags
	uint32_t u64Alignment;			// Alignment
} __attribute__((packed)) SELFProgramHeader64;

typedef struct SELF
{
	EELFState eELFState;		// What state are we in in our state machine?
	uint8_t u8ELFHeader[sizeof(SELFHeaderPrefix) + sizeof(SELFHeaderSuffix) + sizeof(SELFHeaderProgram64)];	// ELF Header (big enough for 32 or 64 bit)
	uint8_t u8ProgramHeader[sizeof(SELFProgramHeader64)];	// Program header block
	uint8_t u8VectorTable[0x1000];	// Vector table (if image loads it)
	uint32_t u32ExecStart;		// Entry point for the ELF program
	bool bTransferToggle;		// Boolean to show activity on POST LEDs

	// Generic variables used for consuming blocks of data
	uint8_t *pu8DataPtr;		// Pointer to next location to write data
	uint32_t u32DataOffset;		// # Of bytes received in this data accumulation
	uint32_t u32DataSize;		// # Of bytes to receive (total)
	uint32_t u32ELFOffset;		// How many bytes are we into the ELF file?
} SELF;

extern EStatus ELFInit(SELF *psELF);
extern EStatus ELFProcessRXData(SELF *psELF,
								uint8_t *pu8Buffer,
								uint32_t u32DataLength);

#endif

