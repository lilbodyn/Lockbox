#ifndef LOCKBOX_PERF_H
#define LOCKBOX_PERF_H
#include <stdint.h>
#include "releases.h"
#include "coarse_clock.h"
typedef struct { uint32_t n, min, max; uint64_t sum; } perf_stat_t;
enum { PERF_RX, PERF_BUTTON, PERF_SAMPLE, PERF_HEARTBEAT, PERF_TELEMETRY,
    PERF_ARCHIVE, PERF_EXPORT, PERF_LOOP, PERF_BUTTON_LATENCY, PERF_PIR_LATENCY, PERF_COMMAND_LATENCY, PERF_COUNT };
void perf_start(void);
uint32_t perf_now(void);
uint32_t perf_ms(void); /* HAL tick; wraps after 2^32 milliseconds. */
void perf_add(unsigned, uint32_t);
void perf_report(void);
void perf_reset(void);
void perf_paint_stack(void);
uint32_t perf_stack_used(void);
void perf_probe(unsigned, int);
void perf_release_report(const char *, const release_t *);
uint32_t board_edge(unsigned);
void board_edges_start(void);
void board_pir_interrupt(void);
#endif
