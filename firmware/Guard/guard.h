#ifndef LOCKBOX_GUARD_H
#define LOCKBOX_GUARD_H
#include <stdbool.h>
#include <stdint.h>
#include "mpu6050.h"
typedef enum { GUARD_OFF, GUARD_CALIBRATING, GUARD_ARMED, GUARD_ALARM } guard_mode_t;
typedef enum { EV_ARMING, EV_ARMED, EV_DISARMED, EV_ARM_REJECTED,
    EV_SHOCK, EV_LIGHT, EV_MOTION_ON, EV_MOTION_OFF,
    EV_IMU_FAULT, EV_IMU_RECOVERED, EV_ADC_FAULT, EV_ADC_RECOVERED } event_type_t;
enum { CAUSE_SHOCK = 1, CAUSE_LIGHT = 2, JOURNAL_CAPACITY = 16 };
typedef struct {
    acceleration_t accel;
    uint16_t adc;
    bool imu_ok, adc_ok, pir;
} observation_t;
typedef struct {
    uint32_t seq, ms;
    event_type_t type;
    guard_mode_t mode;
    uint8_t causes;
    observation_t readings;
} guard_event_t;
typedef struct {
    guard_mode_t mode;
    uint8_t causes;
    observation_t current, previous;
    bool have_previous, button_raw, button_stable;
    uint32_t button_since, calibration_started, sequence, overwritten;
    uint8_t stable_samples, journal_head, journal_count;
    int32_t minimum[3], maximum[3];
    uint16_t light_minimum, light_maximum;
    uint16_t light_samples[3], light_history[5], light_candidate_ref;
    uint8_t light_index, light_count, history_index, history_count, light_confirm;
    guard_event_t journal[JOURNAL_CAPACITY];
    void (*on_event)(void *, const guard_event_t *);
    void *event_context;
} guard_t;
void guard_init(guard_t *guard, bool button_pressed, uint32_t now);
void guard_arm(guard_t *guard, uint32_t now);
void guard_disarm(guard_t *guard, uint32_t now);
void guard_button(guard_t *guard, bool pressed, uint32_t now);
void guard_observe(guard_t *guard, observation_t sample, uint32_t now);
const guard_event_t *guard_journal_at(const guard_t *guard, uint8_t index);
const char *guard_mode_name(guard_mode_t mode);
const char *guard_event_name(event_type_t type);
#endif
