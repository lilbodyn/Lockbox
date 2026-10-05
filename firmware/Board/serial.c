#include "serial.h"
#include "main.h"
#include <stdarg.h>
#include <stdio.h>
static char queue[4096];
static unsigned head, tail;
static uint32_t dropped;
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
    if ((USART2->ISR & USART_ISR_RXNE_RXFNE) == 0) return false;
    *byte = (char)USART2->RDR;
    USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    return true;
}
uint32_t serial_dropped_lines(void) { return dropped; }
