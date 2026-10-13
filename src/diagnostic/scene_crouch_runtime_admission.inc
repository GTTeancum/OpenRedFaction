#ifndef RF_SCENE_CROUCH_RUNTIME_ADMISSION_INCLUDED
#define RF_SCENE_CROUCH_RUNTIME_ADMISSION_INCLUDED
/* Include after shape helpers. New admission does not adopt foreign physical
 * crouch. Existing receipts are retained across combat/order/weapon changes.
 * Resource demand is bounded to seven authored identities, without eviction
 * of earlier requests or publishing a receipt after a failed callback. */
static int scene_crouch_runtime_admit(uint32_t slot,int32_t now)
{
    campaign_npc_body *o;scene_crouch_fire_guard guard;
    scene_crouch_shape_candidate shape;scene_crouch_shape_seal seal;
    rf_entity_motion_catalog catalog;rf_entity_playback_resources playback;
    rf_entity_model_motions motions;rf_entity_playback_model model;
    scene_crouch_receipt next;uint32_t skeleton,cls,target,ids[7],caches[7],count=0,seed;
    const rf_physics_stance_cache *stances;
    void **data;uint32_t *sizes,total;uint64_t missing=0;int status,checked;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;
    if(o->crouch.flags&SCENE_CROUCH_PRESENT)return RF_OK;
    status=scene_crouch_receipt_admit(&o->crouch);if(status)return status;
    if(!scene_tactical_service_allowed(o)||o->view.action_520!=3||
       o->view.weapons[0]==campaign_grenade_id||o->unholster.stance_7bc||o->stance_clock_7b4||
       (o->view.flags_810&0x400u)||o->script_animation.active)return RF_OK;
    status=scene_tactical_owner(slot);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(o->selection.mapping.states[8]<0||o->selection.mapping.states[9]<0)return RF_OK;
    cls=campaign_seeds.items[slot].class_index;skeleton=o->selection.mapping.skeleton;
    if(!campaign_npc_stances||!campaign_npc_eyes||cls>=campaign_seeds.class_count)return RF_FORMAT;
    status=scene_crouch_shape_prepare(&o->body.spheres,campaign_npc_stances+cls,
        o->published,o->view.flags_810,0,&shape);if(status)return status;
    if(memcmp(shape.spheres,o->body.spheres.items,(size_t)shape.count*sizeof(*shape.spheres)))return RF_OK;
    status=scene_crouch_shape_seal_prepare(&o->body.spheres,&shape,&seal);if(status)return status;
    status=scene_crouch_owner_guard_capture(slot,&guard);if(status)return status;
    target=scene_blind_target_handle(o);stances=campaign_npc_stances;
    catalog=campaign_motion_catalog;playback=campaign_playback_resources;
    if(!catalog.models||!playback.models||skeleton>=catalog.model_count||skeleton>=playback.model_count)return RF_FORMAT;
    motions=catalog.models[skeleton];model=playback.models[skeleton];
    data=campaign_npc_motion_data;sizes=campaign_npc_motion_sizes;total=campaign_npc_motion_count;
    if(!motions.items||!model.resources||!model.cache_ids||!data||!sizes)return RF_FORMAT;
    for(uint32_t i=0;i<7;i++){
        int32_t id;uint32_t cache,j;const rf_motion_file *file;
        status=scene_crouch_future_clip(&guard.selection,i,&id);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        if((uint32_t)id>=motions.count||(uint32_t)id>=model.count)return RF_FORMAT;
        cache=model.cache_ids[id];file=&motions.items[id].file;
        if(cache>=total||!file->archive||!file->entry.size||model.resources[id].references<0)return RF_FORMAT;
        if(data[cache]&&sizes[cache]!=file->entry.size)return RF_FORMAT;
        for(j=0;j<count;j++)if(caches[j]==cache)break;
        if(j==count&&!data[cache])missing+=file->entry.size;
        if(j<count&&motions.items[ids[j]].file.entry.size!=file->entry.size)return RF_FORMAT;
        ids[count]=(uint32_t)id;caches[count++]=cache;
    }
    if((uint64_t)campaign_npc_motion_bytes+missing>SCENE_NPC_MOTION_BUDGET)return RF_OK;
    for(uint32_t i=0;i<count;i++){
        status=campaign_npc_motion_require(skeleton,ids[i]);
        checked=scene_crouch_fire_guard_valid(&guard);if(checked)return checked;
        if(memcmp(&campaign_motion_catalog,&catalog,sizeof(catalog))||
           memcmp(&campaign_playback_resources,&playback,sizeof(playback))||
           data!=campaign_npc_motion_data||sizes!=campaign_npc_motion_sizes||total!=campaign_npc_motion_count||
           stances!=campaign_npc_stances)return RF_FORMAT;
        if(memcmp(catalog.models+skeleton,&motions,sizeof(motions))||
           memcmp(playback.models+skeleton,&model,sizeof(model)))return RF_FORMAT;
        if(status)return status;
        o=campaign_npc_bodies+slot;
        if(!scene_tactical_service_allowed(o)||scene_blind_target_handle(o)!=target||
           o->unholster.stance_7bc||o->script_animation.active)return RF_OK;
        checked=scene_crouch_shape_seal_check(&o->body.spheres,campaign_npc_stances+cls,
            &shape,&seal);if(checked)return checked;
    }
    seed=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    seed=(seed^guard.handle)*16777619u;seed=(seed^cls^0x43524348u)*16777619u;
    next=o->crouch;status=scene_crouch_receipt_begin(&next,now,seed);if(status)return status;
    o->crouch=next;return RF_OK;
}
#endif
