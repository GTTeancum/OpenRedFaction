/* Exercise the scene's private residency owner without adding runtime hooks. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"residency line %d\n",__LINE__);return 1;} } while(0)
static int eye_binding_check(void)
{
    campaign_npc_body owner={0};campaign_npc_eye_class eye={0};rf_entity_pose pose={0};
    rf_entity_seed seed={0};rf_entity_seed_class cls={0};rf_level_owned_entity record={0};
    float matrix[1][12]={{1,0,0,0,1,0,0,0,1,0,3,0}};
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;campaign_npc_eyes=&eye;
    campaign_poses.items=&pose;campaign_poses.count=1;pose.matrices=matrix;pose.bone_count=1;
    campaign_seeds.items=&seed;campaign_seeds.classes=&cls;campaign_seeds.class_count=1;
    campaign_seeds.records.items=&record;campaign_seeds.records.count=1;
    record.record.orientation[0][0]=record.record.orientation[1][1]=record.record.orientation[2][2]=1;
    owner.published[0]=10;owner.published[1]=20;owner.published[2]=30;
    eye.tag=-1;CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==20);
    eye.tag=0;eye.parent=0;eye.offsets[1]=2;eye.offsets[4]=1;
    CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==22);
    pose.controller.current=8;pose.controller.next=0;pose.controller.duration=1;pose.controller.elapsed=.25f;
    CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==21.25f);
    cls.physics.flags2=0x20;CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==20);
    cls.physics.flags2=0x40;eye.local[0]=eye.local[4]=eye.local[8]=1;eye.local[10]=.5f;
    CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==23.5f);
    matrix[0][10]=5;CHECK(campaign_npc_eye_update(0)==RF_OK && owner.eye_position[1]==25.5f);
    eye.parent=-1;CHECK(campaign_npc_eye_update(0)==RF_RANGE && owner.eye_position[1]==25.5f);
    CHECK(campaign_npc_eye_update(1)==RF_RANGE);
    campaign_npc_eyes=NULL;campaign_npc_bodies=NULL;campaign_npc_body_count=0;
    memset(&campaign_poses,0,sizeof(campaign_poses));memset(&campaign_seeds,0,sizeof(campaign_seeds));return 0;
}
static int pain_binding_check(void)
{
    campaign_npc_body owner={0};rf_random_state random={1};uint32_t i;
    rf_motion_playback_state saved;uint32_t saved_random;
    rf_entity_state_set *bindings=calloc(1,sizeof(*bindings));CHECK(bindings);
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    campaign_base_motions.classes=bindings;campaign_base_motions.class_count=1;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    owner.view.weapons[0]=owner.view.weapons[1]=-1;owner.view.linked_handle=-1;
    CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration)==RF_OK);
    owner.pain.selected_action=-1;
    for(i=0;i<45;++i)campaign_motion_catalog.mappings[0].actions[i]=-1;
    campaign_motion_catalog.mappings[0].actions[22]=2;
    campaign_playback_resources.models[0].resources=campaign_playback_resources.resources;
    memset(campaign_playback_resources.resources,0,3*sizeof(*campaign_playback_resources.resources));
    campaign_playback_resources.resources[2].comparison.end_tick=2400;
    campaign_playback_resources.resources[2].comparison.weight=1;
    rf_motion_playback_initialize(&campaign_poses.items[0].playback);
    CHECK(rf_scene_npc_pain(owner.registration.handle^0x10000,1000,&random,NULL)==RF_NOT_FOUND && random.value==1);
    CHECK(rf_scene_npc_pain(owner.registration.handle,1000,&random,NULL)==RF_OK);
    CHECK(owner.pain.selected_action==22 && owner.pain.cooldown==2041 && random.value==2745024);
    CHECK(campaign_poses.items[0].playback.completion.active.count==1);
    saved=campaign_poses.items[0].playback;saved_random=random.value;
    CHECK(rf_scene_npc_pain(owner.registration.handle,1000,&random,NULL)==RF_OK);
    CHECK(!memcmp(&saved,&campaign_poses.items[0].playback,sizeof(saved)) && random.value==saved_random);
    owner.view.weapons[0]=0;owner.pain.cooldown=0;owner.pain.selected_action=-1;
    CHECK(rf_scene_npc_pain(owner.registration.handle,3000,&random,NULL)==RF_NOT_FOUND);
    CHECK(owner.pain.selected_action==-1 && owner.pain.cooldown==0 && random.value==saved_random);
    CHECK(!memcmp(&saved,&campaign_poses.items[0].playback,sizeof(saved)));
    owner.view.weapons[0]=-1;strcpy(bindings->action_sounds[22],"test sound");
    CHECK(rf_scene_npc_pain(owner.registration.handle,3000,&random,NULL)==RF_NOT_FOUND);
    CHECK(owner.pain.selected_action==22 && owner.pain.cooldown==0 && random.value==saved_random);
    {int32_t groups[1][2]={{-1,-1}},samples[2]={-1,-1};rf_foley_group group={0};
     campaign_pain_groups=groups;campaign_seeds.class_count=1;owner.damage.effects.health=100;owner.pain_sound.voice=-1;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle^0x10000u,.1f,1000,&random)==RF_NOT_FOUND);
     CHECK(owner.pain_sound.deadline==0 && random.value==saved_random);
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,1000,&random)==RF_OK);
     CHECK(owner.pain_sound.deadline==2000 && owner.pain_sound.voice==-1 && random.value==saved_random);
     groups[0][0]=0;campaign_foley.groups=&group;campaign_foley.group_count=1;
     campaign_foley.samples=samples;campaign_foley.sample_count=2;group.count=2;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,1000,&random)==RF_OK && random.value==saved_random);
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,2000,&random)==RF_OK);
     CHECK(owner.pain_sound.deadline==3000 && random.value!=saved_random && owner.pain_sound.voice==-1);
     saved_random=random.value;group.count=0;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,3000,&random)==RF_FORMAT);
     CHECK(owner.pain_sound.deadline==4000 && random.value==saved_random);
     owner.damage.effects.health=0;
     CHECK(rf_scene_npc_pain_sound(owner.registration.handle,.1f,4000,&random)==RF_NOT_FOUND && owner.pain_sound.deadline==4000);
     memset(&campaign_foley,0,sizeof(campaign_foley));campaign_pain_groups=NULL;campaign_seeds.class_count=0;}
    CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owner.registration)==RF_OK);
    free(bindings);campaign_base_motions.classes=NULL;campaign_npc_bodies=NULL;campaign_npc_body_count=0;
    return 0;
}
int main(void)
{
    rf_vpp archive={0};unsigned char payload[160]={0};
    rf_entity_model_motion motions[3]={0};
    rf_entity_model_motions model={0};rf_entity_playback_model playback={0};
    rf_motion_playback_resource resources[3]={0};
    uint32_t ids[3]={0,0,1};void *data[2]={0};uint32_t sizes[2]={0};
    rf_entity_pose pose={0};rf_entity_seed seed={0};rf_entity_motion_mapping mapping={0};
    uint32_t baseline=2*(sizeof(void*)+sizeof(uint32_t)),i;
    archive.stream=tmpfile();CHECK(archive.stream);archive.length=sizeof(payload);
    payload[80]=1;CHECK(fwrite(payload,1,sizeof(payload),archive.stream)==sizeof(payload));
    for(i=0;i<3;++i){motions[i].file.archive=&archive;motions[i].file.entry.size=80;}
    motions[2].file.entry.offset=80;motions[2].file.header[0]=1;
    model.items=motions;model.count=3;playback.cache_ids=ids;playback.count=3;
    campaign_motion_catalog.models=&model;campaign_motion_catalog.model_count=1;
    campaign_motion_catalog.mappings=&mapping;campaign_motion_catalog.class_count=1;
    campaign_playback_resources.models=&playback;campaign_playback_resources.model_count=1;
    campaign_playback_resources.resources=resources;campaign_playback_resources.cache_ids=ids;
    campaign_playback_resources.resource_count=3;campaign_playback_resources.cache_count=2;
    campaign_npc_motion_data=data;campaign_npc_motion_sizes=sizes;
    campaign_npc_motion_count=2;campaign_npc_motion_bytes=baseline;
    campaign_poses.items=&pose;campaign_poses.count=1;
    campaign_seeds.items=&seed;campaign_seeds.records.count=1;
    pose.controller.current=pose.controller.next=-1;
    pose.playback.completion.active.count=1;pose.playback.completion.active.slots[0].motion=0;
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(data[0] && !data[1] && campaign_npc_motion_bytes==baseline+80);
    CHECK(motions[0].file.resident==data[0]);
    /* A new action selected after startup loads its payload before sampling. */
    pose.playback.completion.active.slots[0].motion=2;
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(data[1] && motions[2].file.resident==data[1] && campaign_npc_motion_bytes==baseline+160);
    /* An uncopied alias binds the same allocation even with archive I/O unavailable. */
    fclose(archive.stream);archive.stream=NULL;
    pose.playback.completion.active.slots[0].motion=1;
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(motions[1].file.resident==data[0] && campaign_npc_motion_bytes==baseline+160);
    CHECK(campaign_npc_pose_residency(0)==RF_OK);
    CHECK(campaign_npc_motion_require(0,3)==RF_RANGE);
    free(data[1]);data[1]=NULL;sizes[1]=0;motions[2].file.resident=NULL;
    campaign_npc_motion_bytes=1024*1024-79;
    resources[1].references=1; /* A different registration keeps identity zero live. */
    CHECK(campaign_npc_motion_require(0,2)==RF_RANGE && !data[1] && !motions[2].file.resident);
    CHECK(data[0] && motions[0].file.resident==data[0] && motions[1].file.resident==data[0]);
    campaign_npc_motion_bytes=baseline+80;
    CHECK(campaign_npc_motion_require(0,2)==RF_IO && !data[1] && !sizes[1]);
    CHECK(campaign_npc_motion_bytes==baseline+80);
    archive.stream=tmpfile();CHECK(archive.stream);
    CHECK(fwrite(payload,1,sizeof(payload),archive.stream)==sizeof(payload));
    motions[2].file.header[0]=99;
    CHECK(campaign_npc_motion_require(0,2)==RF_FORMAT && !data[1] && !motions[2].file.resident);
    CHECK(campaign_npc_motion_bytes==baseline+80);
    motions[2].file.header[0]=1;
    CHECK(campaign_npc_motion_require(0,2)==RF_OK && data[1]);
    CHECK(campaign_npc_motion_bytes==baseline+160);
    /* Full pressure scans reject corrupt references before touching any alias. */
    campaign_npc_motion_bytes=1024*1024;resources[0].references=-1;
    CHECK(campaign_npc_motion_reserve(1)==RF_RANGE && data[0] && data[1]);
    resources[0].references=0;resources[1].references=0;resources[2].references=-1;
    CHECK(campaign_npc_motion_reserve(1)==RF_RANGE && data[0] && data[1]);
    resources[0].references=0;resources[1].references=0;resources[2].references=1;
    CHECK(campaign_npc_motion_reserve(81)==RF_RANGE && data[0] && data[1]);
    CHECK(campaign_npc_motion_reserve(80)==RF_OK && !data[0] && data[1]);
    CHECK(!motions[0].file.resident && !motions[1].file.resident && motions[2].file.resident==data[1]);
    CHECK(!sizes[0] && campaign_npc_motion_bytes==1024*1024-80);
    CHECK(campaign_npc_motion_require(0,1)==RF_OK && data[0] && motions[1].file.resident==data[0]);
    CHECK(campaign_npc_motion_bytes==1024*1024 && sizes[0]==80);
    CHECK(campaign_npc_motion_require(0,0)==RF_OK && motions[0].file.resident==data[0]);
    fclose(archive.stream);archive.stream=NULL;
    CHECK(campaign_npc_motion_reserve(80)==RF_OK && !data[0] && data[1]);
    CHECK(campaign_npc_motion_require(0,0)==RF_IO && !data[0] && data[1]);
    CHECK(!motions[0].file.resident && !motions[1].file.resident && !sizes[0]);
    CHECK(campaign_npc_motion_bytes==1024*1024-80);
    CHECK(pain_binding_check()==0);free(data[1]);CHECK(eye_binding_check()==0);
    puts("PASS: selection, aliases, pressure, reference protection, eviction, reload and failure recovery");return 0;
}
