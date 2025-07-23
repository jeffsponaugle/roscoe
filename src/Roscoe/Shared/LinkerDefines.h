/* Common linker defines */

#ifndef _LINKER_DEFINES_
#define _LINKER_DEFINES_

#include <stdint.h>

// Start of bss (zero init)
extern uint8_t __bss_start;

// End of bss, start of heap/stack
extern uint8_t _end;

// Top of stack
extern uint8_t _stack;

// Get the stack pointer
extern uint32_t StackPointerGet(void);

// Get the frame pointer
extern uint32_t FramePointerGet(void);

#endif
