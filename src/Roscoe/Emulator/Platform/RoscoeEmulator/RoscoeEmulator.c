#include <stdio.h>
#include <stdint.h>
#include <intrin.h>
#include "Shared/types.h"
#include "OS/OS.h"
#include "Shared/Shared.h"
#include "Platform/RoscoeEmulator/RoscoeEmulator.h"
#include "Shared/Graphics/Window.h"
#include "Shared/Graphics/Control.h"
#include "../../../Shared/68030\m68k.h"

#define	CPU_SPEED	25000000
#define	SLICES_PER_SECOND	100

// If you want the diagnostics ROM loaded, uncommenct
//#define	ROM_DIAG			1

// Graphical surface for TRS-80 display
static UINT32 *sg_pu32TRS80Display;
static EWindowHandle sg_eEmulatorWindowHandle;
static EControlImageHandle sg_eTRS80DisplayHandle;
static EFontHandle sg_eButtonFontHandle;
static EControlButtonHandle sg_eReset;			// Reset button
static EControlButtonHandle sg_eReload;			// Reload button
static BOOL sg_bScreenUpdated = TRUE;
static BOOL sg_bRepaint = TRUE;
static BOOL sg_bNVStore = FALSE;				// Set TRUE if the NVStore needs to be written to disk

// Boot loader flash
#define	BASE_FLASH_LOADER		0x00000000
#define	BASE_FLASH_LOADER_SIZE	(512*1024)
static uint8_t sg_u8FlashLoader[BASE_FLASH_LOADER_SIZE];

// 16 MB of 16 bit BIOS flash
#define	BASE_FLASH_BIOS			0x08000000
#define	BASE_FLASH_BIOS_SIZE	(16*1024*1024)
static uint8_t sg_u8FlashBIOS[BASE_FLASH_BIOS_SIZE];

// 4MB of SRAM
#define	BASE_SRAM				0x10000000
#define	BASE_SRAM_SIZE			(4*1024*1024)
static uint8_t sg_u8SRAM[BASE_SRAM_SIZE];

// Extra
#define	BASE_SRAM_EXTRA			0x20000000
#define	BASE_SRAM_EXTRA_SIZE	(0x10000000)

// 512MB of DRAM
#define	BASE_DRAM				0x80000000
#define	BASE_DRAM_SIZE			(512*1024*1024)
static uint8_t sg_u8DRAM[BASE_DRAM_SIZE];

#define	BASE_8BIT_DEVICES					0x30000000
#define	BASE_16BIT_DEVICES					0x40000000
#define	BASE_32BIT_DEVICES					0x50000000

static BOOL sg_bResetFlagged;
static BOOL sg_bReloadFlagged;

static UINT8 sg_u8RTCRegs[0x80];

// # Of cycles to execute per slice
#define	CPU_SLICE	(CPU_SPEED / SLICES_PER_SECOND)

// 7 Segment POST code display positions
#define	SEG7_X				1024
#define	SEG7_Y				0
#define	SEG7_SIZE			100

#define	BUTTON_XPOS			SEG7_X
#define	BUTTON_YPOS			130
#define	BUTTON_YSIZE		55

// RTC Related
// Register definitions

// Time registers
#define	RTC_TIME_SECONDS		0x00
#define	RTC_TIME_MINUTES		0x02
#define	RTC_TIME_HOURS			0x04

// Day of week
#define	RTC_DAY_OF_WEEK			0x06

// Date
#define	RTC_DATE_DAY			0x07
#define	RTC_DATE_MONTH			0x08
#define RTC_DATE_YEAR			0x09

// Control registers
#define	RTC_CTRL_A  			0x0a

// Bit fields for RTC_CTRL_A
#define	RTC_CTRL_A_UIP			0x80
#define	RTC_CTRL_A_DV2			0x40
#define	RTC_CTRL_A_DV1			0x20
#define	RTC_CTRL_A_DV0			0x10
#define	RTC_CTRL_A_RS3			0x08
#define	RTC_CTRL_A_RS2			0x04
#define	RTC_CTRL_A_RS1			0x02
#define	RTC_CTRL_A_RS0			0x01

#define	RTC_CTRL_B				0x0b

// Bit fields for RTC_CTRL_B
#define	RTC_CTRL_B_SET			0x80
#define	RTC_CTRL_B_PIE			0x40
#define	RTC_CTRL_B_AIE			0x20
#define	RTC_CTRL_B_UIE			0x10
#define	RTC_CTRL_B_SQWE			0x08
#define	RTC_CTRL_B_DM			0x04
#define	RTC_CTRL_B_24_12		0x02
#define	RTC_CTRL_B_DSE			0x01

#define	RTC_CTRL_C				0x0c

#define	RTC_CTRL_D				0x0d

// Bit fields for RTC_CTRL_D
#define	RTC_CTRL_D_VRT			0x80


static const SCmdLineOption sg_sCommands[] =
{
	{"-fullscreen",		"Make the app full screen",					FALSE,		FALSE},

	// List terminator
	{NULL}
};

#define	UART_CLOCK		18432000

typedef struct S16550UART
{
	UINT16 u16Stipple;
	UINT8 u8Registers[8];
	UINT8 u8FCR;
	UINT16 u16DLAB;

	// Transmitter output
	UINT8 u8XMitBuffer[16];
	UINT8 u8XMitSize;
	UINT8 u8XMitCount;
	UINT8 u8XMitTail;
	UINT8 u8XMitHead;

	// Receiver input
	UINT8 u8RXBuffer[16];	// RX Data buffer
	UINT8 u8RXSize;			// Total size of RX buffer
	UINT8 u8RXCount;		// # Of characters in RX buffer now
	UINT8 u8RXTail;			// Tail rx pointer
	UINT8 u8RXHead;			// Head rx pointer

	// Used for clocking out characters in emulated time
	UINT32 u32TStateCharTime;
	INT32 s32ClockTimer;

	// Interrupt control
	BOOL bTHREInterruptPending;
	BOOL bRXInterruptPending;

	// Callback for transmit data
	void (*DataTX)(struct S16550UART *psUART,
				   UINT8 u8Data);
} S16550UART;

// 16550 Defines

// 16550 Register offsets
#define	UART_REG_RBRTHR			0x00		// Recieve buffer/transmit holding register
#define	UART_REG_IER			0x01		// Interrupt enable register
#define	UART_REG_IIR			0x02		// Interrupt identification register
#define	UART_REG_LCR			0x03		// Line control register
#define	UART_REG_MCR			0x04		// Modem control register
#define	UART_REG_LSR			0x05		// Line status register
#define	UART_REG_MSR			0x06		// Modem status register
#define	UART_REG_SCR			0x07		// Scratch register

