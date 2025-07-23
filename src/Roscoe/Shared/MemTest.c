#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "Hardware/Roscoe.h"
#include "Shared/Shared.h"

// Macro for outputting A12-A19 address values to POST LEDs
#define	ADDR_POST(addr)	if ((((uint32_t) addr) & 0xff) == 0) { POST_HEX(((uint32_t) (addr) >> 12) & 0xff); }

/* Data bus tests for all sizes. Walking 0/1 through all data sizes */

// Returns TRUE if successful, otherwise *pu8Expected contains the data
// that was expected, and *pu8Got is what was obtained
bool MemTestDataBusUINT8(volatile uint8_t *pu8Address,
						 uint8_t *pu8Expected,
						 uint8_t *pu8Got)
{
	uint8_t u8Pattern = 1;

	while (u8Pattern)
	{
		uint8_t u8Read;

		*pu8Address = u8Pattern;
		u8Read = *pu8Address;

		if (u8Read != u8Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			*pu8Expected = u8Pattern;
			*pu8Got = u8Read;
			return(false);
		}

		// Invert the pattern
		u8Pattern = ~u8Pattern;

		*pu8Address = u8Pattern;
		u8Read = *pu8Address;

		if (u8Read != u8Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			*pu8Expected = u8Pattern;
			*pu8Got = u8Read;
			return(false);
		}

		// Invert the pattern again (back to the original)
		u8Pattern = ~u8Pattern;

		u8Pattern <<= 1;
	}

	return(true);
}

// Returns TRUE if successful, otherwise *pu16Expected contains the data
// that was expected, and *pu16Got is what was obtained
bool MemTestDataBusUINT16(volatile uint16_t *pu16Address,
						  uint16_t *pu16Expected,
						  uint16_t *pu16Got)
{
	uint16_t u16Pattern = 1;

	while (u16Pattern)
	{
		uint16_t u16Read;

		*pu16Address = u16Pattern;
		u16Read = *pu16Address;

		if (u16Read != u16Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			*pu16Expected = u16Pattern;
			*pu16Got = u16Read;
			return(false);
		}

		// Invert the pattern
		u16Pattern = ~u16Pattern;

		*pu16Address = u16Pattern;
		u16Read = *pu16Address;

		if (u16Read != u16Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			*pu16Expected = u16Pattern;
			*pu16Got = u16Read;
			return(false);
		}

		// Invert the pattern again (back to the original)
		u16Pattern = ~u16Pattern;

		u16Pattern <<= 1;
	}

	return(true);
}

// Returns TRUE if successful, otherwise *pu16Expected contains the data
// that was expected, and *pu16Got is what was obtained
bool MemTestDataBusUINT32(volatile uint32_t *pu32Address,
						  uint32_t *pu32Expected,
						  uint32_t *pu32Got)
{
	uint32_t u32Pattern = 1;

	while (u32Pattern)
	{
		uint32_t u32Read;

		*pu32Address = u32Pattern;
		u32Read = *pu32Address;

		if (u32Read != u32Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			*pu32Expected = u32Pattern;
			*pu32Got = u32Read;
			return(false);
		}

		// Invert the pattern
		u32Pattern = ~u32Pattern;

		*pu32Address = u32Pattern;
		u32Read = *pu32Address;

		if (u32Read != u32Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			*pu32Expected = u32Pattern;
			*pu32Got = u32Read;
			return(false);
		}

		// Invert the pattern again (back to the original)
		u32Pattern = ~u32Pattern;

		u32Pattern <<= 1;
	}

	return(true);
}

/* Device tests for all sizes. */

