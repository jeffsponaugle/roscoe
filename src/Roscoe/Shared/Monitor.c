#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "Hardware/Roscoe.h"
#include "BIOS/OS.h"
#include "Shared/Version.h"
#include "Shared/LineInput.h"
#include "Shared/lex.h"
#include "Shared/16550.h"
#include "Shared/sbrk.h"
#include "Shared/dis68k.h"
#include "BIOS/BIOS.h"
#include "Shared/IDE.h"
#include "Shared/MemTest.h"
#include "Shared/AsmUtils.h"
#include "Shared/rtc.h"
#include "Shared/Shared.h"
#include "Shared/ptc.h"
#include "Shared/Stream.h"
#include "Shared/YModem.h"
#include "Shared/elf.h"
#include "Shared/Interrupt.h"
#include "Shared/FaultHandler.h"
#include "Shared/LinkerDefines.h"
#include "Shared/DOS.h"
#include "Shared/82C42.h"

// Maximum # of characters per input line
#define	MONITOR_LINE_MAX		256

// Depth of line history (in lines)
#define	MONITOR_HISTORY_DEPTH	20

// Which UART is the console
#define	MONITOR_CONSOLE_UART	0

// # Of lines to dump/disassemble
static uint16_t sg_u16DumpLines = 8;

static SYModem sg_sYModem;

static EStatus MonitorDataCallback(EStatus eStatusIncoming,
								   uint32_t u32Offset,
								   uint8_t *pu8RXData,
								   uint16_t u16BytesReceived,
								   void *pvUserData)
{
	EStatus eStatus = ESTATUS_OK;

	if (ESTATUS_OK == eStatusIncoming)
	{
		if (0 == u32Offset)
		{
			// Init ELF state machine
			eStatus = ELFInit((SELF *) pvUserData);
			ERR_GOTO();
		}

		// Got some data
		eStatus = ELFProcessRXData((SELF *) pvUserData,
								   pu8RXData,
								   u16BytesReceived);
		ERR_GOTO();
	}

errorExit:
	return(eStatus);
}

// Wait for a character to be typed from the console
static EStatus MonitorInputGet(char *peInputChar)
{
	EStatus eStatus = ESTATUS_OK;

	for (;;)
	{
		uint16_t u16DataReceived;

//		assert(0);
//		eStatus = KeyboardGetKey(peInputChar);
		if (ESTATUS_OK == eStatus)
		{
//			break;
		}

		eStatus = SerialReceiveData(MONITOR_CONSOLE_UART,
									(uint8_t *) peInputChar,
									sizeof(*peInputChar),
									&u16DataReceived);
		if ((ESTATUS_OK == eStatus) && (1 == u16DataReceived))
		{
			break;
		}

		eStatus = YModemPump(&sg_sYModem);
		ERR_GOTO();
	}

errorExit:
	return(eStatus);
}

// Output a character string to the console
static void MonitorOutput(char *peText,
						  uint16_t u16Length)
{
	(void) write(0,
				 peText,
				 (int) u16Length);
}

// Returns TRUE if there's a console character available
static bool MonitorInputAvailable(void)
{
	EStatus eStatus;
	uint16_t u16DataCount;

	YModemPump(&sg_sYModem);

	eStatus = SerialReceiveDataGetCount(MONITOR_CONSOLE_UART,
										&u16DataCount);
	if ((ESTATUS_OK == eStatus) &&
		(u16DataCount))
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// Used to track lexical state
typedef struct SMonitorLineState
{
	char *peLineText;
	char *peLinePointer;
	uint16_t u16LineLength;
} SMonitorLineState;

// Checks to see if an address is within the CPU's execution range
static EStatus MonitorCheckAddress(char *peAddressType,
								   uint32_t u32Address)
{
	EStatus eStatus = ESTATUS_OK;

	if (u32Address > 0xffffff)
	{
		printf("%s0x%.8x is beyond the CPU's address space\n", peAddressType, u32Address);
		eStatus = ESTATUS_MONITOR_ADDRESS_TOO_HIGH;
	}   								   

	return(eStatus);
}

static bool MonitorTerminateDumpCallback(void)
{
	// Look for input of any kind, and if we get it, stop the 
	if (MonitorInputAvailable())
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// Dumps memory contents. Accepts the following:
//
// d 					Dump at pointer for x number of lines)
// d address 			Set pointer to address and dump for # of lines)
// d address count 	 	Set pointer to address and dump for count bytes
// d address-address	Dump from adddress to address
static EStatus MonitorDumpMemory(SLex *psLex,
								 const SMonitorCommands *psMonitorCommand,
								 uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	uint8_t u8LineBufferPrior[16];
	uint8_t u8LineBuffer[16];
	uint32_t u32AddressFrom;
	uint32_t u32AddressTo;
	SToken sToken;
	ETokenType eToken;
	uint32_t u32Loop;
	bool bLineBufferPrimed = false;
	bool bRepeatActive = false;

	ZERO_STRUCT(sToken);
	u32AddressFrom = *pu32AddressPointer;

	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Just the dump by itself. So we go for the # of lines 
		u32AddressTo = u32AddressFrom + (sg_u16DumpLines << 4) - 1;

		// Clip it
		if (u32AddressTo > 0xffffff)
		{
			u32AddressTo = 0xffffff;
		}
	}
	else
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken) ||
		('.' == eToken))
	{
		if ('.' == eToken)
		{
			// From address is already set to the pointer
		}
		else
		{
			// We have (at least) a start address
			u32AddressFrom = (uint32_t) sToken.uData.u64IntValue;
		}

		LexClearToken(&sToken);

		// Let's see if there's more
		eToken = LexGetNextToken(psLex,
								 &sToken);
		if (ELEX_EOF == eToken)
		{
			// It's just the address. Dump a line count full.
			u32AddressTo = u32AddressFrom + (sg_u16DumpLines << 4) - 1;

			// Clip it
			if (u32AddressTo > 0xffffff)
			{
				u32AddressTo = 0xffffff;
			}
		}
		else
		if ((ELEX_INT_SIGNED == eToken) ||
			(ELEX_INT_UNSIGNED == eToken))
		{
			// It's a count of bytes!
			u32AddressTo = u32AddressFrom + ((uint32_t) sToken.uData.u64IntValue);

			// Clip it
			if (u32AddressTo > 0xffffff)
			{
				u32AddressTo = 0xffffff;
			}
		}
		else
		if ('-' == eToken)
		{
			// It's a range
			LexClearToken(&sToken);
			
			eToken = LexGetNextToken(psLex,
									 &sToken);

			// Next should be a number of some sort
			if (ELEX_INT_SIGNED == eToken)
			{
				u32AddressTo = ((uint32_t) (-sToken.uData.s64IntValue));
			}
			else
			if (ELEX_INT_UNSIGNED == eToken)
			{
				u32AddressTo = ((uint32_t) sToken.uData.u64IntValue);
			}
			else
			{
				printf("Error: Expecting dump address-address\n");
				eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
				goto errorExit;
			}
		}
		else
		{
			printf("Error: Expecting either dump address, dump address count, or dump address-address\n");
			eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
			goto errorExit;
		}
	}
	else
	{
		printf("Error: Expecting dump address\n");
		eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
		goto errorExit;
	}

	// If end address is ahead of the start address...
	if (u32AddressTo < u32AddressFrom)
	{
		printf("Start address of 0x%.6x is larger than end address of 0x%.6x\n", u32AddressFrom, u32AddressTo);
		eStatus = ESTATUS_MONITOR_START_LARGER_THAN_END_ADDRESS;
		goto errorExit;
	}

	eStatus = MonitorCheckAddress("From address", u32AddressFrom);
	ERR_GOTO();
	eStatus = MonitorCheckAddress("To address", u32AddressTo);
	ERR_GOTO();

	(void) DumpHex(u32AddressFrom & ~0xf,
				   (u32AddressTo - u32AddressFrom) + 1,
				   0,
				   (uint8_t *) u32AddressFrom,
				   (uint32_t *) pu32AddressPointer,
				   true,
				   MonitorTerminateDumpCallback);

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Show stack and stack pointer info
static EStatus MonitorStack(SLex *psLex,
						    const SMonitorCommands *psMonitorCommand,
						    uint32_t *pu32AddressPointer)
{
	uint32_t u32HeapEnd;
	uint32_t u32LowestStack;
	uint32_t u32StackPointer;

	SBRKGetInfo(&u32HeapEnd,
				&u32LowestStack,
				&u32StackPointer);

	printf("Stack pointer=0x%.6x\n", u32StackPointer);
	printf("Lowest stack =0x%.6x\n", u32LowestStack);
	printf("Heap end     =0x%.6x\n", u32HeapEnd);
	if (u32HeapEnd < u32LowestStack)
	{
		printf("Detected free=%u bytes\n", u32LowestStack - u32HeapEnd);
	}
	else
	{
		printf("Detected free=**COLLISION - OUT OF MEMORY**\n");
	}

	return(ESTATUS_OK);
}

// Fill memory with a byte pattern
//
// fill addrstart count pattern
// fill addrstart-addrend pattern