// Interrupt enable register defines
#define	UART_IER_ERBFI			0x01		// Data available
#define	UART_IER_ETBEI			0x02		// Transmitter holding register empty
#define	UART_IER_ELSI			0x04		// Receiver line status interrupt
#define	UART_IER_EDSSI			0x08		// Modem status interrupt

// Interrupt identification register defines
#define	UART_IIR_NOT_PENDING	0x01		// Status when there's no interrupt pending
#define	UART_IIR_MSR_INT		0x00		// MSR Interrupt
#define	UART_IIR_THRE_INT		0x02		// THRE Interrupt
#define	UART_IIR_RBR_INT		0x04		// Receive interrupt
#define	UART_IIR_LSR_INT		0x06		// Line status interrupt
#define	UART_IIR_FIFOTO_INT		0x0c		// Character timeout indication
#define	UART_IIR_INT_MASK		0x0f		// Interrupt mask (getting rid of the FIFO bits)
#define	UART_IIR_FIFO_1			0x00		// 1 Byte trigger level
#define	UART_IIR_FIFO_4			0x40		// 4 Byte trigger level
#define	UART_IIR_FIFO_8			0x80		// 8 Byte trigger level
#define	UART_IIR_FIFO_14		0xc0		// 14 Byte trigger level
#define	UART_IIR_FIFO_ENABLE	0x01		// FIFO Enable

// Line control registers
#define	UART_LCR_DLAB_ENABLE	0x80		// DLAB bit
#define UART_LCR_8DB			0x03		// 8 Data bits
#define UART_LCR_NO_PARITY		0x00		// No parity
#define UART_LCR_EVEN_PARITY	0x18		// Even parity
#define UART_LCR_ODD_PARITY		0x08		// Odd parity
#define UART_LCR_1SB			0x00		// 1 Stop bit

// Modem control register
#define UART_MCR_DTR			0x01		// DTR
#define UART_MCR_RTS			0x02		// RTS
#define UART_MCR_OUT1			0x04		// OUT 1
#define UART_MCR_OUT2			0x08		// OUT 2

// Modem status register
#define UART_MSR_DCTS			0x01		// Delta CTS
#define UART_MSR_DDSR			0x02		// Delta DSR
#define UART_MSR_TERI			0x04		// Trailing edge ring indicator
#define UART_MSR_DDCD			0x08		// Delta DCD
#define UART_MSR_CTS			0x10		// Clear to send
#define UART_MSR_DSR			0x20		// Data set ready
#define UART_MSR_RI				0x40		// Ring indicator
#define UART_MSR_DCD			0x80		// Carrier detect

// Line status register
#define	UART_LSR_DR				0x01		// Data ready
#define UART_LSR_OE				0x02		// Overrun
#define UART_LSR_PE				0x04		// Parity error
#define UART_LSR_FE				0x08		// Framing error
#define	UART_LSR_BI				0x10		// Break interrupt
#define	UART_LSR_THRE			0x20		// THRE empty
#define	UART_LSR_TEMT			0x40		// TEMT Empty
#define UART_LSR_RCVR_FIFO_ERR	0x80		// Receiver FIFO error

// IIR registers
#define UART_IIR_THRE	0x02
#define UART_IIR_RXDATA	0x04

// Divisor latch access bit
#define	UART_LCR_DLAB	0x80

// OUT2
#define	UART_MCR_OUT2	0x08

// Transmitter stuff
#define	UART_LSR_THRE		0x20
#define	UART_LSR_TEMT		0x40

// Receiver stuff
#define	UART_LSR_DR			0x01

// Supervisor UARTs
static S16550UART sg_sUARTA;
static S16550UART sg_sUARTB;

static void UARTWrite(S16550UART *psUART,
					  UINT8 u8Offset,
					  UINT8 u8Data)
{
	// Scratch register?
	if ((UART_REG_SCR == u8Offset) ||
		(UART_REG_LCR == u8Offset))
	{
		psUART->u8Registers[u8Offset] = u8Data;
	}
	else
	if (UART_REG_IIR == u8Offset)
	{

		// This is really the FCR
		psUART->u8FCR = u8Data & 0xc9;

		if (psUART->u8FCR >> 6)
		{
			// FIFOs are ON! Set them both to 16 characters in length
			psUART->u8RXSize = 16;
			psUART->u8XMitSize = 16;
		}
		else
		{
			// FIFOs are OFF! Set them both to 1 character
			psUART->u8RXSize = 1;
			psUART->u8XMitSize = 1;
		}
	}
	else
	if (UART_REG_RBRTHR == u8Offset)
	{
		// If we are in DLAB mode, then set things up
		if (psUART->u8Registers[UART_REG_LCR] & UART_LCR_DLAB)
		{
			psUART->u16DLAB = (psUART->u16DLAB & 0xff00) | u8Data;
			if (psUART->u16DLAB)
			{
				psUART->u32TStateCharTime = (CPU_SPEED / (((UART_CLOCK / 16) / psUART->u16DLAB) / 10));
			}
			else
			{
				psUART->u32TStateCharTime = 0;
			}
		}
		else
		{
			// Clear any THRE pending interrupts
			psUART->bTHREInterruptPending = FALSE;

			// Data write
			if (psUART->u8XMitCount == psUART->u8XMitSize)
			{
				// Too many! Transmitter overrun!
			}
			else
			{
				// Stuff it in the outgoing transmitter buffer
				psUART->u8XMitBuffer[psUART->u8XMitHead++] = u8Data;
				if (psUART->u8XMitHead >= psUART->u8XMitSize)
				{
					psUART->u8XMitHead = 0;
				}

				DebugOut("%s: Character write - 0x%.2x - '%c'\n", __FUNCTION__, u8Data, u8Data);

				if (0 == psUART->u8XMitCount)
				{
					psUART->s32ClockTimer += (INT32) psUART->u32TStateCharTime;
				}

				psUART->u8XMitCount++;
			}
		}
	}
	else
	if (UART_REG_IER == u8Offset)
	{
		// If we are in DLAB mode, then set things up
		if (psUART->u8Registers[UART_REG_LCR] & UART_LCR_DLAB)
		{
			psUART->u16DLAB = (psUART->u16DLAB & 0xff) | (u8Data << 8);

			if (psUART->u16DLAB)
			{
				psUART->u32TStateCharTime = (CPU_SPEED / (((UART_CLOCK / 16) / psUART->u16DLAB) / 10));
			}
			else
			{
				psUART->u32TStateCharTime = 0;
			}
		}
		else
		{
			// IER write
			psUART->u8Registers[u8Offset] = u8Data;
		}
	}
	else
	if (UART_REG_MCR == u8Offset)
	{
		UINT8 u8Old = psUART->u8Registers[u8Offset];

		// We're updating the modem control register. If we're changing OUT2, then let the
		// display driver know about it
		psUART->u8Registers[u8Offset] = u8Data;

		if ((u8Old ^ u8Data) & UART_MCR_OUT2)
		{
			// Out2 changed. Update the LED.
		}
	}
	else
	{
		// Don't know yet
		BASSERT(0);
	}
}

