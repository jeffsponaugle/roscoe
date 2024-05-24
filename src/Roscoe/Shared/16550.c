#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "BIOS/OS.h"
#include "Shared/16550.h"
#include "Hardware/Roscoe.h"
#include "Shared/Interrupt.h"
#include "Shared/AsmUtils.h"
#include "Shared/Shared.h"
#include "Shared/ptc.h"

// Sends one or more bytes out the serial port via programmed I/O
void SerialSendPIO(S16550UART *psUART,
				   uint8_t *pu8DataToSend,
				   uint32_t u32DataCount)
{
	while (u32DataCount)
	{
		uint8_t u8Chunk;

		while (0 == (psUART->u8LSR & UART_LSR_THRE))
		{
			// Wait for THRE to be empty
		}

		if (u32DataCount > 16)
		{
			u8Chunk = 16;
		}
		else
		{
			u8Chunk = (uint8_t) u32DataCount;
		}

		u32DataCount -= u8Chunk;

		while (u8Chunk)
		{
			psUART->u8RBR = *pu8DataToSend;
			++pu8DataToSend;
			--u8Chunk;
		}
	}
}

// Waits for a byte to become available, then returns it
uint8_t SerialReceiveWaitPIO(S16550UART *psUART)
{
	while (0 == (psUART->u8LSR & UART_LSR_DR))
	{
		// Wait for data to be ready
	}

	return(psUART->u8RBR);
}


