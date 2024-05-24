#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "Hardware/Roscoe.h"
#include "Shared/Version.h"
#include "BIOS/OS.h"
#include "Shared/16550.h"
#include "Shared/Monitor.h"
#include "Shared/Shared.h"
#include "Shared/Interrupt.h"
#include "Shared/IDE.h"
#include "Shared/Stream.h"

// Main entry point for boot loader
void main(void)
{
	// Indicate that we're at main() on the POST code LEDs
	POST_SET(POSTCODE_BOOTLOADER_MAIN);

	(void) MonitorStart();

//	((void (*)(void))psVersion->u32EntryPoint)();

	// SHOULD NOT GET HERE
	printf("\nReturned from dispatch to operational image - this should not happen!\n");
	exit(0);
	
}

