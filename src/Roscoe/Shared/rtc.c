#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "BIOS/OS.h"
#include "Hardware/Roscoe.h"
#include "Shared/Version.h"
#include "Shared/rtc.h"
#include "Shared/Interrupt.h"

// This module supports the Dallas DS12887

#define	RTC_REG(x)		*((volatile uint8_t *) (ROSCOE_RTC + (x)))

// Register definitions

// Time registers
#define	RTC_TIME_SECONDS		0x00
#define	RTC_TIME_MINUTES		0x02
#define	RTC_TIME_HOURS			0x04

// Day of week
#define	RTC_DAY_OF_WEEK			0x06

// Date
#define	RTC_DATE_DAY			0x07
#define	RTC_DATE_MONTH			0x08
#define RTC_DATE_YEAR			0x09

// Control registers
#define	RTC_CTRL_A  			0x0a

// Bit fields for RTC_CTRL_A
#define	RTC_CTRL_A_UIP			0x80
#define	RTC_CTRL_A_DV2			0x40
#define	RTC_CTRL_A_DV1			0x20
#define	RTC_CTRL_A_DV0			0x10
#define	RTC_CTRL_A_RS3			0x08
#define	RTC_CTRL_A_RS2			0x04
#define	RTC_CTRL_A_RS1			0x02
#define	RTC_CTRL_A_RS0			0x01

#define	RTC_CTRL_B				0x0b

// Bit fields for RTC_CTRL_B
#define	RTC_CTRL_B_SET			0x80
#define	RTC_CTRL_B_PIE			0x40
#define	RTC_CTRL_B_AIE			0x20
#define	RTC_CTRL_B_UIE			0x10
#define	RTC_CTRL_B_SQWE			0x08
#define	RTC_CTRL_B_DM			0x04
#define	RTC_CTRL_B_24_12		0x02
#define	RTC_CTRL_B_DSE			0x01

#define	RTC_CTRL_C				0x0c

#define	RTC_CTRL_D				0x0d

// Bit fields for RTC_CTRL_D
#define	RTC_CTRL_D_VRT			0x80

// Needed for get_fattime() function prototype
#include "Shared/FatFS/source/ff.h"

// # Of power-on half seconds
static volatile uint64_t sg_u64PowerOnHalfSeconds;

// Current time_t
static volatile time_t sg_eCurrentTime;

static bool sg_bHalfSecond;

// Interrupt handler to increase power on seconds
static __attribute__ ((interrupt)) void RTC2HzInterruptHandler(void) 
{
	volatile uint8_t u8Value;

	// Clear the interrupt
	u8Value = RTC_REG(RTC_CTRL_C);

	++sg_u64PowerOnHalfSeconds;

	// If we're on an even half second count, increment the second count
	if (0 == (sg_u64PowerOnHalfSeconds & 1))
	{
		sg_eCurrentTime++;
	}
}

// Return # of half seconds
uint32_t RTCGetPowerOnHalfSeconds(void)
{
	return((uint32_t) sg_u64PowerOnHalfSeconds);
}

// Return # of seconds we've been powered on
uint32_t RTCGetPowerOnSeconds(void)
{
	return((uint32_t) (sg_u64PowerOnHalfSeconds >> 1));
}

// Hardware read of the time
static time_t RTCGetTimeHardware(void)
{
	struct tm sTime;
	time_t eTime;

	ZERO_STRUCT(sTime);

	// Wait for UIP to be 0 so we can get a coherent shot of data
	while (RTC_REG(RTC_CTRL_A) & RTC_CTRL_A_UIP);

	sTime.tm_sec = RTC_REG(RTC_TIME_SECONDS);	// Seconds 0-59
	sTime.tm_min = RTC_REG(RTC_TIME_MINUTES);	// Minutes = 0-59
	sTime.tm_hour = RTC_REG(RTC_TIME_HOURS);   	// hour = 0-23

	sTime.tm_mday = RTC_REG(RTC_DATE_DAY);		// mday = 1-31
	sTime.tm_mon = RTC_REG(RTC_DATE_MONTH) - 1;	// month = 0-11
	sTime.tm_year = ((RTC_REG(RTC_DATE_YEAR) + YEAR_BASELINE) - 1900); // year = 1900

	// Turn it in to time(0)
	return(mktime(&sTime));
}