// Returns true if data is available
bool SerialDataAvailablePIO(S16550UART *psUART)
{
	if (psUART->u8LSR & UART_LSR_DR)
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// Waits for a character to come in or u16Timeout loops through
EStatus SerialReceiveWaitTimeoutPIO(S16550UART *psUART,
									uint8_t *pu8Data,
									uint32_t u32Timeout)
{
	while (0 == (psUART->u8LSR & UART_LSR_DR))
	{
		u32Timeout--;
		if (0 == u32Timeout)
		{
			return(ESTATUS_TIMEOUT);
		}
	}

	// Got data
	*pu8Data = psUART->u8RBR;
	return(ESTATUS_OK);

}

// Initializes the 16550. true If successful, otherwise false.
bool SerialInit(S16550UART *psUART,
				uint8_t u8DataBits,
				uint8_t u8StopBits,
				EUARTParity eParity)
{
	uint8_t u8LCR = 0;
	bool bResult = false;

	// Let's see if the UART is there. First check the scratch register
	psUART->u8SCR = 0xa5;
	if (psUART->u8SCR != 0xa5)
	{
		goto errorExit;
	}

	psUART->u8SCR = 0x5a;
	if (psUART->u8SCR != 0x5a)
	{
		goto errorExit;
	}

	// Clear the IER (shut off all interrupts)
	psUART->u8IER = 0;

	// Scratch register appears to be there. Set the IIR to 0 - it should return
	// UART_IIR_NOT_PENDING
	psUART->u8IIR = 0;
	if (psUART->u8IIR != UART_IIR_NOT_PENDING)
	{
		goto errorExit;
	}

	// Now we set up a line crontol register variable
	if ((u8DataBits < 5) || (u8DataBits > 8))
	{
		// Only supports 5-8 data bits
		goto errorExit;
	}

	// 5-8 data bits is 0-3 in the UART, lower 2 bits
	u8LCR = (u8DataBits - 5);

	// Stop bits are either 1 or 2
	if ((u8StopBits != 1) && (u8StopBits != 2))
	{
		// Only supports 1 or 2 stop bits
		goto errorExit;
	}

	u8StopBits--;
	u8LCR |= (u8StopBits << 2);

	if (EUART_PARITY_NONE == eParity)
	{
		u8LCR |= UART_LCR_NO_PARITY;
	}
	else
	if (EUART_PARITY_EVEN == eParity)
	{
		u8LCR |= UART_LCR_EVEN_PARITY;
	}
	else
	if (EUART_PARITY_NONE == eParity)
	{
		u8LCR |= UART_LCR_ODD_PARITY;
	}
	else
	{
		// Bad parity setting
		goto errorExit;
	}

	psUART->u8LCR = u8LCR;

	// Turn on UART FIFOs
	psUART->u8IIR = UART_IIR_FIFO_8 | UART_IIR_FIFO_ENABLE;

	bResult = true;

	// All good

errorExit:
	return(bResult);
}

// Sets the 16550's baud rate
void SerialSetBaudRate(S16550UART *psUART,
					   uint32_t u32BaudClock,
					   uint32_t u32BaudRateDesired,
					   uint32_t *pu32BaudRateObtained)
{
	uint16_t u16Divisor;
	uint8_t u8LCR;

	u16Divisor = (uint16_t) ((u32BaudClock >> 4) / u32BaudRateDesired);

	if (pu32BaudRateObtained)
	{
		*pu32BaudRateObtained = ((u32BaudClock >> 4) / u16Divisor);
	}

	// Read in the line control register's value and OR in DLAB for
	// the baud rate divisor latch access
	psUART->u8LCR = psUART->u8LCR | UART_LCR_DLAB_ENABLE;

	// Set LSB of the divisor
	psUART->u8RBR = (uint8_t) u16Divisor;

	// And the MSB
	psUART->u8IER = (uint8_t) (u16Divisor >> 8);

	// Now clear DLAB
	psUART->u8LCR = psUART->u8LCR & ~UART_LCR_DLAB_ENABLE;
}

// Sets the DTR/RTS/OUT1/OUT2 pins
void SerialSetOutputs(S16550UART *psUART,
 					  bool bDTR,
 					  bool bRTS,
 					  bool bOUT1,
					  bool bOUT2)
{
	uint8_t u8MCR = 0;

	if (bDTR)
	{
		u8MCR |= UART_MCR_DTR;
	}

	if (bRTS)
	{
		u8MCR |= UART_MCR_RTS;
	}

	if (bOUT1)
	{
		u8MCR |= UART_MCR_OUT1;
	}

	if (bOUT2)
	{
		u8MCR |= UART_MCR_OUT2;
	}

	psUART->u8MCR = u8MCR;
}

/******************** EVERYTHING FROM HERE DOWN IS INTERRUPT BASED ***************/

// Sizes (in bytes) of the serial transmit and receive buffers
#define	SERIAL_TX_BUFFER_SIZE		4096
#define	SERIAL_RX_BUFFER_SIZE		4096

// Total # of serial ports in this system
#define	SERIAL_COUNT				2

// Structure for handling a serial port
typedef struct SSerialPort
{
	S16550UART *psUART;						// Pointer to UART hardware
	uint32_t u32InterruptCount;				// How many times has this UART caused an interrupt?

	// Transmit buffer
	uint8_t u8TXBuffer[SERIAL_TX_BUFFER_SIZE];
	volatile uint16_t u16TXHead;			// Head/tail pointers
	volatile uint16_t u16TXTail;
	volatile bool bTransmitting;			// true If transmitting, false if not

	// Receive buffer
	uint8_t u8RXBuffer[SERIAL_RX_BUFFER_SIZE];
	volatile uint16_t u16RXHead;			// Head/tail pointers
	volatile uint16_t u16RXTail;

	// Line status error counters
	volatile uint32_t u32OverrunCount;		// # Of FIFO overruns
	volatile uint32_t u32ParityErrorCount;	// # Of parity errors
	volatile uint32_t u32FramingErrorCount;	// # Of framing errors
} SSerialPort;

// Serial port data
static SSerialPort sg_sSerialPortData[SERIAL_COUNT];

// Generic interrupt handler for interrupt 
static void SerialInterruptHandler(SSerialPort *psPort)
{
	S16550UART *psUART;

	psUART = psPort->psUART;
	psPort->u32InterruptCount++;

	// Sit in a loop and keep servicing the UART until it's done
	for (;;)
	{
		uint8_t u8IIR;

		u8IIR = psUART->u8IIR & UART_IIR_INT_MASK;
		if ((UART_IIR_RBR_INT == u8IIR) ||
			(UART_IIR_FIFOTO_INT == u8IIR))
		{
			// Received data - loop until the FIFO is clear.
			while (psUART->u8LSR & UART_LSR_DR)
			{
				psPort->u8RXBuffer[psPort->u16RXHead++] = psUART->u8RBR;
				if (psPort->u16RXHead >= sizeof(psPort->u8RXBuffer))
				{
					psPort->u16RXHead = 0;
				}

				// Check to see if we've filled up the buffer and overwrite the old data if so
				if (psPort->u16RXHead == psPort->u16RXTail)
				{
					psPort->u16RXTail++;
					if (psPort->u16RXTail >= sizeof(psPort->u8RXBuffer))
					{
						psPort->u16RXTail = 0;
					}
				}
			}
		}
		else
		if (UART_IIR_THRE_INT == u8IIR)
		{
			// If we have nothing left, indicate we're not transmitting any longer
			if (psPort->u16TXHead == psPort->u16TXTail)
			{
				psPort->bTransmitting = false;
			}
			else
			{
				uint8_t u8Count = 16;	// Up to 16 bytes to send

				// Transmit data - either until the FIFO is full or we're out of data to transmit
				while (u8Count && (psPort->u16TXHead != psPort->u16TXTail))
				{
					psUART->u8RBR = psPort->u8TXBuffer[psPort->u16TXTail++];
					if (psPort->u16TXTail >= sizeof(psPort->u8TXBuffer))
					{
						psPort->u16TXTail = 0;
					}
					u8Count--;
				}
			}
		}
		else
		if (UART_IIR_LSR_INT == u8IIR)
		{
			uint8_t u8LSR;

			// Line status register changed
			u8LSR = psUART->u8LSR;

			if (u8LSR & UART_LSR_OE)
			{
				// Overrun
				psPort->u32OverrunCount++;
			}

			if (u8LSR & UART_LSR_PE)
			{
				// Parity error
				psPort->u32ParityErrorCount++;
			}

			if (u8LSR & UART_LSR_FE)
			{
				// Framing error
				psPort->u32FramingErrorCount++;
			}
		}
		else
		if (UART_IIR_NOT_PENDING == u8IIR)
		{
			// Nothing left
			break;
		}
		else
		if (UART_IIR_MSR_INT == u8IIR)
		{
			uint8_t u8MSR;

			// Read modem status register
			u8MSR = psUART->u8MSR;
		}
		else
		{
			// We shouldn't be getting here
			POST_SET(POSTFAULT_UART_ISR);
			while (1);
		}
	}
}

// UART A Interrupt handler
static __attribute__ ((interrupt)) void SerialUARTAHandler(void) 
{
	SerialInterruptHandler(&sg_sSerialPortData[0]);
}

// UART B Interrupt handler
static __attribute__ ((interrupt)) void SerialUARTBHandler(void) 
{
	SerialInterruptHandler(&sg_sSerialPortData[1]);
}

// Receive data from the appropriate serial port
EStatus SerialReceiveData(uint8_t u8SerialPort,
						  uint8_t *pu8Buffer,
						  uint16_t u16BytesToReceive,
						  uint16_t *pu16BytesReceived)
{
	EStatus eStatus = ESTATUS_OK;
	SSerialPort *psPort;

	if (u8SerialPort >= (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
	{
		eStatus = ESTATUS_SERIAL_OUT_OF_RANGE;
		goto errorExit;
	}

	psPort = &sg_sSerialPortData[u8SerialPort];

	if (pu16BytesReceived)
	{
		*pu16BytesReceived = 0;
	}

	// Pull in data until we don't have any more
	while ((psPort->u16RXTail != psPort->u16RXHead) &&
		   (u16BytesToReceive))
	{
		*pu8Buffer = psPort->u8RXBuffer[psPort->u16RXTail++];
		if (psPort->u16RXTail >= sizeof(psPort->u8RXBuffer))
		{
			psPort->u16RXTail = 0;
		}

		u16BytesToReceive--;
		if (pu16BytesReceived)
		{
			(*pu16BytesReceived)++;
		}

		pu8Buffer++;
	}

errorExit:
	return(eStatus);
}

// Just like SerialReceiveData() but it keeps going until everything has been received
EStatus SerialReceiveDataAll(uint8_t u8SerialPort,
							 uint8_t *pu8Buffer,
							 uint16_t u16BytesToReceive,
							 uint16_t *pu16BytesReceived,
							 uint16_t u16Timer1CountTimeout)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32CounterBase = 0;

	if (pu16BytesReceived)
	{
		*pu16BytesReceived = 0;
	}

	if (u16Timer1CountTimeout)
	{
		eStatus = PTCGetInterruptCounter(1,
										 &u32CounterBase);
		ERR_GOTO();
	}

	while (u16BytesToReceive)
	{
		uint16_t u16ReceiveDataCount = 0;
		uint32_t u32InterruptCount;

		eStatus = SerialReceiveData(u8SerialPort,
									pu8Buffer,
									u16BytesToReceive,
									&u16ReceiveDataCount);
		ERR_GOTO();

		eStatus = PTCGetInterruptCounter(1,
										 &u32InterruptCount);

		if (u16ReceiveDataCount)
		{
			pu8Buffer += u16ReceiveDataCount;
			u16BytesToReceive -= u16ReceiveDataCount;
			if (pu16BytesReceived)
			{
				*pu16BytesReceived += u16ReceiveDataCount;
			}

			u32CounterBase = u32InterruptCount;
		}
		else
		if (u16Timer1CountTimeout)
		{
			// We didn't hear anything. Let's see if we've expired
			if ((u32InterruptCount - u32CounterBase) >= u16Timer1CountTimeout)
			{
				eStatus = ESTATUS_TIMEOUT;
				goto errorExit;
			}
		}
	}

errorExit:
	return(eStatus);
}
// Transmit up to u16BytesToSend (if there's room)
EStatus SerialTransmitData(uint8_t u8SerialPort,
						   uint8_t *pu8Buffer,
						   uint16_t u16BytesToSend,
						   uint16_t *pu16BytesSent)
{
	EStatus eStatus = ESTATUS_OK;
	SSerialPort *psPort;
	uint16_t u16TXHeadNew;

	if (u8SerialPort >= (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
	{
		eStatus = ESTATUS_SERIAL_OUT_OF_RANGE;
		goto errorExit;
	}

	psPort = &sg_sSerialPortData[u8SerialPort];
	if (pu16BytesSent)
	{
		*pu16BytesSent = 0;
	}

	// Figure out if there's room in the buffer
	u16TXHeadNew = psPort->u16TXHead + 1;
	if (u16TXHeadNew >= sizeof(psPort->u8TXBuffer))
	{
		u16TXHeadNew = 0;
	}

	// Stuff as much as we can in the outgoing buffer until the buffer
	// is full or we have no more data to send
	while ((u16TXHeadNew != psPort->u16TXTail) &&
		   (u16BytesToSend))
	{
		psPort->u8TXBuffer[psPort->u16TXHead] = *pu8Buffer;
		++pu8Buffer;
		if (pu16BytesSent)
		{
			(*pu16BytesSent)++;
		}
		psPort->u16TXHead = u16TXHeadNew;

		u16TXHeadNew = psPort->u16TXHead + 1;
		if (u16TXHeadNew >= sizeof(psPort->u8TXBuffer))
		{
			u16TXHeadNew = 0;
		}

		--u16BytesToSend;
	}

	// Shut off interrupts temporarily, and see if we're transmitting. If we're
	// transmitting, there's nothing to do. The ISR will take care of it.
	InterruptDisable();
	if (psPort->bTransmitting)
	{
		// Nothing to do
	}
	else
	{
		// Prime the transmitter
		uint8_t u8Count = 16;	// Up to 16 bytes to send

		// Transmit data - either until the FIFO is full or we're out of data to transmit
		while (u8Count && (psPort->u16TXHead != psPort->u16TXTail))
		{
			psPort->psUART->u8RBR = psPort->u8TXBuffer[psPort->u16TXTail++];
			if (psPort->u16TXTail >= sizeof(psPort->u8TXBuffer))
			{
				psPort->u16TXTail = 0;
			}
			u8Count--;
		}

		// Set the transmitting boolean
		psPort->bTransmitting = true;
	}

	InterruptEnable();

errorExit:
	return(eStatus);
}

// Just like SerialTransmitData() but it keeps going until everything has been
// queued up.
EStatus SerialTransmitDataAll(uint8_t u8SerialPort,
							  uint8_t *pu8Buffer,
							  uint16_t u16BytesToSend,
							  uint16_t *pu16BytesSent)
{
	EStatus eStatus = ESTATUS_OK;

	if (pu16BytesSent)
	{
		*pu16BytesSent = 0;
	}

	while (u16BytesToSend)
	{
		EStatus eStatus;
		uint16_t u16SentDataCount = 0;

		eStatus = SerialTransmitData(u8SerialPort,
									 pu8Buffer,
									 u16BytesToSend,
									 &u16SentDataCount);
		ERR_GOTO();

		pu8Buffer += u16SentDataCount;
		u16BytesToSend -= u16SentDataCount;
		if (pu16BytesSent)
		{
			*pu16BytesSent += u16SentDataCount;
		}
	}

errorExit:
	return(eStatus);
}

EStatus SerialFlush(uint8_t u8SerialPort)
{
	EStatus eStatus = ESTATUS_OK;
	SSerialPort *psPort;

	if (u8SerialPort >= (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
	{
		eStatus = ESTATUS_SERIAL_OUT_OF_RANGE;
		goto errorExit;
	}

	// Loop forever until we're no longer transmitting
	while (sg_sSerialPortData[u8SerialPort].bTransmitting);

	// Now wait for assertion of transmitter holding register empty to ensure
	// the FIFOs have been flushed.
	while (0 == (sg_sSerialPortData[u8SerialPort].psUART->u8LSR & UART_LSR_THRE))
	{
		// Wait for THRE to be empty
	}

errorExit:
	return(eStatus);
}

EStatus SerialGetCounters(uint8_t u8SerialPort,
						  uint32_t *pu32InterruptCount,
						  uint32_t *pu32OverrunCount,
						  uint32_t *pu32ParityErrorCount,
						  uint32_t *pu32FramingErrorCount)
{
	EStatus eStatus = ESTATUS_OK;

	if (u8SerialPort >= (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
	{
		eStatus = ESTATUS_SERIAL_OUT_OF_RANGE;
		goto errorExit;
	}

	if (pu32InterruptCount)
	{
		*pu32InterruptCount = sg_sSerialPortData[u8SerialPort].u32InterruptCount;
	}

	if (pu32OverrunCount)
	{
		*pu32OverrunCount = sg_sSerialPortData[u8SerialPort].u32OverrunCount;
	}

	if (pu32ParityErrorCount)
	{
		*pu32ParityErrorCount = sg_sSerialPortData[u8SerialPort].u32ParityErrorCount;
	}

	if (pu32FramingErrorCount)
	{
		*pu32FramingErrorCount = sg_sSerialPortData[u8SerialPort].u32FramingErrorCount;
	}

errorExit:
	return(eStatus);
}

EStatus SerialReceiveDataFlush(uint8_t u8SerialPort)
{
	EStatus eStatus = ESTATUS_OK;

	if (u8SerialPort >= (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
	{
		eStatus = ESTATUS_SERIAL_OUT_OF_RANGE;
		goto errorExit;
	}

	sg_sSerialPortData[u8SerialPort].u16RXHead = sg_sSerialPortData[u8SerialPort].u16RXTail;

errorExit:
	return(eStatus);
}

EStatus SerialReceiveDataGetCount(uint8_t u8SerialPort,
								  uint16_t *pu16Count)
{
	EStatus eStatus = ESTATUS_OK;
	uint16_t u16RXHead;
	uint16_t u16RXTail;
	int32_t s32Count;

	if (u8SerialPort >= (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
	{
		eStatus = ESTATUS_SERIAL_OUT_OF_RANGE;
		goto errorExit;
	}

	InterruptDisable();
	u16RXHead = sg_sSerialPortData[u8SerialPort].u16RXHead;
	u16RXTail = sg_sSerialPortData[u8SerialPort].u16RXTail;
	InterruptEnable();

	s32Count = ((int32_t) u16RXHead) - ((int32_t) u16RXTail);
	if (s32Count < 0)
	{
		s32Count += sizeof(sg_sSerialPortData[u8SerialPort].u8RXBuffer);
	}

	if (pu16Count)
	{
		*pu16Count = (uint16_t) s32Count;
	}

errorExit:
	return(eStatus);
}

EStatus SerialInterruptInit(void)
{
	EStatus eStatus = ESTATUS_OK;

	// Clear out the serial port data structure
	memset((void *) sg_sSerialPortData, 0, sizeof(sg_sSerialPortData));

	// UART A
	sg_sSerialPortData[0].psUART = (S16550UART *) ROSCOE_UART_A;
	SerialSetBaudRate(sg_sSerialPortData[0].psUART,
					  UART_BAUD_CLOCK,
					  BOOTLOADER_BAUD_RATE,
					  NULL);

	sg_sSerialPortData[0].psUART->u8IIR = UART_IIR_FIFO_8 | UART_IIR_FIFO_ENABLE;
	sg_sSerialPortData[0].psUART->u8IER = UART_IER_ERBFI | UART_IER_ETBEI | UART_IER_ELSI;

	// UART B
	sg_sSerialPortData[1].psUART = (S16550UART *) ROSCOE_UART_B;
	SerialSetBaudRate(sg_sSerialPortData[1].psUART,
					  UART_BAUD_CLOCK,
					  BOOTLOADER_BAUD_RATE,
					  NULL);
	sg_sSerialPortData[1].psUART->u8IIR = UART_IIR_FIFO_8 | UART_IIR_FIFO_ENABLE;
	sg_sSerialPortData[1].psUART->u8IER = UART_IER_ERBFI | UART_IER_ETBEI | UART_IER_ELSI;

	// Hook up the interrupt vectors for each serial port
	eStatus = InterruptHook(INTVECT_IRQ4A_UART1,
							SerialUARTAHandler);
	ERR_GOTO();
	eStatus = InterruptHook(INTVECT_IRQ4B_UART2,
							SerialUARTBHandler);
	ERR_GOTO();

	// Now enable their interrupts
	eStatus = InterruptMaskSet(INTVECT_IRQ4A_UART1,
							   false);
	ERR_GOTO();
	eStatus = InterruptMaskSet(INTVECT_IRQ4B_UART2,
							   false);
	ERR_GOTO();

errorExit:
	return(eStatus);
}

EStatus SerialInterruptShutdown(void)
{
	EStatus eStatus = ESTATUS_OK;

	// Shut off interrupts from both UARTs
	eStatus = InterruptMaskSet(INTVECT_IRQ4A_UART1,
							   true);
	ERR_GOTO();
	eStatus = InterruptMaskSet(INTVECT_IRQ4B_UART2,
							   true);
	ERR_GOTO();

errorExit:
	return(eStatus);
}


EStatus SerialInterruptTest(void)
{
	EStatus eStatus;
	uint32_t u32Loop;

	// Make sure no UART interrupts are occurring
	(void) SerialInterruptShutdown();

	// This will clear the serial port structures, including the counters
	eStatus = SerialInterruptInit();
	ERR_GOTO();

	// Wait for interrupts to come in from the UARTs - should be THRE
	u32Loop = 1000000;
	while (u32Loop)
	{
		uint8_t u8Loop;
		uint8_t u8Count;

		// Get the interrupt
		u8Count = 0;
		u8Loop = 0;
		while (u8Loop < (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
		{
			if (sg_sSerialPortData[u8Loop].u32InterruptCount)
			{
				++u8Count;
			}

			++u8Loop;
		}

		// If this is true, then both UARTs have generated interrupts
		if (u8Count == (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
		{
			break;
		}

		u32Loop--;
	}

	// If u32Loop is 0 then we've seen no interrupts from the UARTs
	if (0 == u32Loop)
	{
		uint8_t u8Loop;

		printf("UART Interrupts nonfunctional: ");

		u8Loop = 0;
		while (u8Loop < (sizeof(sg_sSerialPortData) / sizeof(sg_sSerialPortData[0])))
		{
			if (0 == sg_sSerialPortData[u8Loop].u32InterruptCount)
			{
				printf("%c ", u8Loop + 'A');
			}

			u8Loop++;
		}

		printf("\n");
		eStatus = ESTATUS_SERIAL_INTERRUPT_NONFUNCTIONAL;
		goto errorExit2;
	}


errorExit:
	if (eStatus != ESTATUS_OK)
	{
		printf("Failed - %s\n", GetErrorText(eStatus));
	}

errorExit2:

	// Stop the UARTs' interrupts
	(void) SerialInterruptShutdown();

	return(eStatus);
}

