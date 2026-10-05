#ifndef LOCKBOX_COMMANDS_H
#define LOCKBOX_COMMANDS_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { CMD_NONE, CMD_STATUS, CMD_ARM, CMD_DISARM, CMD_PROBE, CMD_NACK,
    CMD_JOURNAL, CMD_HELP, CMD_IMU68, CMD_IMU69, CMD_INVALID } command_t;
typedef struct { char text[32]; uint8_t length; bool overflow; } command_input_t;
command_t command_feed(command_input_t *input, char byte);
#endif
