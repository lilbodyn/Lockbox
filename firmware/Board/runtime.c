#include "board_io.h"
#include "board_pins.h"
#include <stdio.h>
#include "guard.h"
static mpu6050_t imu;
static guard_t guard;
static uint32_t sampled, heartbeat, printed, retry;
static bool imu_ready, adc_ready;
static acceleration_t acceleration;
static uint16_t light;
void lockbox_start(void)
{
    adc_ready = board_io_start();
    guard_init(&guard, board_button_pressed(), HAL_GetTick());
    imu.bus = board_register_bus();
    imu.address = 0x68;
    mpu_result_t result = mpu6050_start(&imu);
    imu_ready = result == MPU_OK;
    printf("LB BOOT mode=OFF\r\nLB CHECK MPU6050=%s LDR=%s PIR=READABLE\r\n",
        mpu6050_result_name(result), adc_ready ? "OK" : "ERROR");
}
void lockbox_poll(void)
{
    uint32_t now = HAL_GetTick();
    guard_button(&guard, board_button_pressed(), now);
    if (now - sampled >= 20) {
        sampled = now;
        uint8_t raw[6];
        if (imu_ready) imu_ready = mpu6050_read(&imu, &acceleration, raw) == MPU_OK;
        if (!imu_ready && now - retry >= 1000) {
            retry = now;
            imu_ready = mpu6050_start(&imu) == MPU_OK;
        }
        bool light_ok = adc_ready && board_adc_read(&light);
        observation_t observation = {acceleration, light, imu_ready, light_ok, board_pir_level()};
        guard_observe(&guard, observation, now);
    }
    if (now - heartbeat >= 500) {
        heartbeat = now;
        HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin);
    }
    if (now - printed >= 500) {
        printed = now;
        printf("LB SAMPLE ms=%lu ax=%ld ay=%ld az=%ld adc=%u pir=%u imu_ok=%u mode=%s\r\n",
            (unsigned long)now, (long)acceleration.x, (long)acceleration.y,
            (long)acceleration.z, light, board_pir_level(), imu_ready, guard_mode_name(guard.mode));
    }
}
