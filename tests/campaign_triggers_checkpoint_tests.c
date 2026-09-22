#include "rf/campaign_triggers_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"trigger checkpoint line%d\n",__LINE__);return 1;}}while(0)
static rf_campaign_triggers store,out,before;
static unsigned char wire[RF_CAMPAIGN_TRIGGERS_CHECKPOINT_MAX_BYTES],saved[sizeof(wire)];
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void checksum(uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=i>=12&&i<16?0:wire[i];h*=16777619u;}put(wire+12,h);}
int main(void)
{
    unsigned char identity[32]={7},other[32]={8};uint32_t a,b,bytes=999,rows;
    rf_runtime_trigger live={0},fresh={0};int32_t remaining;
    CHECK(!rf_campaign_trigger_register(&store,"L1S1.rfl",100,&a));
    CHECK(!rf_campaign_trigger_register(&store,"L1S2.rfl",100,&b));
    live.object_kind=5;live.handle=123;live.state.handle=123;live.state.flags=64|8;live.state.count=3;
    live.activation.limit=7;live.activation.object_flags=2;live.state.activation_time_bits=0x3f800000;
    CHECK(!rf_timer_set(&live.state.deadline,100,300));CHECK(!rf_timer_set(&live.contact_timer.deadline,100,90));
    CHECK(!rf_runtime_trigger_save(&live,150,store.states+a));
    CHECK(store.states[a].flags==8&&store.states[a].cooldown_remaining==250&&store.states[a].contact_remaining==40);
    live.state.deadline=live.contact_timer.deadline=-1;
    CHECK(!rf_runtime_trigger_save(&live,150,store.states+b));store.items[b].retired=1;
    CHECK(!rf_campaign_triggers_checkpoint_encode(identity,&store,wire,sizeof(wire),&bytes));
    CHECK(bytes==64+2*64+2*40);
    CHECK(!rf_campaign_triggers_checkpoint_decode(wire,bytes,identity,&out)&&!memcmp(&store,&out,sizeof(store)));
    fresh.object_kind=5;fresh.handle=456;fresh.state.handle=456;fresh.volume.radius=2;
    CHECK(!rf_runtime_trigger_restore(&fresh,1000,out.states+a));
    CHECK(fresh.handle==456&&fresh.state.handle==456&&fresh.volume.radius==2&&fresh.state.count==3&&fresh.activation.limit==7);
    CHECK(!rf_timer_remaining(fresh.state.deadline,1000,&remaining)&&remaining==250);
    CHECK(!rf_timer_remaining(fresh.contact_timer.deadline,1000,&remaining)&&remaining==40);
    CHECK(!rf_runtime_trigger_restore(&fresh,1000,out.states+b)&&fresh.state.deadline==-1&&fresh.contact_timer.deadline==-1);
    memcpy(saved,wire,sizeof(wire));memset(&out,0xa5,sizeof(out));before=out;
    CHECK(rf_campaign_triggers_checkpoint_decode(wire,bytes,other,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    wire[bytes-1]^=1;CHECK(rf_campaign_triggers_checkpoint_decode(wire,bytes,identity,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    memcpy(wire,saved,sizeof(wire));rows=64+2*64;put(wire+rows+40,0);checksum(bytes);
    CHECK(rf_campaign_triggers_checkpoint_decode(wire,bytes,identity,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    memcpy(wire,saved,sizeof(wire));put(wire+rows+32,(uint32_t)-2);checksum(bytes);
    CHECK(rf_campaign_triggers_checkpoint_decode(wire,bytes,identity,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    memcpy(wire,saved,sizeof(wire));memcpy(wire+128,wire+64,64);wire[128]='L';checksum(bytes);
    CHECK(rf_campaign_triggers_checkpoint_decode(wire,bytes,identity,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    memcpy(wire,saved,sizeof(wire));store.states[a].flags|=64;bytes=999;
    CHECK(rf_campaign_triggers_checkpoint_encode(identity,&store,wire,sizeof(wire),&bytes)==RF_FORMAT&&bytes==999&&!memcmp(wire,saved,sizeof(wire)));
    memset(&store,0,sizeof(store));CHECK(!rf_campaign_triggers_checkpoint_encode(identity,&store,wire,sizeof(wire),&bytes)&&bytes==64);
    CHECK(!rf_campaign_triggers_checkpoint_decode(wire,bytes,identity,&out)&&!out.count&&!out.level_count);
    puts("PASS trigger store codec, actual runtime timer rebase and unchanged corruption rejection");return 0;
}
