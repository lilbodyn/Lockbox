#include "board_io.h"
#include "board_pins.h"
#include "serial.h"
#include "guard.h"
#include "commands.h"
#include "analog.h"
#include "archive.h"
#include "perf.h"
#include "build_version.h"
#include <string.h>
static mpu6050_t imu;
static guard_t guard;
static command_input_t input;
static uint32_t retry, rx_errors;
static release_t sampling, heartbeat, telemetry, buttons;
static archive_t archive;
extern archive_media_t board_archive_media(void);
static guard_event_t snapshot[JOURNAL_CAPACITY];
static uint8_t export_index, export_count;
static bool flash_export, button_dispatch, reaction_level;
static uint32_t export_ticket;
static unsigned flash_slot;
static void archive_status(void)
{
    serial_line("LB PERSIST ticket=%lu boot=%lu pending=%u lost=%lu failures=%lu failed=%u recovered=%lu corrupt=%lu slot=%u phase=%u\r\n",
        (unsigned long)archive.ticket,(unsigned long)archive.boot,archive.count,
        (unsigned long)archive.lost,(unsigned long)archive.failures,archive.failed,
        (unsigned long)archive.accepted,(unsigned long)archive.corrupt,archive.slot,archive.phase);
}
static void init_releases(uint32_t now, uint32_t sample_period)
{
    release_init(&sampling,now,sample_period,20000);
    release_init(&heartbeat,now,500000,5000);
    release_init(&telemetry,now,500000,20000);
    release_init(&buttons,now,1000,5000);
}
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
static void on_event(void *context, const guard_event_t *event)
{
    (void)context;
    uint32_t source=0;
    const char *name=NULL;
    unsigned metric=0;
    if (button_dispatch && (event->type==EV_ARMING || event->type==EV_DISARMED)) {
        source=board_edge(0); name="button"; metric=PERF_BUTTON_LATENCY;
    } else if (event->type==EV_MOTION_ON || event->type==EV_MOTION_OFF) {
        source=board_edge(1); name="pir"; metric=PERF_PIR_LATENCY;
    }
    /* Read the event before the response time: a concurrent EXTI update must
       not make unsigned latency underflow. One-slot coalescing still applies. */
    uint32_t now=perf_now();
    if (name && source) {
        perf_add(metric,now-source);
        reaction_level=!reaction_level; perf_probe(2,reaction_level);
        serial_line("LB LATENCY name=%s event_us=%lu response_us=%lu latency_us=%lu resolution_us=1000\r\n",
            name,(unsigned long)source,(unsigned long)now,(unsigned long)(now-source));
    }
    board_alarm(event->causes!=0);
    archive_enqueue(&archive,event);
    print_event(NULL,event);
}
static void status(uint32_t now)
{
    serial_line("LB STATUS ms=%lu mode=%s cause=%u adc=%u mv=%lu ax=%ld ay=%ld az=%ld pir=%u imu_ok=%u adc_ok=%u records=%u overwritten=%lu dropped=%lu rx_errors=%lu\r\n",
        (unsigned long)now, guard_mode_name(guard.mode), guard.causes, light,
        (unsigned long)adc_to_mv(light), (long)acceleration.x, (long)acceleration.y,
        (long)acceleration.z, guard.current.pir, guard.current.imu_ok, guard.current.adc_ok,
        guard.journal_count, (unsigned long)guard.overwritten,
        (unsigned long)serial_dropped_lines(), (unsigned long)serial_rx_errors());
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
        export_count=guard.journal_count; export_index=0;
        for (uint8_t i=0; i<export_count; ++i) snapshot[i]=*guard_journal_at(&guard,i);
        flash_export=false;
        break;
    case CMD_IMU68: case CMD_IMU69:
        imu.address = command == CMD_IMU68 ? 0x68 : 0x69;
        imu_ready = false;
        retry = now - 1000;
        serial_line("LB DIAG ms=%lu sampling_addr=0x%02x\r\n", (unsigned long)now, imu.address);
        break;
    case CMD_STATS:
        perf_report();
        perf_release_report("sample",&sampling); perf_release_report("button",&buttons);
        perf_release_report("heartbeat",&heartbeat); perf_release_report("telemetry",&telemetry);
        archive_status(); break;
    case CMD_RESET_STATS: {
        uint32_t period=sampling.period;
        perf_reset(); init_releases(perf_now(),period);
        serial_line("LB STATS reset=1\r\n"); break;
    }
    case CMD_PERSIST: archive_status(); break;
    case CMD_HISTORY:
        flash_slot=0; export_ticket=archive.ticket; flash_export=true; export_count=0;
        serial_line("LB HISTORY max_ticket=%lu physical_order=1\r\n",(unsigned long)export_ticket); break;
    case CMD_SAMPLE20: case CMD_SAMPLE10: case CMD_SAMPLE5: case CMD_SAMPLE2: case CMD_SAMPLE1: {
        static const uint32_t periods[]={20000,10000,5000,2000,1000};
        uint32_t period=periods[command-CMD_SAMPLE20];
        /* Diagnostic load changes sample spacing; the alarm thresholds stay fixed. */
        release_init(&sampling,perf_now(),period,20000);
        serial_line("LB SAMPLE period_us=%lu diagnostic=1\r\n",(unsigned long)period); break;
    }
    case CMD_HELP:
        serial_line("LB VERSION base=%s source=%s lab=3\r\n",LB_BASE_REV,LB_SOURCE_REV);
        serial_line("LB HELP status|arm|disarm|probe|nack|journal|help|imu 68|imu 69|stats|stats reset|persist|history|sample 20/10/5/2/1; LF\r\n");
        break;
    default: serial_line("LB COMMAND result=INVALID\r\n"); break;
    }
}
void lockbox_start(void)
{
    perf_start();
    serial_start();
    archive_open(&archive,board_archive_media());
    adc_ready = board_io_start();
    guard_init(&guard, board_button_pressed(), perf_ms());
    guard.on_event = on_event;
    board_edges_start();
    imu.bus = board_register_bus();
    imu.address = 0x68;
    mpu_result_t result = mpu6050_start(&imu);
    imu_ready = result == MPU_OK;
    uint8_t raw[6] = {0};
    bool fresh = imu_ready && mpu6050_read(&imu, &acceleration, raw) == MPU_OK;
    uint32_t first_imu=perf_now();
    bool light_ok = adc_ready && board_adc_read(&light);
    uint32_t first_adc=perf_now();
    observation_t first = {acceleration, light, fresh, light_ok, board_pir_level()};
    guard_observe(&guard, first, perf_ms());
    serial_line("LB BOOT project=Lockbox lab=3 mode=OFF base=%s source=%s\r\n",LB_BASE_REV,LB_SOURCE_REV);
    serial_line("LB SELFTEST device=MPU6050 addr=0x68 expected_id=0x68 result=%s sample_ok=%u\r\n",
        mpu6050_result_name(result), fresh);
    serial_line("LB SELFTEST device=LDR result=%s adc=%u\r\n", light_ok ? "READABLE" : "ERROR", light);
    serial_line("LB SELFTEST device=PIR result=READABLE level=%u\r\n", first.pir);
    serial_line("LB READY sample_ms=20 heartbeat_ms=500 debounce_ms=30 calibration_samples=25\r\n");
    archive_status();
    serial_line("LB STARTUP first_imu_us=%lu first_adc_us=%lu valid_imu=%u valid_adc=%u\r\n",
        (unsigned long)first_imu,(unsigned long)first_adc,fresh,light_ok);
    init_releases(perf_now(),20000);
    execute(CMD_HELP, perf_ms());
}
static void sample_task(void)
{
    uint32_t now=perf_ms();
    uint8_t raw[6];
    bool fresh=false;
    if (imu_ready) {
        fresh=mpu6050_read(&imu,&acceleration,raw)==MPU_OK;
        if (!fresh) imu_ready=false;
    }
    if (!imu_ready && now-retry>=1000) {
        retry=now;
        imu_ready=mpu6050_start(&imu)==MPU_OK;
        if (imu_ready) fresh=mpu6050_read(&imu,&acceleration,raw)==MPU_OK;
    }
    bool light_ok=adc_ready && board_adc_read(&light);
    observation_t o={acceleration,light,fresh,light_ok,board_pir_level()};
    guard_observe(&guard,o,perf_ms());
    board_alarm(guard.causes!=0);
}
static bool export_step(void)
{
    if (serial_free()<400) return false;
    if (flash_export) {
        guard_event_t e; uint32_t ticket,boot;
        if (archive_read(&archive,flash_slot++,&e,&ticket,&boot) && ticket<=export_ticket) {
            serial_line("LB STORED ticket=%lu boot=%lu\r\n",(unsigned long)ticket,(unsigned long)boot);
            print_event(&guard,&e);
        }
        if (flash_slot==ARCHIVE_SLOTS) { flash_export=false; serial_line("LB HISTORY end=1 live_scan=1\r\n"); }
        return true;
    }
    if (export_index<export_count) { print_event(&guard,&snapshot[export_index++]); return true; }
    return false;
}
void lockbox_poll(void)
{
    uint32_t loop=perf_now(),t;
    perf_probe(0,1);
    serial_pump();
    char byte;
    uint32_t errors=serial_rx_errors();
    if (errors!=rx_errors) { rx_errors=errors; input.overflow=true; }
    if (serial_receive(&byte)) {
        t=perf_now();
        errors=serial_received_errors();
        if (errors!=rx_errors) { rx_errors=errors; input.overflow=true; }
        command_t command=command_feed(&input,byte);
        if (command!=CMD_NONE) {
            uint32_t received=serial_received_at();
            execute(command,perf_ms());
            uint32_t done=perf_now();
            perf_add(PERF_COMMAND_LATENCY,done-received);
            serial_line("LB COMMAND id=%u rx_us=%lu done_us=%lu latency_us=%lu\r\n",command,
                (unsigned long)received,(unsigned long)done,(unsigned long)(done-received));
        }
        perf_add(PERF_RX,perf_now()-t);
    }
    t=perf_now();
    if (release_due(&buttons,t)) {
        button_dispatch=true; guard_button(&guard,board_button_pressed(),perf_ms()); button_dispatch=false;
        uint32_t done=perf_now(); perf_add(PERF_BUTTON,done-t); release_complete(&buttons,done);
    }
    t=perf_now();
    if (release_due(&sampling,t)) {
        perf_probe(1,1); sample_task(); perf_probe(1,0);
        uint32_t done=perf_now(); perf_add(PERF_SAMPLE,done-t); release_complete(&sampling,done);
    }
    t=perf_now();
    if (release_due(&heartbeat,t)) {
        HAL_GPIO_TogglePin(Led_GPIO_Port,Led_Pin);
        uint32_t done=perf_now(); perf_add(PERF_HEARTBEAT,done-t); release_complete(&heartbeat,done);
    }
    t=perf_now();
    if (release_due(&telemetry,t)) {
        status(perf_ms());
        uint32_t done=perf_now(); perf_add(PERF_TELEMETRY,done-t); release_complete(&telemetry,done);
    }
    if (archive.count && !archive.failed) {
        t=perf_now(); perf_probe(3,1); archive_step(&archive); perf_probe(3,0);
        perf_add(PERF_ARCHIVE,perf_now()-t);
        if (archive.failed) archive_status();
    }
    t=perf_now();
    if (export_step()) perf_add(PERF_EXPORT,perf_now()-t);
    perf_probe(0,0);
    perf_add(PERF_LOOP,perf_now()-loop);
}
