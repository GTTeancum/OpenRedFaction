uint32_t rf_scene_fragment_edge_audit[32];
static void scene_fragment_edge_record(uint32_t row,const rf_geometry_body_hit *hit) {
    uint32_t *w=rf_scene_fragment_edge_audit+4+row*7;w[0]=1;memcpy(w+1,&hit->contact.fraction,4);
    memcpy(w+2,hit->contact.normal,12);w[5]=hit->solid==UINT32_MAX?hit->face:hit->contact.object_id;w[6]=hit->contact.material;
}
static int scene_fragment_edge_check(void) {
    rf_geomod_vertex vertices[4]={{{-1,-.5f,-.25f},{0,0}},{{1,-.5f,-.25f},{0,0}},{{1,-.5f,.25f},{0,0}},{{-1,-.5f,.25f},{0,0}}};
    rf_geomod_face polygon={0,4,0,0};rf_geomod_mesh_view mesh={vertices,&polygon,4,1,0};
    float patch[4][3]={{-.25f,0,-1},{-.25f,0,1},{.25f,0,1},{.25f,0,-1}};
    rf_collision_face face={0};rf_geometry_collision_room room={0};rf_collision_room_view view={0};
    rf_geometry_collision_world world={0};rf_physics_body_state body={0};scene_detached_query_context query={0};
    rf_geometry_collision_flat flat={0};rf_group_attached_pose pose={0};rf_collision_solid_view mover_view={0};
    rf_geometry_collision_movers movers={0};rf_geometry_body_hit hit={0},saved;
    scene_stream *scene=NULL;uint32_t i,mode,primary=0,found=0;int status=RF_OK,control;
#define EDGE_CALL(x) do{status=(x);if(status)goto done;}while(0)
#define EDGE_TEST(x) do{if(!(x)){status=RF_FORMAT;goto done;}}while(0)
    memset(rf_scene_fragment_edge_audit,0,sizeof(rf_scene_fragment_edge_audit));rf_scene_fragment_edge_audit[0]=1;
    scene=calloc(1,sizeof(*scene));if(!scene){status=RF_IO;goto done;}
    face.vertices=patch;face.count=4;face.plane[1]=1;face.minimum[0]=-.25f;face.maximum[0]=.25f;face.minimum[2]=-1;face.maximum[2]=1;
    for(i=0;i<3;i++){view.minimum[i]=-5;view.maximum[i]=5;body.orientation[i*4]=body.next_orientation[i*4]=1;}
    body.position[1]=1;body.next_position[1]=-1;body.bounds.radius=.001f;
    EDGE_CALL(rf_collision_tree_open(&face,1,4096,&room.tree));room.tree.source_indices[0]=17;view.tree=&room.tree;
    world.rooms=&room;world.views=&view;world.primary=&primary;world.primary_count=world.room_count=1;
    scene->collision=&world;query.scene=scene;query.mesh=&mesh;
    EDGE_CALL(scene_detached_world_vertex_sweep(&query,&body,&hit,&found,scene_fragment_audit_material,NULL));
    EDGE_TEST(found && hit.contact.fraction==.25f && hit.contact.normal[1]==1 && hit.face==17 && hit.contact.material==3);
    scene_fragment_edge_record(0,&hit);
    flat.faces=&face;flat.count=1;movers.count=1;movers.owned=&flat;movers.views=&mover_view;movers.poses=&pose;
    mover_view.object_id=77;pose.position[0]=3;pose.position[1]=2;
    for(i=0;i<3;i++){pose.minimum[i]=-10;pose.maximum[i]=10;pose.velocity[i]=(float)(i+1);pose.public_position[i]=99;pose.output_matrix[i*4]=-1;}
    for(mode=0;mode<2;mode++) {
        memset(pose.input_matrix,0,36);
        if(!mode)for(i=0;i<3;i++)pose.input_matrix[i*4]=1;
        else {pose.input_matrix[1]=1;pose.input_matrix[3]=-1;pose.input_matrix[8]=1;}
        memcpy(body.orientation,pose.input_matrix,36);memcpy(body.next_orientation,pose.input_matrix,36);
        for(i=0;i<3;i++){body.position[i]=pose.position[i]+pose.input_matrix[3+i];body.next_position[i]=pose.position[i]-pose.input_matrix[3+i];}
        found=0;EDGE_CALL(scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&hit,&found,scene_fragment_audit_material,NULL));
        EDGE_TEST(found && hit.contact.fraction==.25f && hit.contact.object_id==77 && hit.contact.material==7);
        EDGE_TEST(!memcmp(hit.contact.velocity,pose.velocity,12));EDGE_TEST(mode?hit.contact.normal[0]==-1:hit.contact.normal[1]==1);
        scene_fragment_edge_record(1+mode,&hit);
    }
    found=0;saved=hit;control=scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&hit,&found,scene_fragment_audit_material,&found);
    EDGE_TEST(control==RF_IO && !found && !memcmp(&hit,&saved,sizeof(hit)));
    rf_scene_fragment_edge_audit[25]=(uint32_t)control;rf_scene_fragment_edge_audit[26]=1;
    rf_scene_fragment_edge_audit[1]=4;rf_scene_fragment_edge_audit[3]=1;
 done:rf_collision_tree_close(&room.tree);free(scene);rf_scene_fragment_edge_audit[2]=(uint32_t)status;
#undef EDGE_CALL
#undef EDGE_TEST
    return status;
}