// Incoming data into our UART
static void UARTRXData(S16550UART *psUART,
					   UINT8 u8Char)
{
	// If we have receiver interrupts turned on, then signal that we have an interrupt
	if (psUART->u8Registers[UART_REG_IER] & 1)
	{
		psUART->bRXInterruptPending = TRUE;
	}

	if (psUART->u8RXCount >= psUART->u8RXSize)
	{
		// TODO: Overrun error
		return;
	}

	psUART->u8RXBuffer[psUART->u8RXHead++] = u8Char;
	psUART->u8RXCount++;
	if (psUART->u8RXHead >= psUART->u8RXSize)
	{
		psUART->u8RXHead = 0;
	}
}

static UINT8 UARTRead(S16550UART *psUART,
					  UINT8 u8Offset)
{
	if (((UART_REG_IER == u8Offset) ||
	 	(UART_REG_RBRTHR == u8Offset)) && (psUART->u8Registers[UART_REG_LCR] & UART_LCR_DLAB))
	{
		// Divisor access latch. See which we return
		if (UART_REG_RBRTHR == u8Offset)
		{
			return((UINT8) psUART->u16DLAB);
		}
		else
		{
			return((UINT8) (psUART->u16DLAB >> 8));
		}
	}
	else
	if (UART_REG_MSR == u8Offset)
	{
		return(0xb0);	 // Hardcode DCD, DSR, and CTS to asserted
	}
	else
	if (UART_REG_RBRTHR == u8Offset)
	{
		UINT8 u8Data;

		// Receive buffer register
		u8Data = psUART->u8RXBuffer[psUART->u8RXTail];
		if (psUART->u8RXCount)
		{
			// Only advance the tail pointer if there's data in the buffer
			psUART->u8RXTail++;
			if (psUART->u8RXTail >= psUART->u8RXSize)
			{
				psUART->u8RXTail = 0;
			}

			psUART->u8RXCount--;
			if (0 == psUART->u8RXCount)
			{
				// No more interrupt pending if we sucked out the last byte
				psUART->bRXInterruptPending = FALSE;
			}
		}
		else
		{
			psUART->bRXInterruptPending = FALSE;
		}

		return(u8Data);
	}
	if (UART_REG_LSR == u8Offset)
	{
		UINT8 u8Data;

		u8Data = 0;

		// See if the transmitter holding register has anything in it
		if (0 == psUART->u8XMitCount)
		{
			u8Data |= (UART_LSR_THRE | UART_LSR_TEMT);
		}

		// See if we have any data pending
		if (psUART->u8RXCount)
		{
			u8Data |= UART_LSR_DR;
		}

		return(u8Data);
	}
	else
	if (UART_REG_IIR == u8Offset)
	{
		UINT8 u8Data = 0;

		// Pull in FIFO enable bits (if enabled)
		if (psUART->u8FCR & 1)
		{
			// FIFO is enabled.
			u8Data |= (psUART->u8FCR & 0xc0);
		}

		if (psUART->bRXInterruptPending)
		{
			return(UART_IIR_RXDATA | u8Data);
		}

		if (psUART->bTHREInterruptPending)
		{
			psUART->bTHREInterruptPending = FALSE;
			return(UART_IIR_THRE | u8Data);
		}

		u8Data |= 0x01;	// No interrupt pending
		return(u8Data);
	}
	else
	{
		// Return the register
		return(psUART->u8Registers[u8Offset]);
	}
}

static void UARTStep(S16550UART *psUART,
					 uint32_t u32Ticks)
{
	// See if there's something to transmit
	if (psUART->u8XMitCount)
	{
		// Well, we're running, so let's decrement our timer
		psUART->s32ClockTimer -= (INT32) u32Ticks;
		while (psUART->s32ClockTimer < 0)
		{
			UINT8 u8Char;

			// Means we should do something about another character.

			u8Char = psUART->u8XMitBuffer[psUART->u8XMitTail++];
			if (psUART->u8XMitTail >= psUART->u8XMitSize)
			{
				psUART->u8XMitTail = 0;
			}

			if (psUART->DataTX)
			{
				psUART->DataTX(psUART,
							   u8Char);
			}

			psUART->u8XMitCount--;
			if (0 == psUART->u8XMitCount)
			{
				// All done transmitting.
				psUART->s32ClockTimer = 0;

				// If THRE interrupts are turned on, then flag it
				if (psUART->u8Registers[UART_REG_IER] & UART_IIR_THRE)
				{
					psUART->bTHREInterruptPending = TRUE;
				}
			}
			else
			{
				// Not done. Add our character time to the char timer
				psUART->s32ClockTimer += (INT32) psUART->u32TStateCharTime;
			}
		}
	}
}

static void RTCWrite(uint8_t u8Address,
					 uint8_t u8Data)
{
	u8Address &= 0x7f;

	sg_bNVStore = true;
	sg_u8RTCRegs[u8Address] = u8Data;
	DebugOut("RTC: Write: Addr=0x%.2x, value=0x%.2x\n", u8Address, sg_u8RTCRegs[u8Address]);
}

static uint8_t RTCRead(uint8_t u8Address)
{
	// Data read
	if (RTC_CTRL_D == u8Address)
	{
		// Return VRT
		return(RTC_CTRL_D_VRT);
	}

	// If we're asking for date/time, then return the host system's time
	if ((0 == u8Address) ||
		(2 == u8Address) ||
		(4 == u8Address) ||
		((u8Address >= 6) && (u8Address <= 9)))
	{
		time_t s64Time;
		struct tm *psTime = NULL;

		s64Time = time(0);
		psTime = localtime(&s64Time);

		if (0 == u8Address)
		{
			return(psTime->tm_sec);
		}

		if (2 == u8Address)
		{
			return(psTime->tm_min);
		}

		if (4 == u8Address)
		{
			return(psTime->tm_hour);
		}

		if (7 == u8Address)
		{
			return(psTime->tm_mday);
		}

		if (8 == u8Address)
		{
			return(psTime->tm_mon);
		}

		if (9 == u8Address)
		{
			return(psTime->tm_year - 100);
		}

		BASSERT(0);
	}

	return(sg_u8RTCRegs[u8Address]);
}

