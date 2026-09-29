#ifndef LOCKBOX_MPU6050_H
#define LOCKBOX_MPU6050_H
#include "register_bus.h"
typedef enum { MPU_OK, MPU_NACK, MPU_TIMEOUT, MPU_BUS_ERROR, MPU_WRONG_ID } mpu_result_t;
typedef struct { int32_t x, y, z; } acceleration_t;
typedef struct { register_bus_t bus; uint8_t address; } mpu6050_t;
mpu_result_t mpu6050_identify(const mpu6050_t *device, uint8_t *id);
mpu_result_t mpu6050_start(const mpu6050_t *device);
mpu_result_t mpu6050_read(const mpu6050_t *device, acceleration_t *value, uint8_t raw[6]);
int32_t mpu6050_to_mg(uint8_t high, uint8_t low);
const char *mpu6050_result_name(mpu_result_t result);
#endif
