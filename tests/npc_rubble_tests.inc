/* CPU scene-adapter fixture. No renderer or live encounter claim. */
static int npc_rubble_movement_check(void)
{
    rf_geomod_vertex vertices[24]={0};rf_geomod_face faces[6];uint32_t map[6];
    rf_geomod_mesh_view mesh={vertices,faces,24,6,0};rf_collision_face_filter filters[6]={{0}},generated={0};
    rf_geometry_collision_world world={0};scene_stream stream={0};rf_scene_world_geometry follow={0};
    rf_surface_materials palette={0};const rf_geometry *source=NULL;
    campaign_npc_body owner={0};rf_entity_seed seed={0};rf_entity_seed_class cls={0};
    rf_physics_sphere sphere={0};rf_geomod_piece_batch *batch;rf_physics_body *fragment;rf_geomod_owned_piece piece;
    rf_physics_body_state proposal,saved;rf_collision_body_sphere scratch[1];
    rf_geometry_body_hit hit={0},kept;uint32_t found,k;const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};
    for(uint32_t axis=0;axis<3;axis++)for(uint32_t side=0;side<2;side++) {
        uint32_t f=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;faces[f]=(rf_geomod_face){f*4,4,7,f};map[f]=f;
        for(uint32_t j=0;j<4;j++){uint32_t n=side?j:3-j;vertices[f*4+j].position[axis]=side?1:-1;
            vertices[f*4+j].position[a]=(float)u[n];vertices[f*4+j].position[b]=(float)w[n];}
    }
    CHECK(!rf_geomod_piece_registry_open(&generated,7,2.5f,.5f,.25f,0,2097152,&stream.detached_pieces));
    CHECK(!rf_geomod_piece_registry_begin(stream.detached_pieces,0));
    CHECK(!rf_geomod_piece_registry_emit(&mesh,map,filters,6,1,0,stream.detached_pieces));
    rf_geomod_piece_registry_commit(stream.detached_pieces);
    CHECK(!rf_geomod_piece_registry_get(stream.detached_pieces,0,&batch));
    CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,&fragment));
    stream.collision=&world;scene_actor_collision_owner=&stream;actor_follow_world=&follow;
    campaign_surface_palette=&palette;campaign_surface_sources=&source;
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;campaign_seeds.items=&seed;
    campaign_seeds.classes=&cls;campaign_seeds.class_count=campaign_seeds.records.count=1;cls.physics.use_kind=1;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));
    sphere.radius=.1f;owner.body.spheres.items=&sphere;owner.body.spheres.count=1;owner.body.allocated_bytes=sizeof(sphere);
    owner.body.state.mass=10;owner.body.state.flags=0x8000003f;owner.body.state.bounds.radius=.1f;
    for(k=0;k<3;k++) {
        owner.body.state.orientation[k*3+k]=owner.body.state.next_orientation[k*3+k]=1;
        owner.body.state.position[k]=owner.body.state.next_position[k]=fragment->state.position[k]+fragment->spheres.items[0].center[k];
    }
    owner.body.state.position[1]+=4;owner.body.state.next_position[1]-=4;proposal=owner.body.state;saved=fragment->state;
    CHECK(!rf_scene_npc_body_sweep(&world,owner.registration.handle,&proposal,0x460,scratch,1,&hit,&found));
    CHECK(found && hit.contact.fraction>0 && hit.contact.fraction<1 && hit.contact.normal[1]>0);
    CHECK(hit.solid==UINT32_MAX && hit.contact.object_id==UINT32_MAX && hit.hits==1);
    CHECK(!memcmp(&saved,&fragment->state,sizeof(saved)) && !memcmp(&proposal,&owner.body.state,sizeof(proposal)));
    kept=hit;cls.physics.use_kind=0;
    CHECK(!rf_scene_npc_body_sweep(&world,owner.registration.handle,&proposal,0x460,scratch,1,&hit,&found));
    CHECK(!found && !memcmp(&hit,&kept,sizeof(hit)));
    cls.physics.use_kind=1;proposal.position[0]+=20;proposal.next_position[0]+=20;
    CHECK(!rf_scene_npc_body_sweep(&world,owner.registration.handle,&proposal,0x460,scratch,1,&hit,&found));
    CHECK(!found && !memcmp(&hit,&kept,sizeof(hit)));
    proposal=owner.body.state;
    CHECK(!rf_geomod_piece_registry_damage(stream.detached_pieces,0,0,10000));
    CHECK(!rf_scene_npc_body_sweep(&world,owner.registration.handle,&proposal,0x460,scratch,1,&hit,&found));
    CHECK(!found && !memcmp(&hit,&kept,sizeof(hit)));
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owner.registration));
    rf_geomod_piece_registry_close(&stream.detached_pieces);
    scene_actor_collision_owner=NULL;actor_follow_world=NULL;campaign_surface_palette=NULL;campaign_surface_sources=NULL;
    campaign_npc_bodies=NULL;campaign_npc_body_count=0;memset(&campaign_seeds,0,sizeof(campaign_seeds));
    puts("PASS NPC scene rubble crossing, class rejection, miss, retirement, and owner preservation");return 0;
}