typedef enum
{
	ECPU_STOP,
	ECPU_STEP,
	ECPU_STEP_OVER,
	ECPU_RESET,
	ECPU_RUN
} ECPUState;

typedef struct SDigitSegmentOffset
{
	char *pu8ImageFilename;
	INT32 s32XOffset;
	INT32 s32YOffset;
} SDigitSegmentOffset;

static const SDigitSegmentOffset sg_sDigitSegs[] =
{
	{"../files/Images/dotseg.png",	80, 117},		// Lower right dot (dp)
	{"../files/Images/hseg.png",	38, 32},		// Top segment (a)
	{"../files/Images/vseg.png",	67, 38},		// Upper right segment (b)
	{"../files/Images/vseg.png",	62, 75},		// Lower right segment (c)
	{"../files/Images/hseg.png",	29, 107},		// Bottom segment (d)
	{"../files/Images/vseg.png",   22, 75},			// Lower left segment (e)
	{"../files/Images/vseg.png",	26, 38},		// Upper left segment (f)
	{"../files/Images/hseg.png",	34, 70},		// Middle segment (g)
	{"../files/Images/dotseg.png", 7,  117},		// Lower left dot
};

typedef struct S7Seg
{
	EControlImageHandle e7SegBackground;
	EControlImageHandle e7SegSegments[9];	// 7 Segments plus 2 dots
} S7Seg;

// Left 7 segment digit
static S7Seg sg_sDigit1;

// Right 7 segment digit
static S7Seg sg_sDigit2;

#define	KEY_SHIFTED		0x80000000

static const SDL_Scancode sg_eRow1[] = {SDL_SCANCODE_2 | KEY_SHIFTED, SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_NUM_SCANCODES};
static const SDL_Scancode sg_eRow2[] = {SDL_SCANCODE_H, SDL_SCANCODE_I, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O, SDL_NUM_SCANCODES};
static const SDL_Scancode sg_eRow4[] = {SDL_SCANCODE_P, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T, SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_NUM_SCANCODES};
static const SDL_Scancode sg_eRow8[] = {SDL_SCANCODE_X, SDL_SCANCODE_Y, SDL_SCANCODE_Z, SDL_NUM_SCANCODES};
static const SDL_Scancode sg_eRow10[] = {SDL_SCANCODE_0, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_NUM_SCANCODES};
static const SDL_Scancode sg_eRow20[] = {SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_COMMA, SDL_SCANCODE_EQUALS, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_NUM_SCANCODES};
static const SDL_Scancode sg_eRow40[] = {SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_PAUSE, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_RIGHT, SDL_SCANCODE_SPACE, SDL_NUM_SCANCODES};
static const SDL_Scancode sg_eRow80[] = {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_LCTRL, SDL_NUM_SCANCODES};

static const SDL_Scancode *sg_sKeyboardRows[] =
{
	sg_eRow1,
	sg_eRow2,
	sg_eRow4,
	sg_eRow8,
	sg_eRow10,
	sg_eRow20,
	sg_eRow40,
	sg_eRow80
};

// Keyboard matrix
static UINT8 sg_u8KeyboardMatrix[8];

// Set TRUE if we want to override holding the break key down
static BOOL sg_bBreakHold = TRUE;
static BOOL sg_bClearHold = TRUE;

static EStatus Seg7SetStipple(S7Seg *ps7Seg,
							  UINT16 u16Data)
{
	UINT8 u8Loop;
	EStatus eStatus;

	// Turn off left decimal points for now
	u16Data |= 0x100;

	// Run through each segment and see if we redraw it or not
	for (u8Loop = 0; u8Loop < 9; u8Loop++)
	{
		BOOL bVisible = FALSE;

		if (0 == (u16Data & 1))
		{
			bVisible = TRUE;
		}

		eStatus = ControlSetVisible((EControlHandle) ps7Seg->e7SegSegments[u8Loop],
									bVisible);
		ERR_GOTO();
		u16Data >>= 1;
	}

errorExit:
	return(eStatus);
}

static EStatus Seg7Init(EWindowHandle eWindowHandle,
						INT32 s32XPos,
						INT32 s32YPos,
						S7Seg *ps7Seg)
{
	UINT8 u8Loop;
	EStatus eStatus;

	memset((void *) ps7Seg, 0, sizeof(*ps7Seg));

	eStatus = ControlImageCreate(eWindowHandle,
								 &ps7Seg->e7SegBackground,
								 s32XPos,
								 s32YPos);
	ERR_GOTO();

	// Load up the 7 segment image
	eStatus = ControlImageSetFromFile(ps7Seg->e7SegBackground,
									  "../files/Images/7seg.png");
	ERR_GOTO();

	eStatus = ControlSetVisible((EControlHandle) ps7Seg->e7SegBackground,
								TRUE);
	ERR_GOTO();

	// Load up each of the images
	for (u8Loop = 0; u8Loop < (sizeof(sg_sDigitSegs) / sizeof(sg_sDigitSegs[0])); u8Loop++)
	{
		eStatus = ControlImageCreate(eWindowHandle,
									 &ps7Seg->e7SegSegments[u8Loop],
									 s32XPos + sg_sDigitSegs[u8Loop].s32XOffset,
									 s32YPos + sg_sDigitSegs[u8Loop].s32YOffset);
		ERR_GOTO();

		eStatus = ControlImageSetFromFile(ps7Seg->e7SegSegments[u8Loop],
										  sg_sDigitSegs[u8Loop].pu8ImageFilename);
		ERR_GOTO();

		eStatus = ControlSetVisible((EControlHandle) ps7Seg->e7SegSegments[u8Loop],
									TRUE);
		ERR_GOTO();
	}

errorExit:
	return(eStatus);
}