static EStatus MonitorFill(SLex *psLex,
						   const SMonitorCommands *psMonitorCommand,
						   uint32_t *pu32AddressPointer,
						   uint8_t u8Size)
{
	EStatus eStatus;
	uint32_t u32AddressFrom;
	uint32_t u32AddressTo;
	SToken sToken;
	ETokenType eToken;
	bool bRange = false;
	uint8_t u8Pattern;
	uint16_t u16Pattern;
	uint32_t u32Pattern;

	ZERO_STRUCT(sToken);
	u32AddressFrom = *pu32AddressPointer;

	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken) ||
		('.' == eToken))
	{
		if ('.' == eToken)
		{
			// From address is already set to the pointer
		}
		else
		{
			// We have (at least) a start address
			u32AddressFrom = (uint32_t) sToken.uData.u64IntValue;
		}

		LexClearToken(&sToken);
	}
	else
	{
		printf("Error: Expecting start address\n");
		eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
		goto errorExit;
	}

	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ETokenType) '-' == eToken)
	{
		// It's a range
		bRange = true;
		LexClearToken(&sToken);

		// Next should be a count
		eToken = LexGetNextToken(psLex,
								 &sToken);
		if ((ELEX_INT_SIGNED == eToken) ||
			(ELEX_INT_UNSIGNED == eToken))
		{
			u32AddressTo = (uint32_t) sToken.uData.u64IntValue;
		}
		else
		{
			printf("Error: Expecting addfrom-addrto\n");
			eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
			goto errorExit;
		}
	}
	else
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		// It's a count.
		u32AddressTo = u32AddressFrom + ((uint32_t) sToken.uData.u64IntValue);

		// u32AddressTo should be the final address - 1
		if (sToken.uData.u64IntValue)
		{
			u32AddressTo--;
		}
	}
	else
	{
		printf("Error: Expecting either a range - addrfrom-addrto or a count - addrfrom count\n");
		eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
		goto errorExit;
	}

	LexClearToken(&sToken);

	// Now get the pattern byte
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		if (sizeof(uint8_t) == u8Size)
		{
			if (sToken.uData.u64IntValue > 0xff)
			{
				printf("Error: Fill value exceeds 8 bits\n");
				eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
				goto errorExit;
			}

			u8Pattern = (uint8_t) sToken.uData.u64IntValue;
		}
		else
		if (sizeof(uint16_t) == u8Size)
		{
			if (sToken.uData.u64IntValue > 0xffff)
			{
				printf("Error: Fill value exceeds 16 bits\n");
				eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
				goto errorExit;
			}

			u16Pattern = (uint16_t) sToken.uData.u64IntValue;
		}
		else
		{
			// 32 Bit
			u32Pattern = (uint32_t) sToken.uData.u64IntValue;
		}

	}
	else
	{
		printf("Error: Missing pattern to write\n");
		eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
		goto errorExit;
	}

	// If end address is ahead of the start address...
	if (u32AddressTo < u32AddressFrom)
	{
		printf("Error: Start address of 0x%.6x is larger than end address of 0x%.6x\n", u32AddressFrom, u32AddressTo);
		eStatus = ESTATUS_MONITOR_START_LARGER_THAN_END_ADDRESS;
		goto errorExit;
	}

	if ((sizeof(uint16_t) == u8Size) &&
		(u32AddressFrom & 1))
	{
		printf("Error: Start address of 0x%.6x is not aligned on a 16 bit boundary\n", u32AddressFrom);
		eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
		goto errorExit;
	}
		
	if ((sizeof(uint32_t) == u8Size) &&
		(u32AddressFrom & 3))
	{
		printf("Error: Start address of 0x%.6x is not aligned on a 32 bit boundary\n", u32AddressFrom);
		eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
		goto errorExit;
	}

	eStatus = MonitorCheckAddress("Error: From address of ", u32AddressFrom);
	ERR_GOTO();
	eStatus = MonitorCheckAddress("Error: To address of ", u32AddressTo);
	ERR_GOTO();

	if (sizeof(uint8_t) == u8Size)
	{
		printf("Filling 0x%.6x-0x%.6x with 0x%.2x - ", u32AddressFrom, u32AddressTo, u8Pattern);
	}
	else
	if (sizeof(uint16_t) == u8Size)
	{
		printf("Filling 0x%.6x-0x%.6x with 0x%.4x - ", u32AddressFrom, u32AddressTo, u16Pattern);
	}
	else
	if (sizeof(uint32_t) == u8Size)
	{
		printf("Filling 0x%.6x-0x%.6x with 0x%.8x - ", u32AddressFrom, u32AddressTo, u32Pattern);
	}
	else
	{
		assert(0);
	}

	fflush(stdout);

	if (sizeof(uint8_t) == u8Size)
	{
		memset((void *) u32AddressFrom, u8Pattern, (u32AddressTo - u32AddressFrom) + 1);
	}
	else
	if (sizeof(uint16_t) == u8Size)
	{
		while (u32AddressFrom < u32AddressTo)
		{
			*((uint16_t *) u32AddressFrom) = u16Pattern;
			u32AddressFrom += sizeof(uint16_t);
		}
	}
	else
	if (sizeof(uint32_t) == u8Size)
	{
		while (u32AddressFrom < u32AddressTo)
		{
			*((uint32_t *) u32AddressFrom) = u32Pattern;
			u32AddressFrom += sizeof(uint32_t);
		}
	}
	else
	{
		assert(0);
	}

	printf("Done\n");

	eStatus = ESTATUS_OK;
	*pu32AddressPointer = u32AddressTo + 1;

errorExit:
	return(eStatus);
}


// Set the # of lines of memory to dump when not qualified by count or range
static EStatus MonitorLines(SLex *psLex,
							const SMonitorCommands *psMonitorCommand,
							uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	SToken sToken;
	ETokenType eToken;

	ZERO_STRUCT(sToken);

	eToken = LexGetNextToken(psLex,
						  &sToken);
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if ((0 == sToken.uData.u64IntValue) ||
			((sToken.uData.u64IntValue) > 65535))
		{
			printf("Error: Valid line count range is 1-65535\n");
			eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
			goto errorExit;
		}

		// It's good
	}
	else
	{
		printf("Error: Expected a number\n");
		eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
		goto errorExit;
	}

	sg_u16DumpLines = ((uint16_t) sToken.uData.u64IntValue);
	printf("Memory dump/disassembly line count set to %u\n", sg_u16DumpLines);
	eStatus = ESTATUS_OK;

errorExit:
	LexClearToken(&sToken);
	return(eStatus);
}

