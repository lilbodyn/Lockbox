#include "wokwi-api.h"
typedef struct {timer_t timer; unsigned reg, offset, first, phase; bool ready,shock;} state_t;
static state_t s;
static void timer_cb(void *ctx) {state_t *p=ctx; if(p->phase++==0){p->ready=true;timer_start(p->timer,21000000,false);}else p->shock=true;}
static bool connect_cb(void *ctx,uint32_t addr,bool read) {state_t *p=ctx;(void)addr;if(!read)p->first=1;return p->ready;}
static bool write_cb(void *ctx,uint8_t val) {state_t *p=ctx;if(p->first){p->reg=val;p->offset=0;p->first=0;}return true;}
static uint8_t read_cb(void *ctx){state_t *p=ctx;unsigned r=p->reg+p->offset++;if(r==0x75)return 0x68;if(r==0x3b)return p->shock?0x0c:0;if(r==0x3c)return p->shock?0xcc:0;if(r==0x3f)return 0x20;return 0;}
void chip_init(void){timer_config_t t={.user_data=&s,.callback=timer_cb};s.timer=timer_init(&t);timer_start(s.timer,4000000,false);i2c_config_t c={.user_data=&s,.address=0x68,.scl=pin_init("SCL",INPUT),.sda=pin_init("SDA",INPUT),.connect=connect_cb,.read=read_cb,.write=write_cb};i2c_init(&c);}
