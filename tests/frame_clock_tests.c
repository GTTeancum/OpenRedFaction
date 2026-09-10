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
    /* A 100 ms render stall produces six ticks, five skipped images, one draw. */
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,0));CHECK(rf_frame_clock_present(&c,0));
    for(i=0;i<6;++i){CHECK(!rf_frame_clock_step(&c,100));CHECK(rf_frame_clock_present(&c,100)==(i==5));}
    CHECK(c.steps==7 && c.presentations==2 && c.discarded_units==0);
    CHECK(rf_frame_clock_step(&c,100)==17);
    /* A long pause drops excess elapsed time and bounds catch-up work. */
    CHECK(!rf_frame_clock_step(&c,10100));CHECK(c.credit==7000 && c.discarded_units==592000);
    for(i=0;i<7;++i)CHECK(!rf_frame_clock_step(&c,10100));
    CHECK(rf_frame_clock_step(&c,10100)==17);
    /* Fast catch-up ticks still offer a draw after eight skipped images. */
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,0));
    for(i=1;i<=9;++i){CHECK(!rf_frame_clock_step(&c,100+i*20));CHECK(rf_frame_clock_present(&c,101+i*20)==(i==9));}
    /* A slow simulation must not starve presentation in addition to physics. */
    c=(rf_frame_clock){0};CHECK(!rf_frame_clock_step(&c,0));
    for(i=1;i<=9;++i){CHECK(rf_frame_clock_present(&c,i*100));CHECK(!rf_frame_clock_step(&c,i*100));}
    puts("60 Hz pacing, wrap, catch-up, stall cap and presentation fairness passed");return 0;
}
