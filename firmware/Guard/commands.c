#include "commands.h"
#include <string.h>
command_t command_feed(command_input_t *input, char byte)
{
    if (byte == '\r') return CMD_NONE;
    if (byte != '\n' && ((unsigned char)byte < 32 || (unsigned char)byte > 126)) {
        input->overflow = true;
        return CMD_NONE;
    }
    if (byte != '\n') {
        if (input->length < sizeof input->text - 1) input->text[input->length++] = byte;
        else input->overflow = true;
        return CMD_NONE;
    }
    input->text[input->length] = '\0';
    static const char *names[] = {"status", "arm", "disarm", "probe", "nack",
        "journal", "help", "imu 68", "imu 69", "stats", "stats reset", "persist", "history",
        "sample 20", "sample 10", "sample 5", "sample 2", "sample 1"};
    command_t result = input->length == 0 ? CMD_NONE : CMD_INVALID;
    if (!input->overflow) {
        for (unsigned i = 0; i < sizeof names / sizeof names[0]; ++i)
            if (strcmp(input->text, names[i]) == 0) result = (command_t)(CMD_STATUS + i);
    } else result = CMD_INVALID;
    input->length = 0;
    input->overflow = false;
    return result;
}
