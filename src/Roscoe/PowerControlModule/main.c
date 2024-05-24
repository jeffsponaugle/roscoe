// RSC68k - Power control module
//
// Pin/port definitions:
//
// PA0			- Power button (input)
// PA1			- Reset button (input)
// PA2			- Power LED (output)
// PA3			- System reset (output, active low)
// PA7/PCINT7	- PWROK Signal from power supply (input)
// PB0			- PSON Signal to power supply (output)
// PB1			- Reset request (input)
// PB2			- PWR Grant/request (input/output)

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// CPU speed (in hz)
#define CPU_SPEED				8000000

// Timer prescaler
#define TIMER_PRESCALER			1024

// Timer frequency in hz (100hz)
#define TIMER_FREQ				100

// Timer tick frequency (in milliseconds)
#define TIMER_TICK_MS			(1000 / TIMER_FREQ)

// Divisor
#define TIMER_DIVISOR			((CPU_SPEED / TIMER_PRESCALER) / (TIMER_FREQ))

// Tuneables

// Amount of time (in milliseconds) between PSON to PWROK asserting before we consider
// it a fault condition.
#define PSON_TO_PWROK			2000

// Amount of time (milliseconds) between PWROK and reset being deasserted, and the
// length of a reset assertion by the user.
#define PWROK_TO_RESET			150

// Button debounce time (# of milliseconds it must be in a steady state before it's
// considered pressed/released)
#define BUTTON_DEBOUNCE			150

// Power button override time - like the 4 second hold on the power button to power off a system
#define POWER_BUTTON_OVERRIDE	4000

// Minimum time (in milliseconds) PWR GR request to be considered valid (anything shorter is not a controlled shutdown request)
#define PWR_GR_REQUEST_MIN		20

// Maximum time (in milliseconds) PWR GR request to be considered valid (anything longer is not a controlled shutdown request)
#define	PWR_GR_REQUEST_MAX		100

// Length of PWR_GR pulse sent to the host when a power down request is in coordinated shutdown mode
#define PWR_RQ_POWER_HOST		50

// How long (in milliseconds) PWR GR must be asserted (with a PWR GR request active)
// before power is shut off
#define PWR_GR_POWER_DOWN		300

// Last state of PWRRQ through the last tick
static bool sg_bPWRRQLastState;

// State of power control module
typedef enum
{
	ESTATE_INIT,
	ESTATE_MONITOR,
	ESTATE_POWER_ON,
	ESTATE_WAIT_PWROK,
	ESTATE_WAIT_RESET_DEASSERT,
	ESTATE_POWER_OFF,
	ESTATE_WAIT_REQUEST_TIME,
} EPowerState;

// Our controller's power state
static EPowerState sg_eState = ESTATE_INIT;

// LED Alert states
typedef enum
{
	EALERT_OFF,
	EALERT_POWERING_UP,
	EALERT_POWER_FAILURE,
	EALERT_POWER_DOWN_PENDING,
	EALERT_ON,
} EAlerts;

#if defined(__AVR_ATtiny24A__)

// Power button (input) - PA0 - 13
#define BUTTON_POWER_DDR	DDRA
#define BUTTON_POWER_PORT	PORTA
#define BUTTON_POWER_PIN	PINA
#define BUTTON_POWER_BIT	PORTA0

// Reset button (input) - PA1 - 12
#define BUTTON_RESET_DDR	DDRA
#define BUTTON_RESET_PORT	PORTA
#define BUTTON_RESET_PIN	PINA
#define BUTTON_RESET_BIT	PORTA1

// Power LED (output) - PA2 - 11
#define LED_POWER_DDR		DDRA
#define LED_POWER_PORT		PORTA
#define LED_POWER_BIT		PORTA2

// Reset (output) - PA3 - 10
#define RESET_DDR			DDRA
#define RESET_PORT			PORTA
#define RESET_BIT			PORTA3

