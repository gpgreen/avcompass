/*
 * per the HMC5883L datasheet
 * X is facing backward, or away from the dsub connector
 * Y is to right wing, or right of dsub connector
 * Z is up
 */

#include "defs.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>

#include "globals.h"
#include "i2cmaster.h"
#include "gpio.h"
#include "register.h"
#include "uart.h"
#include "timer.h"
#include "hmc5883l.h"
#include "can_func.h"

/*-----------------------------------------------------------------------*/

// timer flags

// 10 hz timer flag
volatile uint8_t g_timer10_set;

// 1 hz timer flag
volatile uint8_t g_timer1hz_set;

/*-----------------------------------------------------------------------*/

// the compass device
struct hmc5883l_dev_t compass_dev = {
    .sensor_sign = {1, 1, 1},
};

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

// implementation of assert
#ifndef NDEBUG
void __compass_assert(const char* msg, const char* file, int line)
{
    printf_P(PSTR("Assertion failed: %s at %s, line %d\nexecution halted\n"), msg, file, line);
    failed(1);
}
#endif

/*-----------------------------------------------------------------------*/

// main entry for showing board failure
// blinks the errorcode then a longer pause
void
failed(uint8_t err)
{
	uint8_t count = 0;
	uint8_t pause = 0;
    int delay = 2;
	led4_on();
    led5_on();
	while (1)
    {
		// 10 hz timer
		if (g_timer10_set)
		{
			g_timer10_set = 0;
            if (delay--)
                continue;
            delay = 2;
			if (pause)
            {
				--pause;
			}
            else
            {
				if (bit_is_set(count, 0))
                {
                    led4_off();
					led5_off();
                }
                else
                {
					led4_on();
                    led5_on();
                }
				if (++count == err * 2)
                {
					pause = 10;
					count = 0;
				}
			}
		}
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

	timer_init();

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

    uart_putchar('H', NULL);
    
    printf_P(PSTR("\nCompass\nHardware: %d Software: %d.%d\nName: %s\n-----------------------------\n"),
               HARDWARE_REVISION, APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_NODE_NAME);

    // print out registers
    dump_register_state_uart();

	// i2c hardware setup
	i2c_init();
	printf_P(PSTR("i2c initialized\n"));

    // setup the magnetometer
    if (hmc5883l_init() == HMC5883L_FAIL)
    {
        set_register(MAGNETOMETER_ENABLED_REG, MAGNETOMETER_ENABLED_SHIFT, 0);
		failed(1);
    }
	printf_P(PSTR("hmc5883l initialized\n"));

    // run the self test
    if (hmc5883l_self_test())
    {
        set_register(MAGNETOMETER_ENABLED_REG, MAGNETOMETER_ENABLED_SHIFT, 0);
		failed(2);
    }
    set_register(MAGNETOMETER_ENABLED_REG, MAGNETOMETER_ENABLED_SHIFT, 1);
	printf_P(PSTR("hmc5883l self-test complete\n"));

    uint32_t bus_speed = get_register_u32(UAVCAN_BUS_SPEED_REG);
    printf_P(PSTR("can bus speed:%lu\n"), bus_speed);

    int err = initializeCanard(bus_speed);
    if (err)
    {
        printf_P(PSTR("can init failed err:%d\n"), err);
        failed(4);
    }
	printf_P(PSTR("can init complete\n"));

    uint8_t nodeid = get_register(UAVCAN_NODE_ID_REG, UAVCAN_NODE_ID_SHIFT);
    uint8_t state = get_register(COMPASS_STATE_REG, COMPASS_STATE_SHIFT);
    uint8_t health = get_register(UAVCAN_NODE_HEALTH_REG, UAVCAN_NODE_HEALTH_SHIFT);
    uint8_t sensor_id = get_register(MAGNETOMETER_SENSOR_ID_REG, MAGNETOMETER_SENSOR_ID_SHIFT);

    printf_P(PSTR("node id: %d can state:%d can health:%d sensor id:%d\n"), nodeid, state, health,
        sensor_id);
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
    
    initialize_registers();

    sei();
    
    ioinit();

    processNodeId();

    while (1)
    {
        // 10 hz timer
        if (g_timer10_set)
        {
            g_timer10_set = 0;

            int cenabled = get_register(MAGNETOMETER_ENABLED_REG, MAGNETOMETER_ENABLED_SHIFT);
			if (cenabled && (hmc5883l_read_data(&compass_dev) != HMC5883L_OK))
                failed(8);

            set_register_float(RAW_MAG_SENSOR_X_REG, hmc5883l_raw_mag_data(&compass_dev, 0));
            set_register_float(RAW_MAG_SENSOR_Y_REG, hmc5883l_raw_mag_data(&compass_dev, 1));
            set_register_float(RAW_MAG_SENSOR_Z_REG, hmc5883l_raw_mag_data(&compass_dev, 2));

			if (cenabled)
            {
                //sendRawMagData();
			}
        }

        // 1 hz timer
        if (g_timer1hz_set)
        {
            g_timer1hz_set = 0;

            process1HzTasks(jiffie());
        }

        // every loop
        processTxRxOnce();
        
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

