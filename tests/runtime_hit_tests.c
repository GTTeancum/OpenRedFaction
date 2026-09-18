#include "rf/event.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t entity_handle,hit_flags=0x200004,pulses,movers,last_source,last_actor;
static int query(void *context,uint32_t handle,uint32_t *flags)
{if(context!=&hit_flags)return RF_FORMAT;if(handle!=entity_handle)return RF_NOT_FOUND;*flags=hit_flags;return RF_OK;}
static int pulse(void *context,const rf_level_event *event)
{(void)context;(void)event;++pulses;return RF_OK;}
static int mover(void *context,uint32_t handle,uint32_t source,uint32_t actor,int32_t now)
{(void)context;(void)handle;(void)now;++movers;last_source=source;last_actor=actor;return RF_OK;}
int main(void)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;rf_runtime_trigger trigger={0};
    rf_level_link_target links[4]={{0}};uint32_t entity=0,controller=8,controller_handle,trigger_handle,pending,i;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    CHECK(rf_object_registry_insert(&registry,&entity,&entity_handle)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,&controller,&controller_handle)==RF_OK);
    trigger.object_kind=5;trigger.state.flags=16;
    CHECK(rf_object_registry_insert(&registry,&trigger,&trigger_handle)==RF_OK);
    for(i=0;i<2;i++){items[i].state.type=52;items[i].links=links;authored[i].record.link_count=4;}
    items[0].state.flags=1;items[2].state.type=63;
    for(i=0;i<4;i++)links[i].kind=1;
    links[0].value=entity_handle;links[1].value=items[2].handle;links[2].value=controller_handle;links[3].value=trigger_handle;
    triggers.teleport_player=pulse;triggers.activate_mover=mover;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1000,NULL,NULL,&report,&pending)==RF_OK && pending==2);
    triggers.query_hit_flags=query;triggers.hit_flags_context=&hit_flags;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1000,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(pulses==2 && movers==2 && hit_flags==0x200004 && trigger.state.flags==16);
    CHECK(items[2].state.source==UINT32_MAX && items[2].state.actor==UINT32_MAX && last_source==UINT32_MAX && last_actor==UINT32_MAX);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1100,NULL,NULL,&report,&pending)==RF_OK && pulses==4);
    hit_flags&=~0x200000u;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1200,NULL,NULL,&report,&pending)==RF_OK && pulses==4);
    hit_flags|=0x200000u;items[0].state.deadline=1500;items[0].state.mode=1;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1499,NULL,NULL,&report,&pending)==RF_OK && pulses==5);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1500,NULL,NULL,&report,&pending)==RF_OK && pulses==8);
    CHECK(items[0].state.deadline==-1 && !report.unsupported_actions);
    puts("Runtime When_Hit shared signal, disabled independence, event/mover routing, trigger exclusion and delayed prefix passed");return 0;
}
