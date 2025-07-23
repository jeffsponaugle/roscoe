#ifndef _MEMTEST_H_
#define _MEMTEST_H_

extern bool MemTestDataBusUINT8(volatile uint8_t *pu8Address,
								uint8_t *pu8Expected,
								uint8_t *pu8Got);
extern bool MemTestDataBusUINT16(volatile uint16_t *pu16Address,
								 uint16_t *pu16Expected,
								 uint16_t *pu16Got);
extern bool MemTestDataBusUINT32(volatile uint32_t *pu32Address,
								 uint32_t *pu32Expected,
								 uint32_t *pu32Got);
extern bool MemTestDeviceUINT8(volatile uint8_t *pu8AddressBase,
							   uint32_t u32Size,
							   uint32_t *pu32FailedAddress,
							   uint8_t *pu8Expected,
							   uint8_t *pu8Got);
extern bool MemTestDeviceUINT16(volatile uint16_t *pu16AddressBase,
								uint32_t u32Size,
								uint32_t *pu32FailedAddress,
								uint16_t *pu16Expected,
								uint16_t *pu16Got);
extern bool MemTestDeviceUINT32(volatile uint32_t *pu32AddressBase,
								uint32_t u32Size,
								uint32_t *pu32FailedAddress,
								uint32_t *pu32Expected,
								uint32_t *pu32Got);

#endif