// Used by time(0)
int _gettimeofday_r (struct _reent *, 
					 struct timeval *__tp, 
					 void *__tzp)
{
	if (__tp)
	{
		__tp->tv_sec = sg_eCurrentTime;
		__tp->tv_usec = 0;
	}

	return(sg_eCurrentTime);
}

// Sets the time
void RTCSetTime(time_t eTime)
{
	struct tm *psTime;

	// Convert to its various bits
	psTime = localtime(&eTime);
	assert(psTime);

	// Lock the set bit to keep the clock from rolling over while it's being updated
	RTC_REG(RTC_CTRL_B) = RTC_REG(RTC_CTRL_B) | RTC_CTRL_B_SET;

	RTC_REG(RTC_TIME_SECONDS) = psTime->tm_sec;		// Seconds 0-59
	RTC_REG(RTC_TIME_MINUTES) = psTime->tm_min;		// Minutes = 0-59
	RTC_REG(RTC_TIME_HOURS) = psTime->tm_hour;		// hour = 0-23

	RTC_REG(RTC_DATE_DAY) = psTime->tm_mday;		// mday = 1-31
	RTC_REG(RTC_DATE_MONTH) = psTime->tm_mon + 1;	// month = 0-11
	RTC_REG(RTC_DATE_YEAR) = (psTime->tm_year + 1900) - YEAR_BASELINE; 		// Year 

	// Unlock the SET bit
	RTC_REG(RTC_CTRL_B) = RTC_REG(RTC_CTRL_B) & ~RTC_CTRL_B_SET;

	sg_eCurrentTime = eTime;
}

// # Of ticks (trips through the timer loop) per 125ms
#define	TICKS_TO_HZ_CONSTANT	1730

// Returns the CPU's speed (in hz). Do not call this if RTCInit() didn't work
EStatus RTCGetCPUSpeed(uint32_t *pu32CPUSpeedHz)
{
	EStatus eStatus = ESTATUS_OK;
	uint32_t u32CountPerTick;
	uint64_t u64OldPowerOnHalfSeconds;

	// Go get the benchmark for this CPU based on RTC interrupts
	u64OldPowerOnHalfSeconds = sg_u64PowerOnHalfSeconds;

	// Set RTC to 8hz
	RTC_REG(RTC_CTRL_A) = (RTC_CTRL_A_DV1 | RTC_CTRL_A_RS3 | RTC_CTRL_A_RS2);

	// Now go figure out how many passes we can do
	u32CountPerTick = RTCGetTickCounts();

	// Restore the RTC to original square wave operation (2hz)
	RTC_REG(RTC_CTRL_A) = (RTC_CTRL_A_DV1 | RTC_CTRL_A_RS3 | RTC_CTRL_A_RS2 | RTC_CTRL_A_RS1 | RTC_CTRL_A_RS0);

	// Stop interrupts on the RTC temporarily
	eStatus = InterruptMaskSet(INTVECT_IRQ1A_RTC,
							   true);
	ERR_GOTO();

	// If our RTC is earlier than our build time minus a day, 
	// then set it to the BIOS's build time
	if (RTCGetTimeHardware() < (g_sImageVersion.u64BuildTimestamp - 86400))
	{
		RTCSetTime(g_sImageVersion.u64BuildTimestamp);
	}

	// Set the initial time 
	sg_eCurrentTime = RTCGetTimeHardware();

	// Restore power on half seconds
	sg_u64PowerOnHalfSeconds = u64OldPowerOnHalfSeconds;

	// Restart interrupts on the RTC
	eStatus = InterruptMaskSet(INTVECT_IRQ1A_RTC,
							   false);
	ERR_GOTO();

	if (pu32CPUSpeedHz)
	{
		*pu32CPUSpeedHz = (uint64_t) (((uint64_t) (u32CountPerTick + 1) * (uint64_t) TICKS_TO_HZ_CONSTANT));
		// Round to nearest MHz
		*pu32CPUSpeedHz /= 1000000;
		*pu32CPUSpeedHz *= 1000000;
	}

errorExit:
	return(eStatus);
}

