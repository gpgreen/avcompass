/*
 * Fuse bits for AtMega644
 * Full swing oscillator, start up 16ck + 4.1ms, crystal occ, fast rising pwr
 * clock prescaler divide by 8
 * clock output on port b
 * serial programming enabled
 * brown-out at 4.3V
 * Low=0x27 Hi=0xd9 Ext=0xfc
 * avrdude settings:
 * -U lfuse:w:0x27:m -U hfuse:w:0xd9:m
 * -U efuse:w:0xfc:m
 * from http://www.engbedded.com/fusecalc/
 *
 * per the HMC5883L datasheet
 * X is facing backward, or away from the dsub connector
 * Y is to right wing, or right of dsub connector
 * Z is up
 */

#include "defs.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "i2cmaster.h"
#include "gpio.h"
#include <stdio.h>
#include <math.h>
#include "uart.h"
#include "timer.h"
#include "spi.h"
#include "hmc5883l.h"
#include "canaero.h"
#include "canaeromsg.h"
#include "canaero_filters.h"
#include "globals.h"

/*-----------------------------------------------------------------------*/

// set serial port to stdio
static FILE uart_ostr = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE uart_istr = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

// timer flags

// 10 hz timer flag
volatile uint8_t g_timer10_set;

// CAN controller flags
volatile uint8_t g_can_int_set;	/* set when CAN controller interrupt signaled */

// global error code
volatile uint8_t errcode;

// the can stack initialization struct
canaero_init_t g_ci;

// the state
enum state g_state;

// compass enabled
int g_compass_enabled;

// the magnetic compass heading
float g_magnetic_heading;

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

/*-----------------------------------------------------------------------*/

void led5_on(void)
{
	PORT_LED5 |= _BV(P_LED5);
}
void led5_off(void)
{
	PORT_LED5 &= ~_BV(P_LED5);
}

/*-----------------------------------------------------------------------*/

// this function is called if CAN doesn't work, otherwise
// use the failed fn below
// blinks continuously at 10 hz to show offline
void
offline(void)
{
	led5_off();
	uint8_t on = -1;
	led4_on();
	while (1) {
		// 10 hz timer
		if (g_timer10_set)
		{
			g_timer10_set = 0;
			if (on)
				led4_off();
			else
				led4_on();
			on = on? 0 : -1;
		}
	}
}

/*-----------------------------------------------------------------------*/

