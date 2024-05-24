/*
  Roscoe Hardware definitions module
*/

#ifndef _ROSCOE_H_
#define _ROSCOE_H_

// CPU Vectors
#define	VECTOR_STACK_POINTER				0x00000000
#define	VECTOR_RESET_PC						0x00000004
#define	VECTOR_BUS_ERROR					0x00000008
#define	VECTOR_ADDRESS_ERROR				0x0000000c
#define	VECTOR_ILLEGAL_INSTRUCTION			0x00000010
#define	VECTOR_DIV0							0x00000014
#define	VECTOR_PRIVILEGE_VIOLATION			0x0000001c

// Boot loader flash (512K)
#define	ROSCOE_FLASH_BOOT_BASE				0x00000000
#define ROSCOE_FLASH_BOOT_SIZE				(512*1024)

// BIOS flash (16MB)
#define	ROSCOE_FLASH_BIOS_BASE				0x08000000
#define	ROSCOE_FLASH_BIOS_SIZE				(16*1024*1024)

// Fast RAM (0WS)
#define	ROSCOE_FAST_SRAM_BASE				0x10000000
#define ROSCOE_FAST_SRAM_SIZE				(1*1024*1024)

// System SRAM
#define	ROSCOE_SYSTEM_SRAM					0x20000000
#define	ROSCOE_SYSTEM_SRAM_SIZE				(2*1024*1024)

// Peripheral mapping areas (8 bit)
#define	ROSCOE_UART_A				  		0x30000000
#define	ROSCOE_UART_B				 	   	(ROSCOE_UART_A + 0x100)
#define	ROSCOE_INTERRUPT_CONTROLLER_BASE	(ROSCOE_UART_B + 0x100)
#define	ROSCOE_INTC_MASK					(ROSCOE_INTERRUPT_CONTROLLER_BASE)
#define	ROSCOE_INTC_MASK2					(ROSCOE_INTC_MASK + 1)
#define	ROSCOE_POWER_RESET_CTRL				(ROSCOE_INTC_MASK2 + 1)
#define	ROSCOE_RTC							(ROSCOE_INTERRUPT_CONTROLLER_BASE + 0x100)
#define	ROSCOE_PTC							(ROSCOE_RTC + 0x100)
#define	ROSCOE_VIDEO_CTRL					(ROSCOE_PTC + 0x100)
#define	ROSCOE_POST_SEGMENT_LEFT   			(ROSCOE_VIDEO_CTRL + 0x100)
#define	ROSCOE_POST_SEGMENT_RIGHT			(ROSCOE_POST_SEGMENT_LEFT + 0x100)
#define	ROSCOE_BOARD_STATUS_LED				(ROSCOE_POST_SEGMENT_RIGHT + 0x100)

// Power/reset control bits
#define	ROSCOE_POWER_RESET_POWER			(1 << 0)
#define	ROSCOE_POWER_RESET_RESET			(1 << 1)

// Peripheral mapping areas (16 bit)
#define	ROSCOE_IDE_1_CSA					0x40000000
#define	ROSCOE_IDE_1_CSB					(ROSCOE_IDE_1_CSA + 0x100)
#define	ROSCOE_IDE_2_CSA					(ROSCOE_IDE_1_CSB + 0x100)
#define	ROSCOE_IDE_2_CSB   					(ROSCOE_IDE_2_CSA + 0x100)
#define	ROSCOE_USB_CONTROLLER				(ROSCOE_IDE_2_CSB + 0x100)

// Video memory
#define	ROSCOE_VIDEO_MEMORY_BASE			0x50000000
#define ROSCOE_VIDEO_FONT_MEMORY			(ROSCOE_VIDEO_MEMORY_BASE + 0x100000)
#define	ROSCOE_NIC							(ROSCOE_VIDEO_FONT_MEMORY + 0x100000)

// System DRAM
#define	ROSCOE_DRAM_BASE					0x80000000

// Baud rate in the boot loader
#define	UART_BAUD_CLOCK						18432000
#define	BOOTLOADER_BAUD_RATE				115200
#define	FLASH_UPDATE_BAUD_RATE				230400
#define	BOOTLOADER_BAUD_RATE_DIVISOR		((UART_BAUD_CLOCK / 16) / BOOTLOADER_BAUD_RATE)

// Locations for boot loader and BIOS
#define	ROSCOE_BOOT_BASE_SRAM				((ROSCOE_FAST_SRAM_BASE + ROSCOE_FAST_SRAM_SIZE) - ROSCOE_FLASH_BOOT_SIZE)
#define	ROSCOE_BIOS_BASE					ROSCOE_SYSTEM_SRAM

// Programmable timer counter divsior (from the main system clock)
#define	PTC_COUNTER0_DIV					64
#define	PTC_COUNTER1_DIV					32
#define	PTC_COUNTER2_DIV					64

// Default PTC rates (in hz)
#define	PTC_COUNTER0_HZ		  				10			// 100ms
#define	PTC_COUNTER1_HZ						100			// 10ms

