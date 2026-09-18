#include "rf/event.h"
#include <stdio.h>
#include <string.h>
static rf_runtime_events campaign_events;
static char campaign_current_level[64];
#include "../src/diagnostic/scene_event_history.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_runtime_event items[2]={{0}};rf_level_owned_event authored[2]={{0}};
    uint32_t pulse;campaign_event_saved saved;rf_runtime_event before;
    campaign_events.items=items;campaign_events.count=2;strcpy(campaign_current_level,"dev.rfl");
    items[0].authored=authored;items[1].authored=authored+1;authored[0].record.uid=20;authored[1].record.uid=87;
    items[0].state.type=20;items[1].state.type=87;items[1].threshold.threshold=50;
    CHECK(rf_event_cycle_init(&items[0].cycle,.5f,5,0,0)==RF_OK);
    campaign_event_history_reset();CHECK(campaign_event_checkpoint(0,0)==RF_OK);
    CHECK(campaign_event_history.count==2);
    items[0].cycle.count=2;items[0].cycle.enabled=1;items[0].cycle.deadline=1000;items[1].threshold.fired=1;
    CHECK(campaign_event_checkpoint(1,800)==RF_OK);
    CHECK(rf_event_cycle_init(&items[0].cycle,.5f,5,0,0)==RF_OK);items[1].threshold.fired=0;
    CHECK(campaign_event_checkpoint(0,0)==RF_OK);
    CHECK(items[0].cycle.count==2 && items[0].cycle.enabled && items[0].cycle.deadline==200);
    CHECK(items[1].threshold.fired && items[1].threshold.threshold==50);
    CHECK(rf_event_cycle_tick(&items[0].cycle,199,&pulse)==RF_OK && !pulse);
    CHECK(rf_event_cycle_tick(&items[0].cycle,200,&pulse)==RF_OK && pulse && items[0].cycle.count==3);
    /* Same UID in a different level has independent history. */
    strcpy(campaign_current_level,"other.rfl");items[0].cycle.count=0;items[1].threshold.fired=0;
    CHECK(campaign_event_checkpoint(0,0)==RF_OK && items[0].cycle.count==0 && !items[1].threshold.fired);
    CHECK(campaign_event_history.count==4);
    strcpy(campaign_current_level,"dev.rfl");CHECK(campaign_event_checkpoint(0,RF_TIMER_PERIOD-100)==RF_OK);
    CHECK(items[0].cycle.deadline==100);
    /* Disabled and overdue state stays disabled, resumes due immediately. */
    items[0].cycle.enabled=0;items[0].cycle.deadline=100;
    CHECK(campaign_event_capture(items,200,&saved)==RF_OK && !saved.cycle_remaining);
    CHECK(campaign_event_restore(items,10,&saved)==RF_OK && items[0].cycle.deadline==10 && !items[0].cycle.enabled);
    before=items[0];saved.type=88;
    CHECK(campaign_event_restore(items,10,&saved)==RF_FORMAT && !memcmp(&before,items,sizeof(before)));
    campaign_event_history_reset();CHECK(!campaign_event_history.count);
    puts("Event section history cycle rebasing/count/enabled, threshold latch, level keys and reset passed");return 0;
}
