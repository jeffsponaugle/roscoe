#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "Shared/Shared.h"
#include "BIOS/OS.h"
#include "Shared/ptc.h"
#include "Hardware/Roscoe.h"
#include "Shared/Interrupt.h"
#include "Shared/AsmUtils.h"

// Our master clock
static uint32_t sg_u32MasterClock = ROSCOE_MASTER_CLOCK_DEFAULT;

// 8254 Timer/counter
typedef struct S8254
{
	volatile uint8_t u8Counter0;
	volatile uint8_t u8Counter1;
	volatile uint8_t u8Counter2;
	volatile uint8_t u8ControlWord;
} __attribute__((packed)) S8254;

// Mode/command register defines

#define	CHANNEL_SELECT_0			0x00
#define	CHANNEL_SELECT_1			0x40
#define	CHANNEL_SELECT_2			0x80
#define	CHANNEL_SELECT_READBACK		0xc0

#define	ACCESS_MODE_LATCH_COUNT		0x00
#define	ACCESS_MODE_LOBYTE			0x10
#define	ACCESS_MODE_HIBYTE			0x20
#define	ACCESS_MODE_LOHIBYTE		0x30

#define	OPMODE_MODE0				0x00		// Interrupt on terminal count
#define	OPMODE_MODE1				0x02		// Hardware retriggerable one-shot
#define	OPMODE_MODE2				0x04		// Rate generator
#define	OPMODE_MODE3				0x06		// Square wave generator
#define	OPMODE_MODE4				0x08		// Software triggered strobe
#define	OPMODE_MODE5				0x0a		// Hardware triggered strobe
#define	OPMODE_MODE2A				0x0c		// Rate generator, same as mode 2
#define	OPMODE_MODE3A   			0x0e		// Square wave generator, same as mode 3

#define	BINARY_BCD_MODE				0x01		// 4 Digit BCD when set

typedef struct SChannelInfo
{
	uint32_t u32MasterClockDivisor;				// What's this counter's divisor from the master clock?
	uint8_t u8ChannelSelect;					// Channel select value
	uint32_t u32DefaultFrequency;				// Default frequency for this channel
	uint8_t u8InterruptVector;					// Interrupt vector (0 if not interrupt capable)
	void (*Interrupt)(void); 				   	// Interrupt handler
} SChannelInfo;

// Array of callbacks for each interrupt handler
static void (*InterruptHandlers[2])(uint8_t u8Channel);
static volatile uint32_t sg_u32InterruptCounters[2];

// Interrupt handlers
static __attribute__ ((interrupt)) void InterruptChannel0(void)
{
	sg_u32InterruptCounters[0]++;
	if (InterruptHandlers[0])
	{
		InterruptHandlers[0](0);
	}
}

static __attribute__ ((interrupt)) void InterruptChannel1(void)
{
	sg_u32InterruptCounters[1]++;
	if (InterruptHandlers[1])
	{
		InterruptHandlers[1](1);
	}
}

// List of all channels and their capabilities/info
static const SChannelInfo sg_sChannelInfo[] =
{
	{PTC_COUNTER0_DIV,	CHANNEL_SELECT_0,		PTC_COUNTER0_HZ,	INTVECT_IRQ6A_PTC1,	InterruptChannel0},
	{PTC_COUNTER1_DIV,	CHANNEL_SELECT_1,		PTC_COUNTER1_HZ,	INTVECT_IRQ6B_PTC2,	InterruptChannel1},
	{PTC_COUNTER2_DIV,	CHANNEL_SELECT_2,		0,					0,					NULL},
};

#define	READBACK_COUNT			0xd0
#define	READBACK_STATUS			0xe0
#define READBACK_COUNT_STATUS	0xc0