// Default master clock speed
#define	ROSCOE_MASTER_CLOCK_DEFAULT			25000000

// POST LED related

// 7 Segment segments + decimal point. Layout is as follows:
//
//      A
//   +-----+
//   |     |
// F |     | B
//   |  G  |
//   +-----+
//   |     |
// E |     | C
//   |     |
//   +-----+
//      D    DP

// Each segment is active LOW
#define	POST_7SEG_DP						0xfe
#define	POST_7SEG_A							0xfd
#define	POST_7SEG_B							0xfb
#define	POST_7SEG_C							0xf7
#define	POST_7SEG_D							0xef
#define	POST_7SEG_E							0xdf
#define	POST_7SEG_F							0xbf
#define	POST_7SEG_G							0x7f

// 7 Segment hex digit values
#define	POSTLED(x)							(x) 
#define	POST_7SEG_HEX_0						POSTLED(POST_7SEG_A & POST_7SEG_B & POST_7SEG_C & POST_7SEG_D & POST_7SEG_E & POST_7SEG_F)
#define	POST_7SEG_HEX_1						POSTLED(POST_7SEG_B & POST_7SEG_C)
#define	POST_7SEG_HEX_2						POSTLED(POST_7SEG_A & POST_7SEG_B & POST_7SEG_D & POST_7SEG_E & POST_7SEG_G)
#define	POST_7SEG_HEX_3						POSTLED(POST_7SEG_A & POST_7SEG_B & POST_7SEG_C & POST_7SEG_D & POST_7SEG_G)
#define	POST_7SEG_HEX_4						POSTLED(POST_7SEG_B & POST_7SEG_C & POST_7SEG_F & POST_7SEG_G)
#define	POST_7SEG_HEX_5						POSTLED(POST_7SEG_A & POST_7SEG_F & POST_7SEG_G & POST_7SEG_C & POST_7SEG_D)
#define POST_7SEG_HEX_6						POSTLED(POST_7SEG_A & POST_7SEG_F & POST_7SEG_G & POST_7SEG_C & POST_7SEG_D & POST_7SEG_E)
#define	POST_7SEG_HEX_7						POSTLED(POST_7SEG_A & POST_7SEG_B & POST_7SEG_C)
#define	POST_7SEG_HEX_8						POSTLED(POST_7SEG_A & POST_7SEG_B & POST_7SEG_C & POST_7SEG_D & POST_7SEG_E & POST_7SEG_F & POST_7SEG_G)
#define	POST_7SEG_HEX_9						POSTLED(POST_7SEG_G & POST_7SEG_F & POST_7SEG_A & POST_7SEG_B & POST_7SEG_C & POST_7SEG_D)
#define	POST_7SEG_HEX_A						POSTLED(POST_7SEG_A & POST_7SEG_B & POST_7SEG_C & POST_7SEG_E & POST_7SEG_F & POST_7SEG_G)
#define	POST_7SEG_HEX_B						POSTLED(POST_7SEG_C & POST_7SEG_D & POST_7SEG_E & POST_7SEG_F & POST_7SEG_G)
#define	POST_7SEG_HEX_C						POSTLED(POST_7SEG_A & POST_7SEG_D & POST_7SEG_E & POST_7SEG_F)
#define	POST_7SEG_HEX_D						POSTLED(POST_7SEG_C & POST_7SEG_D & POST_7SEG_E & POST_7SEG_B & POST_7SEG_G)
#define	POST_7SEG_HEX_E						POSTLED(POST_7SEG_A & POST_7SEG_D & POST_7SEG_E & POST_7SEG_F & POST_7SEG_G)
#define	POST_7SEG_HEX_F						POSTLED(POST_7SEG_A & POST_7SEG_E & POST_7SEG_F & POST_7SEG_G)
#define	POST_7SEG_ALPHA_U					POSTLED(POST_7SEG_B & POST_7SEG_C & POST_7SEG_D & POST_7SEG_E & POST_7SEG_F)

#define POST_7SEG_OFF						POSTLED(0xff)

// POST Codes
#define	POST_SET(x)							*((volatile uint8_t *) ROSCOE_POST_SEGMENT_LEFT) = ((x) >> 8); *((volatile uint8_t *) ROSCOE_POST_SEGMENT_RIGHT) = (uint8_t) (x); 

