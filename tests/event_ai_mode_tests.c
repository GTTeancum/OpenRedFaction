#include "rf/event.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static rf_entity_ai_transition_state ai;
static uint32_t entity_handle,calls,propagations;
static uint32_t form_calls,form_variant,form_enabled;static int32_t form_now;
static int set_form(void *context,uint32_t variant,uint32_t enabled,int32_t now)
{
    if(context!=&ai)return RF_FORMAT;
    ++form_calls;form_variant=variant;form_enabled=enabled;form_now=now;return RF_OK;
}
static int set_mode(void *context,uint32_t handle,int32_t action,int32_t now)
{
    if(context!=&ai)return RF_FORMAT;
    if(handle!=entity_handle)return RF_NOT_FOUND;
    ++calls;return rf_entity_ai_set_action(&ai,action,UINT32_MAX,UINT32_MAX,(float)now,0,0);
}
static int propagated(void *context,const rf_level_event *event)
{(void)context;(void)event;++propagations;return RF_OK;}
typedef struct npc_teleport_test_context {
    uint32_t actors[2],trace[12],count,attempts;int failure;
    const rf_level_event *destination;
} npc_teleport_test_context;
static int teleport_link(void *context,uint32_t handle,const rf_level_event *event)
{
    npc_teleport_test_context *c=context;
    if(event!=c->destination)return RF_FORMAT;
    ++c->attempts;
    if(handle!=c->actors[0] && handle!=c->actors[1])return RF_NOT_FOUND;
    if(c->failure)return c->failure;
    if(c->count>=12)return RF_RANGE;
    c->trace[c->count++]=handle;return RF_OK;
}
static int teleport_downstream(void *context,const rf_level_event *event)
{
    npc_teleport_test_context *c=context;(void)event;
    if(c->count>=12)return RF_RANGE;
    c->trace[c->count++]=UINT32_MAX;return RF_OK;
}
static int npc_teleport_dispatch_test(void)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[5]={{0}},off={0};npc_teleport_test_context c={0};
    uint32_t objects[3]={0},stale,pending,i;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    for(i=0;i<2;i++)CHECK(rf_object_registry_insert(&registry,objects+i,c.actors+i)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,objects+2,&stale)==RF_OK);
    CHECK(rf_object_registry_remove(&registry,stale)==RF_OK);
    items[0].state.type=4;items[0].links=links;authored[0].record.link_count=5;
    authored[0].record.has_orientation=1;authored[0].record.position[0]=49.284691f;c.destination=&authored[0].record;
    items[1].state.type=63;
    items[2].state.type=3;items[2].links=&off;authored[2].record.link_count=1;off.kind=1;off.value=items[0].handle;
    for(i=0;i<5;i++)links[i].kind=i==2?2:1;
    links[0].value=c.actors[0];links[1].value=stale;links[2].value=c.actors[1];
    links[3].value=c.actors[0];links[4].value=items[1].handle;
    triggers.teleport_npc=teleport_link;triggers.npc_teleport_context=&c;
    triggers.teleport_player=teleport_downstream;triggers.teleport_context=&c;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,100,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(c.count==4 && c.attempts==4 && c.trace[0]==c.actors[0] && c.trace[1]==c.actors[1] &&
        c.trace[2]==c.actors[0] && c.trace[3]==UINT32_MAX && report.other_targets>=1);
    /* Invert emits OFF: no NPC relocation or ON-only downstream callback. */
    CHECK(rf_runtime_event_fire(&triggers,items[2].handle,7,8,110,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(c.count==4 && c.attempts==4);
    items[0].state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,200,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(items[0].state.deadline==450 && c.count==4);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,449,NULL,NULL,&report,&pending)==RF_OK && c.count==4);
    triggers.teleport_npc=NULL;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,450,NULL,NULL,&report,&pending)==RF_OK && pending==1 && c.count==4);
    triggers.teleport_npc=teleport_link;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,450,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(c.count==8 && c.attempts==8 && c.trace[4]==c.actors[0] && c.trace[5]==c.actors[1] &&
        c.trace[6]==c.actors[0] && c.trace[7]==UINT32_MAX && items[0].state.deadline==-1);
    items[0].state.delay=0;c.failure=RF_IO;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,500,&gravity,NULL,NULL,&report)==RF_IO && c.count==8);
    return 0;
}
int main(void)
{
    CHECK(npc_teleport_dispatch_test()==0);
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
    items[0].state.type=47;authored[0].record.words[0]=1;
    triggers.set_player_form=set_form;triggers.player_form_context=&ai;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1700,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(form_calls==1 && form_variant==1 && form_enabled==1 && form_now==1700);
    CHECK(rf_runtime_event_fire(&triggers,items[1].handle,7,8,1800,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(form_calls==2 && form_variant==1 && form_enabled==0 && form_now==1800);
    puts("NPC teleport ordering/delay and AI mode/undercover ON/OFF dispatch passed");return 0;
}
