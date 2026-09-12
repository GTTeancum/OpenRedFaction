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
