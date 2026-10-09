#ifndef RF_SCENE_JEEP_GUNNER_VIEW_INCLUDED
#define RF_SCENE_JEEP_GUNNER_VIEW_INCLUDED
/* RF.exe4ab322 ->42acd0 admits the Jeep's second occupant (class flag
 * 0x400000 and occupant index1) past the attached-weapon rejection. It then
 * enters the regular4ab395 first-person weapon draw;4aa762..4aa7a6 places
 * the authored normal offset and4ab3f0..4ab411 sets its own FOV.
 * This adapter uses the live, generation-qualified possession owner and
 * existing actual-shot scheduler. It never grants a weapon or fires a shot. */
/* requested,required,status,resident before image transfer,peak,texture count,
 * transferred material bytes,optional admission free pages */
uint32_t rf_scene_jeep_gunner_view_load[8];
/* existing slots/bytes,view slots/bytes,slot cap,INITIAL world byte cap,
 * projected combined material bytes,merge status; diagnostic only. */
uint32_t rf_scene_jeep_gunner_view_merge[8];
/* frame+1,active gunner,host,clip,vertices,batches,resident before transfer,
 * peak,offset[3],FOV,camera[3],clip scale[3],pose hash,status,actual launches,
 * reserve,pose steps,last stepped frame+1; floats are raw IEEE32 words. */
