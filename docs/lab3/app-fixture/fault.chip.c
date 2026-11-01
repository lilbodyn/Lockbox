#include "wokwi-api.h"
typedef struct {pin_t pir,adc;timer_t t;unsigned phase;} state_t;
static state_t s;
static void cb(void *v){state_t *p=v;switch(p->phase++){
case 0:pin_mode(p->pir,OUTPUT_HIGH);timer_start(p->t,1000000,false);break;
case 1:pin_mode(p->pir,OUTPUT_LOW);timer_start(p->t,3000000,false);break;
case 2:pin_dac_write(p->adc,0.8f);break;}}
void chip_init(void){s.pir=pin_init("PIR",OUTPUT_LOW);s.adc=pin_init("ADC",ANALOG);pin_dac_write(s.adc,2.75f);timer_config_t t={.user_data=&s,.callback=cb};s.t=timer_init(&t);timer_start(s.t,20000000,false);}