// Read transactions from memory
//
// rb addrfrom-addrto addr addr addr ...
// rw addrfrom-addrto addr addr addr ...
// rd addrfrom-addrto addr addr addr ...
static EStatus MonitorRead(SLex *psLex,
						   const SMonitorCommands *psMonitorCommand,
						   uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	uint32_t u32AddressFrom;
	uint32_t u32AddressTo;
	SToken sToken;
	ETokenType eToken;
	uint8_t u8TransactionSize;
	bool bFetchToken = true;

	ZERO_STRUCT(sToken);

	// Figure out the basic transaction size
	u8TransactionSize = toupper(psMonitorCommand->peCommandString[1]);
	if ('B' == u8TransactionSize)
	{
		// byte/uint8_t
		u8TransactionSize = sizeof(uint8_t);
	}
	else
	if ('W' == u8TransactionSize)
	{
		// word/uint16_t
		u8TransactionSize = sizeof(uint16_t);
	}
	else
	if ('D' == u8TransactionSize)
	{
		// dword/uint32_t
		u8TransactionSize = sizeof(uint32_t);
	}
	else
	{
		// Internal error.
		assert(0);
	}

	for (;;)
	{
		if (bFetchToken)
		{
			LexClearToken(&sToken);

			eToken = LexGetNextToken(psLex,
									 &sToken);
		}

		if ((ELEX_INT_SIGNED == eToken) ||
			(ELEX_INT_UNSIGNED == eToken) ||
			((ETokenType) '.' == eToken))
		{
			if ((ETokenType) '.' == eToken)
			{
				u32AddressFrom = *pu32AddressPointer;
			}
			else
			{
				u32AddressFrom = (uint32_t) sToken.uData.u64IntValue;
			}

			// Make from/to the same for now in case it's a single address
			u32AddressTo = u32AddressFrom;
		}
		else
		if (ELEX_EOF == eToken)
		{
			break;
		}
		else
		{
			printf("Error: Expecting starting address\n");
			eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
			goto errorExit;
		}

		LexClearToken(&sToken);

		// Get the next token. If it's a '-', then we're doing and expect a range
		eToken = LexGetNextToken(psLex,
								 &sToken);
		if (((ETokenType) '-') == eToken)
		{
			// It's a range!
			bFetchToken = false;

			LexClearToken(&sToken);

			eToken = LexGetNextToken(psLex,
									 &sToken);

			if ((ELEX_INT_SIGNED == eToken) ||
				(ELEX_INT_UNSIGNED == eToken) ||
				((ETokenType) '.' == eToken))
			{
				if ((ETokenType) '.' == eToken)
				{
					u32AddressTo = *pu32AddressPointer;
				}
				else
				{
					u32AddressTo = (uint32_t) sToken.uData.u64IntValue;
				}
			}
			else
			{
				printf("Error: Expecting ending  address\n");
				eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
				goto errorExit;
			}

			bFetchToken = true;
		}
		else
		{
			// It's not a range, 
			bFetchToken = false;
		}

		eStatus = MonitorCheckAddress("Error: From address of ", u32AddressFrom);
		ERR_GOTO();
		eStatus = MonitorCheckAddress("Error: To address of ", u32AddressTo);
		ERR_GOTO();

		while (u32AddressFrom <= u32AddressTo)
		{
			if (sizeof(uint8_t) == u8TransactionSize)
			{
				// Byte
				printf("0x%.6x=0x%.2x\n", u32AddressFrom, *((uint8_t *) u32AddressFrom));
			}
			else
			if (sizeof(uint16_t) == u8TransactionSize)
			{
				// Word
				printf("0x%.6x=0x%.4x\n", u32AddressFrom, *((uint16_t *) u32AddressFrom));
			}
			else
			if (sizeof(uint32_t) == u8TransactionSize)
			{
				// Dword
				printf("0x%.6x=0x%.8x\n", u32AddressFrom, *((uint32_t *) u32AddressFrom));
			}
			else
			{
				// Internal fault
				assert(0);
			}

			u32AddressFrom += u8TransactionSize;

			// Look for input of any kind, and if we get it, stop the 
			if (MonitorInputAvailable())
			{
				// Set the pointer wherever we've stopped
				*pu32AddressPointer = u32AddressFrom;
				eStatus = ESTATUS_OK;
				goto errorExit;
			}
		}

		*pu32AddressPointer = u32AddressFrom;
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// This determines the final size of the transaction based on the supplied transaction size
// (assuming there is one) then compared to the incoming transaction size

EStatus MonitorDetermineSize(SToken *psToken,
							 uint8_t u8TransactionSizeIncoming,
							 uint8_t *pu8TransactionSizeAdjusted)
{
	EStatus eStatus;
	uint8_t u8TransactionSizeAdjusted = 0xff;

	if (0 == u8TransactionSizeIncoming)
	{
		// Determine the size of the transaction from the token
		if (ELEX_STRING == psToken->eTokenType)
		{
			// If it's a string, we're doing bytes
			u8TransactionSizeAdjusted = sizeof(uint8_t);
		}
		else
		if (psToken->u8Digits <= 2)
		{
			// Single byte
			u8TransactionSizeAdjusted = sizeof(uint8_t);
		}
		else
		if (psToken->u8Digits <= 4)
		{
			// Word
			u8TransactionSizeAdjusted = sizeof(uint16_t);
		}
		else
		{
			// Assume dword
			u8TransactionSizeAdjusted = sizeof(uint32_t);
		}
	}
	else
	{
		// Use the incoming transaction size
		u8TransactionSizeAdjusted = u8TransactionSizeIncoming;
	}

	// Assuming we've got a numeric value, see if it's in range
	if (psToken->eTokenType != ELEX_STRING)
	{
		if (sizeof(uint8_t) == u8TransactionSizeAdjusted)
		{
			if (psToken->uData.u64IntValue > 0xff)
			{
				printf("Error: Value of 0%.8x out of range for byte transaction sizes\n", (uint32_t) psToken->uData.u64IntValue);
				eStatus = ESTATUS_MONITOR_DATA_SIZE_OUT_OF_RANGE;
				goto errorExit;
			}
		}
		else
		if (sizeof(uint16_t) == u8TransactionSizeAdjusted)
		{
			if (psToken->uData.u64IntValue > 0xffff)
			{
				printf("Error: Value of 0%.8x out of range for word/uint16_t transaction sizes\n", (uint32_t) psToken->uData.u64IntValue);
				eStatus = ESTATUS_MONITOR_DATA_SIZE_OUT_OF_RANGE;
				goto errorExit;
			}
		}

	}

	assert(u8TransactionSizeAdjusted != 0xff);
	*pu8TransactionSizeAdjusted = u8TransactionSizeAdjusted;
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Write transactions to memory
//
// w addr=x x x x x,addr=x x x x "xxxx" x x x
// wb addr=x x x x x,addr=x x x x "xxxx" x x x
// ww addr=x x x x x,addr=x x x x "xxxx" x x x
// wd addr=x x x x x,addr=x x x x "xxxx" x x x
static EStatus MonitorWrite(SLex *psLex,
						    const SMonitorCommands *psMonitorCommand,
						    uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	uint32_t u32Address;
	SToken sToken;
	ETokenType eToken;
	uint8_t u8TransactionSize;
	bool bFetchToken = true;

	// Figure out the basic transaction size
	u8TransactionSize = toupper(psMonitorCommand->peCommandString[1]);
	if ('B' == u8TransactionSize)
	{
		// byte/uint8_t
		u8TransactionSize = sizeof(uint8_t);
	}
	else
	if ('W' == u8TransactionSize)
	{
		// word/uint16_t
		u8TransactionSize = sizeof(uint16_t);
	}
	else
	if ('D' == u8TransactionSize)
	{
		// dword/uint32_t
		u8TransactionSize = sizeof(uint32_t);
	}
	else
	if (0 == u8TransactionSize)
	{
		// This means size is derived from the arguments
	}
	else
	{
		// Internal error.
	}

	ZERO_STRUCT(sToken);

	for (;;)
	{
		uint8_t u8TransactionCount;

		u8TransactionCount = 0;

		// Get an address
		eToken = LexGetNextToken(psLex,
								 &sToken);

		if ((ELEX_INT_SIGNED == eToken) ||
			(ELEX_INT_UNSIGNED == eToken) ||
			((ETokenType) '.' == eToken))
		{
			if ((ETokenType) '.' == eToken)
			{
				u32Address = *pu32AddressPointer;
			}
			else
			{
				u32Address = (uint32_t) sToken.uData.u64IntValue;
			}
		}
		else
		{
			printf("Error: Expecting starting address\n");
			eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
			goto errorExit;
		}

		LexClearToken(&sToken);
		*pu32AddressPointer = u32Address;

		// Now look for '='
		eToken = LexGetNextToken(psLex,
								 &sToken);
		if (eToken != ((ETokenType) '='))
		{
			printf("Error: Expecting '=' after address\n");
			eStatus = ESTATUS_MONITOR_EQUALS_EXPECTED;
			goto errorExit;
		}
		LexClearToken(&sToken);

		// Loop until end of line or a comma
		for (;;)
		{
			eToken = LexGetNextToken(psLex,
									 &sToken);

			if ((ELEX_EOF == eToken) ||
				(((ETokenType) ',') == eToken))
			{
				break;
			}

			// Either needs to be a number or a string
			if ((ELEX_INT_SIGNED == eToken) ||
				(ELEX_INT_UNSIGNED == eToken) ||
				(ELEX_STRING == eToken))
			{
				uint8_t u8TransactionSizeAdjusted = 0;

				// Figure out what the transaction for this data item should be
				eStatus = MonitorDetermineSize(&sToken,
											   u8TransactionSize,
											   &u8TransactionSizeAdjusted);
				ERR_GOTO();

				if (ELEX_STRING == eToken)
				{
					char *peString = sToken.peString;

					while ((peString) && (*peString))
					{
						// Single transaction.
						if (sizeof(uint8_t) == u8TransactionSizeAdjusted)
						{
							*((uint8_t *) u32Address) = *peString;
						}
						else
						if (sizeof(uint16_t) == u8TransactionSizeAdjusted)
						{
							*((uint16_t *) u32Address) = *peString;
						}
						else
						if (sizeof(uint32_t) == u8TransactionSizeAdjusted)
						{
							*((uint32_t *) u32Address) = *peString;
						}
						else
						{
							assert(0);
						}

						u32Address += u8TransactionSizeAdjusted;
						++peString;
					}
				}
				else
				{
					// Single transaction.
					if (sizeof(uint8_t) == u8TransactionSizeAdjusted)
					{
						*((uint8_t *) u32Address) = (uint8_t) sToken.uData.u64IntValue;
					}
					else
					if (sizeof(uint16_t) == u8TransactionSizeAdjusted)
					{
						*((uint16_t *) u32Address) = (uint16_t) sToken.uData.u64IntValue;
					}
					else
					if (sizeof(uint32_t) == u8TransactionSizeAdjusted)
					{
						*((uint32_t *) u32Address) = (uint32_t) sToken.uData.u64IntValue;
					}
					else
					{
						assert(0);
					}

					u32Address += u8TransactionSizeAdjusted;
				}

				*pu32AddressPointer = u32Address;
				LexClearToken(&sToken);
			}
			else
			if (ELEX_IDENTIFIER == eToken)
			{
				printf("Error: Expected numeric value or string, not '%s'\n", sToken.peString);
				eStatus = ESTATUS_MONITOR_DATA_EXPECTED;
				goto errorExit;
			}
			else
			{
				printf("Error: Expected numeric value or string, not '%c'\n", eToken);
				eStatus = ESTATUS_MONITOR_DATA_EXPECTED;
				goto errorExit;
			}

			++u8TransactionCount;
		}

		LexClearToken(&sToken);

		if (0 == u8TransactionCount)
		{
			printf("Error: Missing data for target address of 0x%.6\n", u32Address);
			eStatus = ESTATUS_MONITOR_DATA_EXPECTED;
			goto errorExit;
		}

		if (ELEX_EOF == eToken)
		{
			break;
		}
		else
		{
			// Otherwise, it's a , so we continue
		}
	}

	eStatus = ESTATUS_OK;

errorExit:
	LexClearToken(&sToken);
	return(eStatus);
}


// "call" To an address
static EStatus MonitorGo(SLex *psLex,
						 const SMonitorCommands *psMonitorCommand,
						 uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	uint32_t u32AddressGo;
	SToken sToken;
	ETokenType eToken;

	ZERO_STRUCT(sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);

	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken) ||
		((ETokenType) '.' == eToken))
	{
		if ((ETokenType) '.' == eToken)
		{
			u32AddressGo = *pu32AddressPointer;
		}
		else
		{
			u32AddressGo = (uint32_t) sToken.uData.u64IntValue;
		}
	}
	else
	{
		printf("Error: Expected target address\n");
		eStatus = ESTATUS_MONITOR_ADDRESS_EXPECTED;
		goto errorExit;
	}

	eStatus = MonitorCheckAddress("Error: Execution address ", u32AddressGo);
	ERR_GOTO();

	printf("Calling to address 0x%.6x:\n", u32AddressGo);
	fflush(stdout);
	((void (*)(void))u32AddressGo)();
	printf("\nReturning from call to 0x%.6x\n", u32AddressGo);
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Disassembles code.
//
// dis
// dis addr
// dis addrfrom-addrto
static EStatus MonitorDisassemble(SLex *psLex,
								  const SMonitorCommands *psMonitorCommand,
								  uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	uint32_t u32AddressFrom;
	uint32_t u32AddressTo;
	SToken sToken;
	ETokenType eToken;
	bool bRange = false;
	uint8_t u8Pattern;
	uint16_t u16LineCount = sg_u16DumpLines;

	ZERO_STRUCT(sToken);
	u32AddressFrom = *pu32AddressPointer;

	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Disassemble at the address pointer for u16LineCount lines
	}
	else
	{
		if ((ELEX_INT_SIGNED == eToken) ||
			(ELEX_INT_UNSIGNED == eToken) ||
			('.' == eToken))
		{
			if ('.' == eToken)
			{
				// From address is already set to the pointer
			}
			else
			{
				// We have (at least) a start address
				u32AddressFrom = (uint32_t) sToken.uData.u64IntValue;
			}

			LexClearToken(&sToken);
		}
		else
		{
			printf("Error: Expecting start address\n");
			eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
			goto errorExit;
		}

		eToken = LexGetNextToken(psLex,
								 &sToken);
		if ((ETokenType) '-' == eToken)
		{
			// It's a range
			bRange = true;
			LexClearToken(&sToken);

			// Next should be a count
			eToken = LexGetNextToken(psLex,
									 &sToken);
			if ((ELEX_INT_SIGNED == eToken) ||
				(ELEX_INT_UNSIGNED == eToken))
			{
				u32AddressTo = (uint32_t) sToken.uData.u64IntValue;
			}
			else
			{
				printf("Error: Expecting addfrom-addrto\n");
				eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
				goto errorExit;
			}

			u16LineCount = 0;
		}
		else
		if ((ELEX_INT_SIGNED == eToken) ||
			(ELEX_INT_UNSIGNED == eToken))
		{
			// It's a line count
		}
		else
		{
			printf("Error: Expecting a range - addrfrom-addrto \n");
			eStatus = ESTATUS_PARSE_UINT32_EXPECTED;
			goto errorExit;
		}
	}

	LexClearToken(&sToken);

	eStatus = MonitorCheckAddress("Error: Disassembly from address ", u32AddressFrom);
	ERR_GOTO();
	eStatus = MonitorCheckAddress("Error: Disassembly to address ", u32AddressTo);
	ERR_GOTO();

	if (u16LineCount)
	{
		while (u16LineCount)
		{
			u32AddressFrom = disasm(u32AddressFrom);
			*pu32AddressPointer = u32AddressFrom;
			u16LineCount--;

			// Look for input of any kind, and if we get it, stop the 
			if (MonitorInputAvailable())
			{
				u16LineCount = 0;
			}
		}
	}
	else
	{
		// Dissassemble from->to
		while (u32AddressFrom < u32AddressTo)
		{
			u32AddressFrom = disasm(u32AddressFrom);
			*pu32AddressPointer = u32AddressFrom;
			// Look for input of any kind, and if we get it, stop the 
			if (MonitorInputAvailable())
			{
				u32AddressFrom = u32AddressTo;
			}
		}
	}

	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Show the system's memory map
static EStatus MonitorMemoryMap(SLex *psLex,
								const SMonitorCommands *psMonitorCommand,
								uint32_t *pu32AddressPointer)
{
	printf("System memory map:\n");
	printf("  0x%.8x-0x%.8x - Boot loader flash\n", ROSCOE_FLASH_BOOT_BASE, ROSCOE_FLASH_BOOT_BASE + ROSCOE_FLASH_BOOT_SIZE - 1);
	printf("  0x%.8x-0x%.8x - BIOS loader flash\n", ROSCOE_FLASH_BOOT_BASE, ROSCOE_FLASH_BIOS_BASE + ROSCOE_FLASH_BIOS_SIZE - 1);
	printf("  0x%.8x-0x%.8x - Fast RAM\n", ROSCOE_FAST_SRAM_BASE, (ROSCOE_FAST_SRAM_BASE + ROSCOE_FAST_SRAM_SIZE - 1));
	printf("  0x%.8x-0x%.8x - System SRAM\n\n", ROSCOE_SYSTEM_SRAM, (ROSCOE_SYSTEM_SRAM + ROSCOE_SYSTEM_SRAM_SIZE - 1));

	printf("  0x%.8x-0x%.8x - UART A (console)\n", ROSCOE_UART_A, (ROSCOE_UART_B - 1));
	printf("  0x%.8x-0x%.8x - UART B\n", ROSCOE_UART_A, (ROSCOE_INTERRUPT_CONTROLLER_BASE - 1));
	printf("  0x%.8x-0x%.8x - Interrupt mask 1\n", ROSCOE_INTC_MASK, (ROSCOE_INTC_MASK2 - 1));
	printf("  0x%.8x-0x%.8x - Interrupt mask 2\n", ROSCOE_INTC_MASK2, (ROSCOE_POWER_RESET_CTRL - 1));
	printf("  0x%.8x-0x%.8x - Power/reset control\n", ROSCOE_POWER_RESET_CTRL, ROSCOE_POWER_RESET_CTRL);
	printf("  0x%.8x-0x%.8x - RTC\n", ROSCOE_RTC, (ROSCOE_PTC - 1));
	printf("  0x%.8x-0x%.8x - PTC\n", ROSCOE_PTC, (ROSCOE_VIDEO_CTRL - 1));
	printf("  0x%.8x-0x%.8x - Video Control Interface\n", ROSCOE_VIDEO_CTRL, ROSCOE_POST_SEGMENT_LEFT - 1);
	printf("  0x%.8x-0x%.8x - POST LED Left\n", ROSCOE_POST_SEGMENT_LEFT, (ROSCOE_POST_SEGMENT_LEFT + 0xff));
	printf("  0x%.8x-0x%.8x - POST LED Right\n", ROSCOE_POST_SEGMENT_RIGHT, (ROSCOE_POST_SEGMENT_RIGHT + 0xff));
	printf("  0x%.8x-0x%.8x - Status LED\n\n", ROSCOE_BOARD_STATUS_LED, (ROSCOE_IDE_1_CSA - 1));

	printf("  0x%.8x-0x%.8x - IDE 1 CSA\n", ROSCOE_IDE_1_CSA, (ROSCOE_IDE_1_CSB - 1));
	printf("  0x%.8x-0x%.8x - IDE 1 CSB\n", ROSCOE_IDE_1_CSB, (ROSCOE_IDE_2_CSA - 1));
	printf("  0x%.8x-0x%.8x - IDE 2 CSA\n", ROSCOE_IDE_2_CSA, (ROSCOE_IDE_2_CSB - 1));
	printf("  0x%.8x-0x%.8x - IDE 2 CSB\n", ROSCOE_IDE_2_CSB, (ROSCOE_USB_CONTROLLER - 1));
	printf("  0x%.8x-0x%.8x - USB Controller\n", ROSCOE_USB_CONTROLLER, (ROSCOE_USB_CONTROLLER + 0x1ff));

	printf("  0x%.8x-0x%.8x - Video memory\n", ROSCOE_VIDEO_MEMORY_BASE, (ROSCOE_VIDEO_FONT_MEMORY - 1));
	printf("  0x%.8x-0x%.8x - Video font memory\n\n", ROSCOE_VIDEO_FONT_MEMORY, (ROSCOE_DRAM_BASE - 1));
	printf("  0x%.8x-0x%.8x - Network\n", ROSCOE_USB_CONTROLLER, (ROSCOE_NIC - 1));

	printf("  0x%.8x-0x%.8x - DRAM\n", ROSCOE_DRAM_BASE, 0xffffffff);

	return(ESTATUS_OK);
}

static EStatus MonitorReset(SLex *psLex,
						    const SMonitorCommands *psMonitorCommand,
						    uint32_t *pu32AddressPointer)
{
	printf("Hard reset...\n");

	// Give some time for the output to complete
	SharedSleep(500);

	// This causes the system to perform a hardware based reset
	*((volatile uint8_t *) ROSCOE_POWER_RESET_CTRL) = ROSCOE_POWER_RESET_RESET;

	printf("If you see this, the reset hardware is broken.\n");
	return(ESTATUS_OK);
}

#define	MAX_MEMORY_ERRORS	20
#define	PROCESS_ERROR()		u8ErrorCount++; if (u8ErrorCount >= MAX_MEMORY_ERRORS) { printf("Max memory errors of %u reached - stopping\n"); goto errorExit; }

static EStatus MonitorMemoryTest(SLex *psLex,
								 const SMonitorCommands *psMonitorCommand,
								 uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	ETokenType eToken;
	SToken sToken;
	uint32_t u32Address;
	uint32_t u32FailedAddress;
	uint32_t u32Size;
	uint8_t u8Expected;
	uint8_t u8Got;
	uint16_t u16Expected;
	uint16_t u16Got;
	uint32_t u32Expected;
	uint32_t u32Got;
	uint32_t u32Passes = 1;
	uint32_t u32PassCount = 1;
	uint8_t u8ErrorCount = 0;

	*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0xffff;

	ZERO_STRUCT(sToken);
	u32Address = *pu32AddressPointer;

	// First is base address
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		u32Address = (uint32_t) sToken.uData.u64IntValue;
	}
	else
	if ((ETokenType) '.' == eToken)
	{
		// Current address - u32Address already has it
	}
	else
	{
		printf("Error: Expecting an address\n");
		goto errorExit;
	}

	LexClearToken(&sToken);

	// Now get a count
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		u32Size = (uint32_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Error: Expecting size of data to test\n");
		goto errorExit;
	}

	LexClearToken(&sToken);

	// See if they provided a pass count
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Leave the default count if 1
	}
	else
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		u32Passes = (uint32_t) sToken.uData.u64IntValue;
	}
	else
	{
			printf("Error: Expecting pass count - 0==endless\n");
			goto errorExit;
	}

	for (;;)
	{
		printf("Pass #%u\n", u32PassCount);

		// Data line tests
		printf("Data line test - uint8 : ");
		fflush(stdout);

		if (MemTestDataBusUINT8((volatile uint8_t *) u32Address,
								&u8Expected,
								&u8Got))
		{
			printf("OK\n");
		}
		else
		{
			printf("Failed - Address 0x%.6x, expected 0x%.2x, got 0x%.2x\n", u32Address, u8Expected, u8Got);
			PROCESS_ERROR();
		}

		// Look for input of any kind, and if we get it, stop the test
		if (MonitorInputAvailable())
		{
			goto errorExit;
		}

		printf("Data line test - uint16: ");
		fflush(stdout);

		if (MemTestDataBusUINT16((volatile uint16_t *) u32Address,
								 &u16Expected,
								 &u16Got))
		{
			printf("OK\n");
		}
		else
		{
			printf("Failed - Address 0x%.6x, expected 0x%.4x, got 0x%.4x\n", u32Address, u16Expected, u16Got);
			PROCESS_ERROR();
		}

		// Look for input of any kind, and if we get it, stop the test
		if (MonitorInputAvailable())
		{
			goto errorExit;
		}

		printf("Data line test - uint32: ");
		fflush(stdout);

		if (MemTestDataBusUINT32((volatile uint32_t *) u32Address,
								 &u32Expected,
								 &u32Got))
		{
			printf("OK\n");
		}
		else
		{
			printf("Failed - Address 0x%.6x, expected 0x%.8x, got 0x%.8x\n", u32Address, u32Expected, u32Got);
			PROCESS_ERROR();
		}

		// Look for input of any kind, and if we get it, stop the test
		if (MonitorInputAvailable())
		{
			goto errorExit;
		}

		// Device tests
		printf("Device test    - uint8 : ");
		fflush(stdout);

		if (MemTestDeviceUINT8((volatile uint8_t *) u32Address,
							   u32Size,
							   &u32FailedAddress,
							   &u8Expected,
							   &u8Got))
		{
			printf("OK\n");
		}
		else
		{
			printf("Failed - Address 0x%.6x, expected 0x%.2x, got 0x%.2x\n", u32FailedAddress, u8Expected, u8Got);
			PROCESS_ERROR();
		}

		// Look for input of any kind, and if we get it, stop the test
		if (MonitorInputAvailable())
		{
			goto errorExit;
		}

		printf("Device test    - uint16: ");
		fflush(stdout);

		if (MemTestDeviceUINT16((volatile uint16_t *) u32Address,
								u32Size,
								&u32FailedAddress,
								&u16Expected,
								&u16Got))
		{
			printf("OK\n");
		}
		else
		{
			printf("Failed - Address 0x%.6x, expected 0x%.4x, got 0x%.4x\n", u32FailedAddress, u8Expected, u8Got);
			PROCESS_ERROR();
		}

		// Look for input of any kind, and if we get it, stop the test
		if (MonitorInputAvailable())
		{
			goto errorExit;
		}

		printf("Device test    - uint32: ");
		fflush(stdout);

		if (MemTestDeviceUINT32((volatile uint32_t *) u32Address,
								u32Size,
								&u32FailedAddress,
								&u32Expected,
								&u32Got))
		{
			printf("OK\n");
		}
		else
		{
			printf("Failed - Address 0x%.6x, expected 0x%.8x, got 0x%.8x\n", u32FailedAddress, u8Expected, u8Got);
			PROCESS_ERROR();
		}

		// Look for input of any kind, and if we get it, stop the test
		if (MonitorInputAvailable())
		{
			goto errorExit;
		}

		if (0 == u32Passes)
		{
			// Go forever
		}
		else
		{
			u32Passes--;
			if (0 == u32Passes)
			{
				goto errorExit;
			}
		}

		u32PassCount++;
	}

