#include "rf/frame_clock.h"
uint32_t rf_tick_frames=1,rf_tick_frames_next=1;
static uint32_t tick_units(void){return rf_tick_frames==2?2000u:1000u;}
static void update(rf_frame_clock *c,uint32_t now)
{
    uint64_t credit;
    if(!c->initialized){c->initialized=1;c->last_ms=now;c->credit=1000;return;}
    credit=c->credit+(uint64_t)(uint32_t)(now-c->last_ms)*60;
    c->last_ms=now;
    if(credit>8000u+tick_units()){c->discarded_units+=credit-8000u-tick_units();credit=8000u+tick_units();}
    c->credit=(uint32_t)credit;
}
uint32_t rf_frame_clock_step(rf_frame_clock *c,uint32_t now)
{
    update(c,now);
    if(c->credit<tick_units())return (tick_units()-c->credit+59)/60;
    c->credit-=tick_units();++c->steps;return 0;
}
int rf_frame_clock_present(rf_frame_clock *c,uint32_t now)
{
    update(c,now);
    /* Behind real time (a whole tick already owed): skip this picture so the
     * next update runs at once and game time keeps pace, instead of the game
     * slowing down. At most three skips in a row and 100 ms between pictures,
     * so play stays at 10+ pictures per second when simulation alone is slow
     * (the debt limit then discards the rest). */
    if(c->presentations && (uint32_t)(now-c->last_present_ms)<100 &&
       c->credit>=tick_units() && c->skipped<3){++c->skipped;return 0;}
    c->skipped=0;c->last_present_ms=now;++c->presentations;return 1;
}
