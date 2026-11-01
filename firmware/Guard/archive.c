#include "archive.h"
#include <string.h>
/* On-media layout is independent of C enum size and structure padding.
 * 0 magic, 4 ticket, 8 boot, 12 seq, 16 ms, 20 type/mode/cause/valid,
 * 24 adc, 28/32/36 axes, 40..47 reserved; CRC/inverse at 48/52;
 * the final doubleword commits the slot only after all payload writes. */
static uint32_t get(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8*i));
}
uint32_t archive_crc(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; ++i) c = (c>>1) ^ (0xedb88320u & (0u-(c&1u)));
    }
    return ~c;
}
static bool blank(const uint8_t *p)
{
    for (unsigned i = 0; i < ARCHIVE_SLOT_BYTES; ++i) if (p[i] != 0xff) return false;
    return true;
}
static bool valid(const uint8_t *p)
{
    uint32_t crc = archive_crc(p,48);
    return get(p)==0x33424c41u && get(p+48)==crc && get(p+52)==~crc &&
        get(p+56)==0x54494d43u && get(p+60)==~get(p+4) &&
        p[20]<=EV_ADC_RECOVERED && p[21]<=GUARD_ALARM && p[22]<=3 && p[23]<=7 && get(p+24)<=4095;
}
static void encode(archive_t *a, const guard_event_t *e)
{
    uint8_t *p=a->image;
    memset(p,0,ARCHIVE_SLOT_BYTES);
    put(p,0x33424c41u); put(p+4,a->ticket+1); put(p+8,a->boot);
    put(p+12,e->seq); put(p+16,e->ms);
    p[20]=(uint8_t)e->type; p[21]=(uint8_t)e->mode; p[22]=e->causes;
    p[23]=(uint8_t)(e->readings.imu_ok | e->readings.adc_ok<<1 | e->readings.pir<<2);
    put(p+24,e->readings.adc);
    put(p+28,(uint32_t)e->readings.accel.x); put(p+32,(uint32_t)e->readings.accel.y);
    put(p+36,(uint32_t)e->readings.accel.z);
    put(p+48,archive_crc(p,48)); put(p+52,~get(p+48));
    put(p+56,0x54494d43u); put(p+60,~get(p+4));
}
void archive_open(archive_t *a, archive_media_t media)
{
    memset(a,0,sizeof *a); a->media=media;
    unsigned newest=0;
    for (unsigned i=0; i<ARCHIVE_SLOTS; ++i) {
        if (!media.read(media.context,i*ARCHIVE_SLOT_BYTES,a->image,ARCHIVE_SLOT_BYTES)) {
            a->failed=true; ++a->failures; return;
        }
        if (valid(a->image)) {
            ++a->accepted;
            uint32_t t=get(a->image+4);
            if (t>a->ticket) { a->ticket=t; a->boot=get(a->image+8); newest=i; }
        } else if (!blank(a->image)) ++a->corrupt;
    }
    ++a->boot;
    unsigned page=a->ticket ? newest/32 : 0;
    unsigned first=a->ticket ? newest+1 : 0;
    a->slot=(uint16_t)((page^1u)*32);
    a->phase=1; /* No room on active page: erase only the older page. */
    for (unsigned i=first; i<(page+1)*32; ++i) {
        if (!media.read(media.context,i*64,a->image,64)) { a->failed=true; ++a->failures; return; }
        if (blank(a->image)) { a->slot=(uint16_t)i; a->phase=0; break; }
    }
}
bool archive_enqueue(archive_t *a, const guard_event_t *event)
{
    if (a->count==ARCHIVE_PENDING || a->ticket==0xffffffffu || a->boot==0) { ++a->lost; return false; }
    a->pending[(a->head+a->count)%ARCHIVE_PENDING]=*event;
    ++a->count;
    return true;
}
bool archive_step(archive_t *a)
{
    if (a->failed || !a->count) return false;
    if (a->ticket==0xffffffffu) { a->failed=true; ++a->failures; return true; }
    bool ok;
    if (a->phase==1) {
        ok=a->media.erase(a->media.context,a->slot/32);
        if (ok) a->phase=0;
    } else {
        if (a->word==0) encode(a,&a->pending[a->head]);
        ok=a->media.program(a->media.context,a->slot*64+a->word*8,a->image+a->word*8);
        if (ok && ++a->word==8) {
            uint8_t check[64];
            ok=a->media.read(a->media.context,a->slot*64,check,64) &&
                memcmp(check,a->image,64)==0 && valid(check);
            if (ok) {
                ++a->ticket; a->word=0; ++a->slot;
                a->head=(a->head+1)%ARCHIVE_PENDING; --a->count;
                if (a->slot%32==0) { a->slot=(a->slot/32%2)*32; a->phase=1; }
            }
        }
    }
    if (!ok) { ++a->failures; a->failed=true; }
    return true;
}
bool archive_read(archive_t *a, unsigned slot, guard_event_t *e, uint32_t *ticket, uint32_t *boot)
{
    uint8_t p[64];
    if (slot>=ARCHIVE_SLOTS || !a->media.read(a->media.context,slot*64,p,64) || !valid(p)) return false;
    memset(e,0,sizeof *e);
    *ticket=get(p+4); *boot=get(p+8);
    e->seq=get(p+12); e->ms=get(p+16); e->type=(event_type_t)p[20]; e->mode=(guard_mode_t)p[21]; e->causes=p[22];
    e->readings.imu_ok=(p[23]&1)!=0; e->readings.adc_ok=(p[23]&2)!=0; e->readings.pir=(p[23]&4)!=0;
    e->readings.adc=(uint16_t)get(p+24);
    e->readings.accel=(acceleration_t){(int32_t)get(p+28),(int32_t)get(p+32),(int32_t)get(p+36)};
    return true;
}
