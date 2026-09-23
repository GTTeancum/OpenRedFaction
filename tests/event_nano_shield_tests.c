#include "rf/event.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t calls,entity_handle,enabled_state,propagations;static int failure;
static uint32_t endgame_flags,endgame_calls;
static int set_shield(void *context,uint32_t handle,uint32_t enabled)
{
    if(context!=&calls)return RF_FORMAT;
    if(handle!=entity_handle)return RF_NOT_FOUND;
    ++calls;if(failure)return failure;enabled_state=enabled;return RF_OK;
}
static int propagated(void *context,const rf_level_event *event)
{(void)context;(void)event;++propagations;return RF_OK;}
static int endgame_mark(void *context,uint32_t handle,uint32_t clear)
{
    uint32_t *flags=context;
    if(flags!=&endgame_flags)return RF_FORMAT;
    if(handle!=entity_handle)return RF_NOT_FOUND;
    if(clear)*flags&=~0x00400000u;else *flags|=0x00400000u;
    ++endgame_calls;return RF_OK;
}
int main(void)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[4]={{0}},off={0};uint32_t entity=0,pending,i;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    CHECK(rf_object_registry_insert(&registry,&entity,&entity_handle)==RF_OK);
    items[0].state.type=76;items[0].links=links;authored[0].record.link_count=4;
    items[1].state.type=3;items[1].links=&off;authored[1].record.link_count=1;
    items[2].state.type=63;off.kind=1;off.value=items[0].handle;
    for(i=0;i<4;i++)links[i].kind=1;
    links[0].value=links[2].value=entity_handle;links[1].value=UINT32_MAX;links[3].value=items[2].handle;
    triggers.set_nano_shield=set_shield;triggers.nano_shield_context=&calls;triggers.teleport_player=propagated;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,0,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(calls==2 && enabled_state==1 && propagations==1);
    CHECK(rf_runtime_event_fire(&triggers,items[1].handle,7,8,0,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(calls==4 && !enabled_state && propagations==1);
    items[0].state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,100,&gravity,NULL,NULL,&report)==RF_OK && calls==4);
    triggers.set_nano_shield=NULL;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,350,NULL,NULL,&report,&pending)==RF_OK && pending==1);
    triggers.set_nano_shield=set_shield;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,350,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(calls==6 && enabled_state && propagations==2 && items[0].state.deadline==-1);
    CHECK(rf_runtime_event_fire(&triggers,items[1].handle,7,8,400,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,649,NULL,NULL,&report,&pending)==RF_OK && calls==6);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,650,NULL,NULL,&report,&pending)==RF_OK);
    CHECK(calls==8 && !enabled_state);
    items[0].state.delay=0;items[0].state.flags=1;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,700,&gravity,NULL,NULL,&report)==RF_OK && calls==8);
    items[0].state.flags=0;failure=RF_IO;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,700,&gravity,NULL,NULL,&report)==RF_IO && calls==9);
    failure=0;items[0].state.type=67;triggers.clear_endgame_if_killed=endgame_mark;
    triggers.clear_endgame_context=&endgame_flags;endgame_flags=0x004000a5u;endgame_calls=0;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,800,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(endgame_calls==2 && endgame_flags==0xa5u);
    CHECK(rf_runtime_event_fire(&triggers,items[1].handle,7,8,800,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(endgame_calls==4 && endgame_flags==0x004000a5u);
    puts("Event76 and Event67 immediate/delayed ON/OFF, duplicate targets, missing backend, disabled and propagation passed");return 0;
}
