#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_switch_movers.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_group_runtime_entry entry={0};rf_group_registered_controller controller={0};
    rf_level_owned_group source={0};rf_level_group_key keys[2]={{0}};
    campaign_controller_effects effects={0};rf_switch_target target={0};rf_switch_request request={0};
    rf_group_attached_pose saved_pose;uint32_t i,handle;
    rf_object_registry_init(&campaign_registry);
    entry.kind=RF_GROUP_RUNTIME_TRANSLATION;entry.source=&source;source.record.key_count=2;source.keys=keys;
    controller.object_kind=8;controller.runtime=&entry;
    CHECK(rf_object_registry_insert(&campaign_registry,&controller,&handle)==RF_OK);controller.handle=handle;
    campaign_group_runtime.items=&entry;campaign_group_runtime.count=1;campaign_controller_requests=&effects;
    for(i=0;i<4;i++)effects.sounds.samples[i]=effects.sounds.handles[i]=-1;
    CHECK(campaign_switch_mover_lookup(RF_SWITCH_CONTROLLER,handle,&target)==RF_OK && target.token==handle);
    CHECK(campaign_switch_mover_lookup(RF_SWITCH_TRIGGER,handle,&target)==RF_NOT_FOUND);
    entry.translation.motion.current_key=0;entry.translation.motion.next_key=-1;
    request.family=RF_SWITCH_CONTROLLER;request.token=handle;request.enabled=1;request.actor=UINT32_MAX;
    CHECK(campaign_switch_mover_dispatch(&request)==RF_OK);
    CHECK(entry.translation.motion.next_key==1 && effects.starts==1);
    entry.translation.speed=3;entry.translation.distance=.6f;entry.translation.velocity[0]=2;
    saved_pose=entry.pose;request.enabled=0;
    CHECK(campaign_switch_mover_dispatch(&request)==RF_OK);
    CHECK(entry.translation.motion.next_key==-1 && entry.translation.speed==0 && entry.translation.distance==.6f);
    CHECK(entry.translation.velocity[0]==2 && !memcmp(&entry.pose,&saved_pose,sizeof(saved_pose)));
    request.enabled=1;CHECK(campaign_switch_mover_dispatch(&request)==RF_OK && entry.translation.motion.next_key==1);
    /* Timed rotational stop retains active leg for deceleration, resets ramp. */
    entry.kind=RF_GROUP_RUNTIME_ROTATION_PENDING;entry.translation.motion.flags=0x44;
    entry.translation.motion.next_key=0;entry.translation.speed=.3f;entry.translation.distance=.8f;request.enabled=0;
    CHECK(campaign_switch_mover_dispatch(&request)==RF_OK);
    CHECK(entry.translation.motion.next_key==0 && entry.translation.motion.flags==0x64 && entry.translation.speed==0);
    CHECK(entry.translation.distance==.8f);
    /* Immediate rotating stop must preserve the separately owned ramp value. */
    entry.translation.motion.flags=4;entry.translation.speed=.7f;
    CHECK(campaign_switch_mover_dispatch(&request)==RF_OK);
    CHECK(entry.translation.motion.next_key==-1 && entry.translation.speed==.7f && entry.translation.distance==.8f);
    CHECK(rf_object_registry_remove(&campaign_registry,handle)==RF_OK);
    CHECK(campaign_switch_mover_lookup(RF_SWITCH_CONTROLLER,handle,&target)==RF_NOT_FOUND);
    CHECK(campaign_switch_mover_dispatch(&request)==RF_OK);
    puts("Switch controller start, stop, restart, rotation ramp and stale-handle checks passed");return 0;
}
