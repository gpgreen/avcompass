/*
 * blink led's on the compass pcb
 */

#include "defs.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "globals.h"
#include "gpio.h"
#include "uart.h"

/*-----------------------------------------------------------------------*/

// timer flags

// 10 hz timer flag
volatile uint8_t g_timer10_set;

// 1 hz timer flag
volatile uint8_t g_timer1hz_set;

/*-----------------------------------------------------------------------*/

// io
static FILE uartstream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar,
                                           _FDEV_SETUP_RW);

/*-----------------------------------------------------------------------*/

// uart buffers
uint8_t tx_fifo_buffer[TX_FIFO_BUFFER_SIZE];
uint8_t rx_fifo_buffer[RX_FIFO_BUFFER_SIZE];

/*-----------------------------------------------------------------------*/

/*
 * Timer compare output 1A interrupt
 */
ISR(TIMER1_COMPA_vect)
{
    static int count = 0;
    g_timer10_set = 1;
    if (++count == 10)
    {
        g_timer1hz_set = 1;
        count = 0;
    }
}

/*-----------------------------------------------------------------------*/

void
system_start(void)
{
    // first set the clock prescaler change enable
	CLKPR = _BV(CLKPCE);
	// now set the clock prescaler to clk
	CLKPR = 0;
}

/*-----------------------------------------------------------------------*/

void
ioinit(void)
{
	gpio_setup();

	led4_on();
	
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
	uart_init(TX_FIFO_BUFFER_SIZE, tx_fifo_buffer,
              RX_FIFO_BUFFER_SIZE, rx_fifo_buffer);

    printf_P(PSTR("\nCompass\nHardware: %d Software: %d.%d\n"),
               HARDWARE_REVISION, APP_VERSION_MAJOR, APP_VERSION_MINOR);
    printf_P(PSTR("Name: %s\n-----------------------------\n"),
               APP_NODE_NAME);
	printf_P(PSTR("ioinit complete\n"));

    led4_off();
}

/*-----------------------------------------------------------------------*/

int
main(void)
{
    system_start();

    stdout = &uartstream;
    stderr = &uartstream;
    stdin = &uartstream;

    sei();
    
    ioinit();

    while (1)
    {
        // 10 hz timer
        if (g_timer10_set)
        {
            g_timer10_set = 0;
            static int set = 0;
            if (set)
            {
                led4_on();
                set = 0;
            } else {
                led4_off();
                set = 1;
            }
        }

        // 1 hz timer
        if (g_timer1hz_set)
        {
            g_timer1hz_set = 0;
            static int set1 = 0;
            if (set1)
            {
                led5_on();
                set1 = 0;
            } else {
                led5_off();
                set1 = 1;
            }
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

