#include "rf/event.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static rf_runtime_event *monitor;static uint32_t entity_handle,teleports,movers,last_source,last_actor,queries;
static float health=51,armor=80;
static int query(void *context,uint32_t handle,uint32_t which,float *value)
{
    if(context!=&queries)return RF_FORMAT;
    ++queries;if(handle!=entity_handle)return RF_NOT_FOUND;
    *value=which?armor:health;return RF_OK;
}
static int teleport(void *unused,const rf_level_event *event)
{(void)unused;(void)event;if(!monitor->threshold.fired)return RF_FORMAT;++teleports;return RF_OK;}
static int mover(void *unused,uint32_t handle,uint32_t source,uint32_t actor,int32_t now)
{(void)unused;(void)handle;(void)now;if(!monitor->threshold.fired)return RF_FORMAT;++movers;last_source=source;last_actor=actor;return RF_OK;}
int main(void)
{
    rf_runtime_event items[2]={{0}};rf_level_owned_event authored[2]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_runtime_trigger trigger={0};rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[4]={{0}};uint32_t controller=8,entity=0,controller_handle,trigger_handle,pending,i;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=2;
    triggers.teleport_player=teleport;triggers.activate_mover=mover;
    for(i=0;i<2;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    monitor=items;items[0].state.type=87;items[0].state.flags=1;items[0].state.deadline=1500;
    items[0].threshold.threshold=50;items[0].links=links;authored[0].record.link_count=4;
    items[1].state.type=63;
    CHECK(rf_object_registry_insert(&registry,&controller,&controller_handle)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,&entity,&entity_handle)==RF_OK);
    trigger.object_kind=5;trigger.state.flags=0x1010;
    CHECK(rf_object_registry_insert(&registry,&trigger,&trigger_handle)==RF_OK);
    for(i=0;i<4;i++)links[i].kind=1;
    links[0].value=entity_handle;links[1].value=items[1].handle;
    links[2].value=controller_handle;links[3].value=trigger_handle;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1000,NULL,NULL,&report,&pending)==RF_OK && pending==1);
    triggers.query_vitals=query;triggers.query_vitals_context=&queries;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1000,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(!teleports && !movers && !monitor->threshold.fired);
    health=50;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1000,NULL,NULL,&report,&pending)==RF_OK);
    CHECK(teleports==1 && movers==1 && monitor->threshold.fired && trigger.state.flags==0x1000);
    CHECK(items[1].state.source==UINT32_MAX && items[1].state.actor==UINT32_MAX);
    CHECK(last_source==UINT32_MAX && last_actor==UINT32_MAX && monitor->state.deadline==1500);
    health=80;CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1100,NULL,NULL,&report,&pending)==RF_OK);
    health=-1;CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1200,NULL,NULL,&report,&pending)==RF_OK);
    CHECK(teleports==1 && movers==1);
    monitor->threshold.fired=0;monitor->state.type=88;monitor->threshold.threshold=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1200,NULL,NULL,&report,&pending)==RF_OK && !monitor->threshold.fired);
    armor=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1200,NULL,NULL,&report,&pending)==RF_OK && monitor->threshold.fired);
    CHECK(teleports==2 && movers==2);
    puts("Runtime health/armor observers: delayed/disabled independence, event/mover effects, trigger enable and one-shot passed");return 0;
}
