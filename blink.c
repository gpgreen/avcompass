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
#include "timer1.h"

/*-----------------------------------------------------------------------*/

// timer flags

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

void timer1_compareA(void)
{
    static int count = 0;
    static int set1 = 0;
    if (++count == 9)
    {
        g_timer1hz_set = 1;
        count = 0;
        if (set1)
        {
            led5_on();
            set1 = 0;
        } else {
            led5_off();
            set1 = 1;
        }
    }

    static int set2 = 0;
    if (set2)
    {
        led4_on();
        set2 = 0;
    } else {
        led4_off();
        set2 = 1;
    }
    
}

static timer1_init_t timer1_settings = {
    .scale = CLK256,
    .compareA_cb = timer1_compareA,
    .compareB_cb = 0,
    // compare A triggers at 10Hz
    .compareA_val = (F_CPU / 10 / 256),
    .compareB_val = 0,
};

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

    timer1_init(&timer1_settings);
    
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
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

