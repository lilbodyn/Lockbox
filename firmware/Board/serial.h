#ifndef LOCKBOX_SERIAL_H
#define LOCKBOX_SERIAL_H
#include <stdbool.h>
#include <stdint.h>
bool serial_line(const char *format, ...);
void serial_pump(void);
bool serial_receive(char *byte);
uint32_t serial_dropped_lines(void);
#endif
