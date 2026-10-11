#include "serial.h"
#include "main.h"
#include <stdarg.h>
#include <stdio.h>
static char queue[4096];
static unsigned head, tail;
static uint32_t dropped;
static char received[128];
static volatile unsigned rx_head, rx_tail;
static volatile uint32_t rx_errors;
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
        USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    }
    if ((flags & USART_ISR_RXNE_RXFNE) != 0) {
        char byte = (char)USART2->RDR;
        unsigned next = (rx_head + 1) % sizeof received;
        if (next == rx_tail) ++rx_errors;
        else { received[rx_head] = byte; rx_head = next; }
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
    if (rx_tail == rx_head) return false;
    *byte = received[rx_tail];
    rx_tail = (rx_tail + 1) % sizeof received;
    return true;
}
uint32_t serial_dropped_lines(void) { return dropped; }
uint32_t serial_rx_errors(void) { return rx_errors; }
