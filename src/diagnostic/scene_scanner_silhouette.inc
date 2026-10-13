/* Rail scanner presentation: reuse admitted NPC geometry and a complete
 * published pose, with no world/portal occlusion query. This is port policy,
 * not a reconstruction of the original thermal shader. Include after the
 * scanner selector. The normal frame sink owns all draw/fence lifetimes. */
enum {SCENE_SCANNER_VERTICES=16384,SCENE_SCANNER_TRIANGLES=8192,
    SCENE_SCANNER_EMITTED=16384,SCENE_SCANNER_BATCHES=128};
/* Per HUD pass: silhouettes, emitted triangles, source vertices, source
 * triangles, bracket fallbacks, budget fallbacks, geometry errors, last error. */
uint32_t rf_scene_scanner_silhouette[8];
typedef struct scene_scanner_draw_context {
    rf_scene_particle_sink sink;void *context;uint32_t limited;int sink_status;
} scene_scanner_draw_context;
static int scene_model_scratch_prepare(rf_model_render_buffers *,uint32_t);
static int scene_scanner_triangle(void *context,const rf_preview_vertex triangle[3])
{
    scene_scanner_draw_context *draw=context;rf_particle_draw_vertex vertices[3]={{0}};uint32_t i;
    if(rf_scene_scanner_silhouette[1]>=SCENE_SCANNER_EMITTED){draw->limited=1;return RF_RANGE;}
    for(i=0;i<3;i++){
        vertices[i].screen[0]=triangle[i].position[0];vertices[i].screen[1]=triangle[i].position[1];
        vertices[i].reciprocal_w=1;vertices[i].argb=0xff30c060u;
    }
    /* Opaque flat fill keeps intersecting/self-overlapping body triangles
     * uniformly green. HUD mode disables both world depth and depth writes. */
    draw->sink_status=draw->sink(draw->context,vertices,3,NULL,0x18000u);
    if(draw->sink_status)return draw->sink_status;
    ++rf_scene_scanner_silhouette[1];return RF_OK;
}
/* RF_OK with fallback=1 retains the existing bracket. Missing resources, a
 * partially published pose or bounded clipping pressure cannot kill a frame.
 * A platform sink failure still propagates through its ordinary drain path. */