// Reset request (input) - PB1 - 3
#define RESET_REQUEST_DDR	DDRB
#define RESET_REQUEST_PORT	PORTB
#define RESET_REQUEST_BIT	PORTB1
#define	RESET_REQUEST_PIN	PINB

// PWR_OK (input) - PA7 - 6
#define PWR_OK_DDR			DDRA			
#define PWR_OK_PORT			PORTA
#define PWR_OK_PIN			PINA
#define PWR_OK_BIT			PINA7

// PSON (output) - PB0 - 2
#define PSON_DDR			DDRB
#define PSON_PORT			PORTB
#define PSON_BIT			PORTB0

// PWR_REQGR - power control request (input/output) - PB2 - 5
#define PWR_REQ_DDR			INT0_DDR
#define PWR_REQ_PORT		INT0_PORT
#define PWR_REQ_PIN			INT0_PIN
#define PWR_REQ_BIT			INT0_BIT

#else
#error Target CPU not defined
#endif

// Macros for accessing various functions. Note that TRUE means *ASSERTED*
// not a logic 1 necessarily. This deals in logical state
#define BUTTON_POWER()		((BUTTON_POWER_PIN & (1 << BUTTON_POWER_BIT)) ? false : true)
#define BUTTON_RESET()		((BUTTON_RESET_PIN & (1 << BUTTON_RESET_BIT)) ? false : true)
#define LED_POWER_SET(x)	if (x) {cli(); LED_POWER_DDR &= (uint8_t) ~(1 << LED_POWER_BIT); sei();} else {cli(); LED_POWER_DDR |= (1 << LED_POWER_BIT); sei();}
#define RESET_SET(x)		if (x) {cli(); RESET_PORT &= (uint8_t) ~(1 << RESET_BIT); RESET_DDR |= (1 << RESET_BIT); sei();} else { cli(); RESET_DDR &= (uint8_t) ~(1 << RESET_BIT); RESET_PORT |= (uint8_t) (1 << RESET_BIT); sei();}
#define PWR_OK()			((PWR_OK_PIN & (1 << PWR_OK_PIN)) ? true : false)
#define PSON_SET(x)			if (x) {cli(); PSON_PORT &= (uint8_t) ~(1 << PSON_BIT); sei();} else {cli(); PSON_PORT |= (1 << PSON_BIT); sei();}
#define PWRGR_REQ_SET(x)	if (x) {cli(); PWR_REQ_PORT |= (1 << PWR_REQ_BIT); PWR_REQ_DDR |= (1 << PWR_REQ_BIT); sei();} else {cli(); PWR_REQ_DDR &= (uint8_t) ~(1 << PWR_REQ_BIT); PWR_REQ_PORT &= (uint8_t) ~(1 << PWR_REQ_BIT); sei();}
#define PWR_REQ_GET()		((PWR_REQ_PIN & (1 << PWR_REQ_BIT)) ? true : false)
#define RESET_REQUEST_GET()	((RESET_REQUEST_PIN & (1 << RESET_REQUEST_BIT)) ? true : false)

// Other useful macros
#define	IRQ_PWR_OK_ENABLE()					GIMSK |= (1 << PCIE0);
#define	IRQ_PWR_OK_DISABLE()				GIMSK &= ~(1 << PCIE0);

// # Of timer ticks (ever increasing)
static volatile uint16_t sg_u16TimerTicks = 0;

// Amount of ticks that PWR_GR is held for
static volatile uint8_t sg_u8PWRRQAssertedTickTime = 0;