errorExit:
	return(ESTATUS_OK);
}

// Burstr/burstw handler 

static EStatus MonitorBurstReadWrite(SLex *psLex,
									 const SMonitorCommands *psMonitorCommand,
									 uint32_t *pu32AddressPointer,
									 bool bWrite)
{
	EStatus eStatus;
	ETokenType eToken;
	SToken sToken;
	uint32_t u32Address;
	uint32_t u32EndAddress;

	ZERO_STRUCT(sToken);
	u32Address = *pu32AddressPointer;

	// First is base address
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		u32Address = (uint32_t) sToken.uData.u64IntValue;
	}
	else
	if ((ETokenType) '.' == eToken)
	{
		// Current address - u32Address already has it
	}
	else
	{
		printf("Error: Expecting an address\n");
		goto errorExit;
	}

	LexClearToken(&sToken);

	// Expect a '-'
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ETokenType) '-' == eToken)
	{
		// Got it!
	}
	else
	{
		printf("Error: Expecting '-' for address range\n");
		goto errorExit;
	}

	LexClearToken(&sToken);

	// Now get the destination address
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		u32EndAddress = (uint32_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Error: Expecting destination address\n");
		goto errorExit;
	}

	if (u32Address > u32EndAddress)
	{
		printf("Error: End address is lower than the start address\n");
		goto errorExit;
	}

	if (bWrite)
	{
		while (u32Address < u32EndAddress)
		{
			// 32 Bytes per burst
			u32Address += (sizeof(uint32_t) * 8);
			MoveMultipleWrite((void *) u32Address);
		}
	}
	else
	{
		while (u32Address < u32EndAddress)
		{
			// 32 Bytes per burst
			MoveMultipleRead((void *) u32Address);
			u32Address += (sizeof(uint32_t) * 8);
		}
	}

	LexClearToken(&sToken);

