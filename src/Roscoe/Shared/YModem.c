#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "Shared/Shared.h"
#include "BIOS/OS.h"
#include "Hardware/Roscoe.h"
#include "Shared/YModem.h"
#include "Shared/16550.h"
#include "Shared/Stream.h"
#include "Shared/ptc.h"
#include "Shared/Interrupt.h"

// Communication standard characters
#define	COMM_SOH			0x01
#define	COMM_STX			0x02
#define	COMM_EOT			0x04
#define	COMM_ACK			0x06
#define	COMM_NAK			0x15
#define	COMM_CAN			0x18

// Default timeout time (in milliseconds) 
#define	YMODEM_TIMEOUT_DEFAULT	1000

// Which timer are we using to time things out?
#define	YMODEM_TIMER_CHANNEL	0

// What we send back when we're completely done
static const uint8_t sg_u8EndOfTransmission[] = {COMM_ACK, 'C', COMM_ACK};

// Sequence for canceling everything
static const uint8_t sg_u8Cancel[] = {COMM_CAN, COMM_CAN};

static const char *sg_peStates[] =
{
	"YMODEM_STATE_UNKNOWN",
	"YMODEM_STATE_INIT",
	"YMODEM_STATE_WAIT_FOR_BLOCK",
	"YMODEM_STATE_SEND_RESPONSE",
	"YMODEM_STATE_BLOCK_NUMBER",
	"YMODEM_STATE_RX_DATA",
	"YMODEM_STATE_RX_CRC_CHECKSUM",
	"YMODEM_STATE_BLOCK_CHECK",
	"YMODEM_STATE_CANCEL"
};

// # Of ticks for a timeout (converted YMODEM_TIMEOUT_DEFAULT to system ticks)
static uint32_t sg_u32TimeoutTicks;

// Calculate YModem's CRC16
static uint16_t CRC16(uint16_t u16CRC,
					  uint8_t *pu8Data, 
					  uint16_t u16Count)
{
	uint8_t u8Mask;

    while (u16Count)
    {
        u16CRC = u16CRC ^ (((uint16_t) *pu8Data) << 8);
        u8Mask = 0x80;

		while (u8Mask)
        {
            if (u16CRC & 0x8000)
			{
                u16CRC = (u16CRC << 1) ^ 0x1021;
			}
            else
			{
                u16CRC <<= 1;
			}

			u8Mask >>= 1;
        }

		pu8Data++;
		u16Count--;
    }

    return(u16CRC);
}

static EYModemState sg_eLastState = 9999;

