#include "releases.h"
#include <string.h>
void release_init(release_t *r, uint32_t now, uint32_t period, uint32_t deadline)
{
    memset(r,0,sizeof *r); r->next=now+period; r->period=period; r->deadline=deadline;
}
bool release_due(release_t *r, uint32_t now)
{
    if (!r->period || (int32_t)(now-r->next)<0) return false;
    uint32_t late=now-r->next;
    uint32_t skipped=late/r->period;
    r->missed+=skipped;
    if (late>r->late_max) r->late_max=late;
    if (r->started) {
        int32_t jitter=(int32_t)(now-r->previous)-(int32_t)r->period;
        uint32_t n=0;
        for (unsigned i=0; i<6; ++i) n+=r->histogram[i];
        if (!n || jitter<r->jitter_min) r->jitter_min=jitter;
        if (!n || jitter>r->jitter_max) r->jitter_max=jitter;
        uint32_t magnitude=(uint32_t)(jitter<0 ? -jitter : jitter);
        unsigned bin=magnitude<10 ? 0 : magnitude<100 ? 1 : magnitude<1000 ? 2 :
            magnitude<5000 ? 3 : magnitude<20000 ? 4 : 5;
        ++r->histogram[bin];
    }
    r->started=true; r->previous=now; r->active_late=late;
    /* Skip backlog; retain the original phase instead of next=now+period. */
    r->next+=(skipped+1)*r->period;
    return true;
}
void release_complete(release_t *r, uint32_t now)
{
    /* Deadline belongs to the oldest release in this dispatch. */
    if ((uint32_t)(now-r->previous)+r->active_late>r->deadline)
        ++r->deadline_misses;
}
uint32_t uptime_update(uptime_t *u, uint32_t now)
{
    uint32_t elapsed=now-u->last_us;
    u->last_us=now;
    /* Divide before adding the remainder, so even a large valid delta fits. */
    u->ms+=elapsed/1000;
    u->remainder+=elapsed%1000;
    u->ms+=u->remainder/1000;
    u->remainder%=1000;
    return u->ms;
}
uint32_t counter16_update(counter16_t *clock, uint16_t raw)
{
    uint16_t step=(uint16_t)(raw-clock->previous);
    clock->previous=raw;
    clock->us+=step;
    if (step>clock->max_step) clock->max_step=step;
    return clock->us;
}
