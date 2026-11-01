#ifndef LOCKBOX_COARSE_CLOCK_H
#define LOCKBOX_COARSE_CLOCK_H
#include <stdint.h>

/* Unsigned multiplication deliberately wraps modulo 2^32 microseconds.
 * Signed deadline comparisons require intervals shorter than 2^31 us. */
static inline uint32_t coarse_clock_us(uint32_t tick_ms)
{
    return tick_ms * UINT32_C(1000);
}
#endif