// This is the entire YModem transfer/state machine
EStatus YModemPump(SYModem *psYModem)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32InterruptTimerCount;
	uint8_t u8Data;
	uint16_t u16BytesReceived;

	for (;;)
	{
		if (sg_eLastState != psYModem->eState)
		{
//			printf("%s\n", sg_peStates[psYModem->eState]);
			sg_eLastState = psYModem->eState;
		}
		eStatus = PTCGetInterruptCounter(YMODEM_TIMER_CHANNEL,
										 &u32InterruptTimerCount);
		ERR_GOTO();

		switch (psYModem->eState)
		{
			case YMODEM_STATE_INIT:
			{
				psYModem->u8NextBlockNumber = 0;
				psYModem->u32Offset = 0;
				psYModem->eState = YMODEM_STATE_SEND_RESPONSE;
				psYModem->u32LastTimerInterrupt = u32InterruptTimerCount;
				psYModem->u8LastResponse = 'C';
				psYModem->bZeroBlockReceived = false;
				break;
			}

			case YMODEM_STATE_WAIT_FOR_BLOCK:
			{
				eStatus = SerialReceiveData(psYModem->u8SerialIndex,
											&u8Data,
											sizeof(u8Data),
											&u16BytesReceived);
				if ((ESTATUS_OK == eStatus) && (u16BytesReceived == sizeof(u8Data)))
				{
					// We got something!
					psYModem->u32LastTimerInterrupt = u32InterruptTimerCount;

					if (COMM_SOH == u8Data)
					{
						// 128 Byte block
						psYModem->u16BlockOffset = 0;
						psYModem->u16BlockSize = 128;
						psYModem->eState = YMODEM_STATE_BLOCK_NUMBER;
//						printf("128 Byte block\n");
					}
					else
					if (COMM_STX == u8Data)
					{
						// 1K Block
						psYModem->u16BlockOffset = 0;
						psYModem->u16BlockSize = 1024;
						psYModem->eState = YMODEM_STATE_BLOCK_NUMBER;
//						printf("1K Block\n");
					}
					else
					if (COMM_EOT == u8Data)
					{
						// Go back to the init state
						psYModem->eState = YMODEM_STATE_INIT;

						(void) SerialTransmitDataAll(psYModem->u8SerialIndex,
													 (uint8_t *) sg_u8EndOfTransmission,
													 sizeof(sg_u8EndOfTransmission),
													 NULL);

						// Let the data callback know that it's done
						if (psYModem->DataCallback)
						{
							(void )psYModem->DataCallback(ESTATUS_TRANSFER_COMPLETE,
														  psYModem->u32Offset,
														  NULL,
														  0,
														  psYModem->pvUserData);
						}
					}
					else
					{
						// No idea what this is - ignore it
						eStatus = ESTATUS_OK;
						goto errorExit;
					}
				}
				else
				{
					// See if it's time for us to send
					if ((u32InterruptTimerCount - psYModem->u32LastTimerInterrupt) >= sg_u32TimeoutTicks)
					{
						// We've timed out
						psYModem->u32LastTimerInterrupt = u32InterruptTimerCount;

						// Indicate we want CRC
						psYModem->u8LastResponse = 'C';
						psYModem->eState = YMODEM_STATE_SEND_RESPONSE;

						// Only let the caller know it's a timeout if we have a transfer in progress
						if (psYModem->u32Offset)
						{
							if (psYModem->DataCallback)
							{
								(void) psYModem->DataCallback(ESTATUS_TIMEOUT,
															  psYModem->u32Offset,
															  NULL,
															  0,
															  psYModem->pvUserData);
							}
						}

						// Back to the beginning
						psYModem->eState = YMODEM_STATE_INIT;

//						printf("Timeout\n");
					}
					else
					{
						eStatus = ESTATUS_OK;
						goto errorExit;
					}
				}

				break;
			}

			case YMODEM_STATE_SEND_RESPONSE:
			{
				(void) SerialTransmitDataAll(psYModem->u8SerialIndex,
											 &psYModem->u8LastResponse,
											 sizeof(psYModem->u8LastResponse),
											 NULL);
				psYModem->eState = YMODEM_STATE_WAIT_FOR_BLOCK;
				break;
			}

			case YMODEM_STATE_BLOCK_NUMBER:
			{
				eStatus = SerialReceiveData(psYModem->u8SerialIndex,
											&u8Data,
											sizeof(u8Data),
											&u16BytesReceived);
				if (ESTATUS_OK == eStatus)
				{
					if (0 == u16BytesReceived)
					{
						// No data received - check for timeouts
						// See if we've timed out
						if ((u32InterruptTimerCount - psYModem->u32LastTimerInterrupt) >= sg_u32TimeoutTicks)
						{
							// We've timed out. Start over again
							psYModem->eState = YMODEM_STATE_INIT;

							if (psYModem->DataCallback)
							{
								(void) psYModem->DataCallback(ESTATUS_TIMEOUT,
															  psYModem->u32Offset,
															  NULL,
															  0,
															  psYModem->pvUserData);
							}
						}
						else
						{
							goto errorExit;
						}
					}
					else
					{
						// We got something
						psYModem->u32LastTimerInterrupt = u32InterruptTimerCount;

						if (0 == psYModem->u16BlockOffset)
						{
							// If we have a matching block #, then we advance
							if (u8Data == psYModem->u8NextBlockNumber)
							{
//								printf("Block # 0x%.2x\n", u8Data);
								psYModem->u16BlockOffset++;
							}
							else
							{
								// Back to the beginning
								psYModem->eState = YMODEM_STATE_INIT;
							}
						}
						else
						if (1 == psYModem->u16BlockOffset)
						{
							// If we have a matching inverted block #, then we advance
							if (u8Data == (uint8_t) (~psYModem->u8NextBlockNumber))
							{
								psYModem->eState = YMODEM_STATE_RX_DATA;
								psYModem->u16BlockOffset = 0;
								psYModem->u16CRCRX = 0;
								psYModem->u16CRCCalc = 0;
							}
							else
							{
								// Back to the beginning
								psYModem->eState = YMODEM_STATE_INIT;
								if (psYModem->DataCallback)
								{
									(void) psYModem->DataCallback(ESTATUS_TIMEOUT,
																  psYModem->u32Offset,
																  NULL,
																  0,
																  psYModem->pvUserData);
								}
							}
						}
						else
						{
							assert(0);
						}
					}
				}
				else
				{
					goto errorExit;
				}

				break;
			}

			case YMODEM_STATE_RX_DATA:
			{
				eStatus = SerialReceiveData(psYModem->u8SerialIndex,
											&psYModem->u8Buffer[psYModem->u16BlockOffset],
											psYModem->u16BlockSize - psYModem->u16BlockOffset,
											&u16BytesReceived);
				if (ESTATUS_OK == eStatus)
				{
					if (0 == u16BytesReceived)
					{
						// No data received - check for timeouts
						if ((u32InterruptTimerCount - psYModem->u32LastTimerInterrupt) >= sg_u32TimeoutTicks)
						{
							// We've timed out. Start over again
							psYModem->eState = YMODEM_STATE_INIT;
							if (psYModem->DataCallback)
							{
								(void) psYModem->DataCallback(ESTATUS_TIMEOUT,
															  psYModem->u32Offset,
															  NULL,
															  0,
															  psYModem->pvUserData);
							}
						}
						else
						{
							goto errorExit;
						}
					}
					else
					{
						// We got some data.

						psYModem->u16CRCCalc = CRC16(psYModem->u16CRCCalc,
													 &psYModem->u8Buffer[psYModem->u16BlockOffset],
													 u16BytesReceived);

						psYModem->u32LastTimerInterrupt = u32InterruptTimerCount;
						psYModem->u16BlockOffset += u16BytesReceived;

						if (psYModem->u16BlockOffset == psYModem->u16BlockSize)
						{
							// Got it!
							psYModem->u16BlockOffset = 0;
							psYModem->eState = YMODEM_STATE_RX_CRC_CHECKSUM;
						}
						else
						{
							// More to do!
						}
					}
				}
				else
				{
					goto errorExit;
				}

				break;
			}

			case YMODEM_STATE_RX_CRC_CHECKSUM:
			{
				eStatus = SerialReceiveData(psYModem->u8SerialIndex,
											&u8Data,
											sizeof(u8Data),
											&u16BytesReceived);
				if (ESTATUS_OK == eStatus)
				{
					if (0 == u16BytesReceived)
					{
						// No data received - check for timeouts
						if ((u32InterruptTimerCount - psYModem->u32LastTimerInterrupt) >= sg_u32TimeoutTicks)
						{
							// We've timed out. Start over again
							psYModem->eState = YMODEM_STATE_INIT;
							if (psYModem->DataCallback)
							{
								(void) psYModem->DataCallback(ESTATUS_TIMEOUT,
															  psYModem->u32Offset,
															  NULL,
															  0,
															  psYModem->pvUserData);
							}
						}
						else
						{
							goto errorExit;
						}
					}
					else
					{
						// We got some data.
						psYModem->u32LastTimerInterrupt = u32InterruptTimerCount;

						// If 0th byte, then it's either the MSB of the CRC16 or the checksum
						if (0 == psYModem->u16BlockOffset)
						{
							psYModem->u16CRCRX = ((uint16_t) u8Data << 8);
							++psYModem->u16BlockOffset;
						}
						else
						if (1 == psYModem->u16BlockOffset)
						{
							psYModem->u16CRCRX |= u8Data;
							psYModem->eState = YMODEM_STATE_BLOCK_CHECK;
						}
						else
						{
							// Shouldn't have gotten here.
						}
					}
				}
				else
				{
					goto errorExit;
				}

				break;
			}

			case YMODEM_STATE_BLOCK_CHECK:
			{
				psYModem->eState = YMODEM_STATE_SEND_RESPONSE;

				if (psYModem->u16CRCCalc == psYModem->u16CRCRX)
				{
					psYModem->u8LastResponse = COMM_ACK;

					// Next block!
					if ((false == psYModem->bZeroBlockReceived) &&
						(0 == psYModem->u8NextBlockNumber))
					{
						// If it's block 0, then we ignore it (it's just the filename)
						psYModem->u8LastResponse = COMM_ACK;

						// Send an ACK, then a C
						(void) SerialTransmitDataAll(psYModem->u8SerialIndex,
													 &psYModem->u8LastResponse,
													 sizeof(psYModem->u8LastResponse),
													 NULL);

						psYModem->u8LastResponse = 'C';
						psYModem->bZeroBlockReceived = true;
					}
					else
					{
						// This is cheating, but it sends the acknowledement back to the remote end
						// while the data is being processed. This allows things to overlap
						psYModem->eState = YMODEM_STATE_WAIT_FOR_BLOCK;

						(void) SerialTransmitDataAll(psYModem->u8SerialIndex,
													 &psYModem->u8LastResponse,
													 sizeof(psYModem->u8LastResponse),
													 NULL);

						// Tell the consumer about it
						if (psYModem->DataCallback)
						{
							eStatus = psYModem->DataCallback(ESTATUS_OK,
															 psYModem->u32Offset,
															 psYModem->u8Buffer,
															 psYModem->u16BlockSize,
															 psYModem->pvUserData);
							if (eStatus != ESTATUS_OK)
							{
								if (ESTATUS_TRANSFER_COMPLETE == eStatus)
								{
									// Special case - cancel the transfer since the target is aborting
									// it early and gracefully. Send a CAN back to the sender

									(void) SerialTransmitDataAll(psYModem->u8SerialIndex,
																 (uint8_t *) sg_u8Cancel,
																 sizeof(sg_u8Cancel),
																 NULL);

									psYModem->eState = YMODEM_STATE_INIT;
								}
								else
								{
									// Cancel things
									psYModem->eState = YMODEM_STATE_CANCEL;
								}

								goto errorExit;
							}
						}

						if (psYModem->eState != YMODEM_STATE_CANCEL)
						{
							psYModem->u32Offset += psYModem->u16BlockSize;
						}
					}

					psYModem->u8NextBlockNumber++;
				}
				else
				{
					// NAK!
					printf("CRC Fault - expected 0x%.4x, got 0x%.4x\n", psYModem->u16CRCCalc, psYModem->u16CRCRX);
					psYModem->u8LastResponse = COMM_NAK;
				}

				break;
			}

			case YMODEM_STATE_CANCEL:
			{
				// Cancel things
				(void) SerialTransmitDataAll(psYModem->u8SerialIndex,
											 (uint8_t *) sg_u8Cancel,
											 sizeof(sg_u8Cancel),
											 NULL);

				// Let the data callback know
				if (psYModem->DataCallback)
				{
					(void )psYModem->DataCallback(ESTATUS_CANCELED,
												  psYModem->u32Offset,
												  psYModem->u8Buffer,
												  psYModem->u16BlockSize,
												  psYModem->pvUserData);
				}

				// Reinit everything
				psYModem->eState = YMODEM_STATE_INIT;
				break;
			}

			default:
		   	{
				eStatus = ESTATUS_FUNCTION_NOT_SUPPORTED;
				goto errorExit;
			}
		}
	}

