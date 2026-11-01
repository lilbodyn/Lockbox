#ifndef LOCKBOX_TEST_MAIN_H
#define LOCKBOX_TEST_MAIN_H
#include <stdint.h>
/* Minimal register/IRQ substitutes only for compiling the production serial.c.
 * No MCU timing or electrical behavior is represented by this host fixture. */
typedef struct { uint32_t CR1, ISR, ICR, RDR, TDR; } uart_registers_t;
extern uart_registers_t test_uart;
extern uint32_t test_primask;
extern unsigned test_irq_enabled;
#define USART2 (&test_uart)
#define USART2_IRQn 28
#define USART_CR1_RXNEIE_RXFNEIE 32u
#define USART_ISR_ORE 8u
#define USART_ISR_FE 2u
#define USART_ISR_NE 4u
#define USART_ISR_RXNE_RXFNE 32u
#define USART_ISR_TXE_TXFNF 128u
#define USART_ICR_ORECF 8u
#define USART_ICR_FECF 2u
#define USART_ICR_NECF 4u
static inline void HAL_NVIC_SetPriority(unsigned n,unsigned p,unsigned s) { (void)n;(void)p;(void)s; }
static inline void HAL_NVIC_EnableIRQ(unsigned n) { if (n==USART2_IRQn) test_irq_enabled=1; }
static inline uint32_t __get_PRIMASK(void) { return test_primask; }
static inline void __disable_irq(void) { test_primask=1; }
static inline void __set_PRIMASK(uint32_t p) { test_primask=p; }
#endif
