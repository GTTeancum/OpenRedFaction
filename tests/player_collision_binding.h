static int player_collision_binding_check(void)
{
 rf_animation_model_view model={0};rf_model_file file={0};rf_model_bone bone={0};float matrices[1][12]={{0}};rf_motion_playback_state playback={0};
 rf_collision_pair_actor_state out,before;rf_movement_descriptor saved=campaign_modes[4];uint32_t handle;
 rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));memset(&campaign_player_view,0,sizeof(campaign_player_view));
 CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object)==RF_OK);handle=campaign_player_object.handle;
 campaign_spawn=1;campaign_player_view.flags_7c=8;campaign_player_view.linked_handle=-1;rf_scene_actor_landing[1]=4;campaign_modes[4].index=2;
 scene_actor_body.allocated_bytes=1;scene_actor_body.state.flags=0x40000020;
 scene_actor_body.state.position[0]=99;rf_scene_actor_pose.public_position[0]=17;scene_actor_body.state.orientation[6]=.5f;
 model.model=&file;model.bones=&bone;model.bone_count=1;model.matrices=(const float (*)[12])matrices;model.playback=&playback;
 memset(&out,0xa5,sizeof(out));before=out;
 CHECK(rf_scene_player_collision_view(handle,&out)==RF_RANGE && !memcmp(&out,&before,sizeof(out)));
 campaign_player_model=&model;
 CHECK(rf_scene_player_collision_view(handle^0x10000,&out)==RF_NOT_FOUND && !memcmp(&out,&before,sizeof(out)));
 CHECK(rf_scene_player_collision_view(handle,NULL)==RF_RANGE);
 CHECK(rf_scene_player_collision_view(handle,&out)==RF_OK && out.kind==0 && out.model==RF_SCENE_PLAYER_MODEL && out.body_flags==0x40000020);
 CHECK(out.handle==handle && out.parent_handle==UINT32_MAX && out.object_flags==8 && out.movement_mode==2 && out.position[0]==17 && out.forward[0]==.5f);
 CHECK(!out.trigger_filter && !out.allowed_count && !out.allowed_handles);
 {
  rf_collision_actor_general_response response,untouched;rf_collision_actor_contact contact;
  rf_physics_body saved_body,expected_body;rf_collision_contact_extra saved_extra,expected_extra;
  rf_physics_sphere spheres[2]={0};rf_entity_view saved_view=campaign_player_view;
  scene_actor_body.spheres.items=spheres;scene_actor_body.spheres.count=2;
  scene_actor_body.state.next_position[0]=123;scene_actor_body.state.mass=100;scene_actor_body.state.velocity[1]=8;
  scene_actor_body.state.bounds.radius=2;scene_actor_body.state.bounds.minimum[1]=-3;scene_actor_body.state.bounds.maximum[2]=7;
  scene_actor_body.state.next_orientation[6]=-.5f;campaign_player_material=4;
  memset(&contact,0xa5,sizeof(contact));CHECK(rf_collision_contact_write(&scene_actor_body.state,&campaign_player_contact,&contact)==RF_OK);
  saved_body=scene_actor_body;saved_extra=campaign_player_contact;
  CHECK(rf_scene_player_collision_response(handle,&response)==RF_OK);
  CHECK(!memcmp(&scene_actor_body,&saved_body,sizeof(saved_body)) && !memcmp(&campaign_player_contact,&saved_extra,sizeof(saved_extra)));
  CHECK(response.actor.position[0]==99 && response.actor.next_position[0]==123 && response.actor.mass==100 && response.actor.velocity[1]==8);
  CHECK(response.actor.material==4 && response.actor.handle==handle && response.actor.spheres==spheres && response.actor.sphere_count==2);
  CHECK(response.actor.minimum[1]==-3 && response.actor.maximum[2]==7 && response.extent==2 && response.kind==0);
  CHECK(response.orientation[6]==.5f && response.next_orientation[6]==-.5f && !memcmp(&response.actor.contact,&contact,68));
  memset(&contact,0x35,sizeof(contact));expected_body=saved_body;expected_extra=saved_extra;
  CHECK(rf_collision_contact_write(&expected_body.state,&expected_extra,&contact)==RF_OK);expected_body.state.flags=0x60000000;
  CHECK(rf_scene_player_collision_publish(handle,0x60000000,&contact)==RF_OK);
  CHECK(!memcmp(&scene_actor_body,&expected_body,sizeof(expected_body)) && !memcmp(&campaign_player_contact,&expected_extra,sizeof(expected_extra)));
  CHECK(!memcmp(&campaign_player_view,&saved_view,sizeof(saved_view)) && campaign_player_material==4);
  CHECK(rf_scene_player_collision_response(handle,&response)==RF_OK && response.actor.body_flags==0x60000000 && !memcmp(&response.actor.contact,&contact,68));
  untouched=response;saved_body=scene_actor_body;saved_extra=campaign_player_contact;
  CHECK(rf_scene_player_collision_response(handle^0x10000,&response)==RF_NOT_FOUND && !memcmp(&response,&untouched,sizeof(response)));
  CHECK(rf_scene_player_collision_publish(handle^0x10000,0,&contact)==RF_NOT_FOUND);
  CHECK(rf_scene_player_collision_publish(handle,0,NULL)==RF_RANGE && rf_scene_player_collision_response(handle,NULL)==RF_RANGE);
  campaign_player_model=NULL;
  CHECK(rf_scene_player_collision_publish(handle,0,&contact)==RF_RANGE);
  CHECK(rf_scene_player_collision_response(handle,&response)==RF_RANGE && !memcmp(&response,&untouched,sizeof(response)));campaign_player_model=&model;
  CHECK(!memcmp(&scene_actor_body,&saved_body,sizeof(saved_body)) && !memcmp(&campaign_player_contact,&saved_extra,sizeof(saved_extra)));
  scene_actor_body.spheres.items=NULL;
  CHECK(rf_scene_player_collision_response(handle,&response)==RF_RANGE && !memcmp(&response,&untouched,sizeof(response)));
  scene_actor_body.spheres.count=0;
  CHECK(rf_scene_player_collision_response(handle,&response)==RF_OK && !response.actor.spheres && !response.actor.sphere_count);
  memset(&campaign_player_contact,0,sizeof(campaign_player_contact));campaign_player_material=0;
 }
 before=out;rf_scene_actor_landing[1]=16;
 CHECK(rf_scene_player_collision_view(handle,&out)==RF_RANGE && !memcmp(&out,&before,sizeof(out)));rf_scene_actor_landing[1]=4;
 campaign_player_model=NULL;CHECK(rf_scene_player_collision_view(handle,&out)==RF_RANGE && !memcmp(&out,&before,sizeof(out)));
 CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object)==RF_OK);
 CHECK(rf_scene_player_collision_view(handle,&out)==RF_NOT_FOUND && !memcmp(&out,&before,sizeof(out)));
 campaign_spawn=0;campaign_modes[4]=saved;memset(&scene_actor_body,0,sizeof(scene_actor_body));memset(&rf_scene_actor_pose,0,sizeof(rf_scene_actor_pose));memset(rf_scene_actor_landing,0,sizeof(rf_scene_actor_landing));return 0;
}
typedef struct model_publication_test {const rf_animation_model_view *view;uint32_t calls;int stop;} model_publication_test;
static int model_publication_frame(void *context,uint32_t frame,rf_preview_mesh *mesh)
{
 model_publication_test *c=context;(void)frame;(void)mesh;
 CHECK(c->view && c->view->model && c->view->bones && c->view->bone_count && c->view->matrices && c->view->playback);
 CHECK(isfinite(c->view->matrices[0][0]));++c->calls;return c->stop;
}
static int model_publication_check(char **argv)
{
 model_publication_test c={0};rf_animation_placement placement={0};rf_level level={0};rf_level_entity entity={0};int status;
 level.player_orientation[0][0]=level.player_orientation[1][1]=level.player_orientation[2][2]=1;
 entity.orientation[0][0]=entity.orientation[1][1]=entity.orientation[2][2]=1;entity.position[2]=10;
 CHECK(rf_animation_placement_from_level(&level,&entity,&placement)==RF_OK);
 placement.published_model=&c.view;placement.frame_count=2;
 status=rf_animation_stream_placed(argv[2],argv[3],4*1024*1024,&placement,model_publication_frame,&c);
 if(status)fprintf(stderr,"publication status %d calls %u\n",status,c.calls);
 CHECK(status==RF_OK && c.calls==2 && !c.view);
 c.calls=0;c.stop=RF_FORMAT;
 status=rf_animation_stream_placed(argv[2],argv[3],4*1024*1024,&placement,model_publication_frame,&c);
 CHECK(status==RF_FORMAT && c.calls==1 && !c.view);
 c.calls=0;status=rf_animation_stream_placed("missing-model-publication.vpp",argv[3],4*1024*1024,&placement,model_publication_frame,&c);
 CHECK(status!=RF_OK && !c.calls && !c.view);
 puts("MODEL_PUBLICATION_PASS success, callback failure, open failure");return 0;
}