bool MemTestDeviceUINT8(volatile uint8_t *pu8AddressBase,
						uint32_t u32Size,
						uint32_t *pu32FailedAddress,
						uint8_t *pu8Expected,
						uint8_t *pu8Got)
{
	uint32_t u32Count = u32Size;
	uint8_t u8Pattern;
	uint32_t u32Loop;
	volatile uint8_t *pu8Address;

	// Fill with our pattern
	u32Loop = u32Size;
	u8Pattern = 1;
	pu8Address = pu8AddressBase;
	while (u32Loop)
	{
		ADDR_POST(pu8Address);
		*pu8Address = u8Pattern;
		++pu8Address;
		u8Pattern++;
		u32Loop--;
	}

	// Another pass. This time we check to see if our pattern
	// is good, and we then fill it with the reverse
	u32Loop = u32Size;
	u8Pattern = 1;
	pu8Address = pu8AddressBase;
	while (u32Loop)
	{
		uint8_t u8Data;

		u8Data = *pu8Address;
		if (u8Data != u8Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			printf("Failed read - Address 0x%.6x, expected 0x%.2x, got 0x%.2x - trying the read again\n", ((uint32_t) pu8Address), u8Pattern, u8Data);

			u8Data = *pu8Address;
			if (u8Data != u8Pattern)
			{
				printf("Failed read twice - Address 0x%.6x, expected 0x%.2x, got 0x%.2x - writing again\n", ((uint32_t) pu8Address), u8Pattern, u8Data);

				*pu8Address = u8Pattern;
				u8Data = *pu8Address;
				if (u8Data != u8Pattern)
				{
					printf("Failed read after a write - Address 0x%.6x, expected 0x%.2x, got 0x%.2x - giving up\n", ((uint32_t) pu8Address), u8Pattern, u8Data);
					*pu32FailedAddress = (uint32_t) pu8Address;
					*pu8Expected = u8Pattern;
					*pu8Got = u8Data;
					return(false);
				}
				else
				{
					printf("Read after second write OK. Continuing\n");
				}
			}
			else
			{
				printf("Second read OK. Continuing\n");
			}
		}
		else
		{
			// Passed
		}

		ADDR_POST(pu8Address);
		*pu8Address = ~u8Pattern;
		++pu8Address;
		u8Pattern++;
		u32Loop--;
	}

	// Now scan for the inverted
	u32Loop = u32Size;
	u8Pattern = 1;
	pu8Address = pu8AddressBase;
	while (u32Loop)
	{
		uint8_t u8Data;

		u8Data = *pu8Address;
		if (u8Data != (uint8_t) ~u8Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			printf("Failed inverted read - Address 0x%.6x, expected 0x%.2x, got 0x%.2x - trying the read again\n", ((uint32_t) pu8Address), (uint8_t) ~u8Pattern, u8Data);

			u8Data = *pu8Address;
			if (u8Data != (uint8_t) ~u8Pattern)
			{
				printf("Failed inverted read twice - Address 0x%.6x, expected 0x%.2x, got 0x%.2x - writing inverted again\n", ((uint32_t) pu8Address), (uint8_t) ~u8Pattern, u8Data);

				*pu8Address = ~u8Pattern;
				u8Data = *pu8Address;
				if (u8Data != (uint8_t) ~u8Pattern)
				{
					printf("Failed inverted read after a write - Address 0x%.6x, expected 0x%.2x, got 0x%.2x - giving up\n", ((uint32_t) pu8Address), (uint8_t) ~u8Pattern, u8Data);
					*pu32FailedAddress = (uint32_t) pu8Address;
					*pu8Expected = ~u8Pattern;
					*pu8Got = u8Data;
					return(false);
				}
				else
				{
					printf("Inverted read ead after second write OK. Continuing\n");
				}
			}
			else
			{
				printf("Second inverted read OK. Continuing\n");
			}
		}
		else
		{
			// Passed
		}

		ADDR_POST(pu8Address);
		++pu8Address;
		u8Pattern++;
		u32Loop--;
	}


	return(true);
}

