static int unholster_test_sound(void *context,uint32_t handle,const char *name)
{
 campaign_npc_body *owner=context;
 if(handle!=owner->registration.handle || strcmp(name,"unholster-test") || owner->death.action_824!=40)return RF_FORMAT;
 owner->view.flags_810|=0x200;return RF_IO;
}
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
  {
   campaign_npc_body kept=*owner;rf_motion_playback_state kept_pose=pose->playback,started;
   int32_t old40=campaign_motion_catalog.mappings[0].actions[40];
   campaign_motion_catalog.mappings[0].actions[40]=2;cls.unholster_delay=.33f;owner->view.flags_810=0;owner->view.flags_7d0=0x100;
   owner->view.action_520=3;owner->pain.ai_timer=0;owner->unholster.deadline_518=17;
   CHECK(rf_scene_npc_recover_unholster(handle^0x10000,1000,NULL,NULL)==RF_NOT_FOUND);
   CHECK(rf_scene_npc_recover_unholster(handle,-1,NULL,NULL)==RF_RANGE);
   CHECK(rf_scene_npc_recover_unholster(handle,1000,NULL,NULL)==RF_OK);
   CHECK(owner->death.action_824==40 && owner->pain.ai_timer==1500 && owner->unholster.deadline_518==1330);
   started=pose->playback;CHECK(rf_scene_npc_recover_unholster(handle,1000,NULL,NULL)==RF_OK && !memcmp(&started,&pose->playback,sizeof(started)));
   owner->view.action_520=13;owner->view.linked_handle=(int32_t)handle;owner->damage.effects.health=1;owner->unholster.stance_7bc=1;owner->pain.ai_timer=0;
   CHECK(rf_scene_npc_recover_unholster(handle,1000,NULL,NULL)==RF_OK && owner->unholster.stance_7bc==1 && owner->pain.ai_timer==0);
   owner->damage.effects.health=0;campaign_motion_catalog.mappings[0].actions[40]=-1;
   CHECK(rf_scene_npc_recover_unholster(handle,1000,NULL,NULL)==RF_OK && owner->unholster.stance_7bc==0 && owner->pain.ai_timer==0);
   campaign_motion_catalog.mappings[0].actions[40]=2;owner->view.action_520=3;strcpy(bindings->action_sounds[40],"unholster-test");
   owner->unholster.deadline_518=17;
   CHECK(rf_scene_npc_recover_unholster(handle,1000,unholster_test_sound,owner)==RF_IO);
   CHECK(owner->death.action_824==40 && owner->pain.ai_timer==0 && owner->unholster.deadline_518==17 && (owner->view.flags_810&0x200));
   bindings->action_sounds[40][0]=0;campaign_motion_catalog.mappings[0].actions[40]=old40;*owner=kept;pose->playback=kept_pose;
  }

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

  {
   rf_geometry_collision_world world={0};rf_random_state rng={7},reference=rng;uint32_t draw,old_spawn=campaign_spawn;
   rf_scene_death_selection_context selection={&world,NULL,0,&rng};int32_t selected=-99;static const int32_t choices[3]={5,14,15};
   campaign_motion_catalog.mappings[0].actions[5]=campaign_motion_catalog.mappings[0].actions[15]=campaign_motion_catalog.mappings[0].actions[16]=2;
   owner->view.flags_810=0;owner->death.action_824=-1;pose->controller.current=0;pose->controller.next=-1;
   rf_random_next(&reference,&draw);
   CHECK(rf_scene_npc_death_select(&selection,handle,&selected)==RF_OK && selected==choices[draw%3] && rng.value==reference.value && owner->death.action_824==-1);
   pose->controller.next=13;reference=rng;
   CHECK(rf_scene_npc_death_select(&selection,handle,&selected)==RF_OK && selected==16 && rng.value==reference.value);
   pose->controller.next=-1;pose->controller.current=13;
   CHECK(rf_scene_npc_death_select(&selection,handle,&selected)==RF_OK && selected==16 && rng.value==reference.value);
   pose->controller.current=0;owner->view.flags_810=0x400;
   CHECK(rf_scene_npc_death_select(&selection,handle,&selected)==RF_OK && selected==16 && rng.value==reference.value);
   owner->view.flags_810=0;owner->death.action_824=6;campaign_spawn=0;selected=-99;
   CHECK(rf_scene_npc_death_select(&selection,handle,&selected)==RF_NOT_FOUND && selected==-99 && rng.value==reference.value);
   CHECK(rf_scene_npc_death_select(&selection,handle^0x10000,&selected)==RF_NOT_FOUND && selected==-99 && rng.value==reference.value);
   campaign_spawn=old_spawn;owner->death.action_824=-1;owner->death.requested_83c=-1;
   ops.select=rf_scene_npc_death_select;ops.sound=NULL;ops.context=&selection;
   CHECK(rf_scene_npc_death_motion(handle,&ops)==RF_OK && owner->death.action_824>=0 && (owner->view.flags_810&8));
   {
    rf_foley_group group={0};int32_t samples[2]={-1,-1};rf_foley_owner old_foley=campaign_foley;uint32_t first_draw;
    strcpy(group.name,"death-test");group.count=2;campaign_foley.groups=&group;campaign_foley.group_count=1;campaign_foley.samples=samples;campaign_foley.sample_count=2;
    rng.value=7;reference=rng;rf_random_next(&reference,&first_draw);rf_random_next(&reference,&draw);
    strcpy(bindings->action_sounds[5],"death-test");strcpy(bindings->action_sounds[14],"death-test");strcpy(bindings->action_sounds[15],"death-test");
    owner->death.action_824=-1;owner->view.flags_810=0;ops.sound=rf_scene_npc_death_sound;
    CHECK(rf_scene_npc_death_motion(handle,&ops)==RF_OK && owner->death.action_824==choices[first_draw%3] && rng.value==reference.value);
    CHECK(rf_scene_npc_death_sound(&selection,handle,"missing")==RF_OK && rng.value==reference.value);
    CHECK(rf_scene_npc_death_sound(&selection,handle^0x10000,"death-test")==RF_NOT_FOUND && rng.value==reference.value);
    group.count=0;CHECK(rf_scene_npc_death_sound(&selection,handle,"death-test")==RF_FORMAT && rng.value==reference.value);
    bindings->action_sounds[5][0]=bindings->action_sounds[14][0]=bindings->action_sounds[15][0]=0;campaign_foley=old_foley;
   }

  }
  bindings->action_sounds[14][0]=0;pose->overrides=old_overrides;pose->bone_count=saved_bone_count;
  campaign_seeds.classes=saved_classes;campaign_seeds.class_count=saved_count;campaign_skeletons=saved_skeletons;
 }
 return 0;
}
