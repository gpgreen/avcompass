#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <inttypes.h>
#include "canaero.h"

// the can stack initialization struct
extern canaero_init_t g_ci;

// global state enumeration
enum state {INIT, LISTEN, ACTIVE};

// the global state
extern enum state g_state;

// compass enabled
extern int g_compass_enabled;

// magnetic heading
extern float g_magnetic_heading;

// error number
extern volatile uint8_t errcode;

// can stack offline
extern void offline(void);

// main error entry
extern void failed(uint8_t errcode);

#endif  // GLOBALS_H_
