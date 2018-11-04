# Compass

Firmware project for compass hardware. The firmware runs on
a custom pcb using an atmel ATmega644 MCU with a MCP2515 CAN chip and
MCP 2551 transceiver. The magnetometer is a HMC5883L.

## UAVCAN

The protocol used on the CAN bus is UAVCAN. The library used is libcanard.

## pcb design
The pcb board is designed in GEDA pcb and gschem. The project is a [legacy github project](https://github.com/gpgreen/AvHardware/tree/master/hardware/compass)

## fuse setting for atmega644
 * External Crystal Oscillator, start up + 16ck + 4.1ms
 * clock prescaler divide by 8
 * clock output on port B to drive MCP2515 device
 * serial programming enabled
 * brown-out at 4.3V
 * Fuses are: Low=0x27 Hi=0xd9 Ext=0xfc
 * avrdude settings:
 ** -U lfuse:w:0x27:m -U hfuse:w:0xd9:m -U efuse:w:0xfc:m
 * from http://www.engbedded.com/fusecalc/

## Makefile

Useful targets for GNU make

* all: make software, don't change hardware in any way
* read-fuses: read current state of fuses on atmega644 using avrdude
* write-fuses: write fuses using avrdude
* program: download hex file to device, using avrdude

Complete list of targets is in the prolog of the Makefile