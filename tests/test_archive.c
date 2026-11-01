#include "archive.h"
#include "releases.h"
#include "../firmware/Board/coarse_clock.h"
#include "commands.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t bytes[4096]; int budget; bool erased; } medium_t;
static medium_t disk;
static int read_budget=-1;
static bool read_memory(void *ctx,uint32_t offset,void *out,size_t n)
{
    if (read_budget==0) return false;
    if (read_budget>0) --read_budget;
    medium_t *m=ctx; assert(offset+n<=4096); memcpy(out,m->bytes+offset,n); return true;
}
static bool spend(medium_t *m)
{
    if (m->budget==0) return false;
    if (m->budget>0) --m->budget;
    return true;
}
static bool erase_memory(void *ctx,unsigned page)
{
    medium_t *m=ctx; assert(page<2); m->erased=true;
    for (unsigned i=page*2048; i<(page+1)*2048; ++i) {
        if (!spend(m)) return false;
        m->bytes[i]=255;
    }
    return true;
}
static bool program_memory(void *ctx,uint32_t offset,const uint8_t p[8])
{
    medium_t *m=ctx; assert(!(offset%8)); assert(offset<=4088);
    for (unsigned i=0; i<8; ++i) {
        assert((m->bytes[offset+i]&p[i])==p[i]);
        if (!spend(m)) return false;
        m->bytes[offset+i]&=p[i];
    }
    return true;
}
static archive_media_t media(void) { return (archive_media_t){&disk,read_memory,erase_memory,program_memory}; }
static guard_event_t event(unsigned n)
{
    return (guard_event_t){n,20*n,EV_SHOCK,GUARD_ALARM,CAUSE_SHOCK,{{-350,0,1000},3413,true,true,false}};
}
static void drain(archive_t *a)
{
    unsigned limit=1000;
    while (a->count && !a->failed) { assert(limit--); assert(archive_step(a)); }
}
static void write_event(archive_t *a,unsigned n)
{
    guard_event_t e=event(n); assert(archive_enqueue(a,&e)); drain(a);
}
static void reset_media(void) { memset(&disk,255,sizeof disk); disk.budget=-1; disk.erased=false; }
static void scan_expected(unsigned newest)
{
    archive_t reboot; archive_open(&reboot,media()); assert(!reboot.failed); assert(reboot.ticket==newest);
    if (newest>=32) assert(reboot.accepted>=32);
    guard_event_t e; uint32_t ticket,boot;
    for (unsigned s=0; s<64; ++s) if (archive_read(&reboot,s,&e,&ticket,&boot)) {
        assert(e.seq==ticket); assert(e.ms==20*ticket); assert(e.causes==1);
        assert(e.readings.accel.x==-350 && e.readings.adc==3413); assert(boot>=1);
    }
}
int main(void)
{
    assert(archive_crc((const uint8_t *)"123456789",9)==0xcbf43926u);
    reset_media(); archive_t a; archive_open(&a,media());
    assert(a.ticket==0 && a.corrupt==0 && a.boot==1);
    for (unsigned n=1; n<=200; ++n) { write_event(&a,n); assert(!a.failed); }
    scan_expected(200); assert(disk.erased);
    /* Every byte cut of a complete 64-byte append, with reboot/recovery. */
    for (unsigned cut=0; cut<=64; ++cut) {
        reset_media(); archive_open(&a,media()); write_event(&a,1);
        disk.budget=(int)cut; write_event(&a,2);
        archive_t recovered; archive_open(&recovered,media());
        assert(recovered.ticket==1 || recovered.ticket==2);
        if (cut<56) assert(recovered.ticket==1);
        if (cut==64) assert(recovered.ticket==2);
        unsigned newest=recovered.ticket;
        scan_expected(newest);
        disk.budget=-1; archive_open(&a,media());
        write_event(&a,newest+1); scan_expected(newest+1);
    }
    /* Erase the older page at the third page transition; retain ticket 64. */
    reset_media(); archive_open(&a,media());
    for (unsigned n=1; n<=64; ++n) write_event(&a,n);
    medium_t saved=disk;
    for (unsigned cut=0; cut<=2112; ++cut) {
        disk=saved; archive_open(&a,media()); disk.budget=(int)cut;
        write_event(&a,65);
        archive_t recovered; archive_open(&recovered,media());
        assert(recovered.ticket==64 || recovered.ticket==65);
        if (cut<2104) assert(recovered.ticket==64);
        if (cut==2112) assert(recovered.ticket==65);
        unsigned newest=recovered.ticket;
        scan_expected(newest);
        disk.budget=-1; archive_open(&a,media());
        write_event(&a,newest+1); scan_expected(newest+1);
    }
    reset_media(); archive_open(&a,media()); write_event(&a,1);
    medium_t one=disk;
    unsigned rejected=0;
    for (unsigned byte=0; byte<64; ++byte) for (unsigned bit=0; bit<8; ++bit) {
        disk=one; disk.bytes[byte]^=(uint8_t)(1u<<bit);
        archive_open(&a,media()); assert(a.ticket==0 && a.corrupt==1); ++rejected;
    }
    reset_media(); archive_open(&a,media());
    guard_event_t e=event(1);
    for (unsigned i=0;i<8;++i) assert(archive_enqueue(&a,&e));
    assert(!archive_enqueue(&a,&e) && a.lost==1);
    disk.budget=0; drain(&a); assert(a.failed && a.failures==1 && a.count==8);
    assert(!archive_step(&a));
    reset_media(); read_budget=0; archive_open(&a,media()); assert(a.failed && a.failures==1);
    read_budget=64; archive_open(&a,media()); assert(a.failed && a.failures==1);
    read_budget=-1; archive_open(&a,media()); assert(archive_enqueue(&a,&e));
    for (unsigned i=0; i<7; ++i) assert(archive_step(&a));
    read_budget=0; assert(archive_step(&a)); assert(a.failed && a.ticket==0 && a.count==1);
    read_budget=-1; archive_open(&a,media());
    uint32_t rt,rb; assert(!archive_read(&a,64,&e,&rt,&rb));
    read_budget=0; assert(!archive_read(&a,0,&e,&rt,&rb)); read_budget=-1;
    assert(archive_enqueue(&a,&e)); a.ticket=0xffffffffu;
    assert(!archive_enqueue(&a,&e)); assert(archive_step(&a) && a.failed);
    /* Real guard events, not a second copy of the alarm policy. */
    reset_media(); archive_open(&a,media()); guard_t g;
    guard_init(&g,false,0);
    guard_arm(&g,1);
    for (unsigned i=0;i<25;++i) guard_observe(&g,(observation_t){{0,0,1000},3413,true,true,false},20+20*i);
    assert(g.mode==GUARD_ARMED);
    guard_observe(&g,(observation_t){{350,0,1000},3413,true,true,true},520);
    assert(g.causes==CAUSE_SHOCK);
    for (unsigned i=0;i<g.journal_count;++i) assert(archive_enqueue(&a,guard_journal_at(&g,i)));
    drain(&a); assert(a.ticket==g.journal_count);
    release_t r; release_init(&r,0xfffffff0u,20,10);
    assert(!release_due(&r,3)); assert(release_due(&r,4)); release_complete(&r,14);
    assert(r.deadline_misses==0); assert(release_due(&r,69)); release_complete(&r,70);
    assert(r.missed==2 && r.deadline_misses==1 && r.late_max==45 && r.jitter_max==45);
    assert(!release_due(&r,70)); assert(release_due(&r,84));
    assert(r.jitter_min==-5 && r.jitter_max==45);
    release_init(&r,0,1000,1000);
    assert(!release_due(&r,999));
    for (uint32_t t=1000; t<=3600000000u; t+=1000) {
        assert(release_due(&r,t)); release_complete(&r,t+50);
    }
    assert(r.missed==0 && r.deadline_misses==0 && r.histogram[0]==3599999u);
    release_init(&r,0,100,50);
    assert(release_due(&r,100)); assert(release_due(&r,230)); assert(r.jitter_min==30);
    assert(release_due(&r,400)); assert(r.histogram[1]==2 && r.missed==1);
    release_init(&r,0,10000,20000);
    assert(release_due(&r,10000)); assert(release_due(&r,22000));
    assert(release_due(&r,37000)); assert(release_due(&r,62000)); assert(release_due(&r,102000));
    assert(r.histogram[3] && r.histogram[4] && r.histogram[5]);
    release_init(&r,0,0,0); assert(!release_due(&r,0));
    /* Both the microsecond representation and the HAL tick itself wrap. */
    assert((uint32_t)(coarse_clock_us(4294968u)-coarse_clock_us(4294967u))==1000u);
    assert((uint32_t)(coarse_clock_us(0)-coarse_clock_us(UINT32_MAX))==1000u);
    assert((uint32_t)(0u-UINT32_MAX)==1u);
    release_init(&r,coarse_clock_us(4294967u),1000,1000);
    assert(release_due(&r,coarse_clock_us(4294968u)));
    release_init(&r,coarse_clock_us(UINT32_MAX),1000,1000);
    assert(release_due(&r,coarse_clock_us(0)));
    uptime_t u={.last_us=0xfffffff0u};
    assert(uptime_update(&u,984)==1);
    assert(uptime_update(&u,1484)==1 && u.remainder==500);
    assert(uptime_update(&u,1984)==2 && u.remainder==0);
    uint32_t raw=1984;
    for (unsigned i=0;i<100000;++i) { raw+=60000; uptime_update(&u,raw); }
    assert(u.ms==6000002 && u.remainder==0);
    counter16_t clock={0};
    uint64_t total=0;
    static const unsigned steps[]={1,999,1000,32767,60000,65535,0,7};
    for (unsigned i=0;i<400000;++i) {
        total+=steps[i%8];
        assert(counter16_update(&clock,(uint16_t)total)==(uint32_t)total);
    }
    assert(total>UINT32_MAX && clock.max_step==65535);
    /* A complete raw wrap is indistinguishable from zero progress. This
       demonstrates the documented strict polling bound, not a passing MCU test. */
    uint32_t held=clock.us;
    assert(counter16_update(&clock,clock.previous)==held);
    /* A faulty extended IRQ clock moving backwards by one microsecond made
       the old uint32 uptime accumulator interpret it as almost a full wrap. */
    uptime_t old_clock={.last_us=21087500};
    assert(uptime_update(&old_clock,21087499)==4294967);
    command_input_t c={0}; const char *s="stats reset\n"; command_t cmd=CMD_NONE;
    while (*s) cmd=command_feed(&c,*s++);
    assert(cmd==CMD_RESET_STATS);
    printf("PASS Lockbox archive: 200 writes, 65 append cuts, 2113 erase+append cuts, %u bit corruptions; guard integration; release wrap/deadline/jitter; 3600000 synthetic millisecond releases\n",rejected);
    return 0;
}
