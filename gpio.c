#include <inttypes.h>
#include <avr/io.h>

#include "gpio.h"

void gpio_setup(void)
{
	// setup the led pins, level lo
	DDR_LED4 |= _BV(P_LED4);
	DDR_LED5 |= _BV(P_LED5);

	// MCP2515 hardware
	
	// CAN_CS, output, level hi
	DDR_CANCS |= _BV(P_CANCS);
	PORT_CANCS |= _BV(P_CANCS);

	// CAN_INT, input, no level change
	DDR_CANINT &= ~(_BV(P_CANINT));

}