// Read the counter's current value
EStatus PTCGetCount(uint8_t u8Channel,
					uint16_t *pu16Count)
{
	EStatus eStatus = ESTATUS_OK;
	volatile S8254 *psPTC = (volatile S8254 *) ROSCOE_PTC;

	if (u8Channel >= (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		eStatus = ESTATUS_PTC_CHANNEL_OUT_OF_RANGE;
		goto errorExit;
	}

	// Mode/command register
	psPTC->u8ControlWord = READBACK_COUNT | (1 << (u8Channel + 1));

	// Read the counter value
	switch (u8Channel)
	{
		case 0:
		{
			*pu16Count = psPTC->u8Counter0;
			*pu16Count |= (psPTC->u8Counter0 << 8);
			break;
		}
		case 1:
		{
			*pu16Count = psPTC->u8Counter1;
			*pu16Count |= (psPTC->u8Counter1 << 8);
			break;
		}
		case 2:
		{
			*pu16Count = psPTC->u8Counter2;
			*pu16Count |= (psPTC->u8Counter2 << 8);
			break;
		}

		default:
		{
			assert(0);
		}
	}

errorExit:
	return(eStatus);
}

EStatus PTCGetInterruptCounter(uint8_t u8Channel,
							   uint32_t *pu32InterruptCount)
{
	EStatus eStatus = ESTATUS_OK;

	if (u8Channel >= (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		eStatus = ESTATUS_PTC_CHANNEL_OUT_OF_RANGE;
		goto errorExit;
	}

	// See if this channel has an interrupt vector. If not, interrupts aren't supported
	if (0 == sg_sChannelInfo[u8Channel].u8InterruptVector)
	{
		eStatus = ESTATUS_PTC_NOT_INT_CAPABLE;
		goto errorExit;
	}

	if (pu32InterruptCount)
	{
		*pu32InterruptCount = sg_u32InterruptCounters[u8Channel];
	}

errorExit:
	return(eStatus);

}

// Install an interrupt handler for a particular channel (if supported)
EStatus PTCSetInterruptHandler(uint8_t u8Channel,
							   void (*Interrupt)(uint8_t u8Channel))
{
	EStatus eStatus = ESTATUS_OK;

	if (u8Channel >= (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		eStatus = ESTATUS_PTC_CHANNEL_OUT_OF_RANGE;
		goto errorExit;
	}

	// See if this channel has an interrupt vector
	if (0 == sg_sChannelInfo[u8Channel].u8InterruptVector)
	{
		eStatus = ESTATUS_PTC_NOT_INT_CAPABLE;
		goto errorExit;
	}

	InterruptHandlers[u8Channel] = Interrupt;

errorExit:
	return(eStatus);
}

// Mask appropriate interrupt
EStatus PTCSetInterruptMask(uint8_t u8Channel,
							bool bInterruptMask)
{
	EStatus eStatus = ESTATUS_OK;

	if (u8Channel >= (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		eStatus = ESTATUS_PTC_CHANNEL_OUT_OF_RANGE;
		goto errorExit;
	}

	// See if this channel has an interrupt vector
	if (0 == sg_sChannelInfo[u8Channel].u8InterruptVector)
	{
		eStatus = ESTATUS_PTC_NOT_INT_CAPABLE;
		goto errorExit;
	}

	eStatus = InterruptMaskSet(sg_sChannelInfo[u8Channel].u8InterruptVector,
							   bInterruptMask);

errorExit:
	return(eStatus);
}

// Returns the rate of interrupt for a particular PTC channel
EStatus PTCGetInterruptRate(uint8_t u8Channel,
							uint32_t *pu32InterruptRate)
{
	EStatus eStatus;

	if (u8Channel >= (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		eStatus = ESTATUS_PTC_CHANNEL_OUT_OF_RANGE;
		goto errorExit;
	}

	// See if this channel has an interrupt vector. If not, interrupts aren't supported
	if (0 == sg_sChannelInfo[u8Channel].u8InterruptVector)
	{
		eStatus = ESTATUS_PTC_NOT_INT_CAPABLE;
		goto errorExit;
	}

	if (pu32InterruptRate)
	{
		*pu32InterruptRate = sg_sChannelInfo[u8Channel].u32DefaultFrequency;
	}
	eStatus = ESTATUS_OK;

errorExit:
	return(eStatus);
}

// Set a channel's update rate
EStatus PTCSetChannelRate(uint8_t u8Channel,
						  uint32_t u32Hz)
{
	EStatus eStatus = ESTATUS_OK;
	volatile S8254 *psPTC = (volatile S8254 *) ROSCOE_PTC;
	uint32_t u32Divisor;

	if (u8Channel >= (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		eStatus = ESTATUS_PTC_CHANNEL_OUT_OF_RANGE;
		goto errorExit;
	}

	// A 0 u32Hz indicates that the counter should count the full spread rather than
	// being set at a specific frequency
	if (u32Hz)
	{
		// Figure out what our divisor should be. If it's greater than 16 bits, then
		// we need to tell the caller we can't go that fast (or slow)
		u32Divisor = (sg_u32MasterClock / sg_sChannelInfo[u8Channel].u32MasterClockDivisor) / u32Hz;

		if ((u32Divisor > 0xffff) ||
			(u32Hz > (sg_u32MasterClock / sg_sChannelInfo[u8Channel].u32MasterClockDivisor)))
		{
			eStatus = ESTATUS_PTC_RATE_OUT_OF_RANGE;
			goto errorExit;
		}
	}
	else
	{
		// Allow the down counter to reload for all 16 bits
		u32Divisor = 0xffff;
	}

	// Mode/command register
	psPTC->u8ControlWord = sg_sChannelInfo[u8Channel].u8ChannelSelect | ACCESS_MODE_LOHIBYTE | OPMODE_MODE2;

	// Output the divisor - LSB/MSB order
	switch (u8Channel)
	{
		case 0:
		{
			psPTC->u8Counter0 = (uint8_t) u32Divisor;
			psPTC->u8Counter0 = (uint8_t) (u32Divisor >> 8);
			break;
		}
		case 1:
		{
			psPTC->u8Counter1 = (uint8_t) u32Divisor;
			psPTC->u8Counter1 = (uint8_t) (u32Divisor >> 8);
			break;
		}
		case 2:
		{
			psPTC->u8Counter2 = (uint8_t) u32Divisor;
			psPTC->u8Counter2 = (uint8_t) (u32Divisor >> 8);
			break;
		}

		default:
		{
			assert(0);
		}
	}

errorExit:
	return(eStatus);
}

// Initializes all channels of the programmable timer/counter
EStatus PTCInit(uint32_t u32MasterClockSpeed)
{
	EStatus eStatus = ESTATUS_OK;
	const SChannelInfo *psChannel = sg_sChannelInfo;
	uint8_t u8Channel = 0;
	uint32_t u32Loop;
	uint16_t u16Counters[(sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0]))];
	bool bChanged[(sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0]))];

	return(ESTATUS_OK);

	// If we provide a nonzero number, then use the incoming value as our master clock speed
	if (u32MasterClockSpeed)
	{
		sg_u32MasterClock = u32MasterClockSpeed;
	}

	// Run through all channels and set their default rate and vectors
	while (u8Channel < (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		// Set the channel's rate
		eStatus = PTCSetChannelRate(u8Channel,
									psChannel->u32DefaultFrequency);
		ERR_GOTO();

		// If this channel is interrupt capable, mask it, and set its vector
		if (psChannel->u8InterruptVector)
		{
			// Hook it
			eStatus = InterruptHook(psChannel->u8InterruptVector,
									psChannel->Interrupt);
			ERR_GOTO();

			// Shut off the interrupt mask
			eStatus = InterruptMaskSet(psChannel->u8InterruptVector,
									   false);
			ERR_GOTO();
		}

		++psChannel;
		u8Channel++;
	}

	// OK, looking good. Let's see if we're counting down and things are changing

	// Get the initial seed value
	u8Channel = 0;
	while (u8Channel < (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
	{
		bChanged[u8Channel] = false;
		eStatus = PTCGetCount(u8Channel,
							  &u16Counters[u8Channel]);
		ERR_GOTO();
		u8Channel++;
	}

	// Loop through and figure out what is or isn't changing
	u32Loop = 250000;
	while (u32Loop)
	{
		uint8_t u8ChangedCount;
		uint16_t u16Counter;

		u8Channel = 0;
		u8ChangedCount = 0;

		// Run through all channels and read their current countdown values and
		// see if they've changed
		while (u8Channel < (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
		{
			if (false == bChanged[u8Channel])
			{
				eStatus =  PTCGetCount(u8Channel,
									   &u16Counter);
				ERR_GOTO();

				// If the counter isn't the same from the last pass, indicate it changed
				if (u16Counter != u16Counters[u8Channel])
				{
					bChanged[u8Channel] = true;
					++u8ChangedCount;
				}
				else
				{
					// Count hasn't changed. Keep looping.
				}
			}
			else
			{
				u8ChangedCount++;
			}

			u8Channel++;
		}

		// If all 3 counters have changed, we're good
		if ((sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])) == u8ChangedCount)
		{
			break;
		}

		u32Loop--;
	}

	// If we're at 0, then not all channel counters have advanced
	if (0 == u32Loop)
	{
		printf("Counters nonfunctional: ");

		u8Channel = 0;
		while (u8Channel < (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
		{
			if (false == bChanged[u8Channel])
			{
				printf("%u ", u8Channel);
			}

			u8Channel++;
		}

		printf("\n");
		eStatus = ESTATUS_PTC_CHANNEL_NONFUNCTIONAL;
		goto errorExit2;
	}

	// Now let's see if counter 0 and counter 1 are generating interrupts
	u32Loop = 250000;
	while (u32Loop)
	{
		uint8_t u8ChangedCount;
		uint8_t u8TotalInterruptable;
		uint16_t u16Counter;

		u8Channel = 0;
		u8ChangedCount = 0;
		u8TotalInterruptable = 0;

		// Run through all channels and read their interrupt counts (if applicable)
		while (u8Channel < (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
		{
			uint32_t u32InterruptCount;

			eStatus = PTCGetInterruptCounter(u8Channel,
											 &u32InterruptCount);
			if (ESTATUS_PTC_NOT_INT_CAPABLE == eStatus)
			{
				// This is OK - just move on to the next channel
			}
			else
			if (ESTATUS_OK == eStatus)
			{
				// If our interrupt count is nonzero, it means interrupts are working
				if (u32InterruptCount)
				{
					++u8ChangedCount;
				}

				++u8TotalInterruptable;
			}
			else
			{
				// Some other error
				goto errorExit;
			}

			u8Channel++;
		}

		// If all counters with interrupts have been accounted for, then we're good
		if (u8TotalInterruptable == u8ChangedCount)
		{
			eStatus = ESTATUS_OK;
			break;
		}

		u32Loop--;
	}

	// If u32Loop is nonzero, then we're getting expected interrupts
	if (0 == u32Loop)
	{
		printf("Counter interrupts nonfunctional: ");

		u8Channel = 0;
		while (u8Channel < (sizeof(sg_sChannelInfo) / sizeof(sg_sChannelInfo[0])))
		{
			uint32_t u32InterruptCount;

			eStatus = PTCGetInterruptCounter(u8Channel,
											 &u32InterruptCount);
			if (ESTATUS_PTC_NOT_INT_CAPABLE == eStatus)
			{
				// This is OK - just move on to the next channel
			}
			else
			if (ESTATUS_OK == eStatus)
			{
				if (0 == u32InterruptCount)
				{
					printf("%u ", u8Channel);
				}
			}
			else
			{
				goto errorExit;
			}

			u8Channel++;
		}

		printf("\n");
		eStatus = ESTATUS_PTC_INTERRUPT_NONFUNCTIONAL;
		goto errorExit2;
	}

errorExit:
	if (eStatus != ESTATUS_OK)
	{
		printf("Failed - %s\n", GetErrorText(eStatus));
	}

errorExit2:
	return(eStatus);
}

