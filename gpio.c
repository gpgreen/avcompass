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