errorExit:
	return(ESTATUS_OK);
}

// Burstr fromaddr-toaddr
static EStatus MonitorBurstRead(SLex *psLex,
								const SMonitorCommands *psMonitorCommand,
								uint32_t *pu32AddressPointer)
{
	return(MonitorBurstReadWrite(psLex,
								 psMonitorCommand,
								 pu32AddressPointer,
								 false));
}


// Burstw fromaddr-toaddr
static EStatus MonitorBurstWrite(SLex *psLex,
								 const SMonitorCommands *psMonitorCommand,
								 uint32_t *pu32AddressPointer)
{
	return(MonitorBurstReadWrite(psLex,
								 psMonitorCommand,
								 pu32AddressPointer,
								 true));
}

// Flash update
static EStatus MonitorFlashUpdate(SLex *psLex,
								  const SMonitorCommands *psMonitorCommand,
								  uint32_t *pu32AddressPointer)
{
	EVersionCode eVersionCode;
	SImageVersion *psImageVersion;
	EStatus eStatus;

	assert(0);

	// Mask all interrupts, otherwise bad things happen
	*((volatile uint8_t *)ROSCOE_INTC_MASK) = 0xff;
	*((volatile uint8_t *)ROSCOE_INTC_MASK2) = 0xff;

errorExit:
	return(ESTATUS_OK);
}

// Read/write sector(s)
//
// Parses one of the following:
// sector
// sector address
// sector(count)
// sector(count) address

static EStatus MonitorIDEReadWrite(SLex *psLex,
								   const SMonitorCommands *psMonitorCommand,
								   uint32_t *pu32AddressPointer,
								   bool bWrite)
{
	EStatus eStatus;
	SToken sToken;
	ETokenType eToken;
	uint32_t u32Sector;
	uint32_t u32SectorCount = 1;
	uint32_t u32Address = 0;
	uint8_t *pu8TempBufferBase = NULL;
	uint8_t *pu8TempBuffer = NULL;
	bool bAddressProvided = false;

	// Get the sector #
	ZERO_STRUCT(sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (eToken != ELEX_INT_UNSIGNED)
	{
		printf("Expected sector #\n");
		eStatus = ESTATUS_PARSE_ERROR;
		goto errorExit;
	}

	u32Sector = (uint32_t) sToken.uData.u64IntValue;

	// Now look for another number or (
	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Just supplied the sector
	}
	else
	if ((ETokenType) '(' == eToken)
	{
		// They are providing a count
		LexClearToken(&sToken);
		eToken = LexGetNextToken(psLex,
								 &sToken);
		if (ELEX_INT_UNSIGNED == eToken)
		{
			u32SectorCount = (uint32_t) sToken.uData.u64IntValue;
			if (0 == u32SectorCount)
			{
				printf("Sector count can't be 0\n");
				eStatus = ESTATUS_PARSE_ERROR;
				goto errorExit;
			}
		}
		else
		{
			printf("Sector count expected\n");
			eStatus = ESTATUS_PARSE_ERROR;
			goto errorExit;
		}

		LexClearToken(&sToken);
		eToken = LexGetNextToken(psLex,
								 &sToken);
		if (eToken != (ETokenType) ')')
		{
			printf("')' Expected\n");
			eStatus = ESTATUS_PARSE_ERROR;
			goto errorExit;
		}

		LexClearToken(&sToken);
		eToken = LexGetNextToken(psLex,
								 &sToken);
		if (ELEX_EOF == eToken)
		{
			// All good - no address provided
		}
		else
		if (ELEX_INT_UNSIGNED == eToken)
		{
			// This is the address
			u32Address = (uint32_t) sToken.uData.u64IntValue;
			bAddressProvided = true;
		}
		else
		{
			printf("Address expected\n");
			eStatus = ESTATUS_PARSE_ERROR;
			goto errorExit;
		}
	}
	else
	if (ELEX_INT_UNSIGNED == eToken)
	{
		// This is the address
		u32Address = (uint32_t) sToken.uData.u64IntValue;
		bAddressProvided = true;
	}
	else
	{
		printf("Expected address or (sector count)\n");
		eStatus = ESTATUS_PARSE_ERROR;
		goto errorExit;
	}

	if (bAddressProvided)
	{
		if (u32Address > 0xffffff)
		{
			printf("Address beyond range of CPU's address space\n");
			eStatus = ESTATUS_MONITOR_ADDRESS_TOO_HIGH;
			goto errorExit;
		}
	}

	// u32Sector = Sector # base to read/write
	// u32SectorCount = # Of sectors to read/write
	// u32Address = Data buffer

	if (bWrite)
	{
		if (false == bAddressProvided)
		{
			printf("Missing source address to write from\n");
			eStatus = ESTATUS_PARSE_ERROR;
			goto errorExit;
		}

		printf("Writing sectors %u-%u from memory @ 0x%.6x-0x%.6x: ", u32Sector, u32Sector + u32SectorCount - 1, u32Address, u32Address + (u32SectorCount << 9) - 1);

		eStatus = IDEWriteSector(0,
								 u32Sector,
								 u32SectorCount,
								 (uint8_t *) u32Address);

		printf("%s\n", GetErrorText(eStatus));
		goto errorExit;
	}
	else
	{
		// We're reading into a memory location
		if (bAddressProvided)
		{
			// Reading in to a memory location
			printf("Reading sectors %u-%u into memory @ 0x%.6x-0x%.6x: ", u32Sector, u32Sector + u32SectorCount - 1, u32Address, u32Address + (u32SectorCount << 9) - 1);

			eStatus = IDEReadSector(0,
									u32Sector,
									u32SectorCount,
									(uint8_t *) u32Address);

			printf("%s\n", GetErrorText(eStatus));
			goto errorExit;
		}
		
		// We're reading the sectors to the console
		pu8TempBufferBase = malloc(512+16);
		if (NULL == pu8TempBufferBase)
		{
			printf("Could not allocate 512+16 bytes for sector buffer\n");
			eStatus = ESTATUS_OUT_OF_MEMORY;
			goto errorExit;
		}

		pu8TempBuffer = (uint8_t *) ((((uint32_t) pu8TempBufferBase) + 15) & ~15);

		while (u32SectorCount)
		{
			printf("Sector %u:\n", u32Sector);
			eStatus = IDEReadSector(0,
									u32Sector,
									1,
									pu8TempBuffer);
			if (eStatus != ESTATUS_OK)
			{
				printf("Error: %s\n", GetErrorText(eStatus));
				goto errorExit;
			}

			// Dump the sector to the console
			DumpHex(0,
					512,
					0,
					pu8TempBuffer,
					NULL,
					false,
					NULL);
			u32Sector++;
			u32SectorCount--;
		}
	}

errorExit:
	if (pu8TempBufferBase)
	{
		free(pu8TempBufferBase);
	}

	return(eStatus);
}

static EStatus MonitorIDEID(SLex *psLex,
							const SMonitorCommands *psMonitorCommand,
							uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	volatile uint8_t *pu8IdentityStructureOriginal;
	volatile uint8_t *pu8IdentityStructure;

	pu8IdentityStructureOriginal = malloc(512+16);
	if (NULL == pu8IdentityStructureOriginal)
	{
		printf("Out of memory while attempting to allocate 512+16 bytes\n");
		goto errorExit;
	}

	pu8IdentityStructure = (volatile uint8_t *) ((((uint32_t ) pu8IdentityStructureOriginal) + 15) & ~15);

	// Go get the disk
	eStatus = IDEIdentify(0,
						  pu8IdentityStructure);
	if (eStatus != ESTATUS_OK)
	{
		printf("Failed to get identity sector - %s\n", GetErrorText(eStatus));
		goto errorExit;
	}

	// Dump it to the console
	(void) DumpHex(0,
				   0x200,
				   0,
				   (uint8_t *) pu8IdentityStructure,
				   NULL,
				   false,
				   NULL);

errorExit:
	free((void *) pu8IdentityStructureOriginal);
	return(ESTATUS_OK);
}

