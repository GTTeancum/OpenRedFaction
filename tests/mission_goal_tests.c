#include "rf/event.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mission line %u\n",(unsigned)__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_campaign_goals goals={0};rf_object_registry registry;rf_runtime_triggers triggers={0};
    rf_runtime_events events={0};rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_vpp archive={0};rf_level level;uint32_t i,passed,pending;int found=0;
    rf_runtime_event check={0};rf_level_owned_event authored={0};rf_level_link_target link={0};
    rf_runtime_trigger target={0};
    CHECK(argc==2);rf_object_registry_init(&registry);triggers.registry=&registry;triggers.goals=&goals;
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
