#include "guard.h"
#include <string.h>

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
void guard_observe(guard_t *guard, observation_t sample, uint32_t now)
{
    guard->current = sample;
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
