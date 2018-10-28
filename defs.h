/*
 * Define registers and pins
 * AtMega644
 * ahrs board
 */
#ifndef DEFS_H_
#define DEFS_H_

/*-----------------------------------------------------------------------*/

/*
 * Hardware and software revisions
 */
#define HARDWARE_REVISION 2
#define SOFTWARE_REVISION 1

/*-----------------------------------------------------------------------*/

/* functions available */
#define HAVE_UART_DEVICE (1)

/* define baud rate for serial comm */
#define BAUD 57600

/*-----------------------------------------------------------------------*/

/* DEBUGGING */
       
// #define MCPDEBUG (1)
// #define CANDEBUG (1)
// #define CANAERODEBUG (1)
// #define HMCDEBUG (1)

/*-----------------------------------------------------------------------*/

/* LEDS */

#define DDR_LED4    DDRB
#define PORT_LED4   PORTB
#define P_LED4      3
       
#define DDR_LED5    DDRB
#define PORT_LED5   PORTB
#define P_LED5      4
       
/*-----------------------------------------------------------------------*/

/* MCP2515 */

/* MCP clock */
#define MCP2515_8MHZ (1)

/* CS pin */
#define DDR_CANCS   DDRA
#define PORT_CANCS  PORTA
#define P_CANCS     4

/* INT pin */
#define DDR_CANINT  DDRC
#define PORT_CANINT PORTC
#define PIN_CANINT  PINC
#define P_CANINT    6

/* Define software interrupt pin */
#define CAN_SW_INT  PCINT22
#define CAN_SW_INT_MASK_REG PCMSK2
#define CAN_SW_INT_ENABLE PCIE2
#define CAN_SW_INT_REG PCICR

/*-----------------------------------------------------------------------*/

/* I2C pins */
#define PORT_SDA    PORTC
#define P_SDA       1
#define PORT_SCL    PORTC
#define P_SCL       0

/* SPI pins */
#define DDR_SPI     DDRB
#define PORT_SPI    PORTB
#define P_MOSI      5
#define P_MISO      6
#define P_SCK       7

/*-----------------------------------------------------------------------*/

#endif  // DEFS_H_