uint32_t rf_scene_jeep_gunner_view[24];
#ifdef RF_IMAGE_XBOX_NATIVE
extern uint32_t rf_xbox_available_memory_pages(void);
#endif
static int scene_jeep_gunner_view_resources(scene_stream *s,const char *tables_path,
    rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    rf_materials *materials,uint32_t material_budget)
{
    enum { view_budget=1536*1024 };
    scene_jeep_gunner_view *owner=NULL;rf_vpp tables={0};uint32_t required,bytes;int status;
    memset(rf_scene_jeep_gunner_view_load,0,sizeof(rf_scene_jeep_gunner_view_load));
    memset(rf_scene_jeep_gunner_view_merge,0,sizeof(rf_scene_jeep_gunner_view_merge));
    memset(rf_scene_jeep_gunner_view,0,sizeof(rf_scene_jeep_gunner_view));
    if(!s || s->jeep_gunner_view || !materials)return RF_RANGE;
    required=rf_scene_vehicle_enabled==3 && s->driller;
    if(!required && !scene_vehicle_profile_pack_get(s,3))return RF_OK;
    rf_scene_jeep_gunner_view_load[0]=1;rf_scene_jeep_gunner_view_load[1]=required;
#ifdef RF_IMAGE_XBOX_NATIVE
    if(!required){
        uint32_t pages=rf_xbox_available_memory_pages();rf_scene_jeep_gunner_view_load[7]=pages;
        /* Same16MiB reserve as optional profile admission, now including this
         * owner. A declined optional view retains the existing world gun. */
        if(pages<4096u+view_budget/4096u){status=RF_RANGE;goto done;}
    }
#endif
    status=rf_vpp_open(&tables,tables_path);if(status)goto done;
    status=scene_jeep_gunner_view_open(&tables,meshes,motions,maps,map_count,view_budget,&owner);
    rf_vpp_close(&tables);if(status)goto done;
    rf_scene_jeep_gunner_view_load[3]=owner->resident_bytes;
    rf_scene_jeep_gunner_view_load[4]=owner->peak_bytes;
    rf_scene_jeep_gunner_view_load[5]=owner->weapon->materials.textures.count;
    bytes=owner->weapon->materials.textures.allocated_bytes;
    rf_scene_jeep_gunner_view_merge[0]=materials->count;
    rf_scene_jeep_gunner_view_merge[1]=materials->allocated_bytes;
    rf_scene_jeep_gunner_view_merge[2]=owner->weapon->materials.textures.count;
    rf_scene_jeep_gunner_view_merge[3]=bytes;
    rf_scene_jeep_gunner_view_merge[4]=RF_CAMPAIGN_TEXTURE_SLOTS;
    rf_scene_jeep_gunner_view_merge[5]=material_budget;
    rf_scene_jeep_gunner_view_merge[6]=materials->allocated_bytes+bytes;
    status=scene_jeep_gunner_view_merge(owner,materials,RF_CAMPAIGN_TEXTURE_SLOTS);
    rf_scene_jeep_gunner_view_merge[7]=(uint32_t)status;if(status)goto done;
    rf_scene_jeep_gunner_view_load[6]=bytes;s->jeep_gunner_view=owner;owner=NULL;
done:
    scene_jeep_gunner_view_close(&owner);rf_scene_jeep_gunner_view_load[2]=(uint32_t)status;
    if(status)fprintf(stderr,"JEEP_GUNNER_VIEW_LOAD required=%u status=%d\n",required,status);
    return required?status:RF_OK;
}
static uint32_t scene_jeep_gunner_view_active(const scene_stream *s)
{
    return s && s->jeep_gunner_view && s->jeep_gunner_view->merged &&
        rf_scene_vehicle_enabled==3 && scene_driller_active(s) && scene_driller_visible(s) &&
        scene_jeep_can_fire(&s->driller_runtime->jeep_seat) && s->driller_runtime->entry.host.alive &&
        campaign_player_damage.state.effects.health>0 && !campaign_cutscene_runtime.active;
}
static int scene_jeep_gunner_view_draw(scene_stream *s,uint32_t frame)
{
    scene_jeep_gunner_view *o=s->jeep_gunner_view;rf_player_weapon *w=o?o->weapon:NULL;
    rf_model_projection view={0};rf_model_render_buffers buffers={0};rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};
    uint32_t *p=rf_scene_jeep_gunner_view,batch,k,start=s->mesh->count,host,shots;int request=-1,status=RF_OK;
    memset(p,0,sizeof(rf_scene_jeep_gunner_view));p[0]=frame+1;p[2]=UINT32_MAX;
    p[6]=rf_scene_jeep_gunner_view_load[3];p[7]=rf_scene_jeep_gunner_view_load[4];
    p[20]=rf_scene_apc_primary[1];p[21]=(uint32_t)s->apc_primary.reserve;
    if(o){p[22]=o->steps;p[23]=o->pose_frame;}
    if(!scene_jeep_gunner_view_active(s)){
        if(o){o->active=0;o->host=UINT32_MAX;o->shots=s->apc_primary.scheduler.shots;}
        return RF_OK;
    }
    p[1]=1;host=s->driller_runtime->entry.host.handle;p[2]=host;
    shots=s->apc_primary.scheduler.shots;
    if(!o->active || o->host!=host)request=0;
    else if(shots!=o->shots)request=1;
    /* The authored fire is an action, not a fabricated looping state. Actual
     * accepted shots restart it; completion returns through the shared idle
     * path. A held trigger during warmup/empty/full-pool creates no recoil. */
    if(o->pose_frame!=frame+1 || !o->active || o->host!=host){
        status=rf_player_weapon_step(w,request,frame?1.f/60.f:0);if(status)goto done;
        o->active=1;o->host=host;o->shots=shots;o->pose_frame=frame+1;++o->steps;
    }
    p[3]=w->current;p[18]=scene_diagnostic_hash(2166136261u,w->prepared,w->bone_count*48);
    p[22]=o->steps;p[23]=o->pose_frame;
    status=scene_player_weapon_view_prepare(w,&view,&projection);if(status)goto done;
    memcpy(p+8,w->position,12);memcpy(p+11,&w->fov,4);memcpy(p+12,view.camera,12);
    for(k=0;k<3;k++)memcpy(p+15+k,view.rotation+k*4,4);
    planes.near_depth=.01f;planes.far_depth=1000;
    buffers.cache=s->npc_memory;buffers.clip=(float(*)[3])((uint8_t*)s->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t*)s->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t*)s->npc_memory+4096*56);buffers.capacity=4096;
    lights.ambient[0]=lights.ambient[1]=lights.ambient[2]=180;
    for(batch=0;batch<w->geometry.batch_count;batch++){
        uint32_t material=w->geometry.batches[batch].material,slot,first=s->mesh->count,emitted;
        if(material==UINT32_MAX)continue;
        if(material>=w->materials.count){status=RF_FORMAT;goto done;}
        memcpy(&slot,w->materials.items[material].record.bytes+0x10,4);
        if(slot>=o->textures){status=RF_FORMAT;goto done;}
        status=scene_model_scratch_prepare(&buffers,w->geometry.batches[batch].vertices);if(status)goto done;
        status=rf_model_geometry_render_batch(&w->geometry,batch,w->prepared,w->bone_count,&view,&lights,&attributes,&buffers);if(status)goto done;
        status=rf_preview_model_emit(&w->geometry,batch,&buffers,s->npc_indices,s->npc_pool,&view,&planes,&projection,
            &attributes,s->mesh,s->capacity,&emitted);if(status)goto done;
        ++p[5];
        for(k=first;k<s->mesh->count;k++){
            rf_preview_vertex *v=s->mesh->vertices+k;v->material=o->base+slot;
            v->position[2]=16384.f/(1.f+v->texture[2]);
        }
    }
done:p[4]=s->mesh->count-start;p[19]=(uint32_t)status;return status;
}
#endif
