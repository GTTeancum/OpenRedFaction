#include "rf/event.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mission line %u\n",(unsigned)__LINE__);return 1;}}while(0)
static uint32_t death_present[2],death_alive[2],death_unknown;
static int death_query(void *context,uint32_t uid,uint32_t *present,uint32_t *alive)
{
    (void)context;if(uid<10 || uid>11 || death_unknown)return RF_NOT_FOUND;
    *present=death_present[uid-10];*alive=death_alive[uid-10];return RF_OK;
}
static int death_watch_check(void)
{
    rf_object_registry registry;rf_runtime_triggers triggers={0};rf_runtime_events events={0};
    rf_runtime_event watcher={0};rf_runtime_trigger target={0};rf_level_owned_event authored={0};
    rf_level_link_target links[3]={{0}};uint32_t uids[3]={10,11,12},pending;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_object_registry_init(&registry);triggers.registry=&registry;triggers.death_query=death_query;
    target.object_kind=5;target.state.flags=16;
    CHECK(rf_object_registry_insert(&registry,&target,&links[2].value)==RF_OK);links[2].kind=1;
    authored.links=uids;authored.record.link_count=3;watcher.authored=&authored;watcher.links=links;
    watcher.state.type=16;watcher.state.deadline=-1;watcher.state.delay=3.5f;
    events.items=&watcher;events.count=1;events.registry=&registry;
    death_present[0]=death_present[1]=death_alive[0]=death_alive[1]=1;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,0,0,0,&report,&pending)==RF_OK && !watcher.death_fired);
    death_alive[0]=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1,0,0,&report,&pending)==RF_OK && !watcher.death_fired);
    death_alive[1]=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,2,0,0,&report,&pending)==RF_OK && watcher.death_fired && watcher.death_time==2 && !(target.state.flags&16));
    target.state.flags|=16;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,3,0,0,&report,&pending)==RF_OK && (target.state.flags&16));
    watcher.death_fired=0;authored.record.flags[0]=1;death_alive[1]=1;death_present[0]=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,4,0,0,&report,&pending)==RF_OK && watcher.death_fired && !(target.state.flags&16));
    watcher.death_fired=0;death_unknown=1;target.state.flags|=16;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,5,0,0,&report,&pending)==RF_OK && !watcher.death_fired && report.unsupported_actions==1);
    death_unknown=0;watcher.state.deadline=100;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,99,0,0,&report,&pending)==RF_OK && !watcher.death_fired);
    return 0;
}
static uint32_t mover_calls,mover_source,mover_actor;static int32_t mover_now;static int mover_error;
static int mover_activate(void *context,uint32_t handle,uint32_t source,uint32_t actor,int32_t now)
{
    (void)context;(void)handle;++mover_calls;mover_source=source;mover_actor=actor;mover_now=now;return mover_error;
}
static int authored_mover_check(const char *path)
{
    rf_vpp archive={0};rf_level level;rf_object_registry registry;rf_runtime_events events={0};
    rf_runtime_triggers triggers={0};rf_campaign_goals goals={0};rf_physics_gravity gravity={0};
    rf_startup_events_report report;rf_runtime_event *check=NULL,*setter=NULL;uint32_t i,controller=8,handle,pending;
    rf_object_registry_init(&registry);triggers.registry=&registry;triggers.goals=&goals;triggers.activate_mover=mover_activate;
    CHECK(rf_level_campaign_open(&level,&archive,path,"L7S2.rfl")==RF_OK);
    CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&events)==RF_OK);
    CHECK(rf_runtime_goals_initialize(&events,&goals)==RF_OK);
    for(i=0;i<events.count;i++) {
        if(events.items[i].authored->record.uid==5015)check=events.items+i;
        if(events.items[i].authored->record.uid==5011)setter=events.items+i;
    }
    CHECK(check && setter && check->authored->record.words[0]==2 && check->authored->links[0]==4994);
    CHECK(rf_object_registry_insert(&registry,&controller,&handle)==RF_OK);
    check->links[0].kind=1;check->links[0].value=handle;
    CHECK(rf_runtime_event_fire(&triggers,setter->handle,7,9,0,&gravity,0,0,&report)==RF_OK);
    CHECK(rf_runtime_event_fire(&triggers,check->handle,444,555,10,&gravity,0,0,&report)==RF_OK && !mover_calls);
    CHECK(rf_runtime_event_fire(&triggers,setter->handle,7,9,20,&gravity,0,0,&report)==RF_OK);
    CHECK(rf_runtime_event_fire(&triggers,check->handle,444,555,30,&gravity,0,0,&report)==RF_OK && mover_calls==1 && !report.other_targets);
    CHECK(mover_source==444 && mover_actor==555 && mover_now==30);
    check->state.deadline=40;check->state.mode=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,40,0,0,&report,&pending)==RF_OK && mover_calls==1);
    triggers.activate_mover=NULL;
    CHECK(rf_runtime_event_fire(&triggers,check->handle,444,555,50,&gravity,0,0,&report)==RF_OK && report.other_targets==1);
    triggers.activate_mover=mover_activate;mover_error=RF_IO;
    CHECK(rf_runtime_event_fire(&triggers,check->handle,444,555,60,&gravity,0,0,&report)==RF_IO);
    rf_runtime_events_close(&events);rf_vpp_close(&archive);return 0;
}
static uint32_t move_calls,move_on;static rf_level_event move_event;
static int move_command(void *context,uint32_t handle,const rf_level_event *event,uint32_t on)
{(void)context;(void)handle;++move_calls;move_on=on;move_event=*event;return RF_OK;}
static int goto_dispatch_check(const char *path)
{
    rf_vpp archive={0};rf_level level;rf_object_registry registry;rf_runtime_events events={0};rf_runtime_triggers triggers={0};
    rf_physics_gravity gravity={0};rf_startup_events_report report;rf_runtime_event *event=NULL;uint32_t i,pending,object=0,handle;
    rf_object_registry_init(&registry);triggers.registry=&registry;triggers.move_npc=move_command;
    CHECK(rf_level_campaign_open(&level,&archive,path,"L7S2.rfl")==RF_OK);
    CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&events)==RF_OK);
    for(i=0;i<events.count;i++)if(events.items[i].authored->record.uid==4994)event=events.items+i;
    CHECK(event && event->state.type==5 && event->state.delay==.5f && event->authored->links[0]==4952);
    CHECK(rf_object_registry_insert(&registry,&object,&handle)==RF_OK);event->links[0].kind=1;event->links[0].value=handle;
    CHECK(rf_runtime_event_fire(&triggers,event->handle,7,9,500,&gravity,0,0,&report)==RF_OK && !move_calls);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,999,0,0,&report,&pending)==RF_OK && !move_calls);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1000,0,0,&report,&pending)==RF_OK && move_calls==1 && move_on && !pending);
    CHECK(!memcmp(&move_event,&event->authored->record,sizeof(move_event)));
    event->state.deadline=1100;event->state.mode=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1100,0,0,&report,&pending)==RF_OK && move_calls==2 && !move_on);
    rf_runtime_events_close(&events);rf_vpp_close(&archive);return 0;
}
static uint32_t visible_calls,visible_value,visible_handle;
static int visibility_command(void *context,uint32_t handle,uint32_t visible)
{(void)context;++visible_calls;visible_value=visible;visible_handle=handle;return RF_OK;}
static int unhide_dispatch_check(const char *path)
{
    rf_vpp archive={0};rf_level level;rf_object_registry registry;rf_runtime_events events={0};rf_runtime_triggers triggers={0};
    rf_physics_gravity gravity={0};rf_startup_events_report report;rf_runtime_event *event=NULL;uint32_t i,pending,object=0,handle;
    rf_object_registry_init(&registry);triggers.registry=&registry;triggers.set_visible=visibility_command;
    CHECK(rf_level_campaign_open(&level,&archive,path,"L7S2.rfl")==RF_OK);
    CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&events)==RF_OK);
    for(i=0;i<events.count;i++)if(events.items[i].authored->record.uid==4962)event=events.items+i;
    CHECK(event && event->state.type==50 && event->authored->links[0]==4952);
    CHECK(rf_object_registry_insert(&registry,&object,&handle)==RF_OK);event->links[0].kind=1;event->links[0].value=handle;
    CHECK(rf_runtime_event_fire(&triggers,event->handle,7,9,500,&gravity,0,0,&report)==RF_OK && !visible_calls);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,500,0,0,&report,&pending)==RF_OK && visible_calls==1 && visible_value && visible_handle==handle);
    CHECK(rf_unhide_request(&event->unhide,0)==RF_OK);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,999,0,0,&report,&pending)==RF_OK && visible_calls==1);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1000,0,0,&report,&pending)==RF_OK && visible_calls==2 && !visible_value);
    triggers.set_visible=NULL;CHECK(rf_unhide_request(&event->unhide,1)==RF_OK);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1500,0,0,&report,&pending)==RF_OK && pending==1 && visible_calls==2);
    triggers.set_visible=visibility_command;event->links[0].kind=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1500,0,0,&report,&pending)==RF_OK && !event->unhide.on && visible_calls==2);
    rf_runtime_events_close(&events);rf_vpp_close(&archive);return 0;
}
int main(int argc,char **argv)
{
    CHECK(argc==2);CHECK(unhide_dispatch_check(argv[1])==0);
    rf_campaign_goals goals={0};rf_object_registry registry;rf_runtime_triggers triggers={0};
    rf_runtime_events events={0};rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_vpp archive={0};rf_level level;uint32_t i,passed,pending;int found=0;
    rf_runtime_event check={0};rf_level_owned_event authored={0};rf_level_link_target link={0};
    rf_runtime_trigger target={0};
    CHECK(death_watch_check()==0);
    CHECK(argc==2);CHECK(goto_dispatch_check(argv[1])==0);CHECK(authored_mover_check(argv[1])==0);rf_object_registry_init(&registry);triggers.registry=&registry;triggers.goals=&goals;
    CHECK(rf_level_campaign_open(&level,&archive,argv[1],"L8S1.rfl")==RF_OK);
    CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&events)==RF_OK);
    CHECK(rf_runtime_goals_initialize(&events,&goals)==RF_OK);
    CHECK(rf_campaign_goal_check(&goals,"VAT",1,&passed)==RF_OK && !passed);
    for(i=0;i<events.count;i++)if(events.items[i].authored->record.uid==8898) {
        CHECK(events.items[i].state.type==37 && !events.items[i].authored->record.link_count);
        CHECK(rf_runtime_event_fire(&triggers,events.items[i].handle,7,9,0,&gravity,0,0,&report)==RF_OK && !report.unsupported_actions);
        found=1;
    }
    CHECK(found && rf_campaign_goal_check(&goals,"VAT",1,&passed)==RF_OK && passed);
    rf_runtime_events_close(&events);rf_vpp_close(&archive);
    CHECK(rf_campaign_goals_next_section(&goals)==RF_OK);
    rf_object_registry_init(&registry);
    CHECK(rf_level_campaign_open(&level,&archive,argv[1],"L8S2.rfl")==RF_OK);
    CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&events)==RF_OK);
    CHECK(rf_runtime_goals_initialize(&events,&goals)==RF_OK);
    CHECK(rf_campaign_goal_check(&goals,"VAT",1,&passed)==RF_OK && passed);
    /* Route the real authored VAT check to a contained trigger to observe its
     * branch without requiring unrelated audio/NPC/mover backends. */
    found=0;
    for(i=0;i<events.count;i++)if(events.items[i].authored->record.uid==9381) {
        authored.record=events.items[i].authored->record;found=1;
    }
    CHECK(found && !strcmp(authored.record.texts[0],"VAT") && authored.record.words[0]==1);
    rf_runtime_events_close(&events);rf_vpp_close(&archive);rf_object_registry_init(&registry);
    target.object_kind=5;target.state.flags=16;
    CHECK(rf_object_registry_insert(&registry,&target,&link.value)==RF_OK);link.kind=1;
    check.object_kind=6;check.state.type=36;check.state.deadline=-1;
    check.authored=&authored;check.links=&link;authored.record.link_count=1;
    CHECK(rf_object_registry_insert(&registry,&check,&check.handle)==RF_OK);
    CHECK(rf_runtime_event_fire(&triggers,check.handle,7,9,100,&gravity,0,0,&report)==RF_OK && !(target.state.flags&16));
    CHECK(rf_campaign_goal_adjust(&goals,"VAT",0)==RF_OK);target.state.flags=16;
    CHECK(rf_runtime_event_fire(&triggers,check.handle,7,9,100,&gravity,0,0,&report)==RF_OK && (target.state.flags&16));
    check.state.delay=.25f;events.items=&check;events.count=1;events.registry=&registry;
    CHECK(rf_runtime_event_fire(&triggers,check.handle,7,9,100,&gravity,0,0,&report)==RF_OK);
    CHECK(rf_campaign_goal_adjust(&goals,"VAT",1)==RF_OK);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,349,0,0,&report,&pending)==RF_OK && (target.state.flags&16) && !pending);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,350,0,0,&report,&pending)==RF_OK && !(target.state.flags&16) && !pending);
    check.state.deadline=400;check.state.mode=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,400,0,0,&report,&pending)==RF_OK && (target.state.flags&16));
    check.state.type=35;strcpy(authored.record.name,"initial-count");authored.record.words[0]=99;
    authored.record.words[1]=3;authored.record.flags[0]=1;
    CHECK(rf_runtime_goals_initialize(&events,&goals)==RF_OK);
    CHECK(rf_campaign_goal_check(&goals,"initial-count",3,&passed)==RF_OK && passed);
    CHECK(rf_campaign_goal_check(&goals,"initial-count",4,&passed)==RF_OK && !passed);
    CHECK(rf_campaign_goal_adjust(&goals,"initial-count",1)==RF_OK);
    CHECK(rf_campaign_goals_next_section(&goals)==RF_OK);
    CHECK(rf_runtime_goals_initialize(&events,&goals)==RF_OK);
    CHECK(rf_campaign_goal_check(&goals,"initial-count",4,&passed)==RF_OK && passed);
    puts("PASS authored L8S1 setter, owned L8S2 carry, conditional propagation and delayed live counter check");return 0;
}
