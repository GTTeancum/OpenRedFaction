#include "rf/frame_clock.h"
static void update(rf_frame_clock *c,uint32_t now)
{
    uint64_t credit;
    if(!c->initialized){c->initialized=1;c->last_ms=now;c->credit=1000;return;}
    credit=c->credit+(uint64_t)(uint32_t)(now-c->last_ms)*60;
    c->last_ms=now;
    if(credit>8000){c->discarded_units+=credit-8000;credit=8000;}
    c->credit=(uint32_t)credit;
}
uint32_t rf_frame_clock_step(rf_frame_clock *c,uint32_t now)
{
    update(c,now);
    if(c->credit<1000)return (1000-c->credit+59)/60;
    c->credit-=1000;++c->steps;return 0;
}
int rf_frame_clock_present(rf_frame_clock *c,uint32_t now)
{
    int overloaded=c->initialized && (uint32_t)(now-c->last_ms)>=17;
    update(c,now);
    /* Skipping draws cannot restore 60 Hz when simulation alone exceeds its
     * budget. Preserve visual updates while the owner profiles that workload. */
    if(!overloaded && c->credit>=1000 && c->skipped<8){++c->skipped;return 0;}
    c->skipped=0;++c->presentations;return 1;
}
