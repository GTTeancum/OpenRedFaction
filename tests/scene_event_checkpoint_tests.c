#include "../src/diagnostic/scene_event_checkpoint.inc"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"scene event checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_object_registry old_registry,new_registry;
static int admit(void *context,const rf_runtime_event *event)
{
    /* Fixture has no running world effects. Real scene composer must implement
     * per-system settled/captured checks, not reuse this fixture callback. */
    if(context)return RF_NOT_FOUND;
    return event->state.type==5||event->state.type==15||event->state.type==32?RF_OK:RF_NOT_FOUND;
}
static void switch_effect(void *context,const rf_switch_state *state,uint32_t effect)
{(void)context;(void)state;(void)effect;}
int main(void)
{
    rf_runtime_event old[3]={{0}},fresh[3]={{0}},before[3];rf_level_owned_event authored[3]={{0}};
    rf_switch_state old_switch,new_switch,switch_before;
    rf_runtime_events original={0},restored={0};rf_level_uid_object old_map[4],new_map[4];
    scene_event_checkpoint_map source={old_map,4,&old_registry},destination={new_map,4,&new_registry};
    scene_event_checkpoint_blob blob={0},empty_blob={0};scene_event_checkpoint_stage stage={0},empty={0};
    unsigned char identity[32]={4},corrupt[3*RF_EVENT_CHECKPOINT_BYTES];
    uint32_t old_actor=7,new_actor=7,old_actor_handle,new_actor_handle,i;
    rf_object_registry_init(&old_registry);rf_object_registry_init(&new_registry);
    CHECK(!rf_object_registry_insert(&old_registry,&old_actor,&old_actor_handle));
    for(i=0;i<3;i++){
        authored[i].record.uid=100+i;old[i].authored=authored+i;old[i].object_kind=6;
        old[i].state.type=i==0?5:i==1?32:15;old[i].state.deadline=-1;
        CHECK(!rf_object_registry_insert(&old_registry,old+i,&old[i].handle));
        old_map[i]=(rf_level_uid_object){100+i,old[i].handle,0};
    }
    old_map[3]=(rf_level_uid_object){8456,old_actor_handle,0};
    old[0].state.source=old_actor_handle;old[0].state.actor=UINT32_MAX;old[0].state.mode=1;
    CHECK(!rf_event_switch_init(&old_switch,0,10,0,0));CHECK(!rf_event_switch_on(&old_switch,switch_effect,NULL));old[1].switch_state=&old_switch;
    old[2].retired=1;old[2].death_fired=1;old[2].state.flags=1;
    CHECK(!rf_object_registry_remove(&old_registry,old[2].handle));
    original.items=old;original.count=3;original.registry=&old_registry;
    CHECK(scene_event_checkpoint_capture(&original,&source,identity,100,NULL,NULL,65536,&blob)==RF_RANGE&&!memcmp(&blob,&empty_blob,sizeof(blob)));
    CHECK(!scene_event_checkpoint_capture(&original,&source,identity,100,admit,NULL,65536,&blob)&&blob.bytes==sizeof(corrupt));
    /* Reversed creation order produces different handles and owner ordering. */
    CHECK(!rf_event_switch_init(&new_switch,0,10,0,0));
    for(i=0;i<3;i++){
        fresh[i]=old[2-i];fresh[i].state.flags=fresh[i].state.mode=0;fresh[i].state.source=fresh[i].state.actor=0;
        fresh[i].retired=fresh[i].death_fired=0;
        if(fresh[i].state.type==32)fresh[i].switch_state=&new_switch;
        CHECK(!rf_object_registry_insert(&new_registry,fresh+i,&fresh[i].handle));
        new_map[i]=(rf_level_uid_object){fresh[i].authored->record.uid,fresh[i].handle,0};
    }
    CHECK(!rf_object_registry_insert(&new_registry,&new_actor,&new_actor_handle));new_map[3]=(rf_level_uid_object){8456,new_actor_handle,0};
    restored.items=fresh;restored.count=3;restored.registry=&new_registry;
    memcpy(before,fresh,sizeof(before));switch_before=new_switch;
    CHECK(!scene_event_checkpoint_prepare(&restored,&destination,identity,blob.data,blob.bytes,1000,admit,NULL,65536,&stage));
    CHECK(!memcmp(before,fresh,sizeof(before))&&!memcmp(&switch_before,&new_switch,sizeof(new_switch)));
    fresh[2].state.mode=8;
    CHECK(scene_event_checkpoint_commit(&stage)==RF_FORMAT&&fresh[0].retired==0&&new_switch.activations==0);
    fresh[2].state.mode=0;
    CHECK(!scene_event_checkpoint_commit(&stage)&&!memcmp(&stage,&empty,sizeof(stage)));
    CHECK(fresh[2].state.source==new_actor_handle&&fresh[2].state.actor==UINT32_MAX&&fresh[2].state.mode==1);
    CHECK(new_switch.activations==1&&fresh[1].switch_state==&new_switch);
    CHECK(fresh[0].retired&&fresh[0].death_fired&&!rf_object_registry_lookup(&new_registry,fresh[0].handle));
    memcpy(before,fresh,sizeof(before));switch_before=new_switch;
    memcpy(corrupt,blob.data,sizeof(corrupt));memcpy(corrupt+RF_EVENT_CHECKPOINT_BYTES,corrupt,RF_EVENT_CHECKPOINT_BYTES);
    CHECK(scene_event_checkpoint_prepare(&restored,&destination,identity,corrupt,sizeof(corrupt),1000,admit,NULL,65536,&stage)==RF_FORMAT);
    CHECK(!memcmp(&stage,&empty,sizeof(stage))&&!memcmp(before,fresh,sizeof(before))&&!memcmp(&switch_before,&new_switch,sizeof(new_switch)));
    CHECK(scene_event_checkpoint_prepare(&restored,&destination,identity,blob.data,blob.bytes,1000,admit,(void *)1,65536,&stage)==RF_NOT_FOUND);
    CHECK(!memcmp(&stage,&empty,sizeof(stage)));
    CHECK(!rf_object_registry_remove(&new_registry,new_actor_handle));
    CHECK(scene_event_checkpoint_prepare(&restored,&destination,identity,blob.data,blob.bytes,1000,admit,NULL,65536,&stage)==RF_NOT_FOUND);
    CHECK(!memcmp(&stage,&empty,sizeof(stage))&&!memcmp(before,fresh,sizeof(before)));
    scene_event_checkpoint_blob_close(&blob);CHECK(!memcmp(&blob,&empty_blob,sizeof(blob)));
    puts("PASS event scene staging, reordered UID refs, Switch publication, registry retirement and atomic rejection");return 0;
}
