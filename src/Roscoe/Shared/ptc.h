#ifndef _PTC_H_
#define _PTC_H_

extern EStatus PTCInit(uint32_t u32MasterClockSpeed);
extern EStatus PTCSetInterruptHandler(uint8_t u8Channel,
									  void (*Interrupt)(uint8_t u8Channel));
extern EStatus PTCSetChannelRate(uint8_t u8Channel,
								 uint32_t u32Hz);
extern EStatus PTCGetCount(uint8_t u8Channel,
						   uint16_t *pu16Count);
extern EStatus PTCGetInterruptCounter(uint8_t u8Channel,
									  uint32_t *pu32InterruptCount);
extern EStatus PTCGetInterruptRate(uint8_t u8Channel,
								   uint32_t *pu32InterruptRate);
extern EStatus PTCSetInterruptMask(uint8_t u8Channel,
								   bool bInterruptMask);

#endif