// iderd sector
// iderd sector address
// iderd sector(count)
// iderd sector(count) address
//
static EStatus MonitorIDERd(SLex *psLex,
							const SMonitorCommands *psMonitorCommand,
							uint32_t *pu32AddressPointer)
{
	MonitorIDEReadWrite(psLex,
						psMonitorCommand,
						pu32AddressPointer,
						false);

	return(ESTATUS_OK);
}

// idewr sector
// idewr sector address
// idewr sector(count)
// idewr sector(count) address
//
static EStatus MonitorIDEWr(SLex *psLex,
							const SMonitorCommands *psMonitorCommand,
							uint32_t *pu32AddressPointer)
{
	MonitorIDEReadWrite(psLex,
						psMonitorCommand,
						pu32AddressPointer,
						true);

	return(ESTATUS_OK);
}
static void ShowDateTime(void)
{
	time_t eTime;
	struct tm *psTime;

	eTime = time(0);
	psTime = localtime((time_t *) &eTime);

	printf("Current time/date is %.4u-%.2u-%.2u %.2u:%.2u:%.2u\n", psTime->tm_year + 1900, psTime->tm_mon + 1, psTime->tm_mday, psTime->tm_hour, psTime->tm_min, psTime->tm_sec);
}

// Time command - show or set
static EStatus MonitorTime(SLex *psLex,
						   const SMonitorCommands *psMonitorCommand,
						   uint32_t *pu32AddressPointer)
{
	SToken sToken;
	ETokenType eToken;
	uint8_t u8Hour;
	uint8_t u8Minute;
	uint8_t u8Second;
	time_t eTime;
	struct tm *psTime;

	ZERO_STRUCT(sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		ShowDateTime();
		goto errorExit;
	}
	else
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if (sToken.uData.u64IntValue >= 24)
		{
			printf("Hour must be in 0-23 range\n");
			goto errorExit;
		}

		u8Hour = (uint8_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected hour - 0-23\n");
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ETokenType) ':' == eToken)
	{
		// All good!
	}
	else
	{
		printf(": Expected after hour\n");
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if (sToken.uData.u64IntValue >= 60)
		{
			printf("Minute must be in 0-59 range\n");
			goto errorExit;
		}

		u8Minute = (uint8_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected minute - 0-59\n");
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ETokenType) ':' == eToken)
	{
		// All good!
	}
	else
	{
		printf(": Expected after minute\n");
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if (sToken.uData.u64IntValue >= 60)
		{
			printf("Second must be in 0-59 range\n");
			goto errorExit;
		}

		u8Second = (uint8_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected second - 0-59\n");
		goto errorExit;
	}

	// u8Hour/u8Minute/u8Second have been set! Now let's pull things apart via time(0)
	eTime = time(0);
	psTime = localtime((time_t *) &eTime);

	psTime->tm_hour = u8Hour;
	psTime->tm_min = u8Minute;
	psTime->tm_sec = u8Second;

	RTCSetTime(mktime(psTime));

	ShowDateTime();

errorExit:
	return(ESTATUS_OK);
}

// Date command - show or set
static EStatus MonitorDate(SLex *psLex,
						   const SMonitorCommands *psMonitorCommand,
						   uint32_t *pu32AddressPointer)
{
	SToken sToken;
	ETokenType eToken;
	uint16_t u16Year;
	uint8_t u8Month;
	uint8_t u8Day;
	time_t eTime;
	struct tm *psTime;

	ZERO_STRUCT(sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		ShowDateTime();
		goto errorExit;
	}
	else
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if ((sToken.uData.u64IntValue < YEAR_BASELINE) || (sToken.uData.u64IntValue >= (YEAR_BASELINE + 100)))
		{
			printf("Year must be in %u-%u range\n", YEAR_BASELINE, YEAR_BASELINE + 99);
			goto errorExit;
		}

		u16Year = (uint16_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected year - %u-%u range\n", YEAR_BASELINE, YEAR_BASELINE + 99);
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ETokenType) '-' == eToken)
	{
		// All good!
	}
	else
	{
		printf("- Expected after year\n");
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if ((sToken.uData.u64IntValue < 1) || (sToken.uData.u64IntValue > 12))
		{
			printf("Month must be in 1-12 range\n");
			goto errorExit;
		}

		u8Month = (uint8_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected month - 1-12\n");
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if ((ETokenType) '-' == eToken)
	{
		// All good!
	}
	else
	{
		printf("- Expected after month\n");
		goto errorExit;
	}

	LexClearToken(&sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if ((sToken.uData.u64IntValue < 1) ||
			(sToken.uData.u64IntValue > 31))
		{
			printf("Day must be in 1-31 range\n");
			goto errorExit;
		}

		u8Day = (uint8_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected day - 1-31\n");
		goto errorExit;
	}

	// u8Hour/u8Minute/u8Second have been set! Now let's pull things apart via time(0)
	eTime = time(0);
	psTime = localtime((time_t *) &eTime);

	psTime->tm_year = u16Year - 1900;
	psTime->tm_mon = u8Month - 1;
	psTime->tm_mday = u8Day;

	RTCSetTime(mktime(psTime));

	ShowDateTime();

errorExit:
	return(ESTATUS_OK);
}

// How many seconds do we test for by default?
#define	IDE_TEST_TIME_DEFAULT	10

// # Of sectors per pass
#define	IDE_SECTOR_CHUNK_SIZE	1024

// IDE speed test
// idetest (default 10 seconds)
// idetest seconds
static EStatus MonitorIDETest(SLex *psLex,
							  const SMonitorCommands *psMonitorCommand,
							  uint32_t *pu32AddressPointer)
{
	uint8_t *pu8BufferBase = NULL;
	uint8_t *pu8Buffer = NULL;
	uint32_t u32TestTime = IDE_TEST_TIME_DEFAULT;
	uint32_t u32SectorsRead = 0;
	uint32_t u32KPerSec = 0;
	time_t eStartTime;
	EStatus eStatus;
	SToken sToken;
	ETokenType eToken;

	ZERO_STRUCT(sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Use the default test time
	}
	else
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if (0 == sToken.uData.u64IntValue)
		{
			printf("Second count must be 1 or greater\n");
			goto errorExit;
		}

		u32TestTime = (uint32_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected time (in seconds)\n", YEAR_BASELINE, YEAR_BASELINE + 99);
		goto errorExit;
	}

	LexClearToken(&sToken);

/*	pu8BufferBase = malloc((IDE_SECTOR_CHUNK_SIZE << 9) + 4);
	if (NULL == pu8BufferBase)
	{
		printf("Out of memory while trying to allocate %u bytes\n", (IDE_SECTOR_CHUNK_SIZE << 9) + 4);
		goto errorExit;
	}

	// Aligned buffer for reading
	pu8Buffer = (uint8_t *) ((((uint32_t) pu8BufferBase) + 3) & ~3); */
	pu8Buffer = (uint8_t *) 0x10000;

	// Get second aligned
	eStartTime = time(0);
	while (eStartTime == time(0));
	eStartTime = time(0);

	printf("Testing disk speed for %u second(s)\n", u32TestTime);

	// Consume data repeatedly
	while ((time(0) - eStartTime) < u32TestTime)
	{
		eStatus = IDEReadSector(0,
								0,
								IDE_SECTOR_CHUNK_SIZE,
								pu8Buffer);
		if (eStatus != ESTATUS_OK)
		{
			printf("Error during read: %s\n", GetErrorText(eStatus));
			goto errorExit;
		}

		u32SectorsRead += IDE_SECTOR_CHUNK_SIZE;
	}

	u32KPerSec = (u32SectorsRead >> 1) / u32TestTime;
	printf("Disk read speed: %u total sectors, %uk/sec\n", u32SectorsRead, u32KPerSec);

errorExit:
	if (pu8BufferBase)
	{
//		free(pu8BufferBase);
	}

	return(ESTATUS_OK);
}

// iderw passes
#define	IDERW_BUFFER_SIZE	(1024*1024)
#define	IDERW_BUFFER_BASE	0x100000

static EStatus MonitorIDEReadWriteTest(SLex *psLex,
									   const SMonitorCommands *psMonitorCommand,
									   uint32_t *pu32AddressPointer)
{
	uint32_t u32Pass = 1;
	uint32_t u32PassesRemaining = 10;
	SToken sToken;
	ETokenType eToken;
	char *pu8Buffer1 = NULL;
	char *pu8Buffer2 = NULL;
	uint16_t u16Pattern = 0;
	uint16_t *pu16BufferBase;
	uint16_t *pu16BufferBase2;
	uint32_t *pu32BufferBase;
	uint32_t *pu32BufferBase2;
	uint32_t u32Loop;
	EStatus eStatus;
	bool bErrors = false;
	uint32_t u32Offset = 0;
	uint64_t u64Sector = 0;
	uint64_t u64SectorCount = 0;
	
	ZERO_STRUCT(sToken);
	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Use the default test time
	}
	else
	if (ELEX_INT_UNSIGNED == eToken)
	{
		if (0 == sToken.uData.u64IntValue)
		{
			printf("Pass count must be 1 or greater\n");
			goto errorExit;
		}

		u32PassesRemaining = (uint32_t) sToken.uData.u64IntValue;
	}
	else
	{
		printf("Expected a pass count\n");
		goto errorExit;
	}

	eStatus = IDEGetSize(0,
						 &u64SectorCount);
	if (eStatus != ESTATUS_OK)
	{
		printf("IDEGetSize() call failed - %s\n", GetErrorText(eStatus));
		goto errorExit;
	}

	if (u64SectorCount < (IDERW_BUFFER_SIZE / 512))
	{
		printf("Disk doesn't have enough sectors for test - %u discovered, at least %u needed\n", u64SectorCount, IDERW_BUFFER_SIZE / 512);
		goto errorExit;
	}

	// Make sure we don't go past the end of the disk
	u64SectorCount -= (IDERW_BUFFER_SIZE / 512);

	pu8Buffer1 = (uint8_t *) IDERW_BUFFER_BASE;
	pu8Buffer2 = pu8Buffer1 + IDERW_BUFFER_SIZE;

	printf("IDE read/write test - %u passes, %u byte buffer\n", u32PassesRemaining, IDERW_BUFFER_SIZE);

	while (u32PassesRemaining)
	{
		uint32_t *pu32BufferBase = NULL;

		printf("Read/write test pass #%u\n", u32Pass);

		// Fill the primary buffer with an alternating 16 bit pattern. This will produce
		// the following fill pattern:
		// 0000
		// ffff
		// 0001
		// fffe
		// 0002
		// fffd
		// ...

		printf("Fill buffer\n");
		u32Loop = IDERW_BUFFER_SIZE / sizeof(*pu16BufferBase) / sizeof(*pu16BufferBase);
		pu16BufferBase = (uint16_t *) pu8Buffer1;
		u16Pattern = (uint16_t) u32Pass;
		while (u32Loop)
		{
			*pu16BufferBase = u16Pattern;
			++pu16BufferBase;
			u16Pattern = (uint16_t) ~u16Pattern;
			*pu16BufferBase = u16Pattern;
			++pu16BufferBase;
			u16Pattern = (uint16_t) ~u16Pattern;
			u16Pattern++;
			u32Loop--;
		}

		// Now write it to disk
		printf("Write to disk... sectors %u-%u\n", (uint32_t) u64Sector, (uint32_t) (u64Sector + (IDERW_BUFFER_SIZE / 512) - 1));

		eStatus = IDEWriteSector(0,
								 u64Sector,
								 IDERW_BUFFER_SIZE / 512,
								 pu8Buffer1);
		if (eStatus != ESTATUS_OK)
		{
			printf("Writing data to IDE failed: %s\n", GetErrorText(eStatus));
			goto errorExit;
		}

		// Now read it back from disk
		printf("Read from disk... sectors %u-%u\n", (uint32_t) u64Sector, (uint32_t) (u64Sector + (IDERW_BUFFER_SIZE / 512) - 1));

		// Clear out the read buffer
		u32Loop = IDERW_BUFFER_SIZE / sizeof(*pu16BufferBase) / sizeof(*pu16BufferBase);
		pu32BufferBase = (uint32_t *) pu8Buffer2;
		while (u32Loop)
		{
			*pu32BufferBase = 0xffffffff;
			++pu32BufferBase;
			--u32Loop;
		}

		// Now read all the data back in to memory
		eStatus = IDEReadSector(0,
								u64Sector,
								IDERW_BUFFER_SIZE / 512,
								pu8Buffer2);
		if (eStatus != ESTATUS_OK)
		{
			printf("Reading data from IDE failed: %s\n", GetErrorText(eStatus));
			goto errorExit;
		}

		// Now compare the buffers
		u32Loop = IDERW_BUFFER_SIZE / sizeof(*pu16BufferBase) / 2;
		pu32BufferBase = (uint32_t *) pu8Buffer1;
		pu32BufferBase2 = (uint32_t *) pu8Buffer2;

		printf("Comparing read vs. written...\n");

		// Compare the read vs. written
		u32Offset = 0;
		while (u32Loop)
		{
			if (*pu32BufferBase != *pu32BufferBase2)
			{
				printf("Offset 0x%.6x: Expected 0x%.8x, got 0x%.8x\n", u32Offset, *pu32BufferBase, *pu32BufferBase2);
				bErrors = true;
			}

			++pu32BufferBase;
			++pu32BufferBase2;
			u32Offset += 4;
			u32Loop--;
		}

		if (bErrors)
		{
			printf("Errors found - stopping test\n");
			goto errorExit;
		}

		u64Sector += (IDERW_BUFFER_SIZE / 512);
		if (u64Sector >=  u64SectorCount)
		{
			u64Sector = 0;
		}

		u32PassesRemaining--;
		++u32Pass;
	}

errorExit:
	return(ESTATUS_OK);
}

static EStatus MonitorIDEProbe(SLex *psLex,
							   const SMonitorCommands *psMonitorCommand,
							   uint32_t *pu32AddressPointer)
{
	(void) IDEProbe();

	return(ESTATUS_OK);
}

#define	SR_INT_BIT				8
#define	SR_INT_PRIORITY_MASK	(0x7 << SR_INT_BIT)

static EStatus MonitorIntMask(SLex *psLex,
							  const SMonitorCommands *psMonitorCommand,
							  uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	uint8_t u8LineBufferPrior[16];
	uint8_t u8LineBuffer[16];
	uint32_t u32AddressFrom;
	uint32_t u32AddressTo;
	SToken sToken;
	ETokenType eToken;
	uint32_t u32Loop;
	bool bLineBufferPrimed = false;
	bool bRepeatActive = false;

	ZERO_STRUCT(sToken);
	u32AddressFrom = *pu32AddressPointer;

	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Just dump the interrupt mask
		printf("Interrupt mask: %u\n", (SRGet() & SR_INT_PRIORITY_MASK) >> SR_INT_BIT);
	}
	else
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		if (sToken.uData.u64IntValue > 7)
		{
			printf("Interrupt mask out of range - must be 0-7\n");
		}
		else
		{
			SRSet((SRGet() & ~SR_INT_PRIORITY_MASK) | (((uint16_t) sToken.uData.u64IntValue) << SR_INT_BIT));
			printf("Interrupt mask set to %u\n", (uint8_t) (((SRGet() & SR_INT_PRIORITY_MASK) >> SR_INT_BIT)));
		}

		LexClearToken(&sToken);
	}
	else
	{
		printf("Expecting nothing or an (up to) 8 bit mask\n");
	}

	return(ESTATUS_OK);
}

