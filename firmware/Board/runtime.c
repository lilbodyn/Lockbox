#include "board_io.h"
#include "board_pins.h"
#include "serial.h"
#include "guard.h"
#include "commands.h"
#include "analog.h"
static mpu6050_t imu;
static guard_t guard;
static command_input_t input;
static uint32_t sampled, heartbeat, printed, retry;
static bool imu_ready, adc_ready;
static acceleration_t acceleration;
static uint16_t light;

static void print_event(void *context, const guard_event_t *event)
{
    const char *label = context ? "RECORD" : "EVENT";
    serial_line("LB %s seq=%lu ms=%lu type=%s mode=%s cause=%u adc=%u ax=%ld ay=%ld az=%ld pir=%u imu_ok=%u adc_ok=%u\r\n",
        label, (unsigned long)event->seq, (unsigned long)event->ms,
        guard_event_name(event->type), guard_mode_name(event->mode), event->causes,
        event->readings.adc, (long)event->readings.accel.x, (long)event->readings.accel.y,
        (long)event->readings.accel.z, event->readings.pir,
        event->readings.imu_ok, event->readings.adc_ok);
}
static void status(uint32_t now)
{
    serial_line("LB STATUS ms=%lu mode=%s cause=%u adc=%u mv=%lu ax=%ld ay=%ld az=%ld pir=%u imu_ok=%u adc_ok=%u records=%u overwritten=%lu dropped=%lu\r\n",
        (unsigned long)now, guard_mode_name(guard.mode), guard.causes, light,
        (unsigned long)adc_to_mv(light), (long)acceleration.x, (long)acceleration.y,
        (long)acceleration.z, guard.current.pir, guard.current.imu_ok, guard.current.adc_ok,
        guard.journal_count, (unsigned long)guard.overwritten,
        (unsigned long)serial_dropped_lines());
}
static void probe(bool wrong_address, uint32_t now)
{
    mpu6050_t target = imu;
    if (wrong_address) target.address = 0x69;
    uint8_t id = 0, raw[6] = {0};
    acceleration_t value = {0};
    mpu_result_t result = mpu6050_identify(&target, &id);
    if (result == MPU_OK) result = mpu6050_read(&target, &value, raw);
    serial_line("LB PROBE ms=%lu addr=0x%02x id=0x%02x result=%s raw=%02x,%02x,%02x,%02x,%02x,%02x ax=%ld ay=%ld az=%ld\r\n",
        (unsigned long)now, target.address, id, mpu6050_result_name(result),
        raw[0], raw[1], raw[2], raw[3], raw[4], raw[5],
        (long)value.x, (long)value.y, (long)value.z);
}
static void execute(command_t command, uint32_t now)
{
    switch (command) {
    case CMD_NONE: break;
    case CMD_STATUS: status(now); break;
    case CMD_ARM: guard_arm(&guard, now); break;
    case CMD_DISARM: guard_disarm(&guard, now); break;
    case CMD_PROBE: probe(false, now); break;
    case CMD_NACK: probe(true, now); break;
    case CMD_JOURNAL:
        serial_line("LB JOURNAL count=%u overwritten=%lu\r\n", guard.journal_count,
            (unsigned long)guard.overwritten);
        for (uint8_t i = 0; i < guard.journal_count; ++i)
            print_event(&guard, guard_journal_at(&guard, i));
        break;
    case CMD_IMU68: case CMD_IMU69:
        imu.address = command == CMD_IMU68 ? 0x68 : 0x69;
        imu_ready = false;
        retry = now - 1000;
        serial_line("LB DIAG ms=%lu sampling_addr=0x%02x\r\n", (unsigned long)now, imu.address);
        break;
    case CMD_HELP:
        serial_line("LB HELP status|arm|disarm|probe|nack|journal|help|imu 68|imu 69; LF executes\r\n");
        break;
    default: serial_line("LB COMMAND result=INVALID\r\n"); break;
    }
}
void lockbox_start(void)
{
    adc_ready = board_io_start();
    guard_init(&guard, board_button_pressed(), HAL_GetTick());
    guard.on_event = print_event;
    imu.bus = board_register_bus();
    imu.address = 0x68;
    mpu_result_t result = mpu6050_start(&imu);
    imu_ready = result == MPU_OK;
    uint8_t raw[6] = {0};
    bool fresh = imu_ready && mpu6050_read(&imu, &acceleration, raw) == MPU_OK;
    bool light_ok = adc_ready && board_adc_read(&light);
    observation_t first = {acceleration, light, fresh, light_ok, board_pir_level()};
    guard_observe(&guard, first, HAL_GetTick());
    serial_line("LB BOOT project=Lockbox lab=2 mode=OFF\r\n");
    serial_line("LB SELFTEST device=MPU6050 addr=0x68 expected_id=0x68 result=%s sample_ok=%u\r\n",
        mpu6050_result_name(result), fresh);
    serial_line("LB SELFTEST device=LDR result=%s adc=%u\r\n", light_ok ? "READABLE" : "ERROR", light);
    serial_line("LB SELFTEST device=PIR result=READABLE level=%u\r\n", first.pir);
    serial_line("LB READY sample_ms=20 heartbeat_ms=500 debounce_ms=30 calibration_samples=25\r\n");
    execute(CMD_HELP, HAL_GetTick());
}
void lockbox_poll(void)
{
    serial_pump();
    uint32_t now = HAL_GetTick();
    guard_button(&guard, board_button_pressed(), now);
    char byte;
    if (serial_receive(&byte)) execute(command_feed(&input, byte), now);
    if (now - sampled >= 20) {
        sampled = now;
        uint8_t raw[6];
        bool fresh = false;
        if (imu_ready) {
            fresh = mpu6050_read(&imu, &acceleration, raw) == MPU_OK;
            if (!fresh) imu_ready = false;
        }
        if (!imu_ready && now - retry >= 1000) {
            retry = now;
            imu_ready = mpu6050_start(&imu) == MPU_OK;
            if (imu_ready) fresh = mpu6050_read(&imu, &acceleration, raw) == MPU_OK;
        }
        bool light_ok = adc_ready && board_adc_read(&light);
        observation_t observation = {acceleration, light, fresh, light_ok, board_pir_level()};
        guard_observe(&guard, observation, HAL_GetTick());
        board_alarm(guard.causes != 0);
    }
    if (now - heartbeat >= 500) {
        heartbeat = now;
        HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin);
    }
    if (now - printed >= 500) {
        printed = now;
        status(now);
    }
}
