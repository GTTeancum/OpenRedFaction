/* DEV cavity edits: stage lighting, draw data and collision before core commit. */
typedef struct scene_legacy_edit {
    scene_stream *scene;scene_terrain_lighting_stage *lighting;
    rf_geometry_collision_overlay overlay;
    uint32_t ids[SCENE_TERRAIN_FACES],faces,cuts,generation,resident,peak,old_tree_bytes;
} scene_legacy_edit;
static int scene_legacy_edit_check(const rf_geomod_terrain_view *candidate,void *opaque)
{
    scene_legacy_edit *p=opaque;scene_stream *s=p->scene;uint32_t i;uint64_t reserved;int status;
    reserved=(uint64_t)candidate->resident_bytes+sizeof(*p)+SCENE_LIGHTING_STAGE_BUDGET+
        2*s->terrain_collision.resident_bytes+rf_scene_terrain_atlas[3]+sizeof(*s->terrain_draw)+
        SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s->terrain_colors)+SCENE_TERRAIN_FACES*sizeof(*s->terrain_ids)+
        sizeof(*s->terrain_light_cache)+sizeof(*s->terrain_template)+sizeof(*s->debris)+
        1100u*(sizeof(uint32_t)+sizeof(rf_vfx_light_source))+
        s->terrain_region_count*sizeof(*s->terrain_regions);
    if(reserved>SCENE_DESTRUCTION_BUDGET || candidate->mesh.face_count>SCENE_TERRAIN_FACES)return RF_RANGE;
    status=scene_terrain_lighting_stage_clone(s,candidate,&p->lighting);if(status)return status;
    /* Legacy authored faces resolve their original chart in preview.c. Their
     * generated-map binding stays absent; do not inherit a previous face slot. */
    status=scene_terrain_lighting_stage_bake(p->lighting,p->lighting->clone.terrain_bindings,s->particle_frame);if(status)return status;
    status=scene_terrain_lighting_stage_draw(p->lighting);if(status)return status;
    for(i=0;i<candidate->mesh.face_count;i++)p->ids[i]=candidate->mesh.faces[i].source_face==UINT32_MAX?
        s->terrain_fallback:candidate->mesh.faces[i].source_face;
    status=rf_geometry_collision_overlay_open(&s->terrain_collision.world,s->terrain_collision.room,
        s->terrain_collision.index_capacity,s->terrain_collision.resident_bytes,&p->overlay);if(status)return status;
    status=rf_geometry_collision_overlay_bind(&p->overlay,candidate->tree,p->ids,candidate->mesh.face_count);if(status)return status;
    p->faces=candidate->mesh.face_count;p->cuts=candidate->cuts;p->generation=candidate->mesh.generation;
    p->resident=candidate->resident_bytes-p->old_tree_bytes+s->terrain_collision.resident_bytes;
    p->peak=candidate->peak_bytes+s->terrain_collision.resident_bytes;
    return RF_OK;
}
static int scene_terrain_legacy_template_edit(scene_stream *s,const float center[3],const float basis[9],
    float scale,const rf_geomod_shallow_limit *limits,uint32_t count)
{
    scene_legacy_edit *p=calloc(1,sizeof(*p));rf_geomod_terrain_view before;uint32_t room;int status;
    if(!p)return RF_IO;p->scene=s;
    status=rf_geomod_terrain_get(s->terrain,&before);if(status){free(p);return status;}
    p->old_tree_bytes=before.tree->allocated_bytes;
    status=rf_geomod_terrain_cut_template_checked(s->terrain,s->terrain_template,center,basis,scale,
        s->terrain_material,limits,count,scene_legacy_edit_check,p);
    if(!status) {
        rf_geometry_collision_overlay *live=&s->terrain_collision;
        /* Everything below is a nonfailing copy into existing live owners.
         * Candidate tree storage is now committed; preserve overlay addresses. */
        room=live->room;
        memcpy(live->source_indices,p->overlay.source_indices,p->faces*sizeof(*live->source_indices));
        live->world.rooms[room]=p->overlay.world.rooms[room];
        live->world.rooms[room].tree.source_indices=live->source_indices;
        live->world.views[room]=p->overlay.world.views[room];
        live->world.views[room].tree=&live->world.rooms[room].tree;
        live->world.liquids[room]=p->overlay.world.liquids[room];
        memcpy(live->world.minimum,p->overlay.world.minimum,sizeof(live->world.minimum));
        memcpy(live->world.maximum,p->overlay.world.maximum,sizeof(live->world.maximum));
        memcpy(s->terrain_ids,p->ids,p->faces*sizeof(*p->ids));
        scene_terrain_lighting_stage_commit(p->lighting);
        rf_scene_geomod[0]=1;rf_scene_geomod[1]=p->cuts;rf_scene_geomod[2]=p->generation;
        rf_scene_geomod[3]=p->resident;rf_scene_geomod[4]=p->peak;
    }
    rf_geometry_collision_overlay_close(&p->overlay);
    scene_terrain_lighting_stage_discard(&p->lighting);free(p);return status;
}