// Keyboard read. Convert SDL scancodes to something we feed to the TRS-80
// keyboard matrix.
static UINT8 KeyboardRead(UINT16 u16Offset)
{
	UINT8 u8Index = 0;
	UINT8 u8ShiftsRemaining = 8;

	// Only LSB is relevant
	u16Offset &= 0xff;

	if (u16Offset)
	{
		while (0 == (u16Offset & (1 << u8Index)))
		{
			++u8Index;
		}
	}

	BASSERT(u8Index < (sizeof(sg_sKeyboardRows) / sizeof(sg_sKeyboardRows[0])));
	return(sg_u8KeyboardMatrix[u8Index]);
}

typedef struct SKey
{
	SDL_Scancode eSDLScancode;
	UINT8 u8Index0;
	UINT8 u8OR0;
	UINT8 u8Index1;
	UINT8 u8OR1;
	BOOL bClearShift;
} SKey;

#define	SDL_SHIFTED		0x80000000		// Shifted
#define	SDL_SHIFT_DC	0x40000000		// Don't care what state it is
#define	SCANCODE_MASK	(~(SDL_SHIFTED | SDL_SHIFT_DC))

static const SKey sg_sKeyMap[] =
{
	// Row 0
	{SDL_SCANCODE_2 | SDL_SHIFTED,				0,	0x01,	0,	0x00},		// @
	{SDL_SCANCODE_A | SDL_SHIFT_DC,				0,	0x02,	0,	0x00},		// A
	{SDL_SCANCODE_B | SDL_SHIFT_DC,				0,	0x04,	0,	0x00},		// B
	{SDL_SCANCODE_C | SDL_SHIFT_DC,				0,	0x08,	0,	0x00},		// C
	{SDL_SCANCODE_D | SDL_SHIFT_DC,				0,	0x10,	0,	0x00},		// D
	{SDL_SCANCODE_E | SDL_SHIFT_DC,				0,	0x20,	0,	0x00},		// E
	{SDL_SCANCODE_F | SDL_SHIFT_DC,				0,	0x40,	0,	0x00},		// F
	{SDL_SCANCODE_G | SDL_SHIFT_DC,				0,	0x80,	0,	0x00},		// G

	// Row 1
	{SDL_SCANCODE_H | SDL_SHIFT_DC,				1,	0x01,	0,	0x00},		// H
	{SDL_SCANCODE_I | SDL_SHIFT_DC,				1,	0x02,	0,	0x00},		// I
	{SDL_SCANCODE_J | SDL_SHIFT_DC,				1,	0x04,	0,	0x00},		// J
	{SDL_SCANCODE_K | SDL_SHIFT_DC,				1,	0x08,	0,	0x00},		// K
	{SDL_SCANCODE_L | SDL_SHIFT_DC,				1,	0x10,	0,	0x00},		// L
	{SDL_SCANCODE_M | SDL_SHIFT_DC,				1,	0x20,	0,	0x00},		// M
	{SDL_SCANCODE_N | SDL_SHIFT_DC,				1,	0x40,	0,	0x00},		// N
	{SDL_SCANCODE_O | SDL_SHIFT_DC,				1,	0x80,	0,	0x00},		// O

	// Row 2
	{SDL_SCANCODE_P | SDL_SHIFT_DC,				2,	0x01,	0,	0x00},		// P
	{SDL_SCANCODE_Q | SDL_SHIFT_DC,				2,	0x02,	0,	0x00},		// Q
	{SDL_SCANCODE_R | SDL_SHIFT_DC,				2,	0x04,	0,	0x00},		// R
	{SDL_SCANCODE_S | SDL_SHIFT_DC,				2,	0x08,	0,	0x00},		// S
	{SDL_SCANCODE_T | SDL_SHIFT_DC,				2,	0x10,	0,	0x00},		// T
	{SDL_SCANCODE_U | SDL_SHIFT_DC,				2,	0x20,	0,	0x00},		// U
	{SDL_SCANCODE_V | SDL_SHIFT_DC,				2,	0x40,	0,	0x00},		// V
	{SDL_SCANCODE_W | SDL_SHIFT_DC,				2,	0x80,	0,	0x00},		// W

	// Row 3
	{SDL_SCANCODE_X | SDL_SHIFT_DC,				3,	0x01,	0,	0x00},		// X
	{SDL_SCANCODE_Y | SDL_SHIFT_DC,				3,	0x02,	0,	0x00},		// Y
	{SDL_SCANCODE_Z | SDL_SHIFT_DC,				3,	0x04,	0,	0x00},		// Z

	// Row 4
	{SDL_SCANCODE_0,							4,	0x01,	0,	0x00},		// 0
	{SDL_SCANCODE_0 | SDL_SHIFTED,				5,	0x02,	0,	0x00},		// )
	{SDL_SCANCODE_1 | SDL_SHIFT_DC,				4,	0x02,	0,	0x00},		// 1
	{SDL_SCANCODE_2,							4,	0x04,	0,	0x00},		// 2
	{SDL_SCANCODE_3 | SDL_SHIFT_DC,				4,	0x08,	0,	0x00},		// 3
	{SDL_SCANCODE_4 | SDL_SHIFT_DC,				4,	0x10,	0,	0x00},		// 4
	{SDL_SCANCODE_5 | SDL_SHIFT_DC,				4,	0x20,	0,	0x00},		// 5
	{SDL_SCANCODE_6,							4,	0x40,	0,	0x00},		// 6
	{SDL_SCANCODE_7 | SDL_SHIFTED,				4,	0x40,	7,	0x01},		// &
	{SDL_SCANCODE_7 | SDL_SHIFT_DC,				4,	0x80,	0,	0x00},		// 7

	// Row 5
	{SDL_SCANCODE_8,							5,	0x01,	0,	0x00},		// 8
	{SDL_SCANCODE_8 | SDL_SHIFTED,				5,	0x04,	0,	0x00},		// *
	{SDL_SCANCODE_9,							5,	0x02,	0,	0x00},		// 9
	{SDL_SCANCODE_9 | SDL_SHIFTED,				5,	0x01,	0,	0x00},		// 9
	{SDL_SCANCODE_MINUS,						5,	0x20,	0,	0x00},		// -
	{SDL_SCANCODE_EQUALS,						5,	0x20,	7,	0x01},		// -
	{SDL_SCANCODE_COMMA | SDL_SHIFT_DC,			5,	0x10,	0,	0x00},		// <
	{SDL_SCANCODE_PERIOD | SDL_SHIFT_DC,		5,	0x40,	0,	0x00},		// =
	{SDL_SCANCODE_SLASH,						5,	0x80,	0,	0x00},		// >
	{SDL_SCANCODE_SLASH | SDL_SHIFTED,			5,	0x80,	0,	0x00},		// ?
	{SDL_SCANCODE_SEMICOLON,					5,	0x08,	0,	0x00},		// Semicolon
	{SDL_SCANCODE_SEMICOLON | SDL_SHIFTED,		5,	0x04,	0,	0x00,	TRUE},		// Colon
	{SDL_SCANCODE_EQUALS | SDL_SHIFTED,			5,	0x08,	7,	0x01},		// Equals
	{SDL_SCANCODE_APOSTROPHE,					4,	0x80,	7,	0x01},		// Apostrophe			
	{SDL_SCANCODE_APOSTROPHE | SDL_SHIFTED,		4,	0x04,	7,	0x01},		// Double quote

	// Row 6
	{SDL_SCANCODE_RETURN | SDL_SHIFT_DC,		6,	0x01,	0,	0x00},		// Enter
	{SDL_SCANCODE_ESCAPE | SDL_SHIFT_DC,		6,	0x02,	0,	0x00},		// Clear
	{SDL_SCANCODE_PAUSE | SDL_SHIFTED,			6,	0x04,	0,	0x00},		// Break
	{SDL_SCANCODE_UP | SDL_SHIFT_DC,			6,	0x08,	0,	0x00},		// Up
	{SDL_SCANCODE_DOWN | SDL_SHIFT_DC,			6,	0x10,	0,	0x00},		// Down
	{SDL_SCANCODE_LEFT | SDL_SHIFT_DC,			6,	0x20,	0,	0x00},		// Left
	{SDL_SCANCODE_BACKSPACE | SDL_SHIFT_DC,		6,	0x20,	0,	0x00},		// Left (also backspace)
	{SDL_SCANCODE_RIGHT | SDL_SHIFT_DC,			6,	0x40,	0,	0x00},		// Right
	{SDL_SCANCODE_SPACE | SDL_SHIFT_DC,			6,	0x80,	0,	0x00},		// Space

	// Row 7
	{SDL_SCANCODE_LSHIFT | SDL_SHIFT_DC,		7,	0x03,	0,	0x00},		// Left and right shifts
	{SDL_SCANCODE_RSHIFT | SDL_SHIFT_DC,		7,	0x03,	0,	0x00},		// Right shift only
	{SDL_SCANCODE_LCTRL | SDL_SHIFT_DC,			7,	0x10,	0,	0x00},		// Left control
	{SDL_SCANCODE_RCTRL | SDL_SHIFT_DC,			7,	0x10,	0,	0x00},		// Right control

	// No more keys to look at
	{SDL_NUM_SCANCODES}
};