static int scene_scanner_silhouette_draw(scene_stream *stream,
    const rf_weapon_scanner_marker *marker,rf_scene_particle_sink sink,void *context,uint32_t *fallback)
{
    const campaign_npc_body *body;const campaign_model_owner *owner;
    const rf_entity_render_model *model;rf_entity_pose *pose=NULL;
    rf_model_render_buffers buffers;rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};rf_model_projection view;
    scene_scanner_draw_context draw={sink,context,0,0};float prepared[50][12];uint16_t generations[50]={0};
    uint32_t actor=marker->index,lod,batch,k,n,vertices=0,triangles=0,batches=0,before;int status;
    *fallback=0;
    if(!stream || !sink || !campaign_npc_bodies || actor>=campaign_npc_body_count)return RF_OK;
    body=campaign_npc_bodies+actor;
    /* Match the complete generation-bearing handle, registry pointer and
     * live eligibility again. An array index alone never admits a new owner. */
    if(body->registration.handle!=marker->handle || !body->registration.view ||
       rf_object_registry_lookup(&campaign_registry,marker->handle)!=&body->registration ||
       !body->body.allocated_bytes || (body->object_flags&(2u|0x4000u)) ||
       (body->view.flags_810&1u) || !isfinite(body->damage.effects.health) || body->damage.effects.health<=0)return RF_OK;
    *fallback=1;
    if(!campaign_model_owners || actor>=campaign_model_owner_count)return RF_OK;
    owner=campaign_model_owners+actor;
    if(owner->owned || !owner->registration.loaded)return RF_OK;
    if(!stream->npc_memory || !stream->npc_indices || !stream->npc_pool || !campaign_render_models.items)return RF_OK;
    status=campaign_model_pose(actor,&pose);if(status)goto unavailable;
    if(!pose || !pose->matrices || !pose->generations || !pose->bone_count || pose->bone_count>50 ||
       pose->skeleton>=campaign_render_models.count)return RF_OK;
    model=campaign_render_models.items+pose->skeleton;
    if(!model->lods || !model->stored || model->bone_count!=pose->bone_count ||
       !model->file.lod_count || model->file.lod_count>RF_MODEL_MAX_LODS)return RF_OK;
    /* Culled actors can keep an older complete pose. Never evaluate/advance
     * animation or demand-load a motion from inside the HUD frame sink. */
    for(k=0;k<pose->bone_count;k++){
        if(pose->generations[k]!=pose->generations[0])return RF_OK;
        for(n=0;n<12;n++)if(!isfinite(pose->matrices[k][n]) || !isfinite(model->stored[k][n]))return RF_OK;
    }
    /* Match ordinary NPC rendering: first (highest-detail) LOD of each SUBM,
     * excluding negative material slots. Admit a whole owner before spending
     * frame work, so budgets normally fall back without partial silhouettes. */
    for(lod=0;lod<model->file.lod_count;lod++){
        const rf_model_geometry *geometry=model->lods+lod;uint32_t previous;
        for(previous=0;previous<lod;previous++)if(model->file.lods[previous].section_index==model->file.lods[lod].section_index)break;
        if(previous<lod)continue;
        if(geometry->batch_count>SCENE_SCANNER_BATCHES-batches)goto limited;
        batches+=geometry->batch_count;
        if(geometry->batch_count && !geometry->batches)return RF_OK;
        for(batch=0;batch<geometry->batch_count;batch++){
            const rf_model_draw_batch *part=geometry->batches+batch;
            if(part->material==UINT32_MAX)continue;
            if(part->vertices>4096 || part->vertices>SCENE_SCANNER_VERTICES-vertices ||
               part->triangles>SCENE_SCANNER_TRIANGLES-triangles)goto limited;
            vertices+=part->vertices;triangles+=part->triangles;
        }
    }
    if(!vertices || !triangles)return RF_OK;
    if(vertices>SCENE_SCANNER_VERTICES-rf_scene_scanner_silhouette[2] ||
       triangles>SCENE_SCANNER_TRIANGLES-rf_scene_scanner_silhouette[3] ||
       rf_scene_scanner_silhouette[1]>=SCENE_SCANNER_EMITTED)goto limited;
    rf_scene_scanner_silhouette[2]+=vertices;rf_scene_scanner_silhouette[3]+=triangles;
    /* Fresh local preparation uses a fixed nonzero cache generation. Do not
     * mutate the shared collision cache or retain stack matrices in a backend. */
    status=rf_model_prepare_skinning(model->stored,pose->matrices,pose->bone_count,1,prepared,generations,50);
    if(status)goto unavailable;
    status=rf_model_local_view(&stream->npc_view,owner->position,owner->basis,&view);if(status)goto unavailable;
    view.compute_clip=view.clipping=1;view.screen_clip=0;
    buffers.cache=stream->npc_memory;buffers.clip=(float(*)[3])((uint8_t*)stream->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t*)stream->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t*)stream->npc_memory+4096*56);buffers.capacity=4096;
    planes.near_depth=.1f;planes.far_depth=1000;
    projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=lights.ambient[1]=lights.ambient[2]=255;
    before=rf_scene_scanner_silhouette[1];
    for(lod=0;lod<model->file.lod_count;lod++){
        const rf_model_geometry *geometry=model->lods+lod;uint32_t previous;
        for(previous=0;previous<lod;previous++)if(model->file.lods[previous].section_index==model->file.lods[lod].section_index)break;
        if(previous<lod)continue;
        for(batch=0;batch<geometry->batch_count;batch++){
            if(geometry->batches[batch].material==UINT32_MAX)continue;
            status=scene_model_scratch_prepare(&buffers,geometry->batches[batch].vertices);if(status)goto unavailable;
            status=rf_model_geometry_render_batch(geometry,batch,prepared,pose->bone_count,&view,&lights,&attributes,&buffers);
            if(status)goto unavailable;
            /* The visitor borrows NPC scratch serially and copies each final
             * triangle to the HUD sink. It never writes the world/view mesh. */
            status=rf_preview_visit_model(geometry,batch,&buffers,stream->npc_indices,stream->npc_pool,
                &view,&planes,&projection,&attributes,NULL,scene_scanner_triangle,&draw);
            if(draw.sink_status)return draw.sink_status;
            if(draw.limited)goto limited;
            if(status)goto unavailable;
        }
    }
    if(rf_scene_scanner_silhouette[1]>before){*fallback=0;++rf_scene_scanner_silhouette[0];}
    return RF_OK;
limited:
    ++rf_scene_scanner_silhouette[5];return RF_OK;
unavailable:
    ++rf_scene_scanner_silhouette[6];rf_scene_scanner_silhouette[7]=(uint32_t)status;return RF_OK;
}
