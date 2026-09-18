#include "rf/event.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t teleports,movers,last_source,last_actor;
static int teleport(void *unused,const rf_level_event *event)
{(void)unused;(void)event;++teleports;return RF_OK;}
static int mover(void *unused,uint32_t handle,uint32_t source,uint32_t actor,int32_t now)
{(void)unused;(void)handle;(void)now;++movers;last_source=source;last_actor=actor;return RF_OK;}
int main(void)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_runtime_trigger trigger={0};rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[3]={{0}},off_link={0};uint32_t controller=8,controller_handle,trigger_handle,pending,i;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    triggers.teleport_player=teleport;triggers.activate_mover=mover;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    items[0].state.type=20;items[0].state.delay=.1f;items[0].links=links;authored[0].record.link_count=3;
    items[1].state.type=63;items[2].state.type=3;items[2].links=&off_link;authored[2].record.link_count=1;
    off_link.kind=1;off_link.value=items[0].handle;
    CHECK(rf_event_cycle_init(&items[0].cycle,.5f,2,0,0)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,&controller,&controller_handle)==RF_OK);
    trigger.object_kind=5;trigger.state.flags=16;
    CHECK(rf_object_registry_insert(&registry,&trigger,&trigger_handle)==RF_OK);
    links[0].kind=links[1].kind=links[2].kind=1;
    links[0].value=items[1].handle;links[1].value=controller_handle;links[2].value=trigger_handle;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,0,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(!teleports && !movers && !items[0].cycle.enabled);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,99,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(!teleports && !movers);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,100,NULL,NULL,&report,&pending)==RF_OK && !pending);
    /* Existing delayed ON propagation plus first due cyclic pulse. */
    CHECK(teleports==2 && movers==2 && items[0].cycle.count==1 && items[0].cycle.deadline==600);
    CHECK(items[1].state.source==UINT32_MAX && items[1].state.actor==UINT32_MAX);
    CHECK(last_source==7 && last_actor==8 && !(trigger.state.flags&16));
    trigger.state.flags|=16;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,600,NULL,NULL,&report,&pending)==RF_OK);
    CHECK(teleports==3 && movers==3 && items[0].cycle.count==2 && (trigger.state.flags&16));
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,5000,NULL,NULL,&report,&pending)==RF_OK);
    CHECK(teleports==3 && movers==3);
    /* Invert routes OFF through the real runtime, preserving cycle exhaustion. */
    items[0].state.delay=0;
    CHECK(rf_runtime_event_fire(&triggers,items[2].handle,20,21,5000,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(!items[0].cycle.enabled && items[0].cycle.count==2 && items[0].cycle.deadline==1100);
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,5000,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(items[0].cycle.enabled && items[0].cycle.count==2);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,5000,NULL,NULL,&report,&pending)==RF_OK);
    CHECK(teleports==4 && movers==4); /* ON propagation only; finite timer stays exhausted. */
    puts("Runtime cyclic delayed enable, event/mover pulses, trigger exclusion and finite resume passed");return 0;
}
