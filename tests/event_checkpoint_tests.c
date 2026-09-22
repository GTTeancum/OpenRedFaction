#include "rf/event_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"event checkpoint line%d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    unsigned char identity[32]={3},wire[128],saved[128];rf_runtime_event live={0},fresh={0},before;
    rf_level_owned_event authored={0};uint32_t pulse;int32_t remaining;
    authored.record.uid=100;live.authored=&authored;live.object_kind=6;live.handle=10;
    live.state.type=20;live.state.deadline=-1;live.state.source=live.state.actor=UINT32_MAX;
    CHECK(!rf_event_cycle_init(&live.cycle,.1f,4,0,0));CHECK(!rf_event_cycle_enable(&live.cycle,1));
    CHECK(!rf_event_cycle_tick(&live.cycle,1,&pulse)&&pulse&&live.cycle.count==1);
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,sizeof(wire)));memcpy(saved,wire,128);
    fresh=live;fresh.handle=90;CHECK(!rf_event_cycle_init(&fresh.cycle,.1f,4,0,1000));
    CHECK(!rf_event_checkpoint_restore(wire,128,identity,&fresh,1000));
    CHECK(fresh.handle==90&&fresh.cycle.count==1&&fresh.cycle.enabled==1);
    CHECK(!rf_timer_remaining(fresh.cycle.deadline,1000,&remaining)&&remaining==81);
    CHECK(!rf_event_cycle_tick(&fresh.cycle,1082,&pulse)&&pulse&&fresh.cycle.count==2);
    before=fresh;wire[92]^=1;
    CHECK(rf_event_checkpoint_restore(wire,128,identity,&fresh,1000)==RF_FORMAT&&!memcmp(&fresh,&before,sizeof(fresh)));
    memcpy(wire,saved,128);authored.record.uid=101;
    CHECK(rf_event_checkpoint_restore(wire,128,identity,&fresh,1000)==RF_FORMAT&&!memcmp(&fresh,&before,sizeof(fresh)));
    authored.record.uid=100;live.state.source=22;
    CHECK(rf_event_checkpoint_encode(identity,&live,20,wire,128)==RF_NOT_FOUND&&!memcmp(wire,saved,128));
    live.state.source=UINT32_MAX;live.state.deadline=30;
    CHECK(rf_event_checkpoint_encode(identity,&live,20,wire,128)==RF_NOT_FOUND);
    live.state.deadline=-1;live.state.type=32;
    CHECK(rf_event_checkpoint_encode(identity,&live,20,wire,128)==RF_NOT_FOUND);
    live.state.type=16;live.death_fired=1;live.death_time=19;
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,128));fresh=live;fresh.death_fired=fresh.death_time=0;
    CHECK(!rf_event_checkpoint_restore(wire,128,identity,&fresh,1000)&&fresh.death_fired==1&&fresh.death_time==19);
    live.state.type=87;live.threshold.threshold=20;live.threshold.fired=1;
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,128));fresh=live;fresh.threshold.fired=0;
    CHECK(!rf_event_checkpoint_restore(wire,128,identity,&fresh,1000)&&fresh.threshold.fired==1);
    live.state.type=88;
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,128));fresh=live;fresh.threshold.fired=0;
    CHECK(!rf_event_checkpoint_restore(wire,128,identity,&fresh,1000)&&fresh.threshold.fired==1);
    fresh.threshold.threshold=21;before=fresh;
    CHECK(rf_event_checkpoint_restore(wire,128,identity,&fresh,1000)==RF_FORMAT&&!memcmp(&fresh,&before,sizeof(fresh)));
    puts("PASS settled event cycle rebasing, monitor latches and explicit unsupported admission");return 0;
}
