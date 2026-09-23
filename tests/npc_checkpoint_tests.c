#include "rf/npc_checkpoint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void reseal(unsigned char *p,uint32_t n)
{uint32_t i,h=2166136261u;for(i=0;i<n;i++){h^=(i>=12&&i<16)?0:p[i];h*=16777619u;}for(i=0;i<4;i++)p[12+i]=(unsigned char)(h>>(i*8));}
int main(void)
{
    rf_npc_checkpoint_catalog c={0};rf_npc_checkpoint_record rows[2]={0},out[2],saved[2];
    unsigned char identity[32]={9},wrong[32]={8},blob[64+2*RF_NPC_CHECKPOINT_ROW],original[sizeof(blob)],sentinel[sizeof(blob)];
    uint32_t bytes=0,count=99,written=123;int status;
    c.hash=0x12345678;c.count=2;c.supported[0]=c.supported[1]=1;
    c.weapons[0]=(rf_weapon_acquire_definition){0,100,12};c.weapons[1]=(rf_weapon_acquire_definition){1,5,0};
    rows[0].uid=7;rows[0].class_id=3;rows[0].health=80;rows[0].armor=15;rows[0].flags=0x4004;
    rows[0].position[0]=4;rows[0].yaw=.5f;rows[0].primary=0;rows[0].secondary=-1;rows[0].ai_mode=2;
    rows[0].inventory.owned[0]=1;rows[0].inventory.loaded[0]=4;rows[0].inventory.reserve[0]=120;
    rows[0].eye_angles[0]=.218f;rows[0].eye_angles[1]=1e-7f;rows[0].eye_angles[2]=-.03f;
    rows[1]=rows[0];rows[1].uid=10;rows[1].retired=1;rows[1].health=-3;
    rows[1].drop=(rf_campaign_weapon_drop){1,0,0,{4,2,1}};
    CHECK(!rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&bytes)&&bytes==sizeof(blob));
    CHECK(!rf_npc_checkpoint_preflight(blob,bytes,identity,&c,&count)&&count==2);
    CHECK(!rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,2,&count)&&!memcmp(rows,out,sizeof(rows)));
    CHECK(out[0].inventory.reserve[0]==120&&out[1].drop.quantity==0&&out[1].drop.state==1);
    {unsigned char legacy[64+2*RF_NPC_CHECKPOINT_ROW_V1];
     memcpy(legacy,blob,64);legacy[4]=1;
     for(uint32_t i=0;i<4;i++)legacy[8+i]=(unsigned char)(sizeof(legacy)>>(8*i));
     for(uint32_t i=0;i<2;i++)memcpy(legacy+64+i*RF_NPC_CHECKPOINT_ROW_V1,blob+64+i*RF_NPC_CHECKPOINT_ROW,RF_NPC_CHECKPOINT_ROW_V1);
     reseal(legacy,sizeof(legacy));
     CHECK(!rf_npc_checkpoint_decode(legacy,sizeof(legacy),identity,&c,out,2,&count));
     CHECK(out[0].eye_angles[0]==0&&out[0].eye_angles[1]==0&&out[0].eye_angles[2]==0&&out[1].uid==10);
    }
    memcpy(original,blob,bytes);memset(out,0x5a,sizeof(out));memcpy(saved,out,sizeof(out));count=99;
    CHECK(rf_npc_checkpoint_decode(blob,bytes,wrong,&c,out,2,&count)==RF_FORMAT&&count==99&&!memcmp(saved,out,sizeof(out)));
    c.hash++;CHECK(rf_npc_checkpoint_preflight(blob,bytes,identity,&c,&count)==RF_FORMAT&&count==99);c.hash--;
    blob[90]^=1;CHECK(rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,2,&count)==RF_FORMAT&&!memcmp(saved,out,sizeof(out)));
    memcpy(blob,original,bytes);blob[64+RF_NPC_CHECKPOINT_ROW+524]=4;blob[64+RF_NPC_CHECKPOINT_ROW+525]=blob[64+RF_NPC_CHECKPOINT_ROW+526]=blob[64+RF_NPC_CHECKPOINT_ROW+527]=0;reseal(blob,bytes);
    CHECK(rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,2,&count)==RF_FORMAT&&count==99&&!memcmp(saved,out,sizeof(out)));
    memcpy(blob,original,bytes);CHECK(rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,1,&count)==RF_RANGE&&!memcmp(saved,out,sizeof(out)));
    memset(blob,0xa5,sizeof(blob));memcpy(sentinel,blob,sizeof(blob));rows[1].uid=rows[0].uid;
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT&&written==123&&!memcmp(blob,sentinel,sizeof(blob)));
    rows[1].uid=10;rows[1].eye_angles[0]=NAN;
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT&&!memcmp(blob,sentinel,sizeof(blob)));
    rows[1].eye_angles[0]=.218f;rows[0].health=NAN;
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT&&!memcmp(blob,sentinel,sizeof(blob)));
    rows[0].health=80;rows[0].inventory.loaded[0]=13;
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT);
    rows[0].inventory.loaded[0]=4;rows[0].inventory.owned[0]=0;rows[0].primary=-1;
    CHECK(!rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)); /* None retains dormant ammo. */
    rows[0].primary=0;CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT);
    rows[0].primary=-1;rows[1].drop.state=2;
    CHECK(!rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written));
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob)-1,&written)==RF_RANGE);
    /* Optional animation retains the exact mixed-slot continuation state. */
    {
        unsigned char wire[64+RF_NPC_CHECKPOINT_ROW_MAX],broken[sizeof(wire)],legacy2[64+RF_NPC_CHECKPOINT_ROW_V2];
        rf_npc_checkpoint_record actor=rows[0],decoded,untouched;rf_motion_playback_resource resources[2]={0},restored_resources[2];
        rf_motion_playback_state expected;uint32_t n=0,got=0;
        actor.animation_present=1;actor.script_animation.active=1;actor.script_animation.motion=1;
        rf_motion_playback_initialize(&actor.playback);actor.playback.phase=.25f;actor.playback.generation=27;actor.playback.event_mask=2;
        actor.playback.completion.active.count=2;actor.playback.completion.active.primary_slot=1;actor.playback.completion.active.dominant_slot=0;
        actor.playback.completion.active.slots[0]=(rf_motion_active_slot){0,2500,.75f};
        actor.playback.completion.active.slots[1]=(rf_motion_active_slot){1,1800,.5f};
        actor.playback.completion.primary_flag=7;actor.playback.completion.primary_words[0]=0xabc;
        actor.playback.completion.primary_vectors[1][2]=.125f;
        CHECK(!rf_npc_checkpoint_encode(identity,&c,&actor,1,wire,sizeof(wire),&n));
        CHECK(n==64+RF_NPC_CHECKPOINT_ROW+RF_NPC_CHECKPOINT_ANIMATION_BASE+24);
        CHECK(!rf_npc_checkpoint_decode(wire,n,identity,&c,&decoded,1,&got)&&got==1&&!memcmp(&actor,&decoded,sizeof(actor)));
        for(uint32_t i=0;i<2;i++){resources[i].comparison.weight=1;resources[i].comparison.end_tick=10000;resources[i].references=1;}
        resources[0].looping=1;resources[0].markers[0]=3000;resources[0].markers[1]=7500;
        memcpy(restored_resources,resources,sizeof(resources));expected=actor.playback;
        CHECK(!rf_motion_update(&expected,resources,2,.1f));
        CHECK(!rf_motion_update(&decoded.playback,restored_resources,2,.1f));
        CHECK(!memcmp(&expected,&decoded.playback,sizeof(expected))&&!memcmp(resources,restored_resources,sizeof(resources)));
        memset(&decoded,0x5a,sizeof(decoded));untouched=decoded;got=99;
        memcpy(broken,wire,n);broken[64+RF_NPC_CHECKPOINT_ROW+16]=17;reseal(broken,n);
        CHECK(rf_npc_checkpoint_decode(broken,n,identity,&c,&decoded,1,&got)==RF_FORMAT&&got==99&&!memcmp(&decoded,&untouched,sizeof(decoded)));
        CHECK(rf_npc_checkpoint_encode(identity,&c,&actor,1,broken,n-1,&got)==RF_RANGE&&got==99);
        actor.playback.completion.active.slots[1].motion=0;
        CHECK(rf_npc_checkpoint_encode(identity,&c,&actor,1,broken,sizeof(broken),&got)==RF_FORMAT);
        /* Ambient state18 is not a script: retain the actual controller and continuation. */
        actor.playback.completion.active.slots[1].motion=1;memset(&actor.script_animation,0,sizeof(actor.script_animation));
        actor.controller.current=18;actor.controller.next=19;actor.controller.duration=.5f;actor.controller.elapsed=.125f;
        actor.controller.override_state=-1;
        CHECK(!rf_npc_checkpoint_encode(identity,&c,&actor,1,broken,sizeof(broken),&got));
        CHECK(!rf_npc_checkpoint_decode(broken,got,identity,&c,&decoded,1,&count));
        CHECK(!memcmp(&actor,&decoded,sizeof(actor))&&!decoded.script_animation.active&&decoded.controller.current==18);
        {int32_t mappings[23];rf_motion_controller control=actor.controller;
         for(uint32_t i=0;i<23;i++)mappings[i]=-1;mappings[18]=0;mappings[19]=1;
         expected=actor.playback;memcpy(restored_resources,resources,sizeof(resources));
         CHECK(!rf_motion_apply_controller(&control,mappings,.1f,&expected,resources,2));
         CHECK(!rf_motion_apply_controller(&decoded.controller,mappings,.1f,&decoded.playback,restored_resources,2));
         CHECK(!memcmp(&control,&decoded.controller,sizeof(control))&&!memcmp(&expected,&decoded.playback,sizeof(expected)));}
        actor.controller.elapsed=NAN;
        CHECK(rf_npc_checkpoint_encode(identity,&c,&actor,1,broken,sizeof(broken),&got)==RF_FORMAT);
        actor.controller.elapsed=.125f;actor.controller.current=23;
        CHECK(rf_npc_checkpoint_encode(identity,&c,&actor,1,broken,sizeof(broken),&got)==RF_FORMAT);
        /* Legacy RFNC2 has no optional-animation flag or trailer. */
        memcpy(legacy2,wire,64);memcpy(legacy2+64,wire+64,RF_NPC_CHECKPOINT_ROW_V2);legacy2[4]=2;
        for(uint32_t i=0;i<4;i++)legacy2[8+i]=(unsigned char)(sizeof(legacy2)>>(8*i));
        reseal(legacy2,sizeof(legacy2));CHECK(!rf_npc_checkpoint_decode(legacy2,sizeof(legacy2),identity,&c,&decoded,1,&got));
        CHECK(!decoded.animation_present&&decoded.eye_angles[0]==actor.eye_angles[0]);
    }
    status=rf_npc_checkpoint_encode(identity,&c,NULL,0,blob,sizeof(blob),&written);
    CHECK(!status&&written==64&&!rf_npc_checkpoint_decode(blob,written,identity,&c,NULL,0,&count)&&!count);
    puts("PASS NPC checkpoint pose/vitals/inventory/drop roundtrip, identity, malformed state and output preservation");return 0;
}
