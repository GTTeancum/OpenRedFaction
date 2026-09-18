#include "rf/event_hit.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint32_t flags,event_pulses,mover_pulses,queries;} fixture;
static int query(void *context,uint32_t handle,uint32_t *flags)
{fixture *f=context;++f->queries;if(handle!=10)return RF_NOT_FOUND;*flags=f->flags;return RF_OK;}
static int effect(void *context,uint32_t handle)
{fixture *f=context;if(handle==20){++f->event_pulses;return RF_OK;}if(handle==30){++f->mover_pulses;return RF_OK;}return RF_NOT_FOUND;}
int main(void)
{
    rf_level_link_target links[]={{10,1,0},{20,1,0},{30,2,0},{40,0,0}};fixture f={4,0,0,0};
    CHECK(rf_event_hit_poll(-1,links,4,query,effect,&f)==RF_OK && !f.event_pulses);
    f.flags|=0x200000;
    CHECK(rf_event_hit_poll(1000,links,4,query,effect,&f)==RF_OK && !f.event_pulses);
    CHECK(rf_event_hit_poll(-1,links,4,query,effect,&f)==RF_OK && f.event_pulses==1 && f.mover_pulses==1);
    CHECK(f.flags==0x200004); /* Invulnerable hit signal survives observer. */
    CHECK(rf_event_hit_poll(-1,links,4,query,effect,&f)==RF_OK && f.event_pulses==2 && f.mover_pulses==2);
    f.flags&=~0x200000u;
    CHECK(rf_event_hit_poll(-1,links,4,query,effect,&f)==RF_OK && f.event_pulses==2 && f.flags==4);
    puts("When_Hit mixed outputs, common-delay block and shared non-consuming signal passed");return 0;
}