bool MemTestDeviceUINT16(volatile uint16_t *pu16AddressBase,
						 uint32_t u32Size,
						 uint32_t *pu32FailedAddress,
						 uint16_t *pu16Expected,
						 uint16_t *pu16Got)
{
	uint32_t u32Count = u32Size >> 1;
	uint16_t u16Pattern;
	uint32_t u32Loop;
	volatile uint16_t *pu16Address;

	// Fill with our pattern
	u32Loop = u32Count;
	u16Pattern = 1;
	pu16Address = pu16AddressBase;
	while (u32Loop)
	{
		*pu16Address = u16Pattern;
		ADDR_POST(pu16Address);
		++pu16Address;
		u16Pattern++;
		u32Loop--;
	}

	// Another pass. This time we check to see if our pattern
	// is good, and we then fill it with the reverse
	u32Loop = u32Count;
	u16Pattern = 1;
	pu16Address = pu16AddressBase;
	while (u32Loop)
	{
		uint16_t u16Data;

		u16Data = *pu16Address;
		if (u16Data != u16Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			printf("Failed read - Address 0x%.6x, expected 0x%.4x, got 0x%.4x - trying the read again\n", ((uint32_t) pu16Address), u16Pattern, u16Data);

			u16Data = *pu16Address;
			if (u16Data != u16Pattern)
			{
				printf("Failed read twice - Address 0x%.6x, expected 0x%.4x, got 0x%.4x - writing again\n", ((uint32_t) pu16Address), u16Pattern, u16Data);

				*pu16Address = u16Pattern;
				u16Data = *pu16Address;
				if (u16Data != u16Pattern)
				{
					printf("Failed read after a write - Address 0x%.6x, expected 0x%.4x, got 0x%.4x - giving up\n", ((uint32_t) pu16Address), u16Pattern, u16Data);
					*pu32FailedAddress = (uint32_t) pu16Address;
					*pu16Expected = u16Pattern;
					*pu16Got = u16Data;
					return(false);
				}
				else
				{
					printf("Read after second write OK. Continuing\n");
				}
			}
			else
			{
				printf("Second read OK. Continuing\n");
			}
		}
		else
		{
			// Passed
		}

		*pu16Address = ~u16Pattern;
		ADDR_POST(pu16Address);
		++pu16Address;
		u16Pattern++;
		u32Loop--;
	}

	// Now scan for the inverted
	u32Loop = u32Count;
	u16Pattern = 1;
	pu16Address = pu16AddressBase;
	while (u32Loop)
	{
		uint16_t u16Data;

		u16Data = *pu16Address;
		if (u16Data != (uint16_t) ~u16Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			printf("Failed inverted read - Address 0x%.6x, expected 0x%.4x, got 0x%.4x - trying the read again\n", ((uint32_t) pu16Address), (uint16_t) ~u16Pattern, u16Data);

			u16Data = *pu16Address;
			if (u16Data != (uint16_t) ~u16Pattern)
			{
				printf("Failed inverted read twice - Address 0x%.6x, expected 0x%.4x, got 0x%.4x - writing inverted again\n", ((uint32_t) pu16Address), (uint16_t) ~u16Pattern, u16Data);

				*pu16Address = ~u16Pattern;
				u16Data = *pu16Address;
				if (u16Data != (uint16_t) ~u16Pattern)
				{
					printf("Failed inverted read after a write - Address 0x%.6x, expected 0x%.4x, got 0x%.4x - giving up\n", ((uint32_t) pu16Address), (uint16_t) u16Pattern, u16Data);
					*pu32FailedAddress = (uint32_t) pu16Address;
					*pu16Expected = ~u16Pattern;
					*pu16Got = u16Data;
					return(false);
				}
				else
				{
					printf("Read after second inverted write OK. Continuing\n");
				}
			}
			else
			{
				printf("Second inverted read OK. Continuing\n");
			}
		}
		else
		{
			// Passed
		}

		++pu16Address;
		ADDR_POST(pu16Address);
		u16Pattern++;
		u32Loop--;
	}


	return(true);
}

