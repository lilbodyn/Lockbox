#include "wokwi-api.h"

/* Test fixture only: observe real TX records, then pull MODE low briefly.
 * The firmware and its measurements are never modified by this chip. */
typedef struct {
    pin_t mode;
    timer_t timer;
    unsigned phase, count, prefix_pos;
    bool line_start;
} fixture_t;
static fixture_t fixture;
static const char prefix[] = "LB RECORD ";

static void timer_callback(void *context)
{
    fixture_t *s = context;
    if (s->phase == 1) {
        pin_mode(s->mode, INPUT); /* release: MCU pull-up sets the level */
        s->phase = 2;
        timer_start(s->timer, 450000, false);
    } else {
        s->phase = 0;
    }
}

static void receive(void *context, uint8_t byte)
{
    fixture_t *s = context;
    if (s->line_start && s->prefix_pos < sizeof(prefix)-1) {
        if (byte == (uint8_t)prefix[s->prefix_pos]) {
            ++s->prefix_pos;
            if (s->prefix_pos == sizeof(prefix)-1 && !s->phase && s->count < 40) {
                ++s->count;
                s->phase = 1;
                pin_mode(s->mode, OUTPUT_LOW);
                timer_start(s->timer, 80000, false);
            }
        } else {
            s->line_start = false;
        }
    }
    if (byte == '\n') { s->line_start = true; s->prefix_pos = 0; }
}

void chip_init(void)
{
    fixture_t *s = &fixture;
    s->mode = pin_init("MODE", INPUT);
    s->line_start = true;
    const timer_config_t timer = { .user_data = s, .callback = timer_callback };
    s->timer = timer_init(&timer);
    const uart_config_t uart = { .user_data = s, .rx = pin_init("RX", INPUT),
        .tx = NO_PIN, .baud_rate = 115200, .rx_data = receive };
    (void)uart_init(&uart);
}
