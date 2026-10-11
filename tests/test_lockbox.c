#include "guard.h"
#include "commands.h"
#include "analog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/captured_acceleration.h"

typedef struct {
    uint8_t registers[128], writes[4][2];
    unsigned operations, fail_at, written;
    bus_result_t failure;
} fake_bus_t;
static bus_result_t fake_read(void *context, uint8_t address, uint8_t reg, uint8_t *data, size_t n)
{
    fake_bus_t *fake = context;
    assert(address == 0x68);
    if (++fake->operations == fake->fail_at) return fake->failure;
    memcpy(data, fake->registers + reg, n);
    return BUS_OK;
}
static bus_result_t fake_write(void *context, uint8_t address, uint8_t reg, const uint8_t *data, size_t n)
{
    fake_bus_t *fake = context;
    assert(address == 0x68 && n == 1);
    if (++fake->operations == fake->fail_at) return fake->failure;
    assert(fake->written < 4);
    fake->writes[fake->written][0] = reg;
    fake->writes[fake->written++][1] = data[0];
    fake->registers[reg] = data[0];
    return BUS_OK;
}
static mpu6050_t device(fake_bus_t *fake)
{
    fake->registers[0x75] = 0x68;
    return (mpu6050_t){{fake, fake_read, fake_write}, 0x68};
}
static void test_driver(void)
{
    fake_bus_t fake = {0}; mpu6050_t imu = device(&fake);
    assert(mpu6050_start(&imu) == MPU_OK);
    const uint8_t expected[4][2] = {{0x6b,0},{0x1a,3},{0x19,9},{0x1c,8}};
    assert(fake.operations == 5 && memcmp(fake.writes, expected, sizeof expected) == 0);
    for (unsigned fail = 1; fail <= 5; ++fail) {
        for (unsigned error = BUS_NACK; error <= BUS_IO_ERROR; ++error) {
            memset(&fake, 0, sizeof fake); imu = device(&fake);
            fake.fail_at = fail; fake.failure = (bus_result_t)error;
            mpu_result_t wanted = error == BUS_NACK ? MPU_NACK : error == BUS_TIMEOUT ? MPU_TIMEOUT : MPU_BUS_ERROR;
            assert(mpu6050_start(&imu) == wanted);
            assert(fake.operations == fail);
        }
    }
    memset(&fake, 0, sizeof fake); imu = device(&fake); fake.registers[0x75] = 0x70;
    assert(mpu6050_start(&imu) == MPU_WRONG_ID && fake.written == 0);
    uint8_t id = 0; assert(mpu6050_identify(&imu, &id) == MPU_WRONG_ID && id == 0x70);
    assert(strcmp(mpu6050_result_name(MPU_OK),"OK") == 0);
    assert(strcmp(mpu6050_result_name(MPU_NACK),"NACK") == 0);
    assert(strcmp(mpu6050_result_name(MPU_TIMEOUT),"TIMEOUT") == 0);
    assert(strcmp(mpu6050_result_name(MPU_WRONG_ID),"WRONG_ID") == 0);
    assert(strcmp(mpu6050_result_name(MPU_BUS_ERROR),"BUS_ERROR") == 0);
}
static void test_conversion(void)
{
    assert(mpu6050_to_mg(0x20,0) == 1000);
    assert(mpu6050_to_mg(0xe0,0) == -1000);
    assert(mpu6050_to_mg(0x80,0) == -4000);
    assert(mpu6050_to_mg(0x7f,0xff) == 3999);
    fake_bus_t fake = {0}; mpu6050_t imu = device(&fake);
    const uint8_t frame[6] = {0xe0,0,0x20,0,0x10,0};
    memcpy(fake.registers+0x3b, frame,6);
    acceleration_t value = {0}; uint8_t raw[6] = {0};
    assert(mpu6050_read(&imu,&value,raw) == MPU_OK);
    assert(value.x == -1000 && value.y == 1000 && value.z == 500 && memcmp(raw,frame,6) == 0);
    for (unsigned error=BUS_NACK;error<=BUS_IO_ERROR;++error) {
        fake.fail_at = fake.operations+1; fake.failure = (bus_result_t)error;
        acceleration_t old=value; uint8_t old_raw[6]; memcpy(old_raw,raw,6);
        assert(mpu6050_read(&imu,&value,raw) != MPU_OK);
        assert(memcmp(&old,&value,sizeof old) == 0 && memcmp(raw,old_raw,6) == 0);
    }
    assert(adc_to_mv(0)==0 && adc_to_mv(4095)==3300 && adc_to_mv(65535)==3300);
    assert(adc_to_mv(2048)==1650);
    assert(median3(1,2,3)==2 && median3(3,1,2)==2 && median3(2,3,1)==2);
    assert(median3(2,2,1)==2 && median3(3,2,1)==2 && median3(1,3,2)==2);
}
static void test_captured_values(void)
{
    fake_bus_t fake = {0}; mpu6050_t imu = device(&fake);
    for (unsigned i = 0; i < 2; ++i) {
        memcpy(fake.registers + 0x3b, captured_acceleration[i], 6);
        acceleration_t value = {0}; uint8_t raw[6] = {0};
        assert(mpu6050_read(&imu, &value, raw) == MPU_OK);
        assert(value.x == (i == 0 ? 0 : 399) && value.y == 0 && value.z == 1000);
        assert(memcmp(raw, captured_acceleration[i], 6) == 0);
    }
}
static observation_t quiet(void) { return (observation_t){{0,0,1000},1000,true,true,false}; }
static unsigned emitted;
static void event_observer(void *context,const guard_event_t *event)
{
    assert(context == &emitted && event->seq != 0); ++emitted;
}
static void armed(guard_t *guard)
{
    guard_init(guard,false,0); guard_arm(guard,0);
    for (unsigned i=1;i<=25;++i) guard_observe(guard,quiet(),i*20);
    assert(guard->mode==GUARD_ARMED);
}
static void test_button(void)
{
    guard_t g; guard_init(&g,false,0);
    guard_button(&g,true,1);guard_button(&g,false,4);guard_button(&g,true,8);
    guard_button(&g,true,37);assert(g.mode==GUARD_OFF);
    guard_button(&g,true,38);assert(g.mode==GUARD_CALIBRATING);
    guard_button(&g,true,1000);assert(g.sequence==1);
    guard_button(&g,false,1001);guard_button(&g,false,1031);
    guard_button(&g,true,1040);guard_button(&g,true,1070);assert(g.mode==GUARD_OFF);
    guard_init(&g,true,0);guard_button(&g,true,100);assert(g.sequence==0);
    guard_button(&g,false,110);guard_button(&g,false,140);
    guard_button(&g,true,150);guard_button(&g,true,180);assert(g.mode==GUARD_CALIBRATING);
    guard_init(&g,false,UINT32_MAX-20);guard_button(&g,true,UINT32_MAX-15);
    guard_button(&g,true,14);assert(g.mode==GUARD_CALIBRATING);
}
static void test_calibration(void)
{
    guard_t g;guard_init(&g,false,0);guard_arm(&g,0);guard_arm(&g,1);
    for(unsigned i=1;i<=24;++i)guard_observe(&g,quiet(),i*20);
    assert(g.mode==GUARD_CALIBRATING);guard_observe(&g,quiet(),500);assert(g.mode==GUARD_ARMED);
    for(unsigned variant=0;variant<7;++variant){
        guard_init(&g,false,0);guard_arm(&g,0);observation_t s=quiet();
        if(variant==0)s.imu_ok=false;
        if(variant==1)s.adc_ok=false;
        if(variant==2)s.accel.z=799;
        if(variant==3)s.accel.z=1201;
        if(variant==4)s.adc=4096;
        for(unsigned i=1;i<100;++i){
            if(variant==5)s.accel.x=(i%2)?0:151;
            if(variant==6)s.adc=(i%2)?1000:1081;
            guard_observe(&g,s,i*20);
        }
        assert(g.mode==GUARD_CALIBRATING);
        guard_observe(&g,s,2000);assert(g.mode==GUARD_OFF);
        assert(guard_journal_at(&g,g.journal_count-1)->type==EV_ARM_REJECTED);
    }
    guard_init(&g,false,UINT32_MAX-1000);guard_arm(&g,UINT32_MAX-1000);
    observation_t s=quiet();s.imu_ok=false;guard_observe(&g,s,1000);assert(g.mode==GUARD_OFF);
    for(unsigned boundary=0;boundary<2;++boundary){
        guard_init(&g,false,0);guard_arm(&g,0);s=quiet();s.accel.z=boundary?1200:800;
        for(unsigned i=1;i<=25;++i)guard_observe(&g,s,i*20);
        assert(g.mode==GUARD_ARMED);
    }
}
static void test_shock(void)
{
    guard_t g;armed(&g);observation_t s=quiet();s.accel.x=349;
    guard_observe(&g,s,520);
    assert(g.causes==0);
    s.accel.x=699;guard_observe(&g,s,540);
    assert(g.causes==CAUSE_SHOCK);
    unsigned events=g.sequence;s.accel.x=0;guard_observe(&g,s,560);assert(g.sequence==events);
    guard_disarm(&g,580);assert(g.mode==GUARD_OFF && g.causes==0);
    s.accel.x=2000;guard_observe(&g,s,600);
    assert(g.causes==0);
    armed(&g);s=quiet();s.accel.x=210;s.accel.y=280;
    guard_observe(&g,s,520);
    assert(g.causes==CAUSE_SHOCK);
}
static void test_light(void)
{
    guard_t g;observation_t s=quiet();
    armed(&g);
    s.adc=1600;guard_observe(&g,s,520);
    s.adc=1000;
    for(unsigned i=0;i<10;++i)guard_observe(&g,s,540+i*20);
    assert(g.causes==0);
    armed(&g);
    s.adc=1599;
    for(unsigned i=0;i<10;++i)guard_observe(&g,s,520+i*20);
    assert(g.causes==0);
    armed(&g);
    s.adc=1600;
    guard_observe(&g,s,520);guard_observe(&g,s,540);guard_observe(&g,s,560);
    assert(g.causes==0);
    guard_observe(&g,s,580);
    assert(g.causes==CAUSE_LIGHT);
    armed(&g);
    s.adc=400;
    for(unsigned i=0;i<4;++i)guard_observe(&g,s,520+i*20);
    assert(g.causes==CAUSE_LIGHT);
    armed(&g);s=quiet();
    for(unsigned i=1;i<=20;++i){s.adc=1000+i*20;guard_observe(&g,s,500+i*20);}
    assert(g.causes==0);
    s.adc_ok=false;guard_observe(&g,s,920);s.adc_ok=true;
    s.adc=4000;
    for(unsigned i=0;i<10;++i)guard_observe(&g,s,940+i*20);
    assert(g.causes==0);
    s.adc=4096;guard_observe(&g,s,1200);assert(g.history_count==0);
}
static void test_combined_and_faults(void)
{
    guard_t g;armed(&g);observation_t s=quiet();s.pir=true;guard_observe(&g,s,520);
    assert(g.causes==0);
    s.pir=false;guard_observe(&g,s,540);
    assert(g.causes==0);
    s.imu_ok=false;guard_observe(&g,s,560);assert(g.mode==GUARD_ARMED);
    s.adc=1600;for(unsigned i=0;i<4;++i)guard_observe(&g,s,580+i*20);
    assert(g.causes==CAUSE_LIGHT);
    s.imu_ok=true;s.accel.x=2000;guard_observe(&g,s,680);
    assert(g.causes==CAUSE_LIGHT);
    s.accel.x=0;guard_observe(&g,s,700);
    assert(g.causes==3);
    s.adc_ok=false;guard_observe(&g,s,720);s.adc_ok=true;guard_observe(&g,s,740);
    assert(g.causes==3);
    bool saw_imu_fault=false,saw_recovery=false,saw_adc_fault=false,saw_adc_recovery=false;
    for(uint8_t i=0;i<g.journal_count;++i){event_type_t t=guard_journal_at(&g,i)->type;
        saw_imu_fault|=t==EV_IMU_FAULT;saw_recovery|=t==EV_IMU_RECOVERED;
        saw_adc_fault|=t==EV_ADC_FAULT;saw_adc_recovery|=t==EV_ADC_RECOVERED;
    }
    assert(saw_imu_fault&&saw_recovery&&saw_adc_fault&&saw_adc_recovery);
    armed(&g);s=quiet();
    s.adc=1600;
    for(unsigned i=0;i<3;++i)guard_observe(&g,s,520+i*20);
    s.accel.z=1350;s.pir=true;guard_observe(&g,s,580);
    assert(g.causes==3);
}
static void test_journal(void)
{
    guard_t g;guard_init(&g,false,0);g.on_event=event_observer;g.event_context=&emitted;
    observation_t s=quiet();guard_observe(&g,s,0);
    for(unsigned i=1;i<=40;++i){s.pir=!s.pir;guard_observe(&g,s,i*20);}
    assert(g.journal_count==16&&g.overwritten==24&&emitted==40);
    assert(guard_journal_at(&g,0)->seq==25&&guard_journal_at(&g,15)->seq==40);
    assert(guard_journal_at(&g,16)==NULL);
    assert(guard_journal_at(&g,0)->readings.adc==1000&&guard_journal_at(&g,0)->ms==500);
    for(unsigned i=0;i<4;++i)assert(strlen(guard_mode_name((guard_mode_t)i))>0);
    for(unsigned i=0;i<12;++i)assert(strlen(guard_event_name((event_type_t)i))>0);
    guard_disarm(&g,900);assert(g.sequence==40);
}
static void test_commands(void)
{
    const char *names[]={"status","arm","disarm","probe","nack","journal","help","imu 68","imu 69"};
    command_input_t input={0};
    for(unsigned i=0;i<9;++i){for(const char *p=names[i];*p;++p)assert(command_feed(&input,*p)==CMD_NONE);
        assert(command_feed(&input,'\r')==CMD_NONE);assert(command_feed(&input,'\n')==(command_t)(CMD_STATUS+i));}
    assert(command_feed(&input,'\n')==CMD_NONE);
    command_feed(&input,'x');assert(command_feed(&input,'\n')==CMD_INVALID);
    for(unsigned i=0;i<50;++i)command_feed(&input,'a');
    assert(command_feed(&input,'\n')==CMD_INVALID&&input.length==0&&!input.overflow);
    for(const char *p="status\n";*p;++p)command_feed(&input,*p);
    assert(input.length==0);
}
int main(void)
{
    test_driver();puts("PASS register order and all bus error positions");
    test_conversion();puts("PASS signed acceleration ADC and median");
    test_button();puts("PASS debounce hold startup and tick wrap");
    test_calibration();puts("PASS calibration stability timeout and boundaries");
    test_shock();puts("PASS shock threshold vector latch and disarm");
    test_light();puts("PASS light threshold persistence glitch drift and invalid data");
    test_combined_and_faults();puts("PASS concurrent causes PIR fault and recovery");
    test_journal();puts("PASS event snapshots ordering and journal overwrite");
    test_commands();puts("PASS UART grammar overflow and recovery");
    test_captured_values();puts("PASS actual captured UART acceleration payloads");
    return 0;
}
