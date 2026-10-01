#ifndef LOCKBOX_ANALOG_H
#define LOCKBOX_ANALOG_H
#include <stdint.h>
uint32_t adc_to_mv(uint16_t counts);
uint16_t median3(uint16_t a, uint16_t b, uint16_t c);
#endif
