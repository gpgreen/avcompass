/*
 * Define registers and pins
 * AtMega644
 * compass board
 */
#ifndef DEFS_H_
#define DEFS_H_

/*-----------------------------------------------------------------------*/

/*
 * Hardware and software revisions
 */
#define HARDWARE_REVISION               2
#define APP_VERSION_MAJOR               1
#define APP_VERSION_MINOR               0
#define APP_NODE_NAME                   "com.bit-builder.compass"

/*-----------------------------------------------------------------------*/

/* uart */
/* define baud rate for serial comm */
#define BAUD                            115200

/* size of uart buffers */
#define TX_FIFO_BUFFER_SIZE             128
#define RX_FIFO_BUFFER_SIZE             64

/*-----------------------------------------------------------------------*/

/* DEBUGGING */
       
// #define HMCDEBUG                                                  1

/*-----------------------------------------------------------------------*/

/* libcanard */
#define CANARD_MEMORY_POOL_SIZE         1024

/*-----------------------------------------------------------------------*/

/* LEDS */

#define DDR_LED4                        DDRB
#define PORT_LED4                       PORTB
#define P_LED4                          3
       
#define DDR_LED5                        DDRB
#define PORT_LED5                       PORTB
#define P_LED5                          4
       
/*-----------------------------------------------------------------------*/

/* MCP2515 */

/* CS pin */
#define DDR_CANCS                       DDRA
#define PORT_CANCS                      PORTA
#define P_CANCS                         4

/* INT pin */
#define DDR_CANINT                      DDRC
#define PORT_CANINT                     PORTC
#define PIN_CANINT                      PINC
#define P_CANINT                        6          

/* Define software interrupt pin */
#define CAN_SW_INT                      PCINT22
#define CAN_SW_INT_MASK_REG             PCMSK2
#define CAN_SW_INT_ENABLE               PCIE2
#define CAN_SW_INT_REG                  PCICR

/*-----------------------------------------------------------------------*/

/* I2C pins */
#define PORT_SDA                        PORTC
#define P_SDA                           1
#define PORT_SCL                        PORTC
#define P_SCL                           0

/* SPI pins */
#define DDR_SPI                         DDRB
#define PORT_SPI                        PORTB
#define P_MOSI                          5
#define P_MISO                          6
#define P_SCK                           7

/*-----------------------------------------------------------------------*/

#endif  // DEFS_H_