// This will update the global keyboard state variables
static void KeyboardUpdate(void)
{
	BOOL bSDLShifted = FALSE;
	BOOL bClearShift = FALSE;
	const SKey *psKey;

	// Clear the keyboard keys
	memset((void *) sg_u8KeyboardMatrix, 0, sizeof(sg_u8KeyboardMatrix));

	// Get shift state (left and right) for comparison purposes
	if (WindowScancodeGetState(SDL_SCANCODE_LSHIFT) ||
		WindowScancodeGetState(SDL_SCANCODE_RSHIFT))
	{
		bSDLShifted = TRUE;
	}

	psKey = sg_sKeyMap;

	while (psKey->eSDLScancode != SDL_NUM_SCANCODES)
	{
		BOOL bSkipCheck = FALSE;

		// Check shift state
		if (psKey->eSDLScancode & SDL_SHIFT_DC)
		{
			 // It's a 'don't care'
		}
		else
		{
			BOOL bShiftKey = FALSE;

			// Now ensure the shift state actually matches
			if (psKey->eSDLScancode & SDL_SHIFTED)
			{
				bShiftKey = TRUE;
			}

			if (bShiftKey == bSDLShifted)
			{
				// It matches! 
			}
			else
			{
				bSkipCheck = TRUE;
			}
		}

		if (FALSE == bSkipCheck)
		{
			if (WindowScancodeGetState(psKey->eSDLScancode & SCANCODE_MASK))
			{
				sg_u8KeyboardMatrix[psKey->u8Index0] |= psKey->u8OR0;
				sg_u8KeyboardMatrix[psKey->u8Index1] |= psKey->u8OR1;

				if (psKey->bClearShift)
				{
					bClearShift = TRUE;
				}
			}
		}

		++psKey;
	}

	// Special case - if we want to hold break, do it
	if (sg_bBreakHold)
	{
		// Force a break being held
		sg_u8KeyboardMatrix[6] |= 0x04;
	}

	if (sg_bClearHold)
	{
		// Force a clear being held
		sg_u8KeyboardMatrix[6] |= 0x02;
	}

	if (bClearShift)
	{
		sg_u8KeyboardMatrix[7] &= 0xfc;
	}
}

static EStatus LoadROM(char *peFilename,
					   UINT16 u16Offset,
					   UINT8 *pu8LoadAddress,
					   UINT32 u32Size)
{
	SOSFile sFile;
	EStatus eStatus;
	UINT64 u64SizeRead;

	// Assume 1K if size is 0
	if (0 == u32Size)
	{
		u32Size = 1024;
	}

	eStatus = Filefopen(&sFile, 
						peFilename,
						"rb");
	ERR_GOTO();

	eStatus = Filefread((void *) (pu8LoadAddress + u16Offset), u32Size, &u64SizeRead, sFile);

	if (ESTATUS_OK == eStatus)
	{
		DebugOut("Loaded '%s' - %u bytes at 0x%.4x\n", peFilename, (UINT32) u64SizeRead, u16Offset);
		eStatus = Filefclose(&sFile);
	}
	else
	{
		DebugOut("Failed '%s' - %u bytes at 0x%.4x - %s\n", peFilename, (UINT32) u64SizeRead, u16Offset, GetErrorText(eStatus));
		(void) Filefclose(&sFile);
	}

errorExit:
	return(eStatus);
}

