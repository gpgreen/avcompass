#include <inttypes.h>
#include <avr/io.h>

#include "gpio.h"

void gpio_setup(void)
{
	// setup the led pins, level lo
	DDR_LED4 |= _BV(P_LED4);
	DDR_LED5 |= _BV(P_LED5);

    // setup the SCK led, in case we can't initialize the MCP2515
    DDRB |= _BV(7);
}

/*-----------------------------------------------------------------------*/

// turn on/off the leds

void led4_on(void)
{
	PORT_LED4 |= _BV(P_LED4);
}

void led4_off(void)
{
	PORT_LED4 &= ~_BV(P_LED4);
}

void led5_on(void)
{
	PORT_LED5 |= _BV(P_LED5);
}

void led5_off(void)
{
	PORT_LED5 &= ~_BV(P_LED5);
}