// main entry for showing board failure
// blinks the errorcode then a longer pause
void
failed(uint8_t err)
{
	led5_off();
	errcode = err;
	uint8_t count = 0;
	uint8_t pause = 0;
	led4_on();
	while (1) {
		// 10 hz timer
		if (g_timer10_set)
		{
			g_timer10_set = 0;
			if (pause) {
				--pause;
			} else {
				if (bit_is_set(count, 0))
					led4_off();
				else
					led4_on();
				if (++count == errcode * 2) {
					pause = 5;
					count = 0;
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------*/

void
ioinit(void)
{
	gpio_setup();

	timer_init();

	led4_on();
	led5_on();
	
    // setup the 10 hz timer
    // WGM12 = 1, CTC, OCR1A
    // CS11 = 1, CS10 = 1, fosc / 64
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
    // set interrupt to happen at 10hz
    OCR1AH = (F_CPU / 10 / 64) >> 8;
    OCR1AL = (F_CPU / 10 / 64) & 0xff;
    // set OC interrupt 1A
    TIMSK1 = _BV(OCIE1A);

	// setup the serial hardware
	uart_init();

	puts_P(PSTR("Compass"));
	printf_P(PSTR("Hardware: %d Software: %d\n-------------------------\n"),
			 HARDWARE_REVISION, SOFTWARE_REVISION);
	
	// spi needs to be setup first
	spi_init();

	puts_P(PSTR("spi initialized."));

	// i2c hardware setup
	i2c_init();
	
	puts_P(PSTR("i2c initialized."));

	// setup the can initialization struct
	g_ci.can_settings.speed_setting = CAN_250KBPS;
	g_ci.can_settings.loopback_on = 0;
	g_ci.can_settings.tx_wait_ms = 20;
	g_ci.node_id = 3;
	g_ci.svc_channel = 0;
	g_ci.nod_msg_templates = nod_msg_templates;
	g_ci.num_nod_templates = num_nod_templates;
	g_ci.nsl_dispatcher_fn_array = nsl_dispatcher_fn_array;
	g_ci.incoming_msg_dispatcher_fn = 0;

	// setup the filter's
	canaero_high_priority_service_filters(&g_ci);
	
	// now do the CAN stack
	errcode = canaero_init(&g_ci);
	if (errcode == CAN_FAILINIT)
		offline();
	led4_off();

	puts_P(PSTR("can initialized."));

	// self test the CAN stack
	errcode = canaero_self_test();
	if (errcode != CAN_OK)
		offline();

	puts_P(PSTR("canaero self-test complete."));
	
    // setup the magnetometer
    hmc5883l_init();

	puts_P(PSTR("hmc5883l initialized."));

    // run the self test
    if (hmc5883l_self_test())
		failed(2);

	puts_P(PSTR("hmc5883l self-test complete."));
	
	puts_P(PSTR("ioinit complete."));

	led5_off();
}

/*-----------------------------------------------------------------------*/

void compass_heading(float roll, float pitch)
{
	float cos_roll = cos(roll);
	float sin_roll = sin(roll);
	float cos_pitch = cos(pitch);
	float sin_pitch = sin(pitch);

	float x = -hmc5883l_raw_mag_data(0);
	float y = hmc5883l_raw_mag_data(1);
	float z = -hmc5883l_raw_mag_data(2);
	
	// tilt compensated mag x
	float mag_x = x * cos_pitch + y * sin_roll * sin_pitch
		+ z * cos_roll * sin_pitch;
	// tilt compensated mag y
	float mag_y = y * cos_roll - z * sin_roll;

	// magnetic heading
	g_magnetic_heading = atan2(-mag_y, mag_x);
	// convert to degrees
	g_magnetic_heading *= 180.0 / M_PI;
	// now convert to range 0 - 360
	if (g_magnetic_heading < 0)
		g_magnetic_heading += 360.0;
}

/*-----------------------------------------------------------------------*/

int
main(void)
{
	g_state = INIT;
	
	// first set the clock prescaler change enable
	CLKPR = _BV(CLKPCE);
	// now set the clock prescaler to clk/2
	CLKPR = _BV(CLKPS0);

	// stdout is the uart
	stdout = &uart_ostr;
	// stdin is the uart
	stdin = &uart_istr;
	
    ioinit();
	sei();

	g_state = LISTEN;

	int calc_flag = 0;
	
    while(1)
    {
		// write the eeprom if necessary
		canaero_write_eeprom_task();

        // 10 hz timer
        if (g_timer10_set)
        {
            g_timer10_set = 0;

			if (g_compass_enabled && (hmc5883l_read_data() != HMC5883L_OK))
				failed(3);

			if (g_state == ACTIVE && g_compass_enabled) {
				canaero_send_messages(0, 3);
				calc_flag = 1;
			}
        }
        // can controller interrupt
        if (g_can_int_set)
        {
            g_can_int_set = 0;

            // clear the interrupt flag on the device
			canaero_handle_interrupt();

			// poll for received messages
			canaero_poll_messages();
        }
		// calc_flag set, do the magnetic heading
		if (calc_flag)
		{
			calc_flag = 0;
			compass_heading(0.0, 0.0);
			canaero_send_messages(3, 4);
		}
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

/*
 * Pin Change Interrupt 2
 * Called on change of CAN_INT
 */
ISR(PCINT2_vect)
{
	// see if CAN_INT is lo, ie we got a falling edge
	if (bit_is_clear(PIN_CANINT, P_CANINT))
		g_can_int_set = 1;
}

/*-----------------------------------------------------------------------*/

/*
 * Timer compare output 1A interrupt
 */
ISR(TIMER1_COMPA_vect)
{
    g_timer10_set = 1;
}

/*-----------------------------------------------------------------------*/

