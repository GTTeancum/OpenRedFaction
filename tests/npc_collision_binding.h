static int npc_collision_binding_check(void)
{
 campaign_npc_body owner={0};rf_entity_seeds saved=campaign_seeds;rf_level_owned_entity record={0};
 rf_entity_owned_pose owned={0};campaign_model_owner model={0};rf_entity_pose pose={0};rf_collision_pair_actor_state out,before;
 rf_movement_descriptor saved_mode=campaign_modes[3];uint32_t handle;
 campaign_npc_bodies=&owner;campaign_npc_body_count=1;campaign_seeds.records.items=&record;campaign_seeds.records.count=1;
 campaign_model_owners=&model;campaign_model_owner_count=1;
 rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
 CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration)==RF_OK);handle=owner.registration.handle;
 owner.movement_slot=3;campaign_modes[3].index=8;owner.view.linked_handle=-1;
 owner.object_flags=0x12344000;owner.view.flags_7c=0;owner.body.state.flags=0x40000020;
 owner.published[0]=12;owner.published[1]=-5;owner.published[2]=9;
 record.record.orientation[2][0]=1;record.record.orientation[2][1]=2;record.record.orientation[2][2]=3;
 memset(&out,0xa5,sizeof(out));before=out;
 CHECK(rf_scene_npc_collision_view(handle^0x10000,&out)==RF_NOT_FOUND && !memcmp(&out,&before,sizeof(out)));
 CHECK(rf_scene_npc_collision_view(handle,NULL)==RF_RANGE);
 CHECK(rf_scene_npc_collision_view(handle,&out)==RF_OK);
 CHECK(out.kind==0 && out.body_flags==0x40000020 && out.model==0 && out.movement_mode==8);
 CHECK(out.handle==handle && out.parent_handle==UINT32_MAX && out.object_flags==0x12344000);
 CHECK(!out.trigger_filter && !out.allowed_count && !out.allowed_handles);
 CHECK(!memcmp(out.position,owner.published,12) && !memcmp(out.forward,record.record.orientation[2],12));
 model.registration.loaded=1;model.pose=&pose;model.registration.active=&pose.playback.completion.active;
 model.registration.next=model.registration.previous=&model.registration;
 CHECK(rf_scene_npc_collision_view(handle,&out)==RF_OK && out.model==1);
 model.owned=&owned;CHECK(rf_scene_npc_collision_view(handle,&out)==RF_OK && out.model==0);model.owned=0;
 owner.view.linked_handle=123;owner.published[1]=18;campaign_modes[3].index=1;owner.body.state.flags=0;
 CHECK(rf_scene_npc_collision_view(handle,&out)==RF_OK && out.parent_handle==123 && out.position[1]==18 && out.movement_mode==1 && out.body_flags==0);
 before=out;model.pose=NULL;
 CHECK(rf_scene_npc_collision_view(handle,&out)==RF_RANGE && !memcmp(&out,&before,sizeof(out)));model.pose=&pose;
 owner.movement_slot=16;CHECK(rf_scene_npc_collision_view(handle,&out)==RF_RANGE && !memcmp(&out,&before,sizeof(out)));owner.movement_slot=3;
 campaign_seeds.records.count=0;CHECK(rf_scene_npc_collision_view(handle,&out)==RF_RANGE && !memcmp(&out,&before,sizeof(out)));campaign_seeds.records.count=1;
 owner.view.type=2;CHECK(rf_scene_npc_collision_view(handle,&out)==RF_NOT_FOUND && !memcmp(&out,&before,sizeof(out)));owner.view.type=0;
 CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owner.registration)==RF_OK);
 CHECK(rf_scene_npc_collision_view(handle,&out)==RF_NOT_FOUND && !memcmp(&out,&before,sizeof(out)));
 campaign_seeds=saved;campaign_modes[3]=saved_mode;campaign_npc_bodies=NULL;campaign_npc_body_count=0;
 campaign_model_owners=NULL;campaign_model_owner_count=0;return 0;
}