static EStatus EmulatorReload(void)
{
	EStatus eStatus;

	memset((void *) sg_u8FlashLoader, 0xff, 512*1024);

	eStatus = LoadROM("../../../../BootLoader/BootLoader.bin", 0, sg_u8FlashLoader, 512*1024);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

static void EmulatorReset(void)
{
	uint32_t u32Loop;

	m68k_pulse_reset();
	
	// Scramble the contents of SRAM and DRAM
	for (u32Loop = 0; u32Loop < BASE_SRAM_SIZE; u32Loop += sizeof(uint64_t))
	{
		*((uint64_t *) &sg_u8SRAM[u32Loop]) = SharedRandomNumber();
	}

	for (u32Loop = 0; u32Loop < BASE_DRAM_SIZE; u32Loop += sizeof(uint64_t))
	{
//		*((uint64_t *) &sg_u8DRAM[u32Loop]) = SharedRandomNumber();
	}
}

static void ButtonCallback(EControlButtonHandle eButtonHandle,
						   EControlCallbackReason eReason,
						   UINT32 u32ReasonData,
						   INT32 s32ControlRelativeXPos,
						   INT32 s32ControlRelativeYPos)
{
	if ((ECTRLACTION_MOUSEBUTTON == eReason) &&
		(u32ReasonData & BUTTON_PRESSED))
	{
		if (eButtonHandle == sg_eReset)
		{
			sg_bResetFlagged = TRUE;
		}
		if (eButtonHandle == sg_eReload)
		{
			sg_bReloadFlagged = TRUE;
		}
	}
}

static EStatus ButtonCreate(EControlButtonHandle *peButtonHandle,
							char *peButtonTag,
							UINT32 u32XPos,
							UINT32 u32YPos)
{
	EStatus eStatus;
	SControlButtonConfig sButtonConfig;
	ZERO_STRUCT(sButtonConfig);

	eStatus = ControlButtonCreate(sg_eEmulatorWindowHandle,
								  peButtonHandle,
								  u32XPos,
								  u32YPos);
	ERR_GOTO();

	eStatus = GraphicsLoadImage("../Images/button_on_whole.png",
								&sButtonConfig.pu32RGBAButtonNotPressed,
								&sButtonConfig.u32XSizeNotPressed,
								&sButtonConfig.u32YSizeNotPressed);
	ERR_GOTO();

	eStatus = GraphicsLoadImage("../Images/button_pressed_whole.png",
								&sButtonConfig.pu32RGBAButtonPressed,
								&sButtonConfig.u32XSizePressed,
								&sButtonConfig.u32YSizePressed);
	ERR_GOTO();

	eStatus = ControlButtonSetAssets(*peButtonHandle,
									 &sButtonConfig);
	ERR_GOTO();

	eStatus = ControlButtonSetText(*peButtonHandle,
								   0, 0,
								   2, 2,
								   TRUE,
								   sg_eButtonFontHandle,
								   30,
								   peButtonTag,
								   TRUE,
								   0xffffffff);
	ERR_GOTO();

	eStatus = ControlSetDisable((EControlHandle) *peButtonHandle, 
								FALSE);
	ERR_GOTO();

	eStatus = ControlSetVisible((EControlHandle) *peButtonHandle,
								TRUE);
	ERR_GOTO();

	eStatus = ControlButtonSetCallback(*peButtonHandle,
									   ButtonCallback);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

static EStatus EmulatorUISetup(void)
{
	EStatus eStatus;

	// Create the emulator window
	eStatus = WindowCreate(&sg_eEmulatorWindowHandle,
						   0, 0,
						   3840, 2160);
	ERR_GOTO();

	// Set up the 7 segment displays
	eStatus = Seg7Init(sg_eEmulatorWindowHandle,
					   SEG7_X,
					   SEG7_Y,
					   &sg_sDigit1);
	BASSERT(ESTATUS_OK == eStatus);
	eStatus = Seg7Init(sg_eEmulatorWindowHandle,
					   SEG7_X + SEG7_SIZE,
					   SEG7_Y,
					   &sg_sDigit2);
	BASSERT(ESTATUS_OK == eStatus);

	// Now the font we need for button text
	eStatus = FontCreate(&sg_eButtonFontHandle);
	ERR_GOTO();

	eStatus = FontLoad(sg_eButtonFontHandle,
					   "../Fonts/FontsFree-Net-MYRIADPRO-REGULAR.ttf");
	ERR_GOTO();

	// Now buttons
	eStatus = ButtonCreate(&sg_eReset,
						   "Reset",
						   BUTTON_XPOS,
						   BUTTON_YPOS);
	ERR_GOTO();

	eStatus = ButtonCreate(&sg_eReload,
						   "Reload",
						   BUTTON_XPOS,
						   BUTTON_YPOS + BUTTON_YSIZE);
	ERR_GOTO();

	eStatus = WindowSetVisible(sg_eEmulatorWindowHandle,
							   TRUE);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

// Called once per frame
void VideoRepaint(void)
{
}

char *PlatformGetClientName(void)
{
	return("Roscoe emulator");
}

unsigned int  m68k_read_memory_8(unsigned int address)
{
	if ((address >= BASE_FLASH_LOADER) && ((address) < BASE_FLASH_BIOS))
	{
		address -= BASE_FLASH_LOADER;
		return(sg_u8FlashLoader[address]);
	}

	// SRAM?
	if ((address >= BASE_SRAM) && ((address) < BASE_SRAM_EXTRA))
	{
		address -= BASE_SRAM;
		return(sg_u8SRAM[address]);
	}

	// 8 Bit device mapping
	if ((address >= BASE_8BIT_DEVICES) && (address < BASE_16BIT_DEVICES))
	{
		address -= BASE_8BIT_DEVICES;
		if (0 == (address >> 8))
		{
			// UART A
			address &= 0x7;
			return(UARTRead(&sg_sUARTA,
							address));
		}
		else
		if (1 == (address >> 8))
		{
			// UART B
			address &= 0x7;
			return(UARTRead(&sg_sUARTB,
							address));
		}
		else
		if (3 == (address >> 8))
		{
			return(RTCRead(address));
		}

		BASSERT(0);
	}

	BASSERT(0);
	return(0xff);
}

unsigned int  m68k_read_memory_16(unsigned int address)
{
	if ((address >= BASE_FLASH_LOADER) && ((address) < BASE_FLASH_BIOS))
	{
		address -= BASE_FLASH_LOADER;
		return(_byteswap_ushort(*((uint16_t *) &sg_u8FlashLoader[address])));
	}

	// SRAM?
	if ((address >= BASE_SRAM) && ((address) < BASE_SRAM_EXTRA))
	{
		address -= BASE_SRAM;
		return(_byteswap_ushort(*((uint16_t *) &sg_u8SRAM[address])));
	}

	BASSERT(0);
	return(0xffff);
}

unsigned int  m68k_read_memory_32(unsigned int address)
{
	if ((address >= BASE_FLASH_LOADER) && ((address) < BASE_FLASH_BIOS))
	{
		address -= BASE_FLASH_LOADER;
		return(_byteswap_ulong(*((uint32_t *) &sg_u8FlashLoader[address])));
	}

	// SRAM?
	if ((address >= BASE_SRAM) && ((address) < BASE_SRAM_EXTRA))
	{
		address -= BASE_SRAM;
		return(_byteswap_ulong(*((uint32_t *) &sg_u8SRAM[address])));
	}

	BASSERT(0);
	return(0xffff);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
	// Block writes to flash loader
	if ((address >= BASE_FLASH_LOADER) && ((address) < BASE_FLASH_BIOS))
	{
		address -= BASE_FLASH_LOADER;
		return;
	}

	// SRAM?
	if ((address >= BASE_SRAM) && ((address) < BASE_SRAM_EXTRA))
	{
		address -= BASE_SRAM;
		sg_u8SRAM[address] = value;
		return;
	}

	// 8 Bit device mapping
	if ((address >= BASE_8BIT_DEVICES) && (address < BASE_16BIT_DEVICES))
	{
		address -= BASE_8BIT_DEVICES;
		if (0 == (address >> 8))
		{
			// UART A
			address &= 0x7;
			UARTWrite(&sg_sUARTA,
					  address,
					  value);

			return;
		}
		else
		if (1 == (address >> 8))
		{
			// UART B
			address &= 0x7;
			UARTWrite(&sg_sUARTB,
					  address,
					  value);
			return;
		}
		else
		if (2 == (address >> 8))
		{
			// Interrupt controller
			return;
		}
		else
		if (3 == (address >> 8))
		{
			RTCWrite(address,
					 value);
			return;
		}
		else
		if (6 == (address >> 8))
		{
			// Left digit
			Seg7SetStipple(&sg_sDigit1,
						   value);
			return;
		}
		else
		if (7 == (address >> 8))
		{
			// Right digit
			Seg7SetStipple(&sg_sDigit2,
						   value);
			return;
		}
		else
		if (8 == (address >> 8))
		{
			// Status LEDs
			return;
		}


		BASSERT(0);
	}


	BASSERT(0);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	// Block writes to flash loader
	if ((address >= BASE_FLASH_LOADER) && ((address) < BASE_FLASH_BIOS))
	{
		address -= BASE_FLASH_LOADER;
		return;
	}

	// SRAM?
	if ((address >= BASE_SRAM) && ((address) < BASE_SRAM_EXTRA))
	{
		address -= BASE_SRAM;
		*((uint16_t *) &sg_u8SRAM[address]) = _byteswap_ushort(value);
		return;
	}

	// 16 Bit device mapping
	if ((address >= BASE_16BIT_DEVICES) && (address < BASE_32BIT_DEVICES))
	{
		address -= BASE_16BIT_DEVICES;
		BASSERT(0);
	}

	BASSERT(0);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	// Block writes to flash loader
	if ((address >= BASE_FLASH_LOADER) && ((address) < BASE_FLASH_BIOS))
	{
		address -= BASE_FLASH_LOADER;
		return;
	}

	// SRAM?
	if ((address >= BASE_SRAM) && ((address) < BASE_SRAM_EXTRA))
	{
		address -= BASE_SRAM;
		*((uint32_t *) &sg_u8SRAM[address]) = _byteswap_ulong(value);
		return;
	}

	BASSERT(0);
}

static void EmulatorEntry(void)
{
	EStatus eStatus;
	ECPUState eCPUState = ECPU_RESET;
	INT32 s32TimeAccumulator = 0;
	BOOL bHalted = TRUE;
	INT32 s32Delta = 0;
	UINT32 u32ImageUpdateTime = 0;
	FILE *psFile = NULL;

	// 68030 setup
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68030);

	// Set up the UI
	eStatus = EmulatorUISetup();
	BASSERT(ESTATUS_OK == eStatus);

	// Reset it, load up the image
	EmulatorReload();
	EmulatorReset();

	// Try loading up the NVStore stuff
	psFile = fopen("NVStore.bin", "rb");
	if (psFile)
	{
		fread(sg_u8RTCRegs, 1, sizeof(sg_u8RTCRegs), psFile);
		fclose(psFile);
	}
	else
	{
		memset((void *) sg_u8RTCRegs, 0xff, sizeof(sg_u8RTCRegs));
	}

	while (1)
	{
		UINT32 u32TimeSample;

		if (sg_bResetFlagged)
		{
			eCPUState = ECPU_RESET;
			sg_bResetFlagged = FALSE;
		}

		if (sg_bReloadFlagged)
		{
			eCPUState = ECPU_RESET;
			EmulatorReload();
			sg_bReloadFlagged = FALSE;
		}

		if (ECPU_STOP == eCPUState)
		{
			// Don't do anything
			u32TimeSample = 0;
		}
		else
		if (ECPU_RESET == eCPUState)
		{
			// Reset everything
			EmulatorReset();
			eCPUState = ECPU_RUN;
			u32TimeSample = 0;
		}
		else
		if (ECPU_RUN == eCPUState)
		{
			int s32Result;

			s32Result = m68k_execute(CPU_SLICE);
			if (s32Result > 0)
			{
				UARTStep(&sg_sUARTA,
						 s32Result);
				UARTStep(&sg_sUARTB,
						 s32Result);
			}
		}
		else
		{
			//		ECPU_STEP,
			//		ECPU_STEP_OVER,

			BASSERT(0);
		}

		if (sg_bRepaint)
		{
			sg_bRepaint = FALSE;
			VideoRepaint();
			sg_bScreenUpdated = TRUE;
		}

		if (sg_bNVStore)
		{
			psFile = fopen("NVStore.bin", "wb");
			BASSERT(psFile);
			fwrite(sg_u8RTCRegs, 1, sizeof(sg_u8RTCRegs), psFile);
			fclose(psFile);

			sg_bNVStore = FALSE;
		}

		// Update keyboard matrix
		KeyboardUpdate();

		// So we don't wind up with a 3Ghz Z80
		OSSleep(1000 / SLICES_PER_SECOND);
	}
}

int RoscoeEmulatorEntry(char *peCommandLine,
						char **argv,
						int argc)
{
	EStatus eStatus;

	// Initialize shared stuff
	eStatus = SharedInit("Roscoe emulator",
						 sg_sCommands,
						 peCommandLine,
						 0,
						 NULL);
	ERR_GOTO();

	// Init the windowing subsystem
	eStatus = WindowInit();
	ERR_GOTO();

	EmulatorEntry();

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		return(-1);
	}
	else
	{
		return(0);
	}
}
