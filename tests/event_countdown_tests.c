#include "rf/event.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)

static uint32_t activated;
static int load_level(void *context,const rf_level_event *event,uint32_t source,uint32_t actor)
{(void)context;(void)event;(void)source;(void)actor;++activated;return RF_OK;}

int main(void)
{
    rf_runtime_event item[6]={{0}};rf_level_owned_event authored[6]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_campaign_countdown timer={0};rf_level_link_target link[2]={{0}};
    uint32_t pending,i;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;
    events.items=item;events.count=6;triggers.countdown=&timer;triggers.load_level=load_level;
    for(i=0;i<6;i++) {
        item[i].object_kind=6;item[i].authored=authored+i;item[i].state.deadline=-1;
        authored[i].record.uid=100+i;
        CHECK(rf_object_registry_insert(&registry,item+i,&item[i].handle)==RF_OK);
    }
    item[0].state.type=73;authored[0].record.words[0]=600;
    item[1].state.type=74;item[2].state.type=75;item[3].state.type=84;
    item[4].state.type=item[5].state.type=22;
    item[2].links=link;authored[2].record.link_count=1;
    item[3].links=link+1;authored[3].record.link_count=1;authored[3].record.words[0]=60;
    link[0]=(rf_level_link_target){item[4].handle,1,0};
    link[1]=(rf_level_link_target){item[5].handle,1,0};

    CHECK(rf_runtime_event_fire(&triggers,item[0].handle,0,0,0,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(timer.remaining==600 && !timer.expiry_pending);
    CHECK(rf_campaign_countdown_step(&timer,1)==RF_OK && timer.remaining==599);
    strcpy(authored[0].record.name,"station_blowup");timer.difficulty=1;
    CHECK(rf_runtime_event_fire(&triggers,item[0].handle,0,0,100,&gravity,NULL,NULL,&report)==RF_OK && timer.remaining==55);
    triggers.countdown_level="L17S1.rfl";
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,101,NULL,NULL,&report,&pending)==RF_OK && !activated && !item[3].countdown_armed);
    timer.remaining=61;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,102,NULL,NULL,&report,&pending)==RF_OK && item[3].countdown_armed);
    timer.remaining=60;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,103,NULL,NULL,&report,&pending)==RF_OK && !activated);
    timer.remaining=59;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,104,NULL,NULL,&report,&pending)==RF_OK && activated==1 && item[3].countdown_fired);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,105,NULL,NULL,&report,&pending)==RF_OK && activated==1);

    timer.remaining=1;
    CHECK(rf_campaign_countdown_step(&timer,1)==RF_OK && timer.remaining==0 && timer.expiry_pending);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,106,NULL,NULL,&report,&pending)==RF_OK && activated==2 && !timer.expiry_pending);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,107,NULL,NULL,&report,&pending)==RF_OK && activated==2);
    timer.remaining=10;
    CHECK(rf_runtime_event_fire(&triggers,item[1].handle,0,0,108,&gravity,NULL,NULL,&report)==RF_OK && timer.remaining==0 && !timer.expiry_pending);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,109,NULL,NULL,&report,&pending)==RF_OK && activated==2);

    item[3].countdown_armed=item[3].countdown_fired=0;
    strcpy(authored[3].record.name,"countdown_sound");timer.remaining=55;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,110,NULL,NULL,&report,&pending)==RF_OK && activated==3 && item[3].countdown_fired);
    puts("Countdown begin/difficulty, L17 arming/equality, sound exception, exact-zero expiry and End passed");return 0;
}
