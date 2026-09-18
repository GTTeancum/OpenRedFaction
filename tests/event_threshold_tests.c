#include "rf/event_threshold.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {
    rf_event_threshold monitor;rf_level_link_target links[5];float health,armor;
    uint32_t calls,event_on,mover_on,trigger_flags,seen_armor;
} fixture;
static int query(void *context,uint32_t handle,uint32_t armor,float *value)
{
    fixture *f=context;if(handle!=10)return RF_NOT_FOUND;
    f->seen_armor=armor;*value=armor?f->armor:f->health;return RF_OK;
}
static int effect(void *context,uint32_t handle)
{
    fixture *f=context;int status;if(!f->monitor.fired)return RF_FORMAT;
    ++f->calls;
    /* Reentrant outputs must never replay this one-shot observer. */
    status=rf_event_threshold_poll(&f->monitor,0,f->links,5,query,effect,f);if(status)return status;
    if(handle==20){++f->event_on;return RF_OK;}
    if(handle==30){++f->mover_on;return RF_OK;}
    if(handle==40){f->trigger_flags&=~16u;return RF_OK;}
    return RF_NOT_FOUND;
}
int main(void)
{
    fixture f={0};uint32_t i;
    for(i=0;i<5;i++){f.links[i].kind=1;f.links[i].value=10+i*10;}
    f.links[4].kind=0;f.monitor.threshold=50;f.health=51;f.armor=60;f.trigger_flags=0x1010;
    CHECK(rf_event_threshold_poll(&f.monitor,0,f.links,5,query,effect,&f)==RF_OK && !f.calls);
    f.health=50;
    CHECK(rf_event_threshold_poll(&f.monitor,0,f.links,5,query,effect,&f)==RF_OK);
    CHECK(f.monitor.fired && f.calls==4 && f.event_on==1 && f.mover_on==1 && f.trigger_flags==0x1000);
    f.health=80;CHECK(rf_event_threshold_poll(&f.monitor,0,f.links,5,query,effect,&f)==RF_OK);
    f.health=0;CHECK(rf_event_threshold_poll(&f.monitor,0,f.links,5,query,effect,&f)==RF_OK && f.calls==4);
    f.monitor.fired=0;f.monitor.threshold=0;f.armor=-1;
    CHECK(rf_event_threshold_poll(&f.monitor,1,f.links,5,query,effect,&f)==RF_OK && f.seen_armor==1 && f.calls==8);
    puts("Health/armor threshold equality, one-shot, mixed outputs and reentrant latch passed");return 0;
}
