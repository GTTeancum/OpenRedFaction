#include "rf/event.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sound event line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t starts,stops,black_starts,black_stops,blast_starts,blast_stops,look_starts,look_stops;
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
static int explode(void *context,const rf_level_event *event,int32_t now,uint32_t on)
{
    if(context!=&blast_starts||event->uid!=125||now<0||event->values[0]!=6.75f||event->values[1]!=200.0f)return RF_FORMAT;
    if(on)++blast_starts;else ++blast_stops;return RF_OK;
}
static int look_at(void *context,const rf_level_event *event,const rf_level_link_target *links,uint32_t on)
{
    if(context!=&look_starts || event->uid!=126 || event->words[0]!=8322 ||
       event->link_count!=1 || !links || links[0].kind!=1 || links[0].value!=1234)return RF_FORMAT;
    if(on)++look_starts;else ++look_stops;return RF_OK;
}
int main(void)
{
    rf_runtime_event items[5]={{0}};rf_level_owned_event authored[5]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;rf_level_link_target link={0},look_link={0};uint32_t i,pending;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=5;
    for(i=0;i<5;i++){items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(!rf_object_registry_insert(&registry,items+i,&items[i].handle));}
    items[0].state.type=0;authored[0].record.uid=123;
    items[1].state.type=3;items[1].links=&link;authored[1].record.link_count=1;link.kind=1;link.value=items[0].handle;
    items[2].state.type=61;authored[2].record.uid=124;
    items[3].state.type=10;authored[3].record.uid=125;authored[3].record.values[0]=6.75f;authored[3].record.values[1]=200.0f;
    items[4].state.type=7;items[4].links=&look_link;authored[4].record.uid=126;authored[4].record.words[0]=8322;
    authored[4].record.link_count=1;look_link.kind=1;look_link.value=1234;
    triggers.play_sound=sound;triggers.sound_context=&starts;
    triggers.black_out_player=blackout;triggers.blackout_context=&black_starts;
    triggers.explode=explode;triggers.explode_context=&blast_starts;
    triggers.look_at=look_at;triggers.look_at_context=&look_starts;
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
    CHECK(!rf_runtime_event_fire(&triggers,items[3].handle,7,8,2100,&gravity,NULL,NULL,&report));CHECK(blast_starts==1);
    link.value=items[3].handle;
    CHECK(!rf_runtime_event_fire(&triggers,items[1].handle,7,8,2200,&gravity,NULL,NULL,&report));CHECK(blast_stops==1);
    items[3].state.delay=.25f;
    CHECK(!rf_runtime_event_fire(&triggers,items[3].handle,7,8,2300,&gravity,NULL,NULL,&report));CHECK(blast_starts==1);
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,2550,NULL,NULL,&report,&pending));CHECK(blast_starts==2);
    CHECK(!rf_runtime_event_fire(&triggers,items[4].handle,7,8,2600,&gravity,NULL,NULL,&report));CHECK(look_starts==1);
    link.value=items[4].handle;
    CHECK(!rf_runtime_event_fire(&triggers,items[1].handle,7,8,2700,&gravity,NULL,NULL,&report));CHECK(look_stops==1);
    items[4].state.delay=.25f;
    CHECK(!rf_runtime_event_fire(&triggers,items[4].handle,7,8,2800,&gravity,NULL,NULL,&report));CHECK(look_starts==1);
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,3050,NULL,NULL,&report,&pending));CHECK(look_starts==2);
    puts("PASS scripted sound, blackout, explode and look dispatch");return 0;
}
