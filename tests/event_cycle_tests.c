#include "rf/event_cycle.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_event_cycle state,old;uint32_t pulse=99;
    CHECK(rf_event_cycle_init(&state,.5f,2,0,1000)==RF_OK);
    CHECK(!state.enabled && state.count==0 && state.deadline==1000);
    CHECK(rf_event_cycle_tick(&state,1000,&pulse)==RF_OK && !pulse);
    CHECK(rf_event_cycle_enable(&state,1)==RF_OK);
    CHECK(rf_event_cycle_tick(&state,1000,&pulse)==RF_OK && pulse && state.count==1 && state.deadline==1500);
    CHECK(rf_event_cycle_tick(&state,1499,&pulse)==RF_OK && !pulse);
    CHECK(rf_event_cycle_tick(&state,1500,&pulse)==RF_OK && pulse && state.count==2 && state.deadline==2000);
    CHECK(rf_event_cycle_tick(&state,5000,&pulse)==RF_OK && !pulse);
    CHECK(rf_event_cycle_enable(&state,0)==RF_OK && rf_event_cycle_enable(&state,1)==RF_OK);
    CHECK(rf_event_cycle_tick(&state,6000,&pulse)==RF_OK && !pulse && state.count==2);
    CHECK(rf_event_cycle_init(&state,.5f,0,1,1000)==RF_OK);
    CHECK(rf_event_cycle_enable(&state,1)==RF_OK);
    CHECK(rf_event_cycle_tick(&state,5000,&pulse)==RF_OK && pulse && state.count==1 && state.deadline==5500);
    CHECK(rf_event_cycle_enable(&state,0)==RF_OK);
    CHECK(rf_event_cycle_tick(&state,6000,&pulse)==RF_OK && !pulse && state.deadline==5500);
    CHECK(rf_event_cycle_enable(&state,1)==RF_OK);
    CHECK(rf_event_cycle_tick(&state,6000,&pulse)==RF_OK && pulse && state.count==2 && state.deadline==6500);
    CHECK(rf_event_cycle_init(&state,.5f,0,0,0)==RF_OK && rf_event_cycle_enable(&state,1)==RF_OK);
    CHECK(rf_event_cycle_tick(&state,0,&pulse)==RF_OK && !pulse);
    CHECK(rf_event_cycle_init(&state,.01f,1,0,RF_TIMER_PERIOD-5)==RF_OK && rf_event_cycle_enable(&state,1)==RF_OK);
    CHECK(rf_event_cycle_tick(&state,RF_TIMER_PERIOD-5,&pulse)==RF_OK && pulse && state.deadline==5);
    old=state;pulse=99;
    CHECK(rf_event_cycle_init(&state,NAN,1,0,0)==RF_RANGE && !memcmp(&old,&state,sizeof(state)));
    CHECK(rf_event_cycle_tick(&state,-1,&pulse)==RF_RANGE && pulse==99 && !memcmp(&old,&state,sizeof(state)));
    puts("Cyclic timer finite/unlimited pulses, pause/resume, no catch-up and clock wrap passed");return 0;
}
