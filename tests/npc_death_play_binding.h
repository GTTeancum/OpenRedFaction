typedef struct death_play_fixture {campaign_npc_body *owner;uint32_t calls;int failure;} death_play_fixture;
static int death_play_sound(void *context,uint32_t handle,const char *name)
{
 death_play_fixture *f=context;++f->calls;
 if(handle!=f->owner->registration.handle || strcmp(name,"death-test") || f->owner->death.action_824!=14)return RF_FORMAT;
 f->owner->view.flags_810^=0x100;return f->failure;
}
static int death_motion_select(void *context,uint32_t handle,int32_t *action)
{
 death_play_fixture *f=context;if(handle!=f->owner->registration.handle)return RF_FORMAT;
 ++f->calls;f->owner->view.flags_810|=0x200;*action=14;return f->failure;
}
static int death_play_binding_check(campaign_npc_body *owner,rf_entity_state_set *bindings)
{
 rf_entity_pose *pose=campaign_model_owners[0].pose;rf_motion_playback_state saved;death_play_fixture f={owner,0,0};
 uint32_t handle=owner->registration.handle;int32_t refs,freeze_slot;
 campaign_motion_catalog.mapping_count=1;campaign_motion_catalog.mappings[0].weapon=-1;
 campaign_motion_catalog.mappings[0].actions[14]=2;owner->death.action_824=-1;
 saved=pose->playback;
 CHECK(rf_scene_npc_death_play(handle^0x10000,14,1,NULL,NULL)==RF_NOT_FOUND && owner->death.action_824==-1 && !memcmp(&saved,&pose->playback,sizeof(saved)));
 CHECK(rf_scene_npc_death_play(handle,15,1,NULL,NULL)==RF_NOT_FOUND && owner->death.action_824==-1);
 CHECK(rf_scene_npc_death_play(handle,14,1,NULL,NULL)==RF_OK && owner->death.action_824==14);
 CHECK(pose->playback.completion.active.freeze_slot>=0 && pose->playback.completion.active.slots[pose->playback.completion.active.freeze_slot].motion==2);
 refs=campaign_playback_resources.models[0].resources[2].references;
 freeze_slot=pose->playback.completion.active.freeze_slot;
 /* A restart without freeze does not clear a prior freeze designation. */
 CHECK(rf_scene_npc_death_play(handle,14,256,NULL,NULL)==RF_OK && pose->playback.completion.active.freeze_slot==freeze_slot);
 CHECK(rf_scene_model_stop_nonlooping(0)==RF_OK && pose->playback.completion.active.freeze_slot==-1);
 CHECK(rf_scene_npc_death_play(handle,14,256,NULL,NULL)==RF_OK && pose->playback.completion.active.freeze_slot==-1);
 CHECK(rf_scene_npc_death_play(handle,14,257,NULL,NULL)==RF_OK && pose->playback.completion.active.freeze_slot==freeze_slot);
 CHECK(campaign_playback_resources.models[0].resources[2].references==refs);
 strcpy(bindings->action_sounds[14],"death-test");
 CHECK(rf_scene_npc_death_play(handle,14,1,NULL,NULL)==RF_NOT_FOUND && owner->death.action_824==14);
 f.failure=RF_IO;CHECK(rf_scene_npc_death_play(handle,14,1,death_play_sound,&f)==RF_IO && f.calls==1);
 f.failure=0;CHECK(rf_scene_npc_death_play(handle,14,1,death_play_sound,&f)==RF_OK && f.calls==2);
 owner->view.weapons[0]=2;saved=pose->playback;
 CHECK(rf_scene_npc_death_play(handle,14,1,death_play_sound,&f)==RF_NOT_FOUND && f.calls==2 && !memcmp(&saved,&pose->playback,sizeof(saved)));
 owner->view.weapons[0]=-1;bindings->action_sounds[14][0]=0;
 {
  rf_entity_seed_class cls={0};rf_entity_seed_class *saved_classes=campaign_seeds.classes;
  uint32_t saved_count=campaign_seeds.class_count,index=0,saved_bone_count=pose->bone_count;rf_entity_skeleton skeleton={0};rf_entity_skeletons saved_skeletons=campaign_skeletons;
  rf_model_bone bones[50]={{0}};rf_model_bone_override overrides[50],*old_overrides=pose->overrides;
  rf_scene_death_motion_ops ops={0};rf_motion_playback_state before;uint32_t bits=0x12345678;
  cls.model_kind=2;cls.physics.flags=0x20000;campaign_seeds.classes=&cls;campaign_seeds.class_count=1;
  pose->bone_count=1;skeleton.bones=bones;skeleton.count=pose->bone_count;strcpy(bones[0].name,"spinehead");bones[0].parent=-1;
  campaign_skeletons.items=&skeleton;campaign_skeletons.count=campaign_skeletons.class_count=1;campaign_skeletons.class_indices=&index;
  memset(overrides,0x5a,sizeof(overrides));pose->overrides=overrides;
  owner->view.flags_810=0;owner->death.requested_83c=14;owner->death.action_824=-1;
  owner->death_bone_words[0]=123;owner->death_bone_words[1]=456;
  memcpy(&pose->playback.completion.active.slots[15].tick,&bits,4);
  CHECK(rf_scene_npc_death_motion(handle,NULL)==RF_OK);
  CHECK(owner->death.action_824==14 && owner->view.flags_810==8 && owner->damage.effects.flags_810==8);
  CHECK(!owner->death_bone_words[0] && !owner->death_bone_words[1] && !overrides[0].enabled);
  CHECK(pose->playback.completion.active.slots[15].tick==0x12345600 && pose->playback.completion.active.freeze_slot>=0);
  cls.physics.flags|=0x200000;owner->view.flags_810=0;
  CHECK(rf_scene_npc_death_motion(handle,NULL)==RF_OK && owner->view.flags_810==0x02000000 && pose->playback.completion.active.freeze_slot==-1);
  cls.physics.flags&=~0x200000u;owner->view.flags_810=0;strcpy(bindings->action_sounds[14],"death-test");
  ops.sound=death_play_sound;ops.context=&f;f.failure=0;
  CHECK(rf_scene_npc_death_motion(handle,&ops)==RF_OK && owner->view.flags_810==0x108 && owner->damage.effects.flags_810==0x108);
  owner->view.weapons[0]=2;before=pose->playback;
  CHECK(rf_scene_npc_death_motion(handle,&ops)==RF_NOT_FOUND && !memcmp(&before,&pose->playback,sizeof(before)));
  owner->view.weapons[0]=-1;owner->death.requested_83c=-1;
  CHECK(rf_scene_npc_death_motion(handle,NULL)==RF_NOT_FOUND && !memcmp(&before,&pose->playback,sizeof(before)));
  CHECK(rf_scene_npc_death_motion(handle^0x10000,&ops)==RF_NOT_FOUND);
  bindings->action_sounds[14][0]=0;ops.select=death_motion_select;owner->view.flags_810=0;
  CHECK(rf_scene_npc_death_motion(handle,&ops)==RF_OK && owner->view.flags_810==0x208 && owner->death.action_824==14);
  before=pose->playback;f.failure=RF_IO;owner->view.flags_810=0;
  CHECK(rf_scene_npc_death_motion(handle,&ops)==RF_IO && owner->view.flags_810==0x200 && owner->damage.effects.flags_810==0x200);
  CHECK(!memcmp(&before,&pose->playback,sizeof(before)) && owner->death.action_824==14);f.failure=0;

  bindings->action_sounds[14][0]=0;pose->overrides=old_overrides;pose->bone_count=saved_bone_count;
  campaign_seeds.classes=saved_classes;campaign_seeds.class_count=saved_count;campaign_skeletons=saved_skeletons;
 }
 return 0;
}
