static int actor_pair_binding_check(void)
{
 campaign_npc_body owners[2]={{0}};rf_level_owned_entity records[2]={{0}};campaign_model_owner models[2]={{0}};
 rf_entity_seeds saved_seeds=campaign_seeds;rf_physics_sphere spheres[3]={{0}};
 rf_animation_model_view model={0};rf_model_file file={0};rf_model_bone bone={0};float matrices[1][12]={{0}};rf_motion_playback_state playback={0};
 uint32_t handles[3],route,normal,scenario,i,changed;rf_movement_descriptor saved_mode=campaign_modes[0];
 campaign_npc_bodies=owners;campaign_npc_body_count=2;campaign_model_owners=models;campaign_model_owner_count=2;
 campaign_seeds.records.items=records;campaign_seeds.records.count=2;
 rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
 for(i=0;i<2;++i){CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owners[i].view,&owners[i].registration)==RF_OK);handles[i]=owners[i].registration.handle;}
 memset(&campaign_player_view,0,sizeof(campaign_player_view));
 CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object)==RF_OK);handles[2]=campaign_player_object.handle;
 campaign_spawn=1;campaign_player_view.flags_7c=8;rf_scene_actor_landing[1]=0;campaign_modes[0].index=1;
 model.model=&file;model.bones=&bone;model.bone_count=1;model.matrices=(const float (*)[12])matrices;model.playback=&playback;campaign_player_model=&model;
 for(route=0;route<3;++route)for(normal=0;normal<2;++normal)for(scenario=0;scenario<3;++scenario) {
  uint32_t ids[2]={route==1?2:0,route==2?2:1},expected_changed;
  rf_collision_actor_general_response expected[2],actual[2];rf_physics_body *body[2];rf_collision_contact_extra *extra[2];
  for(i=0;i<2;++i) {
   uint32_t id=ids[i],k;body[i]=id==2?&scene_actor_body:&owners[id].body;extra[i]=id==2?&campaign_player_contact:&owners[id].collision_contact;
   memset(body[i],0,sizeof(*body[i]));memset(extra[i],0x35,sizeof(*extra[i]));
   body[i]->allocated_bytes=sizeof(*body[i])+sizeof(spheres[id]);body[i]->spheres.count=1;body[i]->spheres.items=spheres+id;spheres[id].radius=1;
   body[i]->state.mass=2+i;body[i]->state.bounds.radius=1;body[i]->state.scalar_144=(scenario==2 && i==1)?0:1;
   body[i]->state.flags=(scenario==2 && i==0)?0x40000000:0;
   for(k=0;k<3;++k){body[i]->state.bounds.minimum[k]=-10;body[i]->state.bounds.maximum[k]=10;body[i]->state.orientation[k*4]=body[i]->state.next_orientation[k*4]=1;}
   body[i]->state.position[2]=i?2.5f:0;body[i]->state.next_position[2]=i?-1.5f:0;
   if(scenario==1 && i==1)body[i]->state.bounds.minimum[0]=10;
   if(id==2){campaign_player_material=4;campaign_support_velocity[0]=.25f;}
   else {owners[id].collision_material=3+id;owners[id].support_velocity[0]=.5f+id;}
   CHECK((id==2?rf_scene_player_collision_response(handles[id],expected+i):rf_scene_npc_collision_response(handles[id],expected+i))==RF_OK);
  }
  expected_changed=normal?rf_collision_actors_normal_response(&expected[0].actor,&expected[1].actor,rf_scene_collision_extra_velocity,NULL):
      rf_collision_actors_general_response(expected,expected+1,rf_scene_collision_extra_velocity,NULL);
  changed=123;CHECK(rf_scene_actor_pair_response(handles[ids[0]],handles[ids[1]],normal,&changed)==RF_OK && changed==expected_changed);
  for(i=0;i<2;++i) {
   CHECK((ids[i]==2?rf_scene_player_collision_response(handles[ids[i]],actual+i):rf_scene_npc_collision_response(handles[ids[i]],actual+i))==RF_OK);
   CHECK(!memcmp(actual+i,expected+i,sizeof(actual[i])));
  }
  if(scenario==0)CHECK(changed==1 && actual[0].actor.contact.handle==handles[ids[1]] && actual[1].actor.contact.handle==handles[ids[0]]);
  if(scenario==1)CHECK(changed==0);
  if(scenario==2 && !normal)CHECK(changed==0 && actual[0].actor.body_flags==0x60000000 && actual[0].actor.contact.time==0);
  {rf_physics_body saved[2]={*body[0],*body[1]};rf_collision_contact_extra saved_extra[2]={*extra[0],*extra[1]};changed=123;
   CHECK(rf_scene_actor_pair_response(handles[ids[0]],handles[ids[1]]^0x10000,normal,&changed)==RF_NOT_FOUND && changed==123);
   CHECK(rf_scene_actor_pair_response(handles[ids[0]],handles[ids[0]],normal,&changed)==RF_RANGE);
   CHECK(rf_scene_actor_pair_response(handles[ids[0]],handles[ids[1]],2,&changed)==RF_RANGE);
   CHECK(rf_scene_actor_pair_response(handles[ids[0]],handles[ids[1]],normal,NULL)==RF_RANGE);
   for(i=0;i<2;++i)CHECK(!memcmp(body[i],saved+i,sizeof(saved[i])) && !memcmp(extra[i],saved_extra+i,sizeof(saved_extra[i])));
  }
 }
 for(i=0;i<2;++i)CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owners[i].registration)==RF_OK);
 CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object)==RF_OK);
 campaign_npc_bodies=NULL;campaign_npc_body_count=0;campaign_model_owners=NULL;campaign_model_owner_count=0;campaign_seeds=saved_seeds;
 campaign_spawn=0;campaign_player_model=NULL;campaign_modes[0]=saved_mode;memset(&scene_actor_body,0,sizeof(scene_actor_body));
 memset(&campaign_player_contact,0,sizeof(campaign_player_contact));memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));campaign_player_material=0;
 return 0;
}
