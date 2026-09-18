#ifndef RF_SCENE_UNDERCOVER_GAMEPLAY_INC
#define RF_SCENE_UNDERCOVER_GAMEPLAY_INC
/* Requires scene_undercover_mode.inc and scene_stream fields:
 * scene_undercover_resources *undercover;
 * uint32_t undercover_base,undercover_textures,undercover_alt_held;
 * Include after render/scratch helpers. Forward declare input/after_advance
 * where combat/view advance need them. Call close before FP views teardown. */
static int scene_undercover_open(scene_stream *s,rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t count)
{
    if(!s->player_weapon[16])return RF_OK;
    s->undercover_alt_held=0;memset(rf_scene_undercover,0,sizeof(rf_scene_undercover));
    /* Initial unsuppressed state is explicit first-pass DEV-room policy. */
    return scene_undercover_resources_open(s->player_weapon[16],meshes,motions,maps,count,0,512*1024,&s->undercover);
}
static int scene_undercover_merge(scene_stream *s,rf_materials *materials)
{
    rf_materials *textures;rf_material *combined;
    if(!s->undercover)return RF_OK;
    textures=&s->undercover->materials.textures;
    if((uint64_t)materials->count+textures->count>RF_CAMPAIGN_TEXTURE_SLOTS)return RF_RANGE;
    combined=realloc(materials->items,(materials->count+textures->count)*sizeof(*combined));
    if(!combined && materials->count+textures->count)return RF_IO;
    s->undercover_base=materials->count;s->undercover_textures=textures->count;
    materials->items=combined;
    if(textures->count)memcpy(combined+materials->count,textures->items,textures->count*sizeof(*combined));
    materials->count+=textures->count;materials->loaded+=textures->loaded;
    materials->missing+=textures->missing;materials->allocated_bytes+=textures->allocated_bytes;
    free(textures->items);memset(textures,0,sizeof(*textures));return RF_OK;
}
static int scene_undercover_close(scene_stream *s)
{
    int status=scene_undercover_resources_close(&s->undercover,s->player_weapon[16]);
    if(!status)s->undercover_base=s->undercover_textures=s->undercover_alt_held=0;
    return status;
}
static int scene_undercover_input(scene_stream *s,const float position[3])
{
    scene_undercover_resources *o=s->undercover;uint32_t held=!!player_input.alt_fire,edge=held && !s->undercover_alt_held,started;int status;
    s->undercover_alt_held=held;if(!o)return RF_OK;
    if(campaign_equipped_slot!=16 || campaign_explicit_unarmed || campaign_player_damage.state.effects.health<=0 ||
       !campaign_player_inventory.owned[campaign_slot_weapon(16)]) {
        if(o->mode.pending) {
            status=scene_undercover_mode_cancel(&o->mode,o->actions,s->player_weapon[16]);
            rf_scene_undercover[1]=o->mode.pending;rf_scene_undercover[2]=scene_undercover_mode_visible(&o->mode);
            rf_scene_undercover[7]=(uint32_t)status;return status;
        }
        return RF_OK;
    }
    if(edge && !player_input.fire && !player_input.reload && !rf_scene_combat[6]) {
        status=scene_undercover_mode_toggle(&o->mode,o->actions,s->player_weapon[16],&started);
        rf_scene_undercover[7]=(uint32_t)status;if(status)return status;
        if(started) {
            ++rf_scene_undercover[3];rf_scene_undercover[1]=1;rf_scene_undercover[2]=1;
            combat_sound(scene_undercover_mode_transition_sound(&o->mode),position);
            memset(&combat_trigger,0,sizeof(combat_trigger));combat_trigger.held=!!player_input.fire;
            /* Advance must preserve custom slot3 through this frame's select reset. */
            s->player_pose_frame=0;
        }
    }
    return RF_OK;
}
static int scene_undercover_after_advance(scene_stream *s)
{
    uint32_t changed;int status;
    if(!s->undercover || campaign_equipped_slot!=16)return RF_OK;
    status=scene_undercover_mode_poll(&s->undercover->mode,s->undercover->actions,s->player_weapon[16],&changed);
    rf_scene_undercover[0]=s->undercover->mode.attached;rf_scene_undercover[1]=s->undercover->mode.pending;
    rf_scene_undercover[2]=scene_undercover_mode_visible(&s->undercover->mode);rf_scene_undercover[7]=(uint32_t)status;
    if(!status && changed)++rf_scene_undercover[4];
    if(!status && changed && rf_scene_combat_trace)printf("UNDERCOVER_MODE %u\n",s->undercover->mode.attached);
    return status;
}
static int scene_undercover_draw(scene_stream *s)
{
    scene_undercover_resources *o=s->undercover;rf_model_projection camera={0},view;
    rf_model_render_buffers buffers={0};rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};
    float pose[12];uint32_t part,batch,k,initial=s->mesh->count;int status;
    rf_scene_undercover[6]=0;
    if(!o || campaign_equipped_slot!=16 || campaign_explicit_unarmed ||
       !campaign_player_inventory.owned[campaign_slot_weapon(16)] || campaign_player_damage.state.effects.health<=0 ||
       !scene_undercover_mode_visible(&o->mode))return RF_OK;
    status=scene_undercover_mode_pose(&o->mode,s->player_weapon[16],pose);if(status)return status;
    camera.camera[0]=.110f;camera.camera[1]=.140f;camera.camera[2]=.342f;
    camera.rotation[0]=camera.rotation[4]=camera.rotation[8]=1;
    camera.screen[0]=320/tanf(65.0f*3.14159265f/360);camera.screen[1]=-camera.screen[0];
    camera.screen[2]=320;camera.screen[3]=240;camera.perspective=camera.compute_clip=camera.clipping=1;
    status=rf_model_local_view(&camera,pose+9,pose,&view);if(status)return status;
    planes.near_depth=.01f;planes.far_depth=1000;projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    buffers.cache=s->npc_memory;buffers.clip=(float(*)[3])((uint8_t*)s->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t*)s->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t*)s->npc_memory+4096*56);buffers.capacity=4096;
    lights.ambient[0]=lights.ambient[1]=lights.ambient[2]=180;
    for(part=0;part<o->render.part_count;part++) {
        uint32_t li=o->render.parts[part].first_lod;const rf_static_render_lod *lod;const rf_model_geometry *g;
        if(li>=o->render.lod_count)return RF_FORMAT;
        lod=o->render.lods+li;g=&lod->geometry;
        for(batch=0;batch<g->batch_count;batch++) {
            const rf_model_draw_batch *b=g->batches+batch;uint32_t slot,first=s->mesh->count,emitted;
            if(b->material==UINT32_MAX)continue;
            if(b->material>=o->materials.count)return RF_FORMAT;
            memcpy(&slot,o->materials.items[b->material].record.bytes+0x10,4);
            if(slot>=s->undercover_textures)return RF_FORMAT;
            status=scene_model_scratch_prepare(&buffers,b->vertices);if(status)return status;
            status=rf_model_geometry_render_static_batch(g,batch,&view,&lights,&attributes,NULL,&buffers);if(status)return status;
            status=rf_preview_static_model_emit(g,batch,&buffers,s->npc_indices,s->npc_pool,&view,&planes,&projection,
                &attributes,s->mesh,s->capacity,&emitted,lod->planes+b->first_triangle,NULL);if(status)return status;
            for(k=first;k<s->mesh->count;k++) {
                rf_preview_vertex *v=s->mesh->vertices+k;v->material=s->undercover_base+slot;
                v->position[2]=16384.0f/(1.0f+v->texture[2]);
            }
        }
    }
    rf_scene_undercover[6]=s->mesh->count-initial;
    return RF_OK;
}
#endif