// Init the RTC
EStatus RTCInit(void)
{
	EStatus eStatus = ESTATUS_OK;
	struct tm sTime;
	uint32_t u32Loop;

	// No update in process, DV1=1 to start the clock, and square wave output
	// rate at 500ms
	RTC_REG(RTC_CTRL_A) = (RTC_CTRL_A_DV1 | RTC_CTRL_A_RS3 | RTC_CTRL_A_RS2 | RTC_CTRL_A_RS1 | RTC_CTRL_A_RS0);
	if (RTC_REG(RTC_CTRL_A) != (RTC_CTRL_A_DV1 | RTC_CTRL_A_RS3 | RTC_CTRL_A_RS2 | RTC_CTRL_A_RS1 | RTC_CTRL_A_RS0))
	{
		printf("Failed RTC reg A - expected 0x%.2x, got 0x%.2x\n", (RTC_CTRL_A_DV1 | RTC_CTRL_A_RS3 | RTC_CTRL_A_RS2 | RTC_CTRL_A_RS1 | RTC_CTRL_A_RS0), RTC_REG(RTC_CTRL_A));
		eStatus = ESTATUS_RTC_NOT_PRESENT;
		goto errorExit;
	}

	// Enable periodic interrupt, enable square wave output,  binary date/time (not BCD),
	// and 24 hour mode RTC.
	RTC_REG(RTC_CTRL_B) = (RTC_CTRL_B_PIE | RTC_CTRL_B_SQWE | RTC_CTRL_B_DM | RTC_CTRL_B_24_12);
	if (RTC_REG(RTC_CTRL_B) != (RTC_CTRL_B_PIE | RTC_CTRL_B_SQWE | RTC_CTRL_B_DM | RTC_CTRL_B_24_12))
	{
		printf("Failed RTC reg B - expected 0x%.2x, got 0x%.2x\n", (RTC_CTRL_B_PIE | RTC_CTRL_B_SQWE | RTC_CTRL_B_DM | RTC_CTRL_B_24_12), RTC_REG(RTC_CTRL_B));
		eStatus = ESTATUS_RTC_NOT_PRESENT;
		goto errorExit;
	}

	// Take a look at control register D. VRT should be set, all others are clear
	if (RTC_REG(RTC_CTRL_D) != RTC_CTRL_D_VRT)
	{
		printf("Failed RTC reg D - expected 0x%.2x, got 0x%.2x\n", RTC_CTRL_D_VRT, RTC_REG(RTC_CTRL_D));
		eStatus = ESTATUS_RTC_NOT_PRESENT;
		goto errorExit;
	}

	// All good! Let's see if the clock is reasonable
	ZERO_STRUCT(sTime);

	// If our RTC is earlier than our build time minus a day, 
	// then set it to the BIOS's build time
	if (RTCGetTimeHardware() < (g_sImageVersion.u64BuildTimestamp - 86400))
	{
		RTCSetTime(g_sImageVersion.u64BuildTimestamp);
	}

	// Set the initial time 
	sg_eCurrentTime = RTCGetTimeHardware();

	// Hook up the power on half seconds timer
	eStatus = InterruptHook(INTVECT_IRQ1A_RTC,
							RTC2HzInterruptHandler);
	ERR_GOTO();

	// Now unmask the interrupt
	eStatus = InterruptMaskSet(INTVECT_IRQ1A_RTC,
							   false);
	ERR_GOTO();

	// Now we loop and wait for an interrupt to occur
	u32Loop = 1000000;
	while (u32Loop)
	{
		if (sg_u64PowerOnHalfSeconds)
		{
			eStatus = ESTATUS_OK;
			break;
		}

		u32Loop--;
	}

	if (0 == u32Loop)
	{
		// This means we haven't seen an interrupt from the RTC
//		eStatus = ESTATUS_NO_INTERRUPTS;
		goto errorExit;
	}

errorExit:
	return(eStatus);
}

// Gets the current time/date and returns it in the following format:
//
// Bits 31-25 - Year origin from 1980 (e.g. 37 for 2017)
// Bits 24-21 - Month (1-12)
// Bits 20-16 - Day (1-31)
// Bits 15-11 - Hour (0-23)
// Bits 10-5  - Minute (0-59)
// Bits 4-0   - Seconds /2 (So 48 seconds would be a value of 24)

DWORD get_fattime(void)
{
	struct tm *psTime;
	time_t eTime;

	eTime = time(0);
	psTime = localtime(&eTime);
	assert(psTime);

	return (DWORD)(psTime->tm_year - 80) << 25 |
		   (DWORD)(psTime->tm_mon + 1) << 21 |
		   (DWORD)psTime->tm_mday << 16 |
		   (DWORD)psTime->tm_hour << 11 |
		   (DWORD)psTime->tm_min << 5 |
		   (DWORD)psTime->tm_sec >> 1;
}

