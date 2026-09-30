/* Include after scene_model_retain and scene_model_scratch_prepare. The
 * caller selects live/dead resource and performs actor visibility admission. */
static int scene_turret_model_draw(scene_stream *stream,uint32_t index,
    const float position[3],const float basis[9],uint32_t *submissions,uint32_t *vertices)
{
    const scene_turret_model_resource *owner;const rf_static_render_resource *r;
    rf_model_projection view;rf_model_render_buffers buffers={0};
    rf_model_lighting lights={0};rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};
    uint32_t part,batch,k,count=0,initial;int status;
    if(!stream || index>=scene_turret_model_count || !position || !basis || !submissions || !vertices ||
       !stream->mesh || !stream->npc_memory || !stream->npc_indices || !stream->npc_pool)return RF_RANGE;
    owner=scene_turret_models+index;r=&owner->render;
    if(!owner->textures || (uint64_t)owner->base+owner->textures>RF_CAMPAIGN_TEXTURE_SLOTS)return RF_RANGE;
    *submissions=*vertices=0;initial=stream->mesh->count;
    status=rf_model_local_view(&stream->npc_view,position,basis,&view);if(status)return status;
    buffers.cache=stream->npc_memory;buffers.clip=(float(*)[3])((uint8_t*)stream->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t*)stream->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t*)stream->npc_memory+4096*56);buffers.capacity=4096;
    /* Same first-pass lighting/highest-LOD policy as existing static vehicles
     * and clutter; material textures and supplied object transform are authored. */
    lights.ambient[0]=40;lights.ambient[1]=50;lights.ambient[2]=60;
    planes.near_depth=.1f;planes.far_depth=1000;projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    for(part=0;part<r->part_count;part++) {
        uint32_t li=r->parts[part].first_lod;const rf_static_render_lod *lod;const rf_model_geometry *g;
        if(li>=r->lod_count)return RF_FORMAT;lod=r->lods+li;g=&lod->geometry;
        for(batch=0;batch<g->batch_count;batch++) {
            const rf_model_draw_batch *b=g->batches+batch;uint32_t slot,start=stream->mesh->count,emitted;
            if(b->material==UINT32_MAX)continue;
            if(b->material>=owner->materials.count)return RF_FORMAT;
            memcpy(&slot,owner->materials.items[b->material].record.bytes+0x10,4);
            if(slot>=owner->textures)return RF_FORMAT;
            status=scene_model_retain(stream,g,batch,NULL,0,&view,owner->base+slot);
            if(status==RF_OK){++count;continue;}if(status!=RF_NOT_FOUND)return status;
            status=scene_model_scratch_prepare(&buffers,b->vertices);if(status)return status;
            status=rf_model_geometry_render_static_batch(g,batch,&view,&lights,&attributes,NULL,&buffers);if(status)return status;
            status=rf_preview_static_model_emit(g,batch,&buffers,stream->npc_indices,stream->npc_pool,&view,&planes,&projection,
                &attributes,stream->mesh,stream->capacity,&emitted,lod->planes+b->first_triangle,NULL);if(status)return status;
            for(k=start;k<stream->mesh->count;k++)stream->mesh->vertices[k].material=owner->base+slot;
            if(emitted)++count;
        }
    }
    *submissions=count;*vertices=stream->mesh->count-initial;return RF_OK;
}
