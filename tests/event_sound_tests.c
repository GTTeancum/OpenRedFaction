#include "rf/event.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sound event line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t starts,stops,black_starts,black_stops;
static int sound(void *context,const rf_level_event *event,int32_t now,uint32_t on)
{
    if(context!=&starts||event->uid!=123||now<0)return RF_FORMAT;
    if(on)++starts;else ++stops;return RF_OK;
}
static int blackout(void *context,const rf_level_event *event,int32_t now,uint32_t on)
{
    if(context!=&black_starts||event->uid!=124||now<0)return RF_FORMAT;
    if(on)++black_starts;else ++black_stops;return RF_OK;
}
int main(void)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;rf_level_link_target link={0};uint32_t i,pending;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++){items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(!rf_object_registry_insert(&registry,items+i,&items[i].handle));}
    items[0].state.type=0;authored[0].record.uid=123;
    items[1].state.type=3;items[1].links=&link;authored[1].record.link_count=1;link.kind=1;link.value=items[0].handle;
    items[2].state.type=61;authored[2].record.uid=124;
    triggers.play_sound=sound;triggers.sound_context=&starts;
    triggers.black_out_player=blackout;triggers.blackout_context=&black_starts;
    CHECK(!rf_runtime_event_fire(&triggers,items[0].handle,7,8,1000,&gravity,NULL,NULL,&report));
    CHECK(starts==1&&!stops);
    CHECK(!rf_runtime_event_fire(&triggers,items[1].handle,7,8,1000,&gravity,NULL,NULL,&report));CHECK(stops==1);
    items[0].state.delay=.25f;
    CHECK(!rf_runtime_event_fire(&triggers,items[0].handle,7,8,1300,&gravity,NULL,NULL,&report));CHECK(starts==1);
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,1549,NULL,NULL,&report,&pending));CHECK(starts==1);
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,1550,NULL,NULL,&report,&pending));CHECK(starts==2&&!pending);
    triggers.play_sound=NULL;
    CHECK(!rf_runtime_event_fire(&triggers,items[0].handle,7,8,1700,&gravity,NULL,NULL,&report));
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,1950,NULL,NULL,&report,&pending));CHECK(pending==1&&starts==2);
    CHECK(!rf_runtime_event_fire(&triggers,items[2].handle,7,8,2000,&gravity,NULL,NULL,&report));CHECK(black_starts==1);
    link.value=items[2].handle;
    CHECK(!rf_runtime_event_fire(&triggers,items[1].handle,7,8,2000,&gravity,NULL,NULL,&report));CHECK(black_stops==1);
    puts("PASS scripted sound and blackout dispatch");return 0;
}
