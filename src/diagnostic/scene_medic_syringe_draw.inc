#ifndef RF_SCENE_MEDIC_SYRINGE_DRAW_INCLUDED
#define RF_SCENE_MEDIC_SYRINGE_DRAW_INCLUDED
/* Same retained/static highest-LOD and lighting policy as ordinary clutter.
 * Original class model and material rows are independent of the parent's skin. */
static int scene_medic_syringe_draw(scene_stream *stream)
{
    rf_model_render_buffers buffers;rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};
    const rf_static_render_resource *resource=&scene_medic_syringe_resource.model.resource;
    uint32_t i,part,batch,k;int status;
    if(!scene_medic_syringe_resource.ready||!stream->npc_memory)return RF_OK;
    buffers.cache=stream->npc_memory;buffers.clip=(float(*)[3])((uint8_t*)stream->npc_memory+4096*32);
    buffers.second=NULL;buffers.vertices=(uint8_t(*)[40])((uint8_t*)stream->npc_memory+4096*56);buffers.capacity=4096;
    planes.near_depth=.1f;planes.far_depth=1000;
    projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=40;lights.ambient[1]=50;lights.ambient[2]=60;
    for(i=0;i<campaign_npc_body_count;i++){
        rf_clutter_base_owner *child=scene_medic_syringe_child_at(i);
        rf_model_projection view;uint32_t room;
        if(!child||(child->state.flags&(2u|0x4000u)))continue;
        room=child->state.first_word?child->state.first_word-1:UINT32_MAX;
        if(room<stream->visibility.state.count&&!stream->visibility.state.rooms[room].visible)continue;
        status=rf_model_local_view(&stream->npc_view,child->state.position,child->matrix,&view);if(status)return status;
        for(part=0;part<resource->part_count;part++){
            uint32_t li=resource->parts[part].first_lod;const rf_static_render_lod *lod;const rf_model_geometry *g;
            if(li>=resource->lod_count)return RF_FORMAT;lod=resource->lods+li;g=&lod->geometry;
            for(batch=0;batch<g->batch_count;batch++){
                const rf_model_draw_batch *draw=g->batches+batch;uint32_t slot,start=stream->mesh->count,emitted;
                if(draw->material==UINT32_MAX)continue;
                if(draw->material>=scene_medic_syringe_resource.materials.count)return RF_FORMAT;
                memcpy(&slot,scene_medic_syringe_resource.materials.items[draw->material].record.bytes+0x10,4);
                if(slot>=stream->syringe_textures)return RF_FORMAT;
                status=scene_model_retain(stream,g,batch,NULL,0,&view,stream->syringe_base+slot);
                if(status==RF_OK){++rf_scene_medic_syringe[5];continue;}if(status!=RF_NOT_FOUND)return status;
                status=scene_model_scratch_prepare(&buffers,draw->vertices);if(status)return status;
                status=rf_model_geometry_render_static_batch(g,batch,&view,&lights,&attributes,NULL,&buffers);if(status)return status;
                status=rf_preview_static_model_emit(g,batch,&buffers,stream->npc_indices,stream->npc_pool,&view,
                    &planes,&projection,&attributes,stream->mesh,stream->capacity,&emitted,lod->planes+draw->first_triangle,NULL);
                if(status)return status;
                for(k=start;k<stream->mesh->count;k++)stream->mesh->vertices[k].material=stream->syringe_base+slot;
                if(emitted)++rf_scene_medic_syringe[5];
            }
        }
    }
    return RF_OK;
}
#endif
