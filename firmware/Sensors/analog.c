#include "analog.h"
uint32_t adc_to_mv(uint16_t counts)
{
    if (counts > 4095) counts = 4095;
    return (uint32_t)counts * 3300 / 4095;
}
uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    return a > b ? a : b;
}
