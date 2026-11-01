#include "perf.h"
#include "main.h"
#include "serial.h"
#include <string.h>
static volatile uint32_t edge[2];
static perf_stat_t stats[PERF_COUNT];
static uint32_t since;
extern uint32_t _estack, _Min_Stack_Size;
extern uint32_t lockbox_heap_peak(void);
void perf_paint_stack(void)
{
    uint32_t irq=__get_PRIMASK(); __disable_irq();
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    uint32_t *limit=(uint32_t *)__get_MSP()-16;
    while (p<limit) *p++=0xdeadbeefu;
    __set_PRIMASK(irq);
}
uint32_t perf_stack_used(void)
{
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    while (p<&_estack && *p==0xdeadbeefu) ++p;
    return (uint32_t)((uintptr_t)&_estack-(uintptr_t)p);
}
uint32_t perf_now(void)
{
    /* This clock is quantized to 1 ms. A masked SysTick can lose ticks;
       independent model-time validation is required before timed trials. */
    return coarse_clock_us(HAL_GetTick());
}
void perf_start(void)
{
    GPIO_InitTypeDef pin={0};
    pin.Pin=GPIO_PIN_6|GPIO_PIN_7; pin.Mode=GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA,&pin);
    pin.Pin=GPIO_PIN_0|GPIO_PIN_1; HAL_GPIO_Init(GPIOB,&pin);
    perf_reset();
}
uint32_t perf_ms(void) { return HAL_GetTick(); }
void perf_reset(void) { memset(stats,0,sizeof stats); since=perf_now(); }
void perf_probe(unsigned p, int high)
{
    GPIO_TypeDef *port=p<2 ? GPIOA : GPIOB;
    unsigned bit=p<2 ? 6+p : p-2;
    port->BSRR=1u<<(bit+(high ? 0 : 16));
}
void perf_add(unsigned i, uint32_t us)
{
    perf_stat_t *s=&stats[i];
    if (!s->n || us<s->min) s->min=us;
    if (us>s->max) s->max=us;
    ++s->n; s->sum+=us;
}
void perf_report(void)
{
    static const char *names[]={"rx","button","sample","heartbeat","telemetry","archive","export","loop",
        "button_latency","pir_latency","command_latency"};
    uint32_t elapsed=perf_now()-since;
    serial_line("LB PERF window_us=%lu stack_used=%lu stack_reserved=%lu heap_peak=%lu timer=HAL_SysTick_1ms cpu_load=unavailable subms_wcet=unavailable\r\n",
        (unsigned long)elapsed,
        (unsigned long)perf_stack_used(),(unsigned long)(uintptr_t)&_Min_Stack_Size,
        (unsigned long)lockbox_heap_peak());
    serial_line("LB CLOCK resolution_us=1000 independent_vcd_check=required masked_ticks=recovery_unavailable\r\n");
    for (unsigned i=0; i<PERF_COUNT; ++i) {
        perf_stat_t s=stats[i];
        serial_line("LB STAT name=%s n=%lu min_us=%lu avg_us=%lu max_us=%lu resolution_us=1000\r\n",names[i],
            (unsigned long)s.n,(unsigned long)s.min,(unsigned long)(s.n ? s.sum/s.n : 0),(unsigned long)s.max);
    }
}
void perf_release_report(const char *name, const release_t *r)
{
    serial_line("LB RELEASE name=%s period_us=%lu late_max_us=%lu missed=%lu deadlines=%lu jitter_min_us=%ld jitter_max_us=%ld hist=%lu,%lu,%lu,%lu,%lu,%lu\r\n",
        name,(unsigned long)r->period,(unsigned long)r->late_max,(unsigned long)r->missed,
        (unsigned long)r->deadline_misses,(long)r->jitter_min,(long)r->jitter_max,
        (unsigned long)r->histogram[0],(unsigned long)r->histogram[1],(unsigned long)r->histogram[2],
        (unsigned long)r->histogram[3],(unsigned long)r->histogram[4],(unsigned long)r->histogram[5]);
}
void board_edges_start(void)
{
    GPIO_InitTypeDef pin={0};
    pin.Pin=GPIO_PIN_1; pin.Mode=GPIO_MODE_IT_FALLING; pin.Pull=GPIO_PULLUP; HAL_GPIO_Init(GPIOA,&pin);
    pin.Pin=GPIO_PIN_10; pin.Mode=GPIO_MODE_IT_RISING_FALLING; pin.Pull=GPIO_PULLDOWN; HAL_GPIO_Init(GPIOA,&pin);
    HAL_NVIC_SetPriority(EXTI0_1_IRQn,1,0); HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn,1,0); HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}
void EXTI0_1_IRQHandler(void)
{
    if (EXTI->FPR1 & GPIO_PIN_1) { edge[0]=perf_now(); EXTI->FPR1=GPIO_PIN_1; }
}
void board_pir_interrupt(void)
{
    if ((EXTI->RPR1|EXTI->FPR1) & GPIO_PIN_10) {
        edge[1]=perf_now(); EXTI->RPR1=GPIO_PIN_10; EXTI->FPR1=GPIO_PIN_10;
    }
}
uint32_t board_edge(unsigned i) { return edge[i]; }
