#ifndef REGISTER_H_
#define REGISTER_H_

#include <stdint.h>
#include "defs.h"

/*-----------------------------------------------------------------------*/
/* length of registers arrary */
#define REGISTER_LEN                          10
/* length of registers array stored in eeprom, starts at position 0 */
#define PERM_REGISTER_LEN                     6

/*-----------------------------------------------------------------------*/
/* length of unique id */
#define UNIQUE_ID_LENGTH_BYTES                16

// registers
extern uint32_t registers[REGISTER_LEN];

// register offsets, masks and shifts
#define UAVCAN_NODE_ID_REG                    0
#define UAVCAN_NODE_ID_MASK                   0xff
#define UAVCAN_NODE_ID_SHIFT                  0

#define MAGNETOMETER_SENSOR_ID_REG            0
#define MAGNETOMETER_SENSOR_ID_MASK           0xff00
#define MAGNETOMETER_SENSOR_ID_SHIFT          8

#define UAVCAN_BUS_SPEED_REG                  1

#define UNIQUE_ID1_REG                        2

#define UNIQUE_ID2_REG                        3

#define UNIQUE_ID3_REG                        4

#define UNIQUE_ID4_REG                        5

#define COMPASS_STATE_REG                     6
#define COMPASS_STATE_MASK                    0xff
#define COMPASS_STATE_SHIFT                   0

#define UAVCAN_NODE_HEALTH_REG                6
#define UAVCAN_NODE_HEALTH_MASK               0xff00
#define UAVCAN_NODE_HEALTH_SHIFT              8

#define MAGNETOMETER_ENABLED_REG              6
#define MAGNETOMETER_ENABLED_MASK             0xff0000UL
#define MAGNETOMETER_ENABLED_SHIFT            16

#define RAW_MAG_SENSOR_X_REG                  7

#define RAW_MAG_SENSOR_Y_REG                  8

#define RAW_MAG_SENSOR_Z_REG                  9

// values encoded in registers

#define UAVCAN_NODE_MODE_OPERATIONAL                                0
#define UAVCAN_NODE_MODE_INITIALIZATION                             1

#define UAVCAN_NODE_HEALTH_OK                                       0
#define UAVCAN_NODE_HEALTH_WARNING                                  1
#define UAVCAN_NODE_HEALTH_ERROR                                    2
#define UAVCAN_NODE_HEALTH_CRITICAL                                 3

extern void initialize_registers(void);
extern void write_registers_eeprom(void);
extern void dump_register_state_uart(void);

extern uint8_t get_register(int regnum, int shift);
extern void set_register(int regnum, int shift, uint8_t val);

extern int16_t get_register_i16(int regnum, uint32_t mask, int shift);
extern void set_register_i16(int regnum, int shift, int16_t val);

extern uint32_t get_register_u32(int regnum);
extern void set_register_u32(int regnum, uint32_t val);

extern float get_register_float(int regnum);
extern void set_register_float(int regnum, float val);

#endif
