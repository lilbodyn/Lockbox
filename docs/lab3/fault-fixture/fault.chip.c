#include "wokwi-api.h"
typedef struct { pin_t sda, pir; timer_t timer; unsigned phase; } state_t;
static state_t s;
static void advance(void *ctx) {
 state_t *p=ctx;
 switch(p->phase++) {
 case 0: pin_mode(p->pir, OUTPUT_HIGH); timer_start(p->timer,1000000,false); break;
 case 1: pin_mode(p->pir, OUTPUT_LOW); timer_start(p->timer,3000000,false); break;
 case 2: pin_mode(p->sda, OUTPUT_LOW); timer_start(p->timer,3000000,false); break;
 case 3: pin_mode(p->sda, INPUT); break;
 }
}
void chip_init(void) {s.sda=pin_init("SDA",INPUT);s.pir=pin_init("PIR",OUTPUT_LOW);timer_config_t t={.user_data=&s,.callback=advance};s.timer=timer_init(&t);timer_start(s.timer,6000000,false);}
