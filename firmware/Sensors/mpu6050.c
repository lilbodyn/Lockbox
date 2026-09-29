#include "mpu6050.h"
#include <string.h>

static mpu_result_t translate(bus_result_t result)
{
    switch (result) {
    case BUS_OK: return MPU_OK;
    case BUS_NACK: return MPU_NACK;
    case BUS_TIMEOUT: return MPU_TIMEOUT;
    default: return MPU_BUS_ERROR;
    }
}

mpu_result_t mpu6050_identify(const mpu6050_t *device, uint8_t *id)
{
    uint8_t received = 0;
    mpu_result_t result = translate(device->bus.read(device->bus.context,
        device->address, 0x75, &received, 1));
    if (result != MPU_OK) return result;
    *id = received;
    return received == 0x68 ? MPU_OK : MPU_WRONG_ID;
}

mpu_result_t mpu6050_start(const mpu6050_t *device)
{
    /* DLPF 3: accel bandwidth 44 Hz. 1 kHz / (9+1) = 100 Hz.
       +/-4 g gives 8192 LSB/g; gyro is unused by Lockbox. */
    static const uint8_t setup[][2] = {
        {0x6b, 0x00}, {0x1a, 0x03}, {0x19, 0x09}, {0x1c, 0x08}
    };
    uint8_t id = 0;
    mpu_result_t result = mpu6050_identify(device, &id);
    if (result != MPU_OK) return result;
    for (size_t i = 0; i < sizeof setup / sizeof setup[0]; ++i) {
        result = translate(device->bus.write(device->bus.context, device->address,
            setup[i][0], &setup[i][1], 1));
        if (result != MPU_OK) return result;
    }
    return MPU_OK;
}

int32_t mpu6050_to_mg(uint8_t high, uint8_t low)
{
    int32_t signed_value = (int32_t)(((uint32_t)high << 8) | low);
    if (signed_value >= 32768) signed_value -= 65536;
    return signed_value * 1000 / 8192;
}

mpu_result_t mpu6050_read(const mpu6050_t *device, acceleration_t *value, uint8_t raw[6])
{
    uint8_t frame[6];
    mpu_result_t result = translate(device->bus.read(device->bus.context,
        device->address, 0x3b, frame, sizeof frame));
    if (result != MPU_OK) return result;
    acceleration_t fresh = {mpu6050_to_mg(frame[0], frame[1]),
        mpu6050_to_mg(frame[2], frame[3]), mpu6050_to_mg(frame[4], frame[5])};
    *value = fresh;
    memcpy(raw, frame, sizeof frame);
    return MPU_OK;
}

const char *mpu6050_result_name(mpu_result_t result)
{
    switch (result) {
    case MPU_OK: return "OK";
    case MPU_NACK: return "NACK";
    case MPU_TIMEOUT: return "TIMEOUT";
    case MPU_WRONG_ID: return "WRONG_ID";
    default: return "BUS_ERROR";
    }
}
