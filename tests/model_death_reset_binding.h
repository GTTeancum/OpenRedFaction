static int model_death_clear_check(rf_entity_pose *pose)
{
 rf_model_bone_override expected=pose->overrides[0];rf_motion_playback_state playback=pose->playback;
 float matrix[12];uint16_t generation=pose->generations[0];memcpy(matrix,pose->matrices[0],sizeof(matrix));
 CHECK(rf_scene_model_clear_bone_override(0,1)==RF_RANGE && !memcmp(&expected,pose->overrides,sizeof(expected)));
 CHECK(rf_scene_model_clear_bone_override(1,0)==RF_RANGE && !memcmp(&expected,pose->overrides,sizeof(expected)));
 expected.enabled=0;
 CHECK(rf_scene_model_clear_bone_override(0,0)==RF_OK && !memcmp(&expected,pose->overrides,sizeof(expected)));
 CHECK(rf_scene_model_clear_bone_override(0,0)==RF_OK && !memcmp(&expected,pose->overrides,sizeof(expected)));
 CHECK(!memcmp(&playback,&pose->playback,sizeof(playback)) && !memcmp(matrix,pose->matrices[0],sizeof(matrix)) && generation==pose->generations[0]);
 {
  unsigned i;rf_motion_playback_state expected_playback;
  for(i=0;i<256;++i) {
   uint32_t bits=0x81234500u|i;memcpy(&pose->playback.completion.active.slots[15].tick,&bits,4);
   expected_playback=pose->playback;bits&=0xffffff00u;memcpy(&expected_playback.completion.active.slots[15].tick,&bits,4);
   CHECK(rf_scene_model_clear_bone_override(0,UINT32_MAX)==RF_OK);
   CHECK(!memcmp(&expected_playback,&pose->playback,sizeof(expected_playback)) && !memcmp(&expected,pose->overrides,sizeof(expected)));
   CHECK(!memcmp(matrix,pose->matrices[0],sizeof(matrix)) && generation==pose->generations[0]);
  }
  pose->playback=playback;
 }

 return 0;
}
static int model_death_reset_binding_check(void)
{
 rf_model_bone_override override;
 rf_entity_pose pose={0};rf_entity_playback_model model={0};rf_motion_playback_resource resources[3]={{0}};
 rf_motion_playback_state expected,saved;unsigned i;float matrices[1][12]={{0}},position[3]={0},basis[9]={1,0,0,0,1,0,0,0,1};uint16_t generations[1]={0};
 campaign_poses.items=&pose;campaign_poses.count=1;pose.skeleton=0;pose.bone_count=1;pose.matrices=matrices;pose.generations=generations;
 model.resources=resources;model.count=3;campaign_playback_resources.models=&model;campaign_playback_resources.model_count=1;
 rf_motion_playback_initialize(&pose.playback);
 pose.playback.completion.active.count=3;pose.playback.completion.active.primary_slot=1;pose.playback.completion.active.freeze_slot=2;pose.playback.completion.frozen=1;
 for(i=0;i<3;++i){resources[i].looping=i;resources[i].references=1;pose.playback.completion.active.slots[i].motion=i;pose.playback.completion.active.slots[i].weight=(float)(i+1);}
 CHECK(campaign_models_open()==RF_OK);expected=pose.playback;
 CHECK(rf_scene_model_clear_bone_override(0,0)==RF_NOT_FOUND);
 memset(&override,0x5a,sizeof(override));pose.overrides=&override;generations[0]=37;matrices[0][9]=123;
 CHECK(model_death_clear_check(&pose)==0);
 override.enabled=255;
 expected.completion.active.primary_slot=expected.completion.active.freeze_slot=-1;expected.completion.frozen=0;expected.completion.active.slots[0].weight=0;
 CHECK(rf_scene_model_stop_nonlooping(0)==RF_OK && !memcmp(&pose.playback,&expected,sizeof(expected)));
 for(i=0;i<3;++i)CHECK(resources[i].references==1);
 pose.playback.completion.active.slots[1].motion=-1;saved=pose.playback;
 CHECK(rf_scene_model_stop_nonlooping(0)==RF_FORMAT && !memcmp(&pose.playback,&saved,sizeof(saved)));
 pose.playback=expected;
 CHECK(rf_scene_model_stop_nonlooping(1)==RF_RANGE && !memcmp(&pose.playback,&expected,sizeof(expected)));
 /* The model registry must follow the transferred pose, not its old actor slot. */
 pose.playback.completion.active.slots[0].weight=3;pose.playback.completion.active.primary_slot=1;
 pose.playback.completion.active.freeze_slot=2;pose.playback.completion.frozen=1;
 CHECK(rf_scene_model_detach(0,position,basis,0)==RF_OK && campaign_model_owners[0].owned);
 memset(&pose.playback,0xa5,sizeof(pose.playback));saved=pose.playback;
 memset(&override,0xa5,sizeof(override));
 CHECK(campaign_model_owners[0].pose->overrides[0].enabled==255);
 CHECK(model_death_clear_check(campaign_model_owners[0].pose)==0);
 for(i=0;i<sizeof(override);++i)CHECK(((unsigned char *)&override)[i]==0xa5);
 CHECK(rf_scene_model_stop_nonlooping(0)==RF_OK);
 CHECK(!memcmp(&campaign_model_owners[0].pose->playback,&expected,sizeof(expected)) && !memcmp(&pose.playback,&saved,sizeof(saved)));
 for(i=0;i<3;++i)CHECK(resources[i].references==1);
 CHECK(rf_scene_model_retire(0)==RF_OK);
 CHECK(rf_scene_model_stop_nonlooping(0)==RF_NOT_FOUND);
 CHECK(rf_scene_model_clear_bone_override(0,0)==RF_NOT_FOUND);
 for(i=0;i<3;++i)CHECK(resources[i].references==0);
 campaign_models_close();memset(&campaign_poses,0,sizeof(campaign_poses));memset(&campaign_playback_resources,0,sizeof(campaign_playback_resources));return 0;
}
