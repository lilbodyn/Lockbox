#include "serial.h"
#include "main.h"
#include "perf.h"
#include <stdarg.h>
#include <stdio.h>
static char queue[4096];
static unsigned head, tail;
static uint32_t dropped;
static char received[128];
static uint32_t received_time[128], popped_time;
static uint32_t popped_errors;
static volatile unsigned rx_head, rx_tail;
static volatile uint32_t rx_errors;
static bool discard_rx;
void serial_start(void)
{
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    USART2->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
}
void serial_interrupt(void)
{
    uint32_t flags = USART2->ISR;
    if ((flags & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) != 0) {
        ++rx_errors;
        rx_head=rx_tail;
        discard_rx=true;
        USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    }
    if ((flags & USART_ISR_RXNE_RXFNE) != 0) {
        char byte = (char)USART2->RDR;
        if (discard_rx && byte!='\n') return;
        if (discard_rx) discard_rx=false;
        unsigned next = (rx_head + 1) % sizeof received;
        if (next == rx_tail) {
            ++rx_errors; rx_head=rx_tail; discard_rx=true;
        }
        else { received[rx_head] = byte; received_time[rx_head] = perf_now(); rx_head = next; }
    }
}
bool serial_line(const char *format, ...)
{
    char line[256];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(line, sizeof line, format, args);
    va_end(args);
    unsigned free_bytes = (tail + sizeof queue - head - 1) % sizeof queue;
    if (count < 0 || count >= (int)sizeof line || (unsigned)count > free_bytes) {
        ++dropped;
        return false;
    }
    for (int i = 0; i < count; ++i) {
        queue[head] = line[i];
        head = (head + 1) % sizeof queue;
    }
    return true;
}
void serial_pump(void)
{
    if (tail != head && (USART2->ISR & USART_ISR_TXE_TXFNF) != 0) {
        USART2->TDR = (uint8_t)queue[tail];
        tail = (tail + 1) % sizeof queue;
    }
}
bool serial_receive(char *byte)
{
    /* ISR may flush the queue on loss. Pop and timestamp must be atomic. */
    uint32_t irq=__get_PRIMASK(); __disable_irq();
    if (rx_tail == rx_head) { __set_PRIMASK(irq); return false; }
    *byte = received[rx_tail];
    popped_time = received_time[rx_tail];
    popped_errors = rx_errors;
    rx_tail = (rx_tail + 1) % sizeof received;
    __set_PRIMASK(irq);
    return true;
}
uint32_t serial_dropped_lines(void) { return dropped; }
uint32_t serial_rx_errors(void) { return rx_errors; }
uint32_t serial_received_at(void) { return popped_time; }
uint32_t serial_received_errors(void) { return popped_errors; }
unsigned serial_free(void) { return (tail + sizeof queue - head - 1) % sizeof queue; }
