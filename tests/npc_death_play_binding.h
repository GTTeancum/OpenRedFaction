typedef struct death_play_fixture {campaign_npc_body *owner;uint32_t calls;int failure;} death_play_fixture;
static int death_play_sound(void *context,uint32_t handle,const char *name)
{
 death_play_fixture *f=context;++f->calls;
 if(handle!=f->owner->registration.handle || strcmp(name,"death-test") || f->owner->death.action_824!=14)return RF_FORMAT;
 f->owner->view.flags_810^=0x100;return f->failure;
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
 owner->view.weapons[0]=-1;bindings->action_sounds[14][0]=0;return 0;
}