// Show system counters
static EStatus MonitorCounters(SLex *psLex,
							   const SMonitorCommands *psMonitorCommand,
							   uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	uint16_t u16Count;
	uint8_t u8Channel;
	uint32_t u32Interrupts;

	{
		FILE *psFile;

		psFile = fopen("/BigFile.txt", "wb");
		assert(psFile);

		fprintf(psFile, "This is a test to see if it works\n");

		fclose(psFile);
	}

	printf("Power on seconds    : %u\n", RTCGetPowerOnSeconds());

	for (u8Channel = 0; u8Channel < 2; u8Channel++)
	{
		uint32_t u32InterruptCount;

		eStatus = PTCGetInterruptCounter(u8Channel,
										 &u32InterruptCount);
		printf("Counter %u ints      : %u\n", u8Channel, u32InterruptCount);
	}

	for (u8Channel = 0; u8Channel < 3; u8Channel++)
	{
		eStatus = PTCGetCount(u8Channel,
							  &u16Count);
		ERR_GOTO();
		printf("PTC Channel %u value : 0x%.4x\n", u8Channel, u16Count);
	}

	for (u8Channel = 0; u8Channel < 2; u8Channel++)
	{
		uint32_t u32OverrunCount = 0;
		uint32_t u32ParityErrorCount = 0;
		uint32_t u32FramingErrorCount = 0;

		eStatus = SerialGetCounters(u8Channel,
									&u32Interrupts,
									&u32OverrunCount,
									&u32ParityErrorCount,
									&u32FramingErrorCount);
		ERR_GOTO();
		printf("UART %c IRQ count    : %u\n", u8Channel + 'A', u32Interrupts);
		printf("UART %c Overrun      : %u\n", u8Channel + 'A', u32OverrunCount);
		printf("UART %c Parity err   : %u\n", u8Channel + 'A', u32ParityErrorCount);
		printf("UART %c Framing err  : %u\n", u8Channel + 'A', u32FramingErrorCount);
	}

errorExit:
	return(ESTATUS_OK);
}

// Receives a file
static EStatus MonitorRxFile(SLex *psLex,
							 const SMonitorCommands *psMonitorCommand,
							 uint32_t *pu32AddressPointer)
{
	EStatus eStatus;
	FILE *psFile = NULL;
	char *peBufferPtr = NULL;

	eStatus = LexGetBufferPosition(psLex,
								   &peBufferPtr);
	assert(ESTATUS_OK == eStatus);

	psFile = fopen(peBufferPtr, "wb");
	if (NULL == psFile)
	{
		printf("Couldn't open file '%s' for writing\n");
		goto errorExit;
	}


errorExit:
	if (psFile)
	{
		fclose(psFile);
	}

	return(ESTATUS_OK);
}

// Fill command! For bytes
static EStatus MonitorFillByte(SLex *psLex,
							   const SMonitorCommands *psMonitorCommand,
							   uint32_t *pu32AddressPointer)
{
	return(MonitorFill(psLex,
					   psMonitorCommand,
					   pu32AddressPointer, 
					   sizeof(uint8_t)));
}

// Fill command! For 16 bit quantities
static EStatus MonitorFillWord(SLex *psLex,
							   const SMonitorCommands *psMonitorCommand,
							   uint32_t *pu32AddressPointer)
{
	return(MonitorFill(psLex,
					   psMonitorCommand,
					   pu32AddressPointer, 
					   sizeof(uint16_t)));
}

// Fill command! For 32 bit quantities
static EStatus MonitorFillDword(SLex *psLex,
							    const SMonitorCommands *psMonitorCommand,
							    uint32_t *pu32AddressPointer)
{
	return(MonitorFill(psLex,
					   psMonitorCommand,
					   pu32AddressPointer, 
					   sizeof(uint32_t)));
}

// Help command function prototype
static EStatus MonitorHelp(SLex *psLex,
						   const SMonitorCommands *psMonitorCommand,
						   uint32_t *pu32AddressPointer);

