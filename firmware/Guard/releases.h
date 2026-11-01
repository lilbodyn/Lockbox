#ifndef LOCKBOX_RELEASES_H
#define LOCKBOX_RELEASES_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    uint32_t next, period, deadline, missed, late_max, deadline_misses, previous, active_late;
    int32_t jitter_min, jitter_max;
    uint32_t histogram[6];
    bool started;
} release_t;
void release_init(release_t *, uint32_t, uint32_t, uint32_t);
bool release_due(release_t *, uint32_t);
void release_complete(release_t *, uint32_t);
typedef struct { uint32_t last_us, ms, remainder; } uptime_t;
/* Foreground owner; call at least once per uint32 microsecond wrap. */
uint32_t uptime_update(uptime_t *, uint32_t);
typedef struct { uint32_t us; uint16_t previous, max_step; } counter16_t;
/* Every caller gap MUST be <65536 timer ticks. Equal/full wraps cannot be
   detected from a 16-bit register; max_step is modular, not proof of this bound. */
uint32_t counter16_update(counter16_t *, uint16_t);
#endif
