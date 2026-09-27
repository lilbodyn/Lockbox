#ifndef LOCKBOX_REGISTER_BUS_H
#define LOCKBOX_REGISTER_BUS_H
#include <stddef.h>
#include <stdint.h>
typedef enum { BUS_OK, BUS_NACK, BUS_TIMEOUT, BUS_IO_ERROR } bus_result_t;
typedef struct {
    void *context;
    bus_result_t (*read)(void *, uint8_t, uint8_t, uint8_t *, size_t);
    bus_result_t (*write)(void *, uint8_t, uint8_t, const uint8_t *, size_t);
} register_bus_t;
#endif