// Monitor commands go here!
static const SMonitorCommands sg_sMonitorCommands[] =
{
	{"d",  		"Byte-wise memory dump",						MonitorDumpMemory},
	{"dis",		"Disassemble code at supplied address",			MonitorDisassemble},
	{"fillb",	"Fills memory with a uint8_t pattern",	   		MonitorFillByte},
	{"fillw",	"Fills memory with a uint16_t pattern",	   		MonitorFillWord},
	{"filld",	"Fills memory with a uint32_t pattern",	   		MonitorFillDword},
	{"flashupdate",	"Updates the system flash via UART",		MonitorFlashUpdate},
	{"go",		"Call to an address",							MonitorGo},
	{"lines",	"Set memory dump/disassembly line count",  		MonitorLines},
	{"map",		"Show a system memory map",						MonitorMemoryMap},
	{"reset",	"Do a hard reset",								MonitorReset},
	{"rb",		"Read memory (byte)",							MonitorRead},
	{"rw",		"Read memory (word/uint16)",  					MonitorRead},
	{"rd",		"Read memory (dword/uint32)", 					MonitorRead},
	{"help",	"List all commands",							MonitorHelp},
	{"time",	"Show time/date",								MonitorTime},
	{"date",	"Show time/date",								MonitorDate},
	{"stack",	"Show information on heap and stack",			MonitorStack},
	{"memtest",	"Perform a data line, address line, and cell test",	MonitorMemoryTest},
	{"burstr",	"Burst read function",							MonitorBurstRead},
	{"burstw",	"Burst write function",							MonitorBurstWrite},
	{"ideid",	"Dump IDE drive's identity structure",			MonitorIDEID},
	{"iderd",  	"Read IDE sectors",								MonitorIDERd},
	{"idewr",  	"Write IDE sectors",							MonitorIDEWr},
	{"idetest",	"Speed test for IDE drive",						MonitorIDETest},
	{"iderw",	"IDE read/write test",							MonitorIDEReadWriteTest},
	{"ideprobe","Rescan the IDE bus",							MonitorIDEProbe},
	{"intmask",	"Read or set the interrupt mask",				MonitorIntMask},
	{"counters","Show various system counters",					MonitorCounters},
	{"w",		"Write memory (size derived from arguments)",	MonitorWrite},
	{"wb",		"Write memory (byte)",							MonitorWrite},
	{"ww",		"Write memory (word/uint16)",					MonitorWrite},
	{"wd",		"Write memory (dword/uint32)",					MonitorWrite},
	{"format",	"Format a disk for usage",						DOSFormat},
	{"mkdir",	"Make a directory",								DOSMkdir},
	{"rmdir",	"Remove a directory",							DOSRmdir},
	{"cd",		"Change directory",								DOSChdir},
	{"dir",		"View contents of directory",					DOSDir},
	{"del",		"Delete a file",								DOSDelete},
	{"rxfile",	"Receive a file with the name provided",		MonitorRxFile},
};

// Function needs to come after sg_sMonitorCommands[] due to a forward reference
static EStatus MonitorHelp(SLex *psLex,
						   const SMonitorCommands *psMonitorCommand,
						   uint32_t *pu32Address)
{
	uint16_t u16Loop;

	printf("Supported commands:\n");
	for (u16Loop = 0; u16Loop < (sizeof(sg_sMonitorCommands) / sizeof(sg_sMonitorCommands[0])); u16Loop++)
	{
		printf("%-12s - %s\n", sg_sMonitorCommands[u16Loop].peCommandString, sg_sMonitorCommands[u16Loop].peDescription);
	}

	return(ESTATUS_OK);
}

static EStatus MonitorParse(char *peLineText,
							uint16_t u16LineLength,
							uint32_t *pu32AddressPointer)
{
	SLex *psLex = NULL;
	EStatus eStatus = ESTATUS_OK;
	SToken sToken;
	ETokenType eToken;
	uint16_t u16Loop;

	// If we have nothing, then return
	if (0 == u16LineLength)
	{
		goto errorExit;
	}

	// Now open up the lexer stream
	eStatus = LexOpenBuffer(peLineText,
							&psLex,
							NULL);
	ERR_GOTO();

	ZERO_STRUCT(sToken);

	eToken = LexGetNextToken(psLex,
							 &sToken);
	if (ELEX_EOF == eToken)
	{
		// Fall through - blank lines
	}
	else
	if ((ELEX_INT_SIGNED == eToken) ||
		(ELEX_INT_UNSIGNED == eToken))
	{
		uint32_t u32Address;

		// Might be a pointer setting
		u32Address = (uint32_t) sToken.uData.u64IntValue;
		eStatus = MonitorCheckAddress("Error: Pointer ", u32Address);
		ERR_GOTO();

		printf("Address pointer set to 0x%.6x\n", u32Address);
		*pu32AddressPointer = u32Address;
		eStatus = ESTATUS_OK;
	}
	else
	if (ELEX_IDENTIFIER == eToken)
	{
		// Yes, it's an identifier. See if this is a command we know and
		for (u16Loop = 0; u16Loop < (sizeof(sg_sMonitorCommands) / sizeof(sg_sMonitorCommands[0])); u16Loop++)
		{
			if (strcasecmp(sg_sMonitorCommands[u16Loop].peCommandString, sToken.peString) == 0)
			{
				// Found it!
				LexClearToken(&sToken);

				eStatus = sg_sMonitorCommands[u16Loop].Handler(psLex,
															   &sg_sMonitorCommands[u16Loop],
															   pu32AddressPointer);
				break;
			}
		}

		if (u16Loop == (sizeof(sg_sMonitorCommands) / sizeof(sg_sMonitorCommands[0])))
		{
			printf("Unknown command '%s' - type 'help' or '?' for a list of valid commands\n", sToken.peString);
		}

		LexClearToken(&sToken);
	}
	else
	if ('?' == eToken)
	{
		// Alias for help
		eStatus = MonitorHelp(psLex,
							  NULL,
							  pu32AddressPointer);
	}
	else
	{
		printf("Expected a command - type 'help' or '?' for a list of valid commands\n", sToken.peString);
		LexClearToken(&sToken);
		eStatus = ESTATUS_OK;
	}

errorExit:
	LexClose(&psLex);

	if (eStatus != ESTATUS_OK)
	{
		printf("Error on parse - %s\n", GetErrorText(eStatus));
	}

	return(eStatus);
}

// Entry point for the monitor
EStatus MonitorStart(void)
{
	EStatus eStatus;
	SLineInput sLineInput;
	char *peLineText = NULL;
	uint16_t u16LineLength;
	uint32_t u32AddressPointer = 0;
	SELF sELF;
	uint32_t u32CPUSpeed;
	FILE *psFile;
	struct tm *psTime;
	time_t eTime;

	POST_SET(POSTCODE_BOOTLOADER_MONITOR);

	// Initialize the interrupt subsystem
	InterruptInit();

	// Reset the heap pointer
	SBRKHeapReset();

	// Install fault handlers
	FaultHandlerInstall();

	// Install the monitor start
	FaultInstallMonitor(FramePointerGet(),
						(void (*)(void)) MonitorStart);

	// Init the RTC
	eStatus = RTCInit();
	assert(ESTATUS_OK == eStatus);

	// See how fast this CPU is running
	eStatus = RTCGetCPUSpeed(&u32CPUSpeed);
	assert(ESTATUS_OK == eStatus);
	u32CPUSpeed = 25000000;

	// Master clock is CPU speed * 2
	eStatus = PTCInit(u32CPUSpeed<<1);
	assert(ESTATUS_OK == eStatus);

	// Start up the interrupts
	eStatus = StreamSetConsoleSerialInterruptMode(true);
	assert(ESTATUS_OK == eStatus);

	// Display date and time if we're the supervisor

	eTime = time(0);
	psTime = localtime((time_t *) &eTime);
	printf("\n");
	printf("Time/Date is %.4u-%.2u-%.2u %.2u:%.2u:%.2u\n", psTime->tm_year + 1900, psTime->tm_mon + 1, psTime->tm_mday, psTime->tm_hour, psTime->tm_min, psTime->tm_sec);

	printf("\n");
	printf("Roscoe Monitor. Type '?' or 'help' for a list of commands\n");

	(void) DOSInit();

	// Init the line input
	ZERO_STRUCT(sLineInput);

	sLineInput.Output = MonitorOutput;
	sLineInput.InputGet = MonitorInputGet;
	sLineInput.u16InputMax = MONITOR_LINE_MAX;
	sLineInput.u8HistoryMaxDepth = MONITOR_HISTORY_DEPTH;

	eStatus = LineInputInit(&sLineInput);
	assert(ESTATUS_OK == eStatus);

	eStatus = YModemInit(&sg_sYModem,
						 1,
						 FLASH_UPDATE_BAUD_RATE,
						 &sELF,
						 MonitorDataCallback);
	assert(ESTATUS_OK == eStatus);

	while (1)
	{
		peLineText = NULL;
		u16LineLength = MONITOR_LINE_MAX;
		char ePath[256];

		ePath[0] = '0';
		ePath[1] = ':';
		ePath[2] = '\0';

		eStatus = DOSGetDrivePath(ePath,
								  sizeof(ePath));
		if (ESTATUS_OK == eStatus)
		{
			printf("%s@%.6x>", ePath, u32AddressPointer);
		}
		else
		{
			// No disks available. Just show the address
			printf("%.6x>", u32AddressPointer);
		}

		// This is necessary for when printf is used and there isn't a line terminator
		fflush(stdout);

		// Go get a line!
		eStatus = LineInputGet(&sLineInput,
							   &peLineText,
							   &u16LineLength);

		if (ESTATUS_OK == eStatus)
		{
			// They hit enter
			MonitorParse(peLineText,
						 u16LineLength,
						 &u32AddressPointer);
		}
		else
		if (ESTATUS_CTRL_C == eStatus)
		{
			// Ctrl-C
		}
		else
		if (ESTATUS_TRANSFER_COMPLETE == eStatus)
		{
			// We have a program to execute!
			printf("ELF File transfer complete! Transferring control to 0x%.8x...\n", sELF.u32ExecStart);

			// Shut down the console
			eStatus = StreamSetConsoleSerialInterruptMode(false);
			if (eStatus != ESTATUS_OK)
			{
				printf("Error while shutting down interrupt console - %s\n", GetErrorText(eStatus));
			}
			else
			{
				SELFProgramHeader32 *psELFProgramHeader32 = (SELFProgramHeader32 *) sELF.u8ProgramHeader;

				// Stop interrupts
				InterruptDisable();

				// If appropriate, copy in the part of the vector table that makes sense to do so
				if (psELFProgramHeader32->u32PhysicalAddress < sizeof(sELF.u8VectorTable))
				{
					memcpy((void *) psELFProgramHeader32->u32PhysicalAddress,
						   (void *) &sELF.u8VectorTable[psELFProgramHeader32->u32PhysicalAddress],
						   sizeof(sELF.u8VectorTable) - psELFProgramHeader32->u32PhysicalAddress);
				}

				// Mask all interrupts, otherwise bad things happen
				*((volatile uint8_t *)ROSCOE_INTC_MASK) = 0xff;
				*((volatile uint8_t *)ROSCOE_INTC_MASK2) = 0xff;

				POST_SET(POSTCODE_ELF_DISPATCH_PROGRAM);

				// Dispatch to flash update utility
				((void (*)(void))sELF.u32ExecStart)();
			}
		}
		else
		if (ESTATUS_CANCELED == eStatus)
		{
			// Transfer canceled
		}
		else
		{
			// Unknown result.

			printf("eStatus != ESTATUS_OK - 0x%.8x - %s\n", eStatus, GetErrorText(eStatus));
			assert(0);
		}

		free(peLineText);
	}

	return(ESTATUS_OK);
}

