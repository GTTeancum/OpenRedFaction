#include "rf/mover_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mover checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_level_group_key keys[2]={{0}};rf_level_owned_group source={0};rf_group_runtime_entry entry={0};
    rf_mover_checkpoint_record rows[2],decoded[2],saved[2];rf_group_translation_runtime restored,before;
    unsigned char identity[32]={7},wire[240],bad[240];uint32_t bytes=99,count=99;int32_t remaining;
    keys[0].uid=123;keys[1].uid=124;source.keys=keys;source.record.key_count=2;entry.source=&source;entry.kind=RF_GROUP_RUNTIME_TRANSLATION;
    entry.translation.motion=(rf_group_motion_state){0x2000,2,0,1,.25f,-1};
    entry.translation.speed=3;entry.translation.distance=1.5f;entry.translation.position[0]=1;entry.translation.pending[0]=1.25f;entry.translation.velocity[0]=2;
    CHECK(!rf_timer_set(&entry.translation.deadline,RF_TIMER_PERIOD-100,200));
    CHECK(!rf_mover_checkpoint_capture(&entry,RF_TIMER_PERIOD-100,rows));CHECK(rows[0].remaining_ms==200);
    CHECK(!rf_mover_checkpoint_prepare(rows,&entry,500,&restored));
    CHECK(!rf_timer_remaining(restored.deadline,500,&remaining)&&remaining==200);
    CHECK(restored.position[0]==1&&restored.pending[0]==1.25f&&restored.motion.phase==.25f&&restored.velocity[0]==2);
    entry.kind=RF_GROUP_RUNTIME_ROTATION_PENDING;entry.translation.motion.flags|=4;
    keys[0].uid=234;entry.translation.distance=-.75f;entry.translation.deadline=-1;
    CHECK(!rf_mover_checkpoint_capture(&entry,123,rows+1));CHECK(rows[1].remaining_ms==-1);
    CHECK(!rf_mover_checkpoint_prepare(rows+1,&entry,800,&restored)&&restored.distance==-.75f&&restored.deadline==-1);
    CHECK(!rf_mover_checkpoint_encode(identity,rows,2,wire,sizeof(wire),&bytes)&&bytes==240);
    CHECK(!rf_mover_checkpoint_preflight(wire,bytes,identity,&count)&&count==2);
    CHECK(!rf_mover_checkpoint_decode(wire,bytes,identity,decoded,2,&count)&&!memcmp(rows,decoded,sizeof(rows)));
    entry.translation.deadline=50;CHECK(!rf_mover_checkpoint_capture(&entry,100,decoded)&&decoded[0].remaining_ms==0);
    CHECK(!rf_mover_checkpoint_prepare(decoded,&entry,700,&restored)&&restored.deadline==700);
    before=restored;keys[0].uid=999;
    CHECK(rf_mover_checkpoint_prepare(rows+1,&entry,700,&restored)==RF_FORMAT&&!memcmp(&before,&restored,sizeof(before)));
    memset(decoded,0xa5,sizeof(decoded));memcpy(saved,decoded,sizeof(saved));count=99;memcpy(bad,wire,sizeof(wire));bad[100]^=1;
    CHECK(rf_mover_checkpoint_decode(bad,sizeof(bad),identity,decoded,2,&count)==RF_FORMAT&&count==99&&!memcmp(decoded,saved,sizeof(saved)));
    memcpy(bad,wire,sizeof(wire));rows[1].next_key=2;bytes=99;
    CHECK(rf_mover_checkpoint_encode(identity,rows,2,bad,sizeof(bad),&bytes)==RF_FORMAT&&bytes==99&&!memcmp(bad,wire,sizeof(wire)));
    puts("Mover checkpoint: translating/rotating state, timer rebasing, UID binding and atomic malformed rejection passed");return 0;
}
