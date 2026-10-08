/* First ordinary weapon-driven campaign cut: L1S1's paired opening wall.
 * This is a bounded one-cut owner. Both rooms are staged before either becomes
 * visible to simulation, and both trees remain resident until level close. */
typedef struct scene_campaign_wall_room {
    rf_geomod_vertex vertices[4096];rf_geomod_face faces[SCENE_TERRAIN_FACES];
    rf_collision_face_filter filters[SCENE_TERRAIN_FACES];
    rf_collision_face bound[SCENE_TERRAIN_FACES];
    float positions[4096][3],colors[4096][3];
    uint32_t face_ids[SCENE_TERRAIN_FACES];
    rf_geomod_mesh_view mesh;rf_collision_tree tree;uint32_t retained_faces;
} scene_campaign_wall_room;
struct scene_campaign_wall_owner {
    scene_campaign_wall_room rooms[2];
    rf_geometry_collision_overlay overlay[2];
    float center[3],radius;uint32_t active;
};
uint32_t rf_scene_campaign_wall[8]; /* attempts, commits, faces40, faces27, vertices40, vertices27, tree bytes, status */
static void scene_campaign_wall_close(scene_campaign_wall_owner **ptr)
{
    if(!ptr || !*ptr)return;
    rf_geometry_collision_overlay_close(&(*ptr)->overlay[1]);
    rf_geometry_collision_overlay_close(&(*ptr)->overlay[0]);
    rf_collision_tree_close(&(*ptr)->rooms[1].tree);
    rf_collision_tree_close(&(*ptr)->rooms[0].tree);
    free(*ptr);*ptr=NULL;
}
static void scene_campaign_wall_refresh_script_room(scene_stream *s)
{
    uint32_t i;scene_campaign_wall_owner *owner=s->campaign_wall;
    if(!owner || !owner->active)return;
    campaign_dry_sweep_invalidate();
    for(i=0;i<2;i++){
        rf_geometry_collision_world *world=&owner->overlay[i].world;
        world->rooms[28]=s->terrain_collision.world.rooms[28];
        world->views[28]=s->terrain_collision.world.views[28];
        world->views[28].tree=&world->rooms[28].tree;
        world->liquids[28]=s->terrain_collision.world.liquids[28];
    }
}
static rf_geometry_collision_world *scene_campaign_wall_collision(scene_stream *s)
{
    return s->campaign_wall && s->campaign_wall->active?
        &s->campaign_wall->overlay[1].world:&s->terrain_collision.world;
}
static int scene_campaign_wall_checkpoint_get(const scene_stream *s,float center[3],float *radius)
{
    if(!s || !center || !radius)return RF_RANGE;
    if(!s->campaign_wall || !s->campaign_wall->active)return RF_NOT_FOUND;
    memcpy(center,s->campaign_wall->center,3*sizeof(float));
    *radius=s->campaign_wall->radius;return RF_OK;
}
static int scene_campaign_wall_cut(scene_stream *s,const float center[3],float radius)
{
    static const uint32_t room_ids[2]={40,27},brush_ids[2]={8715,8747};
    static const float basis[9]={1,0,0,0,1,0,0,0,1};
    scene_campaign_wall_owner *owner=NULL;
    scene_campaign_geomod_scratch *scratch=NULL;
    rf_editor_brush air[2]={{0}};
    rf_geomod_solid_clip_source sources[2];rf_geomod_solid_union_work work={0};
    rf_geomod_mesh_view cutter,wall,retained;rf_geomod_campaign_room_scope scope;
    uint32_t *offsets=NULL,count=0,i,j;float kernel[3];int status=RF_RANGE;
    if(!s || !s->campaign_geomod || !s->geometry || !s->terrain_face_offsets ||
       !s->terrain_collision.storage || !center || !isfinite(radius) || radius<1 || radius>5 ||
       !isfinite(center[0]) || !isfinite(center[1]) || !isfinite(center[2]))return RF_RANGE;
    if(fabsf(center[0]+21.8f)>=4.f || fabsf(center[2]-29.56f)>=5.f)return RF_NOT_FOUND;
    ++rf_scene_campaign_wall[0];
    if(s->campaign_wall && s->campaign_wall->active)return RF_OK;
    owner=calloc(1,sizeof(*owner));scratch=calloc(1,sizeof(*scratch));
    if(!owner || !scratch){status=RF_IO;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    status=rf_geomod_template_load("D:\\geomod-template.bin",&scratch->shape);
#else
    status=rf_geomod_template_load("build/data/geomod-template.bin",&scratch->shape);
#endif
    if(status)goto done;
    for(i=0;i<2;i++){
        status=rf_editor_brush_open(s->campaign_geomod->level,brush_ids[i],512*1024,air+i);
        if(status)goto done;
        if(air[i].operation!=2){status=RF_FORMAT;goto done;}
        sources[i]=(rf_geomod_solid_clip_source){air[i].faces,air[i].face_count};
        work.clip.vertices[i]=scratch->clip_vertices[i];
        work.clip.fragments[i]=scratch->clip_fragments[i];
        work.vertices[i]=scratch->aggregate_vertices[i];
        work.fragments[i]=scratch->aggregate_fragments[i];
    }
    work.clip.vertex_capacity=work.vertex_capacity=4096;
    work.clip.fragment_capacity=work.fragment_capacity=1024;
    status=rf_geomod_template_mesh(&scratch->shape,center,basis,radius,
        s->terrain_material,scratch->cutter_vertices,scratch->cutter_faces,kernel,&cutter);
    if(status)goto done;
    status=rf_geomod_collision_faces(&cutter,scratch->cutter_filters,
        scratch->cutter_positions,RF_GEOMOD_STAR_VERTEX_LIMIT,
        scratch->cutter_collision,RF_GEOMOD_STAR_FACE_LIMIT);if(status)goto done;
    status=rf_geomod_campaign_room_scope_check_group(s->geometry,room_ids,2,
        scratch->cutter_collision,cutter.face_count,scratch->portals,256,&scope);
    if(status)goto done;
    if(scope.other_solid_faces || scope.touching_portals){status=RF_NOT_FOUND;goto done;}
    status=rf_geomod_campaign_wall_build(&cutter,sources,2,&work,
        scratch->wall_vertices,4096,scratch->wall_faces,1024,&wall);if(status)goto done;
    if(!wall.face_count){status=RF_NOT_FOUND;goto done;}
    for(i=0;i<2;i++){
        scene_campaign_wall_room *part=owner->rooms+i;
        rf_geomod_campaign_room_stage_work stage={part->vertices,part->faces,
            part->filters,part->bound,part->positions,part->face_ids,4096,SCENE_TERRAIN_FACES};
        status=rf_geomod_campaign_room_retain(s->geometry,room_ids[i],
            scratch->cutter_collision,cutter.face_count,&work.clip,
            scratch->retained_vertices,8192,scratch->retained_faces,
            scratch->retained_unchanged,2048,&retained);if(status)goto done;
        part->retained_faces=retained.face_count;
        status=rf_geomod_campaign_room_stage(s->geometry,&retained,
            scratch->retained_unchanged,&wall,actor_follow_world->slots,
            s->geometry->textures,s->terrain_material,s->materials->count,
            (rf_collision_face_filter){0,256,0,0,0,0},i?3501:3460,
            &stage,2*1024*1024,&part->mesh,&part->tree);if(status)goto done;
        for(j=0;j<part->mesh.vertex_count;j++)
            part->colors[j][0]=part->colors[j][1]=part->colors[j][2]=1;
    }
    status=rf_geometry_collision_overlay_open(&s->terrain_collision.world,40,
        SCENE_TERRAIN_FACES,128*1024,&owner->overlay[0]);if(status)goto done;
    status=rf_geometry_collision_overlay_bind(&owner->overlay[0],
        &owner->rooms[0].tree,owner->rooms[0].face_ids,owner->rooms[0].mesh.face_count);
    if(status)goto done;
    status=rf_geometry_collision_overlay_open(&owner->overlay[0].world,27,
        SCENE_TERRAIN_FACES,128*1024,&owner->overlay[1]);if(status)goto done;
    status=rf_geometry_collision_overlay_bind(&owner->overlay[1],
        &owner->rooms[1].tree,owner->rooms[1].face_ids,owner->rooms[1].mesh.face_count);
    if(status)goto done;
    if(s->geometry->faces>UINT32_MAX/sizeof(*offsets)){status=RF_RANGE;goto done;}
    offsets=malloc(s->geometry->faces*sizeof(*offsets));if(!offsets){status=RF_IO;goto done;}
    for(i=0;i<s->geometry->faces;i++){
        rf_geometry_face face;
        status=rf_geometry_get_face(s->geometry,i,&face);if(status)goto done;
        if(face.room!=28 && face.room!=40 && face.room!=27)
            offsets[count++]=s->geometry->face_offsets[i];
    }
    /* All allocations and both tree binds succeeded. Readers see the new
     * face view and collision world together at the next scene tick. */
    free(s->terrain_face_offsets);s->terrain_face_offsets=offsets;offsets=NULL;
    s->terrain_geometry.face_offsets=s->terrain_face_offsets;
    s->terrain_geometry.faces=count;
    campaign_dry_sweep_invalidate();
    s->collision=&owner->overlay[1].world;
    campaign_trigger_collision=s->collision;
    memcpy(owner->center,center,sizeof(owner->center));owner->radius=radius;owner->active=1;
    s->campaign_wall=owner;owner=NULL;
    ++rf_scene_campaign_wall[1];
    rf_scene_campaign_wall[2]=s->campaign_wall->rooms[0].mesh.face_count;
    rf_scene_campaign_wall[3]=s->campaign_wall->rooms[1].mesh.face_count;
    rf_scene_campaign_wall[4]=s->campaign_wall->rooms[0].mesh.vertex_count;
    rf_scene_campaign_wall[5]=s->campaign_wall->rooms[1].mesh.vertex_count;
    rf_scene_campaign_wall[6]=s->campaign_wall->rooms[0].tree.allocated_bytes+
        s->campaign_wall->rooms[1].tree.allocated_bytes;
    status=RF_OK;
done:
    rf_scene_campaign_wall[7]=(uint32_t)status;
    free(offsets);rf_editor_brush_close(air+1);rf_editor_brush_close(air);
    free(scratch);scene_campaign_wall_close(&owner);
    return status;
}
