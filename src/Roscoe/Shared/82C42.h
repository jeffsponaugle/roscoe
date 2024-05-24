#ifndef _82C42_H_
#define _82C42_H_

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stdbool.h>

typedef struct S82C42
{
	volatile uint8_t u8Data;		// Data register
	uint8_t u8Align0;
	volatile uint8_t u8CmdStatus; 	// Command/status register
	uint8_t u8Align1;
} __attribute__((packed)) S82C42;
#endif	// #ifndef __ASSEMBLER__

// Definitions for a keyboard
typedef struct SKeyboardDefinition
{
	char *peKeyboardName;
	void (*Init)(void);
	EStatus (*Receive)(uint8_t u8Scancode,
					   uint8_t *pu8Conversion,
					   uint8_t *pu8Length);
} SKeyboardDefinition;

// Bitfields for setting/clearing LEDs
#define	KEYLED_NUMLOCK			0x01
#define KEYLED_SCROLLLOCK		0x02
#define KEYLED_CAPSLOCK			0x04

extern uint8_t KeyboardGetState(void);
extern EStatus KeyboardSetState(uint8_t u8KeyboardLEDs);
extern EStatus KeyboardGetKey(uint8_t *pu8Key);
extern EStatus KeyboardControllerInit(void);
extern EStatus KeyboardInit(const SKeyboardDefinition *psKeyboardDef,
							uint8_t u8LEDStateInitial);
extern EStatus MouseInit(void);

// For use by the keyboard specific driver
extern void KeyboardSetHeldState(uint8_t u8Scancode,
								 bool bMake);
extern bool KeyboardGetHeldState(uint8_t u8Scancode);

#endif

