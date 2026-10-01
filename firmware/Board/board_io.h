#ifndef LOCKBOX_BOARD_IO_H
#define LOCKBOX_BOARD_IO_H
#include <stdbool.h>
#include "mpu6050.h"
bool board_io_start(void);
register_bus_t board_register_bus(void);
bool board_adc_read(uint16_t *counts);
bool board_button_pressed(void);
bool board_pir_level(void);
void board_alarm(bool active);
void lockbox_start(void);
void lockbox_poll(void);
#endif
