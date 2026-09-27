/* First live campaign owner: L1S1's scripted UID 9456, room 28. This is a
 * bounded single cut, not the general multi-room/repeated-cut implementation. */
struct scene_campaign_geomod_owner {
    const rf_level *level;
    rf_geomod_vertex vertices[4096];rf_geomod_face faces[SCENE_TERRAIN_FACES];
    rf_collision_face_filter filters[SCENE_TERRAIN_FACES];
    rf_collision_face bound[SCENE_TERRAIN_FACES];
    float positions[4096][3],colors[4096][3];
    uint32_t face_ids[SCENE_TERRAIN_FACES];
    rf_geomod_mesh_view mesh;rf_collision_tree tree;
    uint32_t active;
};
typedef struct scene_campaign_geomod_scratch {
    rf_geomod_template shape;
    rf_geomod_vertex cutter_vertices[RF_GEOMOD_STAR_VERTEX_LIMIT],wall_vertices[4096];
    rf_geomod_face cutter_faces[RF_GEOMOD_STAR_FACE_LIMIT],wall_faces[1024];
    rf_collision_face cutter_collision[RF_GEOMOD_STAR_FACE_LIMIT];
    rf_collision_face_filter cutter_filters[RF_GEOMOD_STAR_FACE_LIMIT];
    float cutter_positions[RF_GEOMOD_STAR_VERTEX_LIMIT][3];
    rf_geomod_vertex clip_vertices[2][4096],aggregate_vertices[2][4096];
    rf_geomod_fragment clip_fragments[2][1024],aggregate_fragments[2][1024];
    rf_geomod_vertex retained_vertices[8192];rf_geomod_face retained_faces[2048];
    uint8_t retained_unchanged[2048];rf_geometry_portal portals[256];
} scene_campaign_geomod_scratch;
uint32_t rf_scene_campaign_geomod[8]; /* attempts,committed,room,faces,vertices,tree bytes,detail overlap,last status */
uint32_t rf_scene_campaign_geomod_collision[4]; /* rays,staged new-wall hits,old-room hits,differential */
static void scene_campaign_geomod_close(scene_campaign_geomod_owner **ptr)
{
    if(!ptr || !*ptr)return;
    rf_collision_tree_close(&(*ptr)->tree);free(*ptr);*ptr=NULL;
}
static int scene_campaign_geomod_open(scene_stream *s,const rf_level *level)
{
    scene_campaign_geomod_owner *owner;uint32_t ids[SCENE_TERRAIN_FACES],count=0,i;
    int status;
    if(!s || !level || !s->geometry || !s->collision || !s->materials ||
       !actor_follow_world || s->campaign_geomod || s->terrain_collision.storage ||
       s->terrain_material>=s->materials->count)return RF_RANGE;
    owner=calloc(1,sizeof(*owner));if(!owner)return RF_IO;
    owner->level=level;
    for(i=0;i<s->geometry->faces;i++){
        rf_geometry_face face;
        status=rf_geometry_get_face(s->geometry,i,&face);if(status)goto failed;
        if(face.room==28){if(count==SCENE_TERRAIN_FACES){status=RF_RANGE;goto failed;}ids[count++]=i;}
    }
    if(count!=411){status=RF_FORMAT;goto failed;}
    status=rf_geometry_collision_overlay_open(s->collision,28,SCENE_TERRAIN_FACES,
        128*1024,&s->terrain_collision);if(status)goto failed;
    status=scene_terrain_render_exclude(s,ids,count);if(status){
        rf_geometry_collision_overlay_close(&s->terrain_collision);goto failed;
    }
    s->campaign_geomod=owner;memset(rf_scene_campaign_geomod,0,sizeof(rf_scene_campaign_geomod));
    memset(rf_scene_campaign_geomod_collision,0,sizeof(rf_scene_campaign_geomod_collision));
    return RF_OK;
failed:scene_campaign_geomod_close(&owner);return status;
}
static int scene_campaign_geomod_cut(scene_stream *s,const float center[3],float radius)
{
    static const float basis[9]={1,0,0,0,1,0,0,0,1};
    scene_campaign_geomod_owner *owner=s?s->campaign_geomod:NULL;
    scene_campaign_geomod_scratch *scratch=NULL;rf_editor_brush air[2]={{0}};
    rf_geomod_solid_clip_source sources[2];rf_geomod_solid_union_work work={0};
    rf_geomod_mesh_view cutter,wall,retained;rf_geomod_campaign_room_scope scope;
    rf_geomod_campaign_room_stage_work stage;float kernel[3];
    uint32_t i;int status=RF_RANGE;
    if(!owner || !isfinite(radius) || radius<1 ||
       !s->terrain_collision.storage)return RF_RANGE;
    ++rf_scene_campaign_geomod[0];
    if(owner->active)return RF_OK; /* Repeated event ON retains the first committed edit. */
    scratch=calloc(1,sizeof(*scratch));if(!scratch)return RF_IO;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=rf_geomod_template_load("D:\\geomod-template.bin",&scratch->shape);
#else
    status=rf_geomod_template_load("build/data/geomod-template.bin",&scratch->shape);
#endif
    if(status)goto done;
    status=rf_editor_brush_open(owner->level,8755,512*1024,air);if(status)goto done;
    status=rf_editor_brush_open(owner->level,7778,512*1024,air+1);if(status)goto done;
    for(i=0;i<2;i++){
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
    status=rf_geomod_campaign_room_scope_check(s->geometry,28,scratch->cutter_collision,
        cutter.face_count,scratch->portals,256,&scope);if(status)goto done;
    rf_scene_campaign_geomod[6]=scope.other_detail_faces;
    if(scope.other_solid_faces || scope.touching_portals){status=RF_NOT_FOUND;goto done;}
    status=rf_geomod_campaign_wall_build(&cutter,sources,2,&work,
        scratch->wall_vertices,4096,scratch->wall_faces,1024,&wall);if(status)goto done;
    status=rf_geomod_campaign_room_retain(s->geometry,28,scratch->cutter_collision,
        cutter.face_count,&work.clip,scratch->retained_vertices,8192,
        scratch->retained_faces,scratch->retained_unchanged,2048,&retained);if(status)goto done;
    stage=(rf_geomod_campaign_room_stage_work){owner->vertices,owner->faces,owner->filters,
        owner->bound,owner->positions,owner->face_ids,4096,SCENE_TERRAIN_FACES};
    status=rf_geomod_campaign_room_stage(s->geometry,&retained,scratch->retained_unchanged,
        &wall,actor_follow_world->slots,s->geometry->textures,s->terrain_material,
        s->materials->count,(rf_collision_face_filter){0,256,0,0,0,0},3766,
        &stage,2*1024*1024,&owner->mesh,&owner->tree);if(status)goto done;
    {
        const rf_collision_tree *base=&s->collision->rooms[28].tree;
        for(i=retained.face_count;i<owner->mesh.face_count;i++){
            const rf_geomod_face *f=owner->mesh.faces+i;
            const rf_collision_face *surface=owner->bound+i;
            rf_collision_tree_hit staged_hit,base_hit;
            uint32_t staged_found=0,base_found=0,k,corner,source;
            float start[3]={0},delta[3];
            for(corner=0;corner<f->count;corner++)for(k=0;k<3;k++)
                start[k]+=owner->mesh.vertices[f->first+corner].position[k]/f->count;
            for(k=0;k<3;k++){
                start[k]+=surface->plane[k]*.2f;
                delta[k]=-surface->plane[k]*.4f;
            }
            status=rf_collision_thin_tree(owner->tree.nodes,owner->tree.node_count,
                owner->tree.faces,owner->tree.face_count,0x460,start,delta,1,
                owner->tree.stack,owner->tree.node_count,&staged_hit,&staged_found);
            if(status)goto done;
            status=rf_collision_thin_tree(base->nodes,base->node_count,base->faces,
                base->face_count,0x460,start,delta,1,base->stack,base->node_count,
                &base_hit,&base_found);if(status)goto done;
            ++rf_scene_campaign_geomod_collision[0];
            source=staged_found?owner->tree.source_indices[staged_hit.face_index]:UINT32_MAX;
            if(staged_found && source>=retained.face_count)++rf_scene_campaign_geomod_collision[1];
            if(base_found)++rf_scene_campaign_geomod_collision[2];
            if(staged_found && source>=retained.face_count && !base_found){
                ++rf_scene_campaign_geomod_collision[3];break;
            }
        }
    }
    for(i=0;i<owner->mesh.vertex_count;i++)owner->colors[i][0]=owner->colors[i][1]=owner->colors[i][2]=1;
    status=rf_geometry_collision_overlay_bind(&s->terrain_collision,&owner->tree,
        owner->face_ids,owner->mesh.face_count);if(status){rf_collision_tree_close(&owner->tree);goto done;}
    s->collision=&s->terrain_collision.world;
    campaign_trigger_collision=s->collision;
    owner->active=1;++rf_scene_campaign_geomod[1];
    rf_scene_campaign_geomod[2]=28;rf_scene_campaign_geomod[3]=owner->mesh.face_count;
    rf_scene_campaign_geomod[4]=owner->mesh.vertex_count;
    rf_scene_campaign_geomod[5]=owner->tree.allocated_bytes;
done:
    rf_scene_campaign_geomod[7]=(uint32_t)status;
    rf_editor_brush_close(air+1);rf_editor_brush_close(air);free(scratch);
    return status;
}
/* RFWC DESTRUCTION v1: fixed authored-cut identity. The level/source identity
 * belongs to the enclosing world checkpoint; geometry is regenerated from the
 * installed event and shared CSG, not serialized as unbounded mesh data. */
static int scene_campaign_geomod_checkpoint_write(const scene_stream *s,
    unsigned char *out,uint32_t capacity,uint32_t *written)
{
    if(!s || !out || !written)return RF_RANGE;
    if(!s->campaign_geomod || !s->campaign_geomod->active){*written=0;return RF_OK;}
    if(capacity<12)return RF_RANGE;
    memcpy(out,"RFCG",4);out[4]=1;out[5]=out[6]=out[7]=0;
    out[8]=0xf0;out[9]=0x24;out[10]=out[11]=0; /* UID 9456. */
    *written=12;return RF_OK;
}
static int scene_campaign_geomod_checkpoint_restore(scene_stream *s,
    const void *data,uint32_t bytes)
{
    const unsigned char *p=data;uint32_t i,found=0;int status=RF_NOT_FOUND;
    if(!bytes)return RF_OK;
    if(!s || !s->campaign_geomod || s->campaign_geomod->active || !p || bytes!=12 ||
       memcmp(p,"RFCG",4) || p[4]!=1 || p[5] || p[6] || p[7] ||
       p[8]!=0xf0 || p[9]!=0x24 || p[10] || p[11])return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++){
        const rf_level_event *event=&campaign_events.items[i].authored->record;
        if(event->uid!=9456)continue;
        if(++found!=1 || campaign_events.items[i].state.type!=10 ||
           (event->flags[0]&255u)!=1 || !isfinite(event->values[0]) ||
           event->values[0]<1)return RF_FORMAT;
        status=scene_campaign_geomod_cut(s,event->position,event->values[0]);
        if(status)return status;
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
