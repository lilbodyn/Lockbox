#include "guard.h"
#include <string.h>
#include "analog.h"

static void record(guard_t *guard, event_type_t type, uint32_t now)
{
    guard_event_t *event = &guard->journal[guard->journal_head];
    *event = (guard_event_t){++guard->sequence, now, type,
        guard->mode, guard->causes, guard->current};
    guard->journal_head = (guard->journal_head + 1) % JOURNAL_CAPACITY;
    if (guard->journal_count < JOURNAL_CAPACITY) ++guard->journal_count;
    else ++guard->overwritten;
    if (guard->on_event) guard->on_event(guard->event_context, event);
}
void guard_init(guard_t *guard, bool button_pressed, uint32_t now)
{
    memset(guard, 0, sizeof *guard);
    guard->button_raw = guard->button_stable = button_pressed;
    guard->button_since = now;
}
void guard_arm(guard_t *guard, uint32_t now)
{
    if (guard->mode != GUARD_OFF) return;
    guard->mode = GUARD_CALIBRATING;
    guard->calibration_started = now;
    guard->stable_samples = 0;
    record(guard, EV_ARMING, now);
}
void guard_disarm(guard_t *guard, uint32_t now)
{
    if (guard->mode == GUARD_OFF) return;
    guard->mode = GUARD_OFF;
    guard->causes = 0;
    guard->stable_samples = 0;
    guard->light_confirm = 0;
    record(guard, EV_DISARMED, now);
}
void guard_button(guard_t *guard, bool pressed, uint32_t now)
{
    if (pressed != guard->button_raw) {
        guard->button_raw = pressed;
        guard->button_since = now;
    }
    if (pressed != guard->button_stable && now - guard->button_since >= 30) {
        guard->button_stable = pressed;
        if (pressed) {
            if (guard->mode == GUARD_OFF) guard_arm(guard, now);
            else guard_disarm(guard, now);
        }
    }
}
static bool stationary(observation_t sample)
{
    int64_t x = sample.accel.x, y = sample.accel.y, z = sample.accel.z;
    int64_t norm2 = x*x + y*y + z*z;
    return sample.imu_ok && sample.adc_ok && sample.adc <= 4095 &&
        norm2 >= 800*800 && norm2 <= 1200*1200;
}
static void calibrate(guard_t *guard, observation_t sample, uint32_t now)
{
    if (now - guard->calibration_started >= 2000) {
        guard->mode = GUARD_OFF;
        record(guard, EV_ARM_REJECTED, now);
        return;
    }
    if (!stationary(sample)) { guard->stable_samples = 0; return; }
    int32_t axes[3] = {sample.accel.x, sample.accel.y, sample.accel.z};
    bool unstable = false;
    for (unsigned i = 0; i < 3; ++i) {
        if (guard->stable_samples == 0) guard->minimum[i] = guard->maximum[i] = axes[i];
        if (axes[i] < guard->minimum[i]) guard->minimum[i] = axes[i];
        if (axes[i] > guard->maximum[i]) guard->maximum[i] = axes[i];
        if (guard->maximum[i] - guard->minimum[i] > 150) unstable = true;
    }
    if (guard->stable_samples == 0) guard->light_minimum = guard->light_maximum = sample.adc;
    if (sample.adc < guard->light_minimum) guard->light_minimum = sample.adc;
    if (sample.adc > guard->light_maximum) guard->light_maximum = sample.adc;
    if (guard->light_maximum - guard->light_minimum > 80) unstable = true;
    if (unstable) { guard->stable_samples = 0; return; }
    if (++guard->stable_samples == 25) {
        guard->mode = GUARD_ARMED;
        record(guard, EV_ARMED, now);
    }
}
static void alarm_reason(guard_t *guard, uint8_t cause, event_type_t type, uint32_t now)
{
    if ((guard->causes & cause) != 0) return;
    guard->causes |= cause;
    guard->mode = GUARD_ALARM;
    record(guard, type, now);
}
static void light_step(guard_t *guard, observation_t sample, uint32_t now)
{
    if (!sample.adc_ok || sample.adc > 4095) {
        guard->light_count = guard->history_count = guard->light_confirm = 0;
        return;
    }
    guard->light_samples[guard->light_index] = sample.adc;
    guard->light_index = (guard->light_index + 1) % 3;
    if (guard->light_count < 3) ++guard->light_count;
    if (guard->light_count < 3) return;
    uint16_t filtered = median3(guard->light_samples[0], guard->light_samples[1], guard->light_samples[2]);
    bool enabled = guard->mode == GUARD_ARMED || guard->mode == GUARD_ALARM;
    if (enabled && guard->history_count == 5) {
        uint16_t reference = guard->light_confirm ? guard->light_candidate_ref :
            guard->light_history[guard->history_index];
        int32_t change = (int32_t)filtered - reference;
        if (change < 0) change = -change;
        if (change >= 600) {
            guard->light_candidate_ref = reference;
            if (++guard->light_confirm >= 3) {
                alarm_reason(guard, CAUSE_LIGHT, EV_LIGHT, now);
                guard->light_confirm = 0;
            }
        } else guard->light_confirm = 0;
    } else guard->light_confirm = 0;
    guard->light_history[guard->history_index] = filtered;
    guard->history_index = (guard->history_index + 1) % 5;
    if (guard->history_count < 5) ++guard->history_count;
}
void guard_observe(guard_t *guard, observation_t sample, uint32_t now)
{
    guard->current = sample;
    if (guard->have_previous) {
        if (sample.pir != guard->previous.pir)
            record(guard, sample.pir ? EV_MOTION_ON : EV_MOTION_OFF, now);
        if (sample.imu_ok != guard->previous.imu_ok)
            record(guard, sample.imu_ok ? EV_IMU_RECOVERED : EV_IMU_FAULT, now);
        if (sample.adc_ok != guard->previous.adc_ok)
            record(guard, sample.adc_ok ? EV_ADC_RECOVERED : EV_ADC_FAULT, now);
    }
    bool watching = guard->mode == GUARD_ARMED || guard->mode == GUARD_ALARM;
    if (watching && guard->have_previous && sample.imu_ok && guard->previous.imu_ok) {
        int64_t dx = (int64_t)sample.accel.x - guard->previous.accel.x;
        int64_t dy = (int64_t)sample.accel.y - guard->previous.accel.y;
        int64_t dz = (int64_t)sample.accel.z - guard->previous.accel.z;
        if (dx*dx + dy*dy + dz*dz >= 350*350)
            alarm_reason(guard, CAUSE_SHOCK, EV_SHOCK, now);
    }
    light_step(guard, sample, now);
    if (guard->mode == GUARD_CALIBRATING) calibrate(guard, sample, now);
    guard->previous = sample;
    guard->have_previous = true;
}
const guard_event_t *guard_journal_at(const guard_t *guard, uint8_t index)
{
    if (index >= guard->journal_count) return NULL;
    unsigned oldest = (guard->journal_head + JOURNAL_CAPACITY - guard->journal_count) % JOURNAL_CAPACITY;
    return &guard->journal[(oldest + index) % JOURNAL_CAPACITY];
}
const char *guard_mode_name(guard_mode_t mode)
{
    static const char *names[] = {"OFF", "CALIBRATING", "ARMED", "ALARM"};
    return names[mode];
}
const char *guard_event_name(event_type_t type)
{
    static const char *names[] = {"ARMING", "ARMED", "DISARMED", "ARM_REJECTED",
        "SHOCK", "LIGHT", "MOTION_ON", "MOTION_OFF", "IMU_FAULT", "IMU_RECOVERED",
        "ADC_FAULT", "ADC_RECOVERED"};
    return names[type];
}
