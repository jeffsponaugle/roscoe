#ifndef _MZ80_H_
#define _MZ80_H_

// fwd decl
typedef struct MemoryRead8 MemoryRead8;
typedef struct MemoryWrite8 MemoryWrite8;
typedef struct MemoryRead16 MemoryRead16;
typedef struct MemoryWrite16 MemoryWrite16;
typedef struct IORead8 IORead8;
typedef struct IOWrite8 IOWrite8;

typedef UINT8 (*MemoryReadHandler8)(UINT16 offs, MemoryRead8 *mr);
typedef void (*MemoryWriteHandler8)(UINT16 offs, UINT8 val, MemoryWrite8 *mw);
typedef UINT16 (*MemoryReadHandler16)(UINT32 offs, MemoryRead16 *mr);
typedef void (*MemoryWriteHandler16)(UINT32 offs, UINT16 val, MemoryWrite16 *mw);
typedef UINT8 (*IOReadHandler8)(UINT16 offs);
typedef void (*IOWriteHandler8)(UINT16 offs, UINT8 val);

// generic ranged read-handler for 8-bit data
struct MemoryRead8 
{
    UINT16 lowAddr, highAddr;
    MemoryReadHandler8 memoryCall;
};

// generic ranged write-handler for 8-bit data
struct MemoryWrite8 
{
    UINT16 lowAddr, highAddr;
    MemoryWriteHandler8 memoryCall;
};

// generic ranged read-handler for 16-bit data
struct MemoryRead16 
{
    UINT32 lowAddr, highAddr;
    MemoryReadHandler16 memoryCall;
    void *userData;
};

// generic ranged write-handler for 16-bit data
struct MemoryWrite16 
{
    UINT32 lowAddr, highAddr;
    MemoryWriteHandler16 memoryCall;
    void *userData;
};

// generic ranged read-handler for 8 bit IO
struct IORead8 
{
	UINT16 lowAddr, highAddr;
	IOReadHandler8 IOCall;
};

// generic ranged write-handler for 8 bit IO
struct IOWrite8 
{
	UINT16 lowAddr, highAddr;
	IOWriteHandler8 IOCall;
};

#define	CPU_EXEC_OK	0x80000000

typedef union
{
	UINT16 af;

	struct
	{
#ifdef MZ80_BIG_ENDIAN
		UINT8 a;
		UINT8 f;
#else
		UINT8 f;
		UINT8 a;
#endif
	} half;
} reg_af;

#define	z80AF	z80af.af
#define	z80A	z80af.half.a
#define	z80F	z80af.half.f

typedef union
{
	UINT16 bc;

	struct
	{
#ifdef MZ80_BIG_ENDIAN
		UINT8 b;
		UINT8 c;
#else
		UINT8 c;
		UINT8 b;
#endif
	} half;
} reg_bc;

#define	z80BC	z80bc.bc
#define	z80B	z80bc.half.b
#define	z80C	z80bc.half.c

typedef union
{
	UINT16 de;

	struct
	{
#ifdef MZ80_BIG_ENDIAN
		UINT8 d;
		UINT8 e;
#else
		UINT8 e;
		UINT8 d;
#endif
	} half;
} reg_de;

#define	z80DE	z80de.de
#define	z80D	z80de.half.d
#define	z80E	z80de.half.e

typedef union
{
	UINT16 hl;

	struct
	{
#ifdef MZ80_BIG_ENDIAN
		UINT8 h;
		UINT8 l;
#else
		UINT8 l;
		UINT8 h;
#endif
	} half;
} reg_hl;

#define	z80HL	z80hl.hl
#define	z80H	z80hl.half.h
#define	z80L	z80hl.half.l

#define	z80SP	z80sp.sp

typedef union
{
	UINT16 ix;

	struct
	{
#ifdef MZ80_BIG_ENDIAN
		UINT8 xh;
		UINT8 xl;
#else
		UINT8 xl;
		UINT8 xh;
#endif
	} half;
} reg_ix;

#define	z80IX	z80ix.ix
#define	z80XH	z80ix.half.xh
#define	z80XL	z80ix.half.xl

typedef union
{
	UINT16 iy;

	struct
	{
#ifdef MZ80_BIG_ENDIAN
		UINT8 yh;
		UINT8 yl;
#else
		UINT8 yl;
		UINT8 yh;
#endif
	} half;
} reg_iy;

#define	z80IY	z80iy.iy
#define	z80YH	z80iy.half.yh
#define	z80YL	z80iy.half.yl

// Z80 Context
typedef struct CONTEXTMZ80
{
	UINT8 *pu8Base;			// Pointer to non-code memory
	UINT8 *pu8CodeBase;		// Pointer to code fetch memory
	MemoryRead8 *mz80MemoryRead;		// Read/write handlers for Z80 memory
	MemoryWrite8 *mz80MemoryWrite;
	IORead8 *mz80IORead;		// Read write handlers for Z80 I/O
	IOWrite8 *mz80IOWrite;
	UINT32 u32ClockTicks;	// T-State counter
	reg_af z80af;
	reg_bc z80bc;
	reg_de z80de;
	reg_hl z80hl;
	reg_ix z80ix;
	reg_iy z80iy;
	UINT16 z80sp;
	UINT16 z80pc;
	UINT16 z80afprime;
	UINT16 z80bcprime;
	UINT16 z80deprime;
	UINT16 z80hlprime;
	UINT16 z80intAddr;		// Interrupt address
	UINT8 z80i;				// Interrupt register
	UINT8 z80iff;			// IFF Register (internal)
	UINT8 z80r;				// Refresh register
	UINT8 z80rBit8;			// Bit 8 of the refresh register
	UINT8 z80InterruptMode;	// What's the interrupt mode?
	UINT8 u8Mode2LowAddr;	// For IM2, this is the "low" part of the address
	BOOL bHalted;			// CPU halted?
} CONTEXTMZ80;

// Processor flag defines
#define	Z80F_C		0x01	// Carry
#define Z80F_N		0x02	// Negative
#define Z80F_PV		0x04	// Parity/overflow
#define Z80F_XF		0x08	// Undefined (bit 3 of the result)
#define	Z80F_H		0x10	// Half carry
#define Z80F_YF		0x20	// Undefined (bit 5 of the result)
#define Z80F_Z		0x40	// Zero
#define Z80F_S		0x80	// Sign

// IFF Flag defines
#define	IFF1		0x01
#define IFF2		0x02
#define IRQ_LATCH	0x04	// Used to signify IRQ

extern void MZ80Reset(CONTEXTMZ80 *psCPU);
extern void MZ80Init(CONTEXTMZ80 *psCPU);
extern UINT32 MZ80Exec(CONTEXTMZ80 *psCPU,
					   INT32 s32Cycles);
extern UINT32 z80DisassembleOne(UINT32 address, 
								UINT8 *data, 
								UINT8 *pszOpcodeBytes, 
								UINT8 *pszOperation,
								UINT8 *pszArgument);
extern BOOL MZ80INT(CONTEXTMZ80 *psCPU);
extern void MZ80NMI(CONTEXTMZ80 *psCPU);
extern UINT32 MZ80GetTicks(CONTEXTMZ80 *psCPU, 
						   BOOL bResetTicks);
#ifdef _WIN32
extern void MZ80StartTrace(void);
#endif

extern UINT32 z80DisassembleOne(UINT32 address, 
								UINT8 *data, 
								UINT8 *pszOpcodeBytes, 
								UINT8 *pszOperation,
								UINT8 *pszArgument);

#endif	// #ifndef _MZ80_H_