errorExit:
	return(eStatus);
}

// Init a YModem state machine
EStatus YModemInit(SYModem *psYModem,
				   uint8_t u8SerialIndex,
				   uint32_t u32BaudRate,
				   void *pvUserData,
				   EStatus (*DataCallback)(EStatus eStatus,
										   uint32_t u32Offset,
										   uint8_t *pu8RXData,
										   uint16_t u16BytesReceived,
										   void *pvUserData))
{
	EStatus eStatus;
	bool bResult;
	uint32_t u32InterruptRate;

	// Turn timeouts to ticks
	eStatus = PTCGetInterruptRate(YMODEM_TIMER_CHANNEL,
								  &u32InterruptRate);
	ERR_GOTO();

	// # Of ticks for a timeout period for this timer
	sg_u32TimeoutTicks = YMODEM_TIMEOUT_DEFAULT / (1000 / u32InterruptRate);

	// Clear out our structure
	ZERO_STRUCT(*psYModem);

	// Fill in appropriate info
	psYModem->u8SerialIndex = u8SerialIndex;
	psYModem->eState = YMODEM_STATE_INIT;
	psYModem->DataCallback = DataCallback;
	psYModem->pvUserData = pvUserData;

	// Wait for the console UART to clear its buffer
	eStatus = SerialFlush(0);
	ERR_GOTO();

	// Shut off all UART B's interrupts
	eStatus = InterruptMaskSet(INTVECT_IRQ4B_UART2,
							   true);
	ERR_GOTO();

	// Init UART B to 
	bResult = SerialInit((S16550UART *) ROSCOE_UART_B,
						 8,
						 1,
						 EUART_PARITY_NONE);
	assert(bResult);

	// Turn on interrupts for both UARTs
	eStatus = StreamSetConsoleSerialInterruptMode(true);
	if (eStatus != ESTATUS_OK)
	{
		printf("StreamSetConsoleSerialInterruptMode failed - %s\n", GetErrorText(eStatus));
		goto errorExit;
	}

	// Turn on RTS and DTR and the OUTs
	SerialSetOutputs((S16550UART *) ROSCOE_UART_B,
					 true,
					 true,
					 true,
					 true);

	// Set UART B's baud rate
	SerialSetBaudRate((S16550UART *) ROSCOE_UART_B,
					  UART_BAUD_CLOCK,
					  FLASH_UPDATE_BAUD_RATE,
					  NULL);

	// Now reenable interrupts
	eStatus = InterruptMaskSet(INTVECT_IRQ4B_UART2,
							   false);

errorExit:
	return(eStatus);
}

