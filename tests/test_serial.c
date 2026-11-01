#include "main.h"
#include "serial.h"
#include "commands.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uart_registers_t test_uart;
uint32_t test_primask;
unsigned test_irq_enabled;
static uint32_t clock_us, epoch;
uint32_t perf_now(void) { return ++clock_us; }
static void inject(char b,uint32_t flags)
{
    test_uart.RDR=(uint8_t)b; test_uart.ISR=USART_ISR_RXNE_RXFNE|flags;
    serial_interrupt(); test_uart.ISR=0;
}
static command_t pop(command_input_t *input)
{
    command_t cmd=CMD_NONE; char b;
    while (serial_receive(&b)) {
        uint32_t errors=serial_received_errors();
        if (errors!=epoch) { epoch=errors; input->overflow=true; }
        cmd=command_feed(input,b);
        assert(serial_received_at()>0); assert(test_primask==0);
    }
    return cmd;
}
static void send(const char *s) { while (*s) inject(*s++,0); }
int main(void)
{
    serial_start(); assert(test_uart.CR1 & USART_CR1_RXNEIE_RXFNEIE); assert(test_irq_enabled);
    command_input_t input={0}; char b;
    assert(!serial_receive(&b) && test_primask==0);
    test_primask=1; assert(!serial_receive(&b) && test_primask==1); test_primask=0;
    send("arm"); assert(pop(&input)==CMD_NONE);
    inject('x',USART_ISR_ORE); send("discarded\n");
    assert(pop(&input)==CMD_INVALID && serial_rx_errors()==1);
    send("disarm\n"); assert(pop(&input)==CMD_DISARM);
    for (unsigned i=0;i<128;++i) inject('z',0);
    assert(serial_rx_errors()==2); send("arm\n"); assert(pop(&input)==CMD_INVALID);
    send("status\n"); assert(pop(&input)==CMD_STATUS);
    send("disarm"); assert(pop(&input)==CMD_NONE);
    inject('\n',USART_ISR_FE); assert(pop(&input)==CMD_INVALID);
    send("arm\n"); assert(pop(&input)==CMD_ARM);
    send("arm"); inject('\0',0); send("suffix\n"); assert(pop(&input)==CMD_INVALID);
    send("disarm"); inject((char)0xff,0); inject('\n',0); assert(pop(&input)==CMD_INVALID);
    send("status\n"); test_primask=1;
    assert(serial_receive(&b) && test_primask==1); test_primask=0;
    while (serial_receive(&b)) {}
    unsigned count=0;
    while (serial_free()>=100) { assert(serial_line("%0100u",7u)); ++count; }
    unsigned free_before=serial_free(); uint32_t dropped=serial_dropped_lines();
    assert(!serial_line("%0100u",7u));
    assert(serial_free()==free_before && serial_dropped_lines()==dropped+1);
    test_uart.ISR=USART_ISR_TXE_TXFNF;
    for (unsigned i=0;i<count*100;++i) serial_pump();
    assert(serial_free()==4095);
    printf("PASS production UART RX flush, damaged LF, epoch snapshot, IRQ restore, TX whole-line overflow; host register fixtures only\n");
    return 0;
}