// Returns true if the EEPROM is busy
static bool EEPROMBusy(void)
{
	if (EECR & (1 << EEPE))
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

// Write a byte to the EEPROM. NOTE: Consumers of this function
// must call EEPROMBusy() repeatedly until it returns false before
// calling this function.
static void EEPROMWrite(uint8_t u8Address,
						uint8_t u8Data)
{
	// Set programming mode
	EECR = (0 << EEPM1) | (0 << EEPM0);
	
	// Set the address
	EEAR = u8Address;

	// Now the data register	
	EEDR = u8Data;
	
	// Start write
	EECR |= (1 << EEMPE);
	EECR |= (1 << EEPE);
}

// Read a byte from the EEPROM
static uint8_t EEPROMRead(uint8_t u8Address)
{
	// Set the address
	EEAR = u8Address;

	// Flag a read
	EECR |= (1 << EERE);
	
	return(EEDR);	
}

// Power and reset button debounce
static uint8_t sg_u8PowerButtonDebounce;
static bool sg_bPowerButtonState;
static uint16_t sg_u16PowerOverrideTimer;
static uint8_t sg_u8ResetButtonDebounce;
static bool sg_bResetButtonState;

// And generic, logical states
static bool sg_bPowerRequest;				// Set true if someone has requested a power request, either power button or host
static bool sg_bPowerOverrideHeld;			// Set true if someone held the power button long enough for an override
static bool sg_bResetRequest;				// Set true if someone has requested a reset request, either reset button or host
static bool sg_bCoordinatedShutdownMode;	// Set true if we've had a coordinated shutdown mode requested
static bool sg_bPowerFault;					// Set true if there was a power fault condition (unexpected loss of PSPWRGOOD)

// Read the timer ticks
static uint16_t TimerReadTicks(void)
{
	uint16_t u16Ticks;
	
	cli();
	u16Ticks = sg_u16TimerTicks;
	sei();
	return(u16Ticks);
}

// LED Pattern for various LED states
typedef struct SAlertLEDPattern
{
	const uint16_t u16PatternLengthInBits;
	const uint8_t *pu8PatternBase;
} SAlertLEDPattern;

static const uint8_t sg_u8LEDOff[] =
{
	0x00
};

static const uint8_t sg_u8LEDOn[] =
{
	0x01
};

static const uint8_t sg_u8LEDPoweringUp[] =
{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t sg_u8LEDPowerFault[] =
{
	0xff, 0xff, 0x0f, 0x0, 0x0, 0x0, 0xfc, 0xff, 0x3f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x00
};

// Array of LED patterns/stipples
static const SAlertLEDPattern sg_sLEDAlertPatterns[] =
{
	{1, sg_u8LEDOff},			// EALERT_OFF
	{100, sg_u8LEDPoweringUp},	// EALERT_POWERING_UP	(50% duty cycle per second)
	{150, sg_u8LEDPowerFault},	// EALERT_POWER_FAILURE
	{100, sg_u8LEDPoweringUp},	// EALERT_POWER_DOWN_PENDING (same as up)
	{1, sg_u8LEDOn},			// EALERT_ON
};

static const uint8_t *sg_pu8LEDStippleBase;
static uint16_t sg_u16LEDIndex;
static uint16_t sg_u16LEDIndexMax;

static void AlertSetState(EAlerts eAlertState)
{
	cli();
	sg_pu8LEDStippleBase = sg_sLEDAlertPatterns[eAlertState].pu8PatternBase;
	sg_u16LEDIndexMax = sg_sLEDAlertPatterns[eAlertState].u16PatternLengthInBits;
	sg_u16LEDIndex = 0;
	sei();
}

// Timer interrupt. Called once per TIMER_PERIOD milliseconds
ISR(TIM0_COMPA_vect, ISR_NOBLOCK)
{
	bool bState;
	
	sg_u16TimerTicks++;
	
	// Handle LED state
	if (sg_pu8LEDStippleBase[sg_u16LEDIndex >> 3] & (1 << (sg_u16LEDIndex & 7)))
	{
		bState = true;
	}
	else
	{
		bState = false;
	}
	
	// Set the power LED's state
	LED_POWER_SET(bState);
	
	++sg_u16LEDIndex;
	if (sg_u16LEDIndex >= sg_u16LEDIndexMax)
	{
		sg_u16LEDIndex = 0;
	}
	
	// Handle power button
	bState = BUTTON_POWER();
	if (bState != sg_bPowerButtonState)
	{
		sg_u8PowerButtonDebounce++;
		if (sg_u8PowerButtonDebounce >= (BUTTON_DEBOUNCE / TIMER_TICK_MS))
		{
			// We've changed state!

			// If not pressed to pressed..
			if (bState)			
			{
				// We've gone from not pressed to pressed
				sg_bPowerRequest = true;
				sg_u16PowerOverrideTimer = 0;
			}
			else
			{
				// We've gone from pressed to released
			}
			
			// Record our new state
			sg_bPowerButtonState = bState;
			sg_u8PowerButtonDebounce = 0;
			sg_u16PowerOverrideTimer = 0;
			sg_bPowerOverrideHeld = false;
		}
	}
	else
	{
		// It's steady. Keep the debounce
		sg_u8PowerButtonDebounce = 0;
	}
	
	// If the power button is held and continues to be held, then increase our override counter
	// until it overflows.
	if (sg_bPowerButtonState && bState)
	{
		if (sg_u16PowerOverrideTimer >= (POWER_BUTTON_OVERRIDE / TIMER_TICK_MS))
		{
			// No further!
		}
		else
		{
			sg_u16PowerOverrideTimer++;
			if (sg_u16PowerOverrideTimer >= (POWER_BUTTON_OVERRIDE / TIMER_TICK_MS))
			{
				sg_bPowerOverrideHeld = true;
			}
		}
	}
	
	// And the reset button
	bState = BUTTON_RESET();
	if (bState != sg_bResetButtonState)
	{
		sg_u8ResetButtonDebounce++;
		if (sg_u8ResetButtonDebounce >= (BUTTON_DEBOUNCE / TIMER_TICK_MS))
		{
			// We've changed state!

			// If not pressed to pressed..
			if (bState)
			{
				// Assert reset
				RESET_SET(true);
			}
			else
			{
				// We've gone from pressed to released
				sg_bResetRequest = true;
			}
			
			// Record our new state
			sg_bResetButtonState = bState;
			sg_u8ResetButtonDebounce = 0;
		}
	}
	else
	{
		// It's steady. Keep the debounce
		sg_u8ResetButtonDebounce = 0;
	}

	// Now power request/grant but only if we're monitoring state.
	if (ESTATE_MONITOR == sg_eState)
	{
		bState = PWR_REQ_GET();
		
		// Did we hit a 0->1 transition?
		if ((false == sg_bPWRRQLastState) &&
			(bState))
		{
			// 0->1 transition
			sg_u8PWRRQAssertedTickTime = 0;				
		}
		
		// How about a 1->0 transition
		if ((sg_bPWRRQLastState) &&
			(false == bState))
		{
			// 1->0 transition
			
			// If we're between PWR_GR_REQUEST_MIN and PWR_GR_REQUEST_MAX, then we set
			// the fact that we're in controlled shutdown mode
			if ((sg_u8PWRRQAssertedTickTime >= (PWR_GR_REQUEST_MIN / TIMER_TICK_MS)) &&
				(sg_u8PWRRQAssertedTickTime <= (PWR_GR_REQUEST_MAX / TIMER_TICK_MS)))
			{
				// We've now armed coordinated shutdown mode!
				sg_bCoordinatedShutdownMode = true;
			}
			else
			{
				// Otherwise ignore the pulse. Either too long or too short.
			}
		}

		// Record it!
		sg_bPWRRQLastState = bState;
		
		// If we're not asserted, 
		if (false == bState)
		{
			sg_u8PWRRQAssertedTickTime = 0;
		}
		else
		{
			// Increment time up to PWR_GT_POWER_DOWN milliseconds
			if (sg_u8PWRRQAssertedTickTime <= (PWR_GR_POWER_DOWN / TIMER_TICK_MS))
			{
				sg_u8PWRRQAssertedTickTime++;
			}
			else
			{
				// If we're in controlled shutdown mode, we need to power off
				if (sg_bCoordinatedShutdownMode)
				{
					
				}
			}
		}
	}
	else
	{
		// We're in a state where we don't want to monitor the PWR RQ pin, so
		// keep the counter clear so nothing glitches
		sg_u8PWRRQAssertedTickTime = 0;
	}
}

// Look for PWR_GOOD going away
ISR(PCINT0_vect, ISR_NOBLOCK) 
{
	if (PWR_OK())
	{
		// Don't care about positive going power changes
	}
	else
	{
		// Stop both interrupts
		IRQ_PWR_OK_DISABLE();		// Shut off PWR_OK interrupts
		
		// Power fault! Assert reset.
		RESET_SET(true);
		
		// Deassert PSON
		PSON_SET(false);
		
		// Signal a power fault
		sg_bPowerFault = true;
	}
}

// EEPROM Definitions for power state retention
#define POWER_STATE_RETENTION_ADDRESS	0x00
#define POWER_STATE_ON					0x4c
#define POWER_STATE_OFF					0xeb

int main(void)
{
	bool bResetAssert = false;
	bool bPowerOn = false;
	uint16_t u16Timestamp;
	
	cli();

	// Set up directions on critical pins
	
	// ********** INPUTS
	
	// Power button
	BUTTON_POWER_DDR &= (uint8_t) ~(1 << BUTTON_POWER_BIT);
	BUTTON_POWER_PORT |= (1 << BUTTON_POWER_BIT);
	
	// Reset button
	BUTTON_RESET_DDR &= (uint8_t) ~(1 << BUTTON_RESET_BIT);
	BUTTON_RESET_PORT |= (1 << BUTTON_RESET_BIT);
	
	// PWR_OK
	PWR_OK_DDR &= (uint8_t) ~(1 << PWR_OK_BIT);
	PWR_OK_PORT |= (1 << PWR_OK_BIT);
	
	// Reset request - input with no pullup
	RESET_REQUEST_DDR &= (uint8_t) ~(1 << RESET_REQUEST_BIT);
	RESET_REQUEST_PORT &= (uint8_t) ~(1 << RESET_REQUEST_BIT);
	
	// ********** OUTPUTS
	
	// Power LED (power status)
	LED_POWER_DDR |= (1 << LED_POWER_BIT);
	LED_POWER_PORT |= (1 << LED_POWER_BIT);
	
	// PSON (tells the power supply to turn on)
	PSON_DDR |= (1 << PSON_BIT);
	
	// Reset drive
	RESET_DDR |= (1 << RESET_BIT);
	
	// First thing - read PWROK signal from the power supply. If it's asserted,
	// we need to be certain we're asserting PSON in order to prevent glitching.

	// If PWR_OK is asserted, then keep 
	if (PWR_OK())
	{
		// Power good is OK, assert PSON.
		
		// Drive PB0 low to keep PSON
		PSON_SET(true);
		
		// Jump to the powered-on state
		sg_eState = ESTATE_MONITOR;
	}
	else
	{
		// Power good is not asserted, deassert PSON and assert reset.
		// Drive PB0 high to deassert PSON
		PSON_SET(false);
		
		// Ensure reset gets asserted
		bResetAssert = true;
	}
	
	// Assert reset accordingly
	RESET_SET(bResetAssert);

	// Turn off the power LED
	LED_POWER_SET(false);

	// Turn off the power LED
	AlertSetState(EALERT_OFF);
	
	// Sample the state of the power and reset buttons
	sg_bPowerButtonState = BUTTON_POWER();
	sg_bResetButtonState = BUTTON_RESET();
	
	// Ensure the power request/grant signal is set as an input and not an output
	PWRGR_REQ_SET(false);
	
	// Set clock prescaler to /1 for 8mhz internal operation
	CLKPR = (1 << CLKPCE);
	CLKPR = 0;
	
	// Now init timer 0
	
	// TCCR0A - pins disconnected, CTC mode, reset at OCRA
	TCCR0A = (1 << WGM01);
	
	// Prescaler to /1024
	TCCR0B = (1 << CS02) | (1 << CS00);
	
	// Counter values
	TCNT0 = 0;
	OCR0A = TIMER_DIVISOR;
	
	// Init interrupt on A
	TIMSK0 |= (1 << OCIE0A);
	
	// Enable all interrupts!
	sei();
	
	u16Timestamp = TimerReadTicks();
	
	// Loop forever
	for (;;)	
	{
		switch (sg_eState)
		{
			case ESTATE_INIT:
			{
				uint8_t u8Data;
				
				// Figure out of power state retention should shut off or turn on power.
				u8Data = EEPROMRead(POWER_STATE_RETENTION_ADDRESS);
				if (POWER_STATE_ON == u8Data)
				{
					// Power on!
					sg_eState = ESTATE_POWER_ON;
				}
				else
				{
					// No matter the state, power off and record as such
					sg_eState = ESTATE_POWER_OFF;
				}
				
				break;
			}
			
			case ESTATE_MONITOR:
			{
				// Here's where we're looking for things to do
				
				// If power is on and they're holding the power button down, do a forced shutoff.
				if ((sg_bPowerOverrideHeld) &&
					(bPowerOn))
				{
					sg_bPowerRequest = true;
				}
				
				// If they're pressing the power button and power is currently off, turn power on
				if (sg_bPowerRequest)
				{
					// If power is off, turn power on
					if (false == bPowerOn)
					{
						// Power is off. Turn it on.
						sg_eState = ESTATE_POWER_ON;
					}
					else
					{
						// We're doing a power off, but what kind? If it's not a coordinated shutdown, then
						// just power off
						if ((sg_bCoordinatedShutdownMode) &&
							(false == sg_bPowerOverrideHeld))
						{
							// Assert the power grant/request signal
							PWRGR_REQ_SET(true);
							
							// Now make sure the pulse lasts long enough
							u16Timestamp = TimerReadTicks();
							sg_eState = ESTATE_WAIT_RESET_DEASSERT;
						}
						else
						{
							// Shut things off
							sg_eState = ESTATE_POWER_OFF;
						}
						
						EEPROMWrite(POWER_STATE_RETENTION_ADDRESS,
									POWER_STATE_OFF);
					}
					
					// Clear the fact that we've caught a power request
					sg_bPowerRequest = false;
				}

				// If we've released reset and power is on, then deassert after the reset pulse time
				if (sg_bResetRequest)
				{
					// Only pay attention to it if power is on
					if (bPowerOn)
					{
						// Assert reset
						RESET_SET(true);
										
						// We've released a reset button! Let's delay, then come back to monitor.
						u16Timestamp = TimerReadTicks();
						sg_eState = ESTATE_WAIT_RESET_DEASSERT;
					}
					
					// We always clear the reset button release state
					sg_bResetRequest = false;
				}
				
				break;
			}
			
			case ESTATE_POWER_ON:
			{
				// We're powering on! 

				IRQ_PWR_OK_DISABLE();		// Shut off PWR_OK interrupts
								
				// Set the power LED that we're powering up
				AlertSetState(EALERT_POWERING_UP);
				
				// Assert reset
				RESET_SET(true);
				
				// Turn PSON
				PSON_SET(true);
				
				// Sample our timestamp
				u16Timestamp = TimerReadTicks();
				
				// It's not a coordinated shutdown if we're powering up
				sg_bCoordinatedShutdownMode = false;
				
				// Now we wait for PWROK
				sg_eState = ESTATE_WAIT_PWROK; 				
				break;
			}
			
			case ESTATE_WAIT_PWROK:
			{
				// Waiting for a power OK signal after being powered on
				if (PWR_OK())
				{
					// We have power OK! Deassert reset and change states

					// Deassert reset
					RESET_SET(false);
					
					// Sample our timestamp
					u16Timestamp = TimerReadTicks();
					
					// And jump to reset
					sg_eState = ESTATE_WAIT_RESET_DEASSERT;

					// Write the fact that we're powered up
					EEPROMWrite(POWER_STATE_RETENTION_ADDRESS,
								POWER_STATE_ON);
								
					// Set the LED state accordingly
					AlertSetState(EALERT_ON);
				}
				else
				if ((TimerReadTicks() - u16Timestamp) >= (PSON_TO_PWROK / TIMER_TICK_MS))
				{
					// Fault condition
					
					// Force a power off condition
					sg_eState = ESTATE_POWER_OFF;

					// Indicate a power fault/failure
					sg_bPowerFault = true;
				}
				else
				{
					// Keep waiting until we have our signal
				}
				break;
			}
			
			case ESTATE_WAIT_RESET_DEASSERT:
			{
				// If we've held reset long enough, deassert it and consider ourselves
				// powered on
				if ((TimerReadTicks() - u16Timestamp) >= (PWROK_TO_RESET / TIMER_TICK_MS))
				{
					// We are powered up now. Let's make sure the EEPROM isn't busy
					if (EEPROMBusy()) 
					{
						// Fall through
					}
					else
					{
						sg_eState = ESTATE_MONITOR;
						
						// Indicate we're powered on
						bPowerOn = true;
						
						// Configure the INT0 interrupt so we catch power grant/requests
						MCUCR |= 1 << ISC01;		// Configure INT0 to interrupt on falling edge of INT0 pin
						MCUCR &= ~(1 << ISC00);		// .. interrupt at any logical change at INT0
						
						// Now PCINT0 to catch PWR_OK on PCINT7
						PCMSK0 |= (1 << PCINT7_BIT);
						PCMSK1 |= (1 << PCINT10_BIT);
						IRQ_PWR_OK_ENABLE();
						
						// Deassert reset
						RESET_SET(false);
					}
				}
							
				break;
			}
			
			case ESTATE_POWER_OFF:
			{
				IRQ_PWR_OK_DISABLE();		// Shut off PWR_OK interrupts
				
				// Assert reset
				RESET_SET(true);
				
				// Deassert PWRON
				PSON_SET(false);

				// Release the power/grant request				
				PWRGR_REQ_SET(false);
	
				// Wait for PWROK to go away
				if (PWR_OK())				
				{
					// Keep waiting
				}
				else
				// Wait until the EEPROM isn't busy
				if (EEPROMBusy())
				{
					// Keep looping
				}
				else
				{
					// Indicate power is now shut off
					sg_eState = ESTATE_MONITOR;
					
					// Indicate we're powered off
					bPowerOn = false;
					
					// Clear any pending activities
					sg_bPowerRequest = false;
					sg_bPowerOverrideHeld = false; 
					sg_bCoordinatedShutdownMode = false;
					
					if (sg_bPowerFault)
					{
						sg_bPowerFault = false;
						
						// Indicate fault state
						AlertSetState(EALERT_POWER_FAILURE);
					}
					else
					{
						// Set the LED state accordingly
						AlertSetState(EALERT_OFF);
					}
				}
				
				break;
			}
			
			case ESTATE_WAIT_REQUEST_TIME:
			{
				// Wait until we there's enough time that has passed for the power request
				if ((TimerReadTicks() - u16Timestamp) >= (PWR_RQ_POWER_HOST / TIMER_TICK_MS))
				{
					// Deassert it and go back to monitoring
					PWRGR_REQ_SET(false);

					sg_eState = ESTATE_MONITOR;
				}
				
				break;
			}

			default:
			{
				// Shouldn't get here
				while (1);
			}
		}
	}
}