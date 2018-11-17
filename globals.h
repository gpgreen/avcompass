#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <inttypes.h>

// leds
extern void led4_on(void);
extern void led4_off(void);
extern void led5_on(void);
extern void led5_off(void);

// main error entry
extern void failed(uint8_t errcode);

// define our implementation of assert function
#ifndef NDEBUG
extern void __compass_assert(const char* msg, const char* file, int line);
#endif

// assert
#ifdef NDEBUG
# define COMPASS_ASSERT(EX)
#else
# define COMPASS_ASSERT(EX)  (void)((EX) || (__compass_assert(#EX, __FILE__, __LINE__),0))
#endif

#endif  // GLOBALS_H_
