#include "../src/diagnostic/scene.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"prop checkpoint line %d: %s\n",__LINE__,#x); return 1; } } while (0)

int main(void)
{
    rf_clutter_base_owner owners[3]={0},*pointers[3]={owners,owners+1,owners+2};
    rf_level_clutter records[3]={0};rf_clutter_class classes[2]={0};
    scene_clutter_damage_profile profiles[2]={0};scene_clutter_damage_binding bindings[3]={0};
    scene_clutter_restore_stage *stage=NULL;scene_clutter_damage_live_result damage;
    unsigned char source[32]={1},wrong_source[32]={2},identity[32];
    unsigned char bytes[RF_CLUTTER_CHECKPOINT_HEADER+3*RF_CLUTTER_CHECKPOINT_ROW];
    rf_clutter_checkpoint_record decoded[3];uint32_t size=0,count=0,i,j,old_handles[3];
    const uint32_t uids[3]={300,100,200};
    campaign_clutter_classes.items=classes;campaign_clutter_classes.count=2;
    campaign_clutter_records.items=records;campaign_clutter_records.count=3;
    campaign_clutter_bodies=pointers;campaign_clutter_damage_profiles=profiles;
    campaign_clutter_damage_bindings=bindings;rf_object_registry_init(&campaign_registry);
    for(i=0;i<2;i++){
        classes[i].name=i?"test box":"test lamp";classes[i].model=i?"box.v3m":"lamp.v3m";
        classes[i].flags=2;classes[i].timer=-1;profiles[i].ordinary=1;profiles[i].life=80;
        for(j=0;j<11;j++)profiles[i].damage_factors[j]=1;
    }
    for(i=0;i<3;i++){
        owners[i].uid=records[i].uid=uids[i];owners[i].state.class_index=i==1?1:0;
        owners[i].state.definition=classes+owners[i].state.class_index;
        records[i].class_name=owners[i].state.definition->name;
        records[i].matrix[0][0]=records[i].matrix[1][1]=records[i].matrix[2][2]=1;
        memcpy(owners[i].matrix,records[i].matrix,36);
        records[i].position[0]=(float)i*2;
        memcpy(owners[i].state.position,records[i].position,12);
        memcpy(owners[i].body.state.position,records[i].position,12);
        owners[i].state.flags=0x100000;
        CHECK(!rf_object_registry_insert(&campaign_registry,&owners[i].state,&owners[i].state.handle));
        CHECK(!scene_clutter_damage_bind(&campaign_registry,owners+i,profiles+owners[i].state.class_index,bindings+i));
        old_handles[i]=owners[i].state.handle;
    }
    CHECK(!scene_clutter_damage_receive_live(&campaign_registry,owners,profiles,bindings,40,3,0,&damage));
    CHECK(!scene_clutter_damage_receive_live(&campaign_registry,owners+1,profiles+1,bindings+1,90,3,0,&damage));
    CHECK(bindings[1].break_pending);
    CHECK(scene_clutter_checkpoint_capture(source,1000,bytes,sizeof(bytes),&size)!=RF_OK);
    bindings[1].break_pending=0; /* The gameplay break queue has drained. */
    owners[2].state.flags|=0x4000;classes[0].timer=1035;
    CHECK(!scene_clutter_checkpoint_identity(source,identity));
    CHECK(!scene_clutter_checkpoint_capture(source,1000,bytes,sizeof(bytes),&size));
    CHECK(!rf_clutter_checkpoint_decode(bytes,size,identity,decoded,3,&count)&&count==3);
    CHECK(decoded[0].uid==100&&decoded[0].health==-10&&(decoded[0].flags&2));
    CHECK(decoded[1].uid==200&&(decoded[1].flags&0x4000)&&decoded[1].cooldown_ms==35);
    CHECK(decoded[2].uid==300&&decoded[2].health==40&&decoded[2].cooldown_ms==35);
    CHECK(scene_clutter_checkpoint_prepare(source,bytes,size,2000,1,&stage)!=RF_OK&&!stage);
    CHECK(!scene_clutter_checkpoint_prepare(source,bytes,size,2000,0,&stage));free(stage);stage=NULL;

    /* Recreate registered owners: save rows are UID-sorted, placement slots are not,
     * and none of the saved runtime handles remain valid. */
    for(i=0;i<3;i++){
        CHECK(!rf_object_registry_remove(&campaign_registry,owners[i].state.handle));
        CHECK(!rf_object_registry_insert(&campaign_registry,&owners[i].state,&owners[i].state.handle));
        CHECK(owners[i].state.handle!=old_handles[i]);
        memset(bindings+i,0,sizeof(bindings[i]));owners[i].state.flags=0x100000;
        CHECK(!scene_clutter_damage_bind(&campaign_registry,owners+i,profiles+owners[i].state.class_index,bindings+i));
    }
    classes[0].timer=classes[1].timer=-1;
    CHECK(scene_clutter_checkpoint_prepare(wrong_source,bytes,size,2000,1,&stage)!=RF_OK&&!stage);
    for(i=0;i<3;i++)CHECK(owners[i].state.health==80&&owners[i].state.flags==0x100000);
    CHECK(!scene_clutter_checkpoint_prepare(source,bytes,size,2000,1,&stage));
    CHECK(stage->entries[0].slot==1&&stage->entries[1].slot==2&&stage->entries[2].slot==0);
    /* A stale final target must reject before changing the preceding targets. */
    old_handles[0]=owners[0].state.handle;owners[0].state.handle^=0x10000;
    CHECK(scene_clutter_checkpoint_publish(stage)!=RF_OK);
    for(i=0;i<3;i++)CHECK(owners[i].state.health==80&&owners[i].state.flags==0x100000&&!bindings[i].break_pending);
    CHECK(classes[0].timer==-1&&classes[1].timer==-1);
    owners[0].state.handle=old_handles[0];CHECK(!scene_clutter_checkpoint_publish(stage));free(stage);
    CHECK(owners[0].state.health==40&&!(owners[0].state.flags&2));
    CHECK(owners[1].state.health==-10&&(owners[1].state.flags&2)&&bindings[1].killing_type==3);
    CHECK(owners[2].state.health==80&&(owners[2].state.flags&0x4000));
    CHECK(classes[0].timer==2035&&classes[1].timer==-1);
    for(i=0;i<3;i++)CHECK((owners[i].state.flags&0x100000)&&!bindings[i].break_pending);
    puts("PASS prop checkpoint UID remapping, damaged/dead/hidden restore, cooldown rebasing and atomic target rejection");
    return 0;
}
