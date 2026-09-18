#include "rf/event.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static rf_entity_ai_transition_state ai;
static uint32_t entity_handle,calls,propagations;
static int set_mode(void *context,uint32_t handle,int32_t action,int32_t now)
{
    if(context!=&ai)return RF_FORMAT;
    if(handle!=entity_handle)return RF_NOT_FOUND;
    ++calls;return rf_entity_ai_set_action(&ai,action,UINT32_MAX,UINT32_MAX,(float)now,0,0);
}
static int propagated(void *context,const rf_level_event *event)
{(void)context;(void)event;++propagations;return RF_OK;}
int main(void)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[4]={{0}},off={0};uint32_t entity=0,pending,i,before;
    static const int32_t expected[]={1,2,4,5,11,-1};
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    CHECK(rf_object_registry_insert(&registry,&entity,&entity_handle)==RF_OK);
    items[0].state.type=34;items[0].links=links;authored[0].record.link_count=4;
    items[1].state.type=3;items[1].links=&off;authored[1].record.link_count=1;
    items[2].state.type=63;off.kind=1;off.value=items[0].handle;
    for(i=0;i<4;i++)links[i].kind=1;
    links[0].value=links[2].value=entity_handle;links[1].value=UINT32_MAX;links[3].value=items[2].handle;
    triggers.set_ai_mode=set_mode;triggers.ai_mode_context=&ai;triggers.teleport_player=propagated;
    for(i=0;i<6;i++) {
        authored[0].record.words[0]=i;
        CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1234,&gravity,NULL,NULL,&report)==RF_OK);
        CHECK(ai.action_280==expected[i] && ai.clock_288==1234 && ai.argument_28c==UINT32_MAX && ai.argument_290==UINT32_MAX);
    }
    CHECK(calls==12 && propagations==6);
    CHECK(rf_runtime_event_fire(&triggers,items[1].handle,7,8,1234,&gravity,NULL,NULL,&report)==RF_OK && calls==12);
    authored[0].record.words[0]=1;items[0].state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1300,&gravity,NULL,NULL,&report)==RF_OK && calls==12);
    triggers.set_ai_mode=NULL;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1550,NULL,NULL,&report,&pending)==RF_OK && pending==1);
    triggers.set_ai_mode=set_mode;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1550,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(calls==14 && ai.action_280==2 && ai.clock_288==1550 && propagations==7);
    items[0].state.delay=0;items[0].state.flags=1;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1600,&gravity,NULL,NULL,&report)==RF_OK && calls==14);
    items[0].state.flags=0;authored[0].record.words[0]=6;before=calls;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1600,&gravity,NULL,NULL,&report)==RF_RANGE && calls==before);
    puts("AI mode event translation, real action setter, delayed dispatch, OFF no-op and propagation passed");return 0;
}
