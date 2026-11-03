#include "rf/frame_clock.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void)
{
    rf_frame_clock c={0};uint32_t ms,i;
    CHECK(!rf_frame_clock_step(&c,0));CHECK(rf_frame_clock_present(&c,0));
    for(ms=1;ms<=10000;++ms)while(!rf_frame_clock_step(&c,ms))rf_frame_clock_present(&c,ms);
    CHECK(c.steps==601 && c.credit==0 && c.discarded_units==0);
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,UINT32_MAX-9));
    CHECK(rf_frame_clock_step(&c,0)==7);CHECK(!rf_frame_clock_step(&c,7));
    CHECK(c.credit==20 && c.discarded_units==0);
    /* A 100 ms stall draws immediately, then catches up the remaining debt. */
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,0));CHECK(rf_frame_clock_present(&c,0));
    for(i=0;i<6;++i){CHECK(!rf_frame_clock_step(&c,100));CHECK(rf_frame_clock_present(&c,100)==(i==0 || i==5));}
    CHECK(c.steps==7 && c.presentations==3 && c.discarded_units==0);
    CHECK(rf_frame_clock_step(&c,100)==17);
    /* A long pause drops excess elapsed time and bounds catch-up work
     * (15 ticks, the PS2's 0.25 s frame-time cap). */
    CHECK(!rf_frame_clock_step(&c,10100));CHECK(c.credit==15000 && c.discarded_units==584000);
    for(i=0;i<15;++i)CHECK(!rf_frame_clock_step(&c,10100));
    CHECK(rf_frame_clock_step(&c,10100)==17);
    /* A 50 ms presentation deadline (20 Hz floor) bounds catch-up starvation. */
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,0));
    for(i=1;i<=9;++i){CHECK(!rf_frame_clock_step(&c,100+i*20));CHECK(rf_frame_clock_present(&c,101+i*20)==(i%3==1));}
    /* A slow simulation must not starve presentation in addition to physics. */
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,0));
    for(i=1;i<=9;++i){CHECK(rf_frame_clock_present(&c,i*100));CHECK(!rf_frame_clock_step(&c,i*100));}
    /* Real pipeline: 20 ms physics AFTER presentation, then 5 ms preparation.
     * The old pre-presentation-only overload check skipped eight of nine draws. */
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,0));CHECK(rf_frame_clock_present(&c,0));
    for(i=1;i<=12;++i){CHECK(!rf_frame_clock_step(&c,i*25-5));CHECK(rf_frame_clock_present(&c,i*25)==(i%2==1));}
    CHECK(c.presentations==7);
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,UINT32_MAX-20));
    CHECK(rf_frame_clock_present(&c,UINT32_MAX-20));CHECK(!rf_frame_clock_step(&c,49));
    CHECK(rf_frame_clock_present(&c,50));
    puts("60 Hz pacing, wrap, catch-up, stall cap and presentation fairness passed");return 0;
}
