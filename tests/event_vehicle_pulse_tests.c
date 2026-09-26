#include "rf/event.h"
#include "rf/event_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vehicle event line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t events_fired,movers_fired,last_source,last_actor,exit_lock;
static int load_level(void *context,const rf_level_event *event,uint32_t source,uint32_t actor)
{(void)context;(void)event;events_fired++;last_source=source;last_actor=actor;return RF_OK;}
static int mover(void *context,uint32_t handle,uint32_t source,uint32_t actor,int32_t now)
{(void)context;(void)handle;(void)now;movers_fired++;last_source=source;last_actor=actor;return RF_OK;}
static int lock(void *context,uint32_t handle,uint32_t enabled)
{(void)context;(void)handle;if(enabled)exit_lock|=0x80u;else exit_lock&=~0x80u;return RF_OK;}
int main(void)
{
    rf_runtime_event item[5]={{0}};rf_level_owned_event authored[5]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;uint32_t pending,i;
    rf_level_link_target enter_link[1],exit_link[2],lock_link[1];
    struct {uint32_t kind,handle;} mover_object={8,0};
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;
    events.items=item;events.count=5;triggers.load_level=load_level;triggers.activate_mover=mover;
    triggers.set_vehicle_exit_lock=lock;
    for(i=0;i<5;i++){
        item[i].object_kind=6;item[i].authored=authored+i;item[i].state.deadline=-1;
        authored[i].record.uid=100+i;
        CHECK(!rf_object_registry_insert(&registry,item+i,&item[i].handle));
    }
    CHECK(!rf_object_registry_insert(&registry,&mover_object,&mover_object.handle));
    item[0].state.type=item[1].state.type=77;item[2].state.type=78;item[3].state.type=22;item[4].state.type=80;
    item[0].state.flags=16; /* Disabled, empty first monitor still consumes. */
    enter_link[0]=(rf_level_link_target){item[3].handle,1,0};
    item[1].links=enter_link;authored[1].record.link_count=1;
    exit_link[0]=(rf_level_link_target){item[3].handle,1,0};
    exit_link[1]=(rf_level_link_target){mover_object.handle,1,0};
    item[2].links=exit_link;authored[2].record.link_count=2;
    lock_link[0]=(rf_level_link_target){mover_object.handle,1,0};
    item[4].links=lock_link;authored[4].record.link_count=1;
    CHECK(rf_event_checkpoint_type_supported(77)&&rf_event_checkpoint_type_supported(78)&&rf_event_checkpoint_type_supported(80));
    CHECK(!rf_runtime_event_fire(&triggers,item[4].handle,UINT32_MAX,UINT32_MAX,0,&gravity,NULL,NULL,&report));
    CHECK(exit_lock==0x80u&&!movers_fired);
    triggers.vehicle_pulses=0xc00u|0x100u;
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,10,NULL,NULL,&report,&pending));
    CHECK(triggers.vehicle_pulses==(0xc00u|0x100u)&&!events_fired&&!movers_fired);
    triggers.vehicle_player_present=1;
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,11,NULL,NULL,&report,&pending));
    CHECK(triggers.vehicle_pulses==0x100u&&events_fired==1&&movers_fired==1&&
          last_source==UINT32_MAX&&last_actor==UINT32_MAX);
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,12,NULL,NULL,&report,&pending));
    CHECK(events_fired==1&&movers_fired==1);
    item[0].links=enter_link;authored[0].record.link_count=1;
    triggers.vehicle_pulses|=0x400u;
    CHECK(!rf_runtime_events_tick(&events,&triggers,&gravity,13,NULL,NULL,&report,&pending));
    CHECK(events_fired==2&&movers_fired==1&&!(triggers.vehicle_pulses&0x400u));
    puts("PASS vehicle enter/exit pulses, absent player, disabled empty first monitor, ordered event/mover links and exit lock");
    return 0;
}
