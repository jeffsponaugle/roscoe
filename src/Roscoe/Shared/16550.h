#ifndef _16550_H_
#define _16550_H_

#ifndef __ASSEMBLER__
#include <stdint.h>
#ifndef _WIN32
#include <stdbool.h>

typedef struct S16550UART
{
	volatile uint8_t u8RBR;	// Receive buffer/transmitter holding register
	volatile uint8_t u8IER;	// Interrupt enable register
	volatile uint8_t u8IIR;	// Interrupt identification register
	volatile uint8_t u8LCR;	// Line control register
	volatile uint8_t u8MCR;	// Modem control register
	volatile uint8_t u8LSR;	// Line status register
	volatile uint8_t u8MSR;	// Modem status register
	volatile uint8_t u8SCR;	// Scratch register
} __attribute__((packed)) S16550UART;
#endif	// #ifndef _WIN32
#endif	// #ifndef __ASSEMBLER__

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

#ifndef __ASSEMBLER__
#ifndef _WIN32

// Programmed I/O functions
extern void SerialSendPIO(S16550UART *psUART,
						  uint8_t *pu8DataToSend,
						  uint32_t u32DataCount);
extern uint8_t SerialReceiveWaitPIO(S16550UART *psUART);
extern bool SerialDataAvailablePIO(S16550UART *psUART);
extern EStatus SerialReceiveWaitTimeoutPIO(S16550UART *psUART,
										   uint8_t *pu8Data,
										   uint32_t u32Timeout);

// Serial control functions
typedef enum
{
	EUART_PARITY_NONE,
	EUART_PARITY_EVEN,
	EUART_PARITY_ODD
} EUARTParity;

extern bool SerialInit(S16550UART *psUART,
					   uint8_t u8DataBits,
					   uint8_t u8StopBits,
					   EUARTParity eParity);

extern void SerialSetBaudRate(S16550UART *psUART,
							  uint32_t u32BaudClock,
							  uint32_t u32BaudRateDesired,
							  uint32_t *pu32BaudRateObtained);

extern void SerialSetOutputs(S16550UART *psUART,
							 bool bDTR,
							 bool bRTS,
							 bool bOUT1,
							 bool bOUT2);

/* Interrupt related */
extern EStatus SerialInterruptInit(void);
extern EStatus SerialInterruptShutdown(void);
extern EStatus SerialInterruptTest(void);
extern EStatus SerialFlush(uint8_t u8SerialPort);
extern EStatus SerialReceiveData(uint8_t u8SerialPort,
								 uint8_t *pu8Buffer,
								 uint16_t u16BytesToReceive,
								 uint16_t *pu16BytesReceived);
extern EStatus SerialReceiveDataAll(uint8_t u8SerialPort,
									uint8_t *pu8Buffer,
									uint16_t u16BytesToReceive,
									uint16_t *pu16BytesReceived,
									uint16_t u16Timer1CountTimeout);
extern EStatus SerialReceiveDataFlush(uint8_t u8SerialPort);
extern EStatus SerialReceiveDataGetCount(uint8_t u8SerialPort,
										 uint16_t *pu16Count);
extern EStatus SerialTransmitData(uint8_t u8SerialPort,
								  uint8_t *pu8Buffer,
								  uint16_t u16BytesToSend,
								  uint16_t *pu16BytesSent);
extern EStatus SerialTransmitDataAll(uint8_t u8SerialPort,
									 uint8_t *pu8Buffer,
									 uint16_t u16BytesToSend,
									 uint16_t *pu16BytesSent);
extern EStatus SerialGetCounters(uint8_t u8SerialPort,
								 uint32_t *pu32InterruptCount,
								 uint32_t *pu32OverrunCount,
								 uint32_t *pu32ParityErrorCount,
								 uint32_t *pu32FramingErrorCount);

#endif // #ifndef _WIN32
#endif // #ifndef __ASSEMBLER__
	
#endif

