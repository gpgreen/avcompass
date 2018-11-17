#include <string.h>
#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <canard.h>
#include "register.h"

// the registers data array
uint32_t registers[REGISTER_LEN];

// the unique id - stored in the eeprom, modified during program load into mcu
uint8_t eeprom_register_addr[PERM_REGISTER_LEN * 4] EEMEM = {
    // register 0
//    CANARD_BROADCAST_NODE_ID, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00,
    // register 1 - uavcan bus speed
    0x90, 0xd0, 0x03, 0x00,
    // register 2 - first qtr of unique id
    0x00, 0x01, 0x02, 0x03,
    // register 3 - second qtr of unique id
    0x04, 0x05, 0x06, 0x07,
    // register 4 - third qtr of unique id
    0x08, 0x09, 0x0a, 0x0b,
    // register 5 - fourth qtr of unique id
    0x0c, 0x0d, 0x0e, 0x0f
};

/*-----------------------------------------------------------------------*/

void initialize_registers(void)
{
    eeprom_busy_wait();
    eeprom_read_block(registers, eeprom_register_addr, PERM_REGISTER_LEN * 4);

    memset(&registers[PERM_REGISTER_LEN], 0, (REGISTER_LEN - PERM_REGISTER_LEN) * 4);
}

void write_registers_eeprom(void)
{
    eeprom_busy_wait();
    eeprom_write_block(registers, eeprom_register_addr, PERM_REGISTER_LEN * 4);
}

void dump_register_state_uart(void)
{
    printf_P(PSTR("Contents of registers:\n"));
    for (int i=0; i<REGISTER_LEN; ++i)
    {
        printf("reg[%02d]: ", i);
        uint8_t* byte = (uint8_t*)(&registers[i]);
        for (int j=0; j<4; ++j)
        {
            printf("%02x,", *(byte + j));
        }
        printf("\n");
    }
    printf("\n");
}

uint8_t get_register(int regnum, int shift)
{
    uint8_t* addr = (uint8_t*)&registers[regnum];
    addr += shift;
    return *addr;
}

void set_register(int regnum, int shift, uint8_t val)
{
    uint8_t* addr = (uint8_t*)&registers[regnum];
    addr += shift;
    *addr = val;
}

int16_t get_register_i16(int regnum, uint32_t mask, int shift)
{
    return (int16_t)((registers[regnum] & mask) >> shift);
}

void set_register_i16(int regnum, int shift, int16_t val)
{
    uint8_t* addr = (uint8_t*)&registers[regnum];
    addr += shift;
    int16_t* valaddr = (int16_t*)addr;
    *valaddr = val;
}

uint32_t get_register_u32(int regnum)
{
    return registers[regnum];
}

void set_register_u32(int regnum, uint32_t val)
{
    registers[regnum] = val;
}


float get_register_float(int regnum)
{
    return (float)registers[regnum];
}

void set_register_float(int regnum, float val)
{
    registers[regnum] = (uint32_t)val;
}

