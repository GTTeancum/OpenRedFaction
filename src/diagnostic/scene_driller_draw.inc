#ifndef RF_SCENE_DRILLER_DRAW_INC
#define RF_SCENE_DRILLER_DRAW_INC
/* Include after scene_model_scratch_prepare/scene_model_retain, with
 * scene_driller_resources.inc already visible. Parent owns placement/liveness
 * and room visibility admission. Call alongside world actors before FP draws.
 * Static chassis only: no invented skeletal animation/cockpit transform. */
static int scene_driller_materials_merge(scene_driller_resources *owner,rf_materials *scene,
    uint32_t *base,uint32_t *texture_count)
{
    rf_materials *source;rf_material *combined;uint32_t count;
    if(!owner || !scene || !base || !texture_count)return RF_RANGE;
    source=&owner->materials.textures;count=source->count;
    if(!count || !source->items)return RF_NOT_FOUND;
    if((uint64_t)scene->count+count>RF_CAMPAIGN_TEXTURE_SLOTS)return RF_RANGE;
    combined=realloc(scene->items,(scene->count+count)*sizeof(*combined));if(!combined)return RF_IO;
    memcpy(combined+scene->count,source->items,count*sizeof(*combined));
    *base=scene->count;*texture_count=count;scene->items=combined;
    scene->count+=count;scene->loaded+=source->loaded;scene->missing+=source->missing;scene->allocated_bytes+=source->allocated_bytes;
    free(source->items);memset(source,0,sizeof(*source));return RF_OK;
}
static int scene_driller_draw(scene_stream *stream,const scene_driller_resources *owner,
    const float position[3],const float basis[9],uint32_t material_base,uint32_t texture_count,
    uint32_t *submissions,uint32_t *vertices)
{
    const rf_static_render_resource *r;rf_model_projection view;rf_model_render_buffers buffers={0};
    rf_model_lighting lights={0};rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};
    uint32_t part,batch,k,count=0,initial;int status;
    if(!stream || !owner || !position || !basis || !submissions || !vertices || !stream->mesh ||
       !stream->npc_memory || !stream->npc_indices || !stream->npc_pool || !texture_count ||
       (uint64_t)material_base+texture_count>RF_CAMPAIGN_TEXTURE_SLOTS)return RF_RANGE;
    *submissions=*vertices=0;r=&owner->render;initial=stream->mesh->count;
    status=rf_model_local_view(&stream->npc_view,position,basis,&view);if(status){fprintf(stderr,"DRILLER_DRAW_ERROR line%d status%d\n",__LINE__,status);return status;}
    buffers.cache=stream->npc_memory;buffers.clip=(float(*)[3])((uint8_t*)stream->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t*)stream->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t*)stream->npc_memory+4096*56);buffers.capacity=4096;
    lights.ambient[0]=40;lights.ambient[1]=50;lights.ambient[2]=60;
    planes.near_depth=.1f;planes.far_depth=1000;projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    for(part=0;part<r->part_count;part++) {
        uint32_t li=r->parts[part].first_lod;const rf_static_render_lod *lod;const rf_model_geometry *g;
        if(li>=r->lod_count)return RF_FORMAT;
        lod=r->lods+li;g=&lod->geometry;
        for(batch=0;batch<g->batch_count;batch++) {
            const rf_model_draw_batch *b=g->batches+batch;uint32_t slot,start=stream->mesh->count,emitted;
            if(b->material==UINT32_MAX)continue;
            if(b->material>=owner->materials.count)return RF_FORMAT;
            memcpy(&slot,owner->materials.items[b->material].record.bytes+0x10,4);
            if(slot>=texture_count)return RF_FORMAT;
            status=scene_model_retain(stream,g,batch,NULL,0,&view,material_base+slot);
            if(status==RF_OK){++count;continue;}
            if(status!=RF_NOT_FOUND)return status;
            status=scene_model_scratch_prepare(&buffers,b->vertices);if(status){fprintf(stderr,"DRILLER_DRAW_ERROR line%d status%d\n",__LINE__,status);return status;}
            status=rf_model_geometry_render_static_batch(g,batch,&view,&lights,&attributes,NULL,&buffers);if(status){fprintf(stderr,"DRILLER_DRAW_ERROR line%d status%d\n",__LINE__,status);return status;}
            status=rf_preview_static_model_emit(g,batch,&buffers,stream->npc_indices,stream->npc_pool,&view,&planes,&projection,
                &attributes,stream->mesh,stream->capacity,&emitted,lod->planes+b->first_triangle,NULL);if(status){fprintf(stderr,"DRILLER_DRAW_ERROR line%d status%d\n",__LINE__,status);return status;}
            for(k=start;k<stream->mesh->count;k++)stream->mesh->vertices[k].material=material_base+slot;
            ++count;
        }
    }
    *submissions=count;*vertices=stream->mesh->count-initial;return RF_OK;
}
#endif
