#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <inttypes.h>

// error number
extern volatile uint8_t errcode;

// main error entry
extern void failed(uint8_t errcode);

// assert
#ifdef NDEBUG
# define COMPASS_ASSERT(EX)
#else
# define COMPASS_ASSERT(EX)  (void)((EX) || (__compass_assert(#EX, __FILE__, __LINE__),0))
#endif

#endif  // GLOBALS_H_