// Boot loader POST codes
#define	POSTCODE_UART_A_INIT					((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_0)	// 00 - UART A Init
#define	POSTCODE_BOOTLOADER_COPY				((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_1)	// 01 - Boot loader copy to top of 0WS SRAM
#define POSTCODE_BOOTLOADER_HEAP_INIT			((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_2)	// 02 - Boot loader heap init
#define	POSTCODE_BOOTLOADER_RAMEXEC				((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_3)	// 03 - Boot loader jump to top of SRAM
#define	POSTCODE_BOOTLOADER_MAIN				((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_4)	// 04 - Boot loader has hit main()
#define	POSTCODE_BOOTLOADER_OP_IMAGE_SEARCH		((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_5)	// 05 - Boot loader started operational image (BIOS) search
#define	POSTCODE_BOOTLOADER_OP_IMAGE_SEARCH_OP1 ((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_6)	// 06 - Checking op image 1 region
#define	POSTCODE_BOOTLOADER_OP_IMAGE_SEARCH_OP2 ((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_7)	// 07 - Checking op image 2 region
#define	POSTCODE_BOOTLOADER_CHOOSE_OP_IMAGE		((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_8)	// 08 - Choosing which op image to boot to
#define POSTCODE_BOOTLOADER_COPY_OP_IMAGE		((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_9)	// 09 - Copy operational image to botttom of SRAM
#define POSTCODE_BOOTLOADER_VERIFY_OP_IMAGE_COPY ((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_A)	// 0a - Verify Flash->RAM copy
#define POSTCODE_BOOTLOADER_OP_IMAGE_EXEC		((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_B)	// 0b - Jump to operational BIOS image
#define	POSTCODE_BOOTLOADER_MONITOR				((POST_7SEG_HEX_0 << 8) + POST_7SEG_HEX_C)	// 0c - Boot loader is in monitor (start exec)


// Flash update POST codes
#define	POSTCODE_FWUPD_START					((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_0)	// 40 - Start of firmware update
#define	POSTCODE_FWUPD_START_MAIN				((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_1)	// 41 - Hit main()
#define	POSTCODE_FWUPD_REGION_CHECKS			((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_2)	// 42 - Checking both flash op regions
#define	POSTCODE_FWUPD_ERASE_REGION				((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_3)	// 43 - Erasing the region to be programmed
#define	POSTCODE_FWUPD_START_TRANSFER			((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_4)	// 44 - Start transfer
#define	POSTCODE_FWUPD_BLOCK_RECEIVE			((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_5)	// 45 - Block receive
#define	POSTCODE_FWUPD_PROGRAM_BLOCK			((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_6)	// 46 - Program block
#define POSTCODE_FWUPD_PROGRAM_CHECK			((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_7)	// 47 - Programming complete. Checking region.
#define	POSTCODE_FWUPD_ERASE_STAGING			((POST_7SEG_HEX_4 << 8) + POST_7SEG_HEX_8)	// 48 - Erase staging area

// ELF File transfer POST codes
#define	POSTCODE_ELF_START						((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_0)	// 50 - Start of ELF transfer
#define	POSTCODE_ELF_HEADER                     ((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_1)	// 51 - ELF Header consumed
#define	POSTCODE_ELF_PROGRAM_HEADER             ((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_2)	// 52 - Finding program header
#define	POSTCODE_ELF_PROGRAM_CONSUME1			((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_3)	// 53 - Consuming ELF program
#define	POSTCODE_ELF_PROGRAM_CONSUME2			((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_4)	// 54 - Consuming ELF program
#define	POSTCODE_ELF_COMPLETE                   ((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_5)	// 55 - ELF Program load complete
#define	POSTCODE_ELF_BAD_IMAGE                  ((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_6)	// 56 - Bad ELF image for one or more reasons
#define	POSTCODE_ELF_DISPATCH_PROGRAM           ((POST_7SEG_HEX_5 << 8) + POST_7SEG_HEX_7)	// 57 - Dispatching control to ELF program

// Application POST codes
#define	POSTCODE_APP_START						((POST_7SEG_HEX_6 << 8) + POST_7SEG_HEX_0)	// 60 - Start of application execution
#define	POSTCODE_APP_START_MAIN					((POST_7SEG_HEX_6 << 8) + POST_7SEG_HEX_1)	// 61 - Application execution at main()

// UART fault
#define	POSTFAULT_UART_ISR						((POST_7SEG_ALPHA_U << 8) + POST_7SEG_OFF)	// U  - UART Problem

// Interrupt vectors
#define	INTVECT_IRQL7_DEBUGGER					0xf0
#define	INTVECT_IRQ6A_PTC1						0xe1
#define	INTVECT_IRQ6B_PTC2						0xe2
#define	INTVECT_IRQ5A_NIC						0xd1
#define	INTVECT_IRQ5B_IDE1						0xd2
#define	INTVECT_IRQ5C_IDE2						0xd4
#define	INTVECT_IRQ5D_EXPANSION_I5				0xd8
#define	INTVECT_IRQ4A_UART1						0xc1
#define	INTVECT_IRQ4B_UART2						0xc2
#define	INTVECT_IRQ4C_EXPANSION_T4				0xc4
#define	INTVECT_IRQ3A_USB						0xb1
#define	INTVECT_IRQ3B_EXPANSION_T3				0xb2
#define	INTVECT_IRQ2A_VIDEO						0xa1
#define	INTVECT_IRQ2B_EXPANSION_T2				0xa2
#define	INTVECT_IRQ1A_RTC						0x91
#define	INTVECT_IRQ1B_POWER						0x92

// Helpful/useful macros
#define	ZERO_STRUCT(x)							memset((void *) &(x), 0, sizeof(x));
#define	ERR_GOTO()								if (eStatus != ESTATUS_OK) { goto errorExit; }

#endif
