#ifndef _YMODEM_H_
#define _YMODEM_H_

typedef enum
{
	YMODEM_STATE_UNKNOWN,
	YMODEM_STATE_INIT,
	YMODEM_STATE_WAIT_FOR_BLOCK,
	YMODEM_STATE_SEND_RESPONSE,
	YMODEM_STATE_BLOCK_NUMBER,
	YMODEM_STATE_RX_DATA,
	YMODEM_STATE_RX_CRC_CHECKSUM,
	YMODEM_STATE_BLOCK_CHECK,
	YMODEM_STATE_CANCEL
} EYModemState;

typedef struct SYModem
{
	EYModemState eState;			// What state is this state machine in?
	uint8_t u8SerialIndex;			// Which UART are we dealing with?
	uint32_t u32Offset;				// Overall offset within file
	uint32_t u32LastTimerInterrupt;	// Last timer interrupt count
	uint16_t u16BlockSize;			// How big is this block?
	uint16_t u16BlockOffset;		// How far are we along in this block?
	uint16_t u16CRCRX;				// Our received CRC 16 (or checksum)
	uint16_t u16CRCCalc;			// Calculated CRC 16
	uint8_t u8NextBlockNumber;		// Next block number
	uint8_t u8LastResponse;			// Last response byte
	uint8_t u8Buffer[1024];			// Receive buffer
	bool bZeroBlockReceived;		// Has the "0" block been received yet?
	void *pvUserData;				// User data to pass to the callback
	EStatus (*DataCallback)(EStatus eStatus,
							uint32_t u32Offset,
							uint8_t *pu8RXData,
							uint16_t u16BytesReceived,
							void *pvUserData);
} SYModem;

extern EStatus YModemInit(SYModem *psYModem,
						  uint8_t u8SerialIndex,
						  uint32_t u32BaudRate,
						  void *pvUserData,
						  EStatus (*DataCallback)(EStatus eStatus,
												  uint32_t u32Offset,
												  uint8_t *pu8RXData,
												  uint16_t u16BytesReceived,
												  void *pvUserData));
extern EStatus YModemPump(SYModem *psYModem);

#endif