bool MemTestDeviceUINT32(volatile uint32_t *pu32AddressBase,
						 uint32_t u32Size,
						 uint32_t *pu32FailedAddress,
						 uint32_t *pu32Expected,
						 uint32_t *pu32Got)
{
	uint32_t u32Count = u32Size >> 2;
	uint32_t u32Pattern;
	uint32_t u32Loop;
	volatile uint32_t *pu32Address;

	// Fill with our pattern
	u32Loop = u32Count;
	u32Pattern = 1;
	pu32Address = pu32AddressBase;
	while (u32Loop)
	{
		*pu32Address = u32Pattern;
		ADDR_POST(pu32Address);
		++pu32Address;
		u32Pattern += 3;
		u32Loop--;
	}

	// Another pass. This time we check to see if our pattern
	// is good, and we then fill it with the reverse
	u32Loop = u32Count;
	u32Pattern = 1;
	pu32Address = pu32AddressBase;
	while (u32Loop)
	{
		uint32_t u32Data;

		u32Data = *pu32Address;
		if (u32Data != u32Pattern)
		{
			*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
			printf("Failed read - Address 0x%.6x, expected 0x%.8x, got 0x%.8x - trying the read again\n", ((uint32_t) pu32Address), u32Pattern, u32Data);

			u32Data = *pu32Address;
			if (u32Data != u32Pattern)
			{
				printf("Failed read twice - Address 0x%.6x, expected 0x%.8x, got 0x%.8x - writing again\n", ((uint32_t) pu32Address), u32Pattern, u32Data);

				*pu32Address = u32Pattern;
				u32Data = *pu32Address;

				if (u32Data != u32Pattern)
				{
					printf("Failed read after a write - Address 0x%.6x, expected 0x%.8x, got 0x%.8x - giving up\n", ((uint32_t) pu32Address), u32Pattern, u32Data);

					*pu32FailedAddress = (uint32_t) pu32Address;
					*pu32Expected = u32Pattern;
					*pu32Got = u32Data;
					return(false);
				}
				else
				{
					printf("Read after second write OK. Continuing\n");
				}
			}
			else
			{
				printf("Second read OK. Continuing\n");
			}
		}

		*pu32Address = ~u32Pattern;
		ADDR_POST(pu32Address);
		++pu32Address;
		u32Pattern += 3;
		u32Loop--;
	}

	// Now scan for the inverted
	u32Loop = u32Count;
	u32Pattern = 1;
	pu32Address = pu32AddressBase;
	while (u32Loop)
	{
		uint32_t u32Data;

		u32Data = *pu32Address;
		if (u32Data != ~u32Pattern)
		{
			printf("Failed inverted read - Address 0x%.6x, expected 0x%.8x, got 0x%.8x - trying the read again\n", ((uint32_t) pu32Address), ~u32Pattern, u32Data);

			u32Data = *pu32Address;
			if (u32Data != u32Pattern)
			{
				printf("Failed inverted read twice - Address 0x%.6x, expected 0x%.8x, got 0x%.8x - writing inverted again\n", ((uint32_t) pu32Address), ~u32Pattern, u32Data);

				*pu32Address = ~u32Pattern;
				u32Data = *pu32Address;

				if (u32Data != ~u32Pattern)
				{
					printf("Failed inverted read after a write - Address 0x%.6x, expected 0x%.8x, got 0x%.8x - giving up\n", ((uint32_t) pu32Address), ~u32Pattern, u32Data);

					*((uint16_t *) ROSCOE_BOARD_STATUS_LED) = 0;
					*pu32FailedAddress = (uint32_t) pu32Address;
					*pu32Expected = ~u32Pattern;
					*pu32Got = u32Data;
					return(false);
				}
				else
				{
					printf("Inverted read after second write OK. Continuing\n");
				}
			}
			else
			{
				printf("Second inverted read OK. Continuing\n");
			}
		}

		ADDR_POST(pu32Address);
		++pu32Address;
		u32Pattern += 3;
		u32Loop--;
	}


	return(true);
}


