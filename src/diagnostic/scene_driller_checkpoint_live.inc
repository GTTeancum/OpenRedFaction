/* Shared save dispatch hooks. RFPL still describes an on-foot player; occupied
 * RFVC requires a separate player binding transaction and is rejected here. */
static int scene_vehicle_checkpoint_capture(scene_stream *s,void *blob,uint32_t *bytes)
{
    rf_vehicle_checkpoint record;int status;
    if(!s||!blob||!bytes)return RF_RANGE;
    *bytes=0;if(!s->driller)return RF_OK;
    if(!s->terrain_authored)return RF_RANGE;
    status=scene_driller_checkpoint_capture(s,&record);if(status)return status;
    if(record.player_occupied)return RF_RANGE;
    status=rf_vehicle_checkpoint_encode(&record,blob,RF_VEHICLE_CHECKPOINT_BYTES);
    if(!status)*bytes=RF_VEHICLE_CHECKPOINT_BYTES;
    return status;
}
static int scene_vehicle_checkpoint_read(scene_stream *s,const void *blob,uint32_t bytes,rf_vehicle_checkpoint *out)
{
    scene_driller_physics physics;scene_driller_checkpoint_candidate staged;rf_vehicle_checkpoint record;int status;
    if(!s||!out)return RF_RANGE;
    if(!bytes)return s->driller?RF_FORMAT:RF_OK;
    if(!s->driller||!s->terrain_authored||strcmp(campaign_current_level,"ctf06.rfl")||scene_driller_active(s))return RF_FORMAT;
    if(s->driller_runtime){const scene_driller_runtime *r=s->driller_runtime;
        if(!scene_driller_damage_runtime_valid(&r->damage)||r->damage.entry!=&r->entry||r->entry.physics!=&r->physics||
           r->entry.host.driver!=UINT32_MAX||r->entry.occupant!=-1||r->player.saved||r->damage.damage.state.effects.burn||
           r->entry.player_view->linked_handle==(int32_t)r->entry.host.handle)return RF_FORMAT;
    }
    status=rf_vehicle_checkpoint_decode(blob,bytes,&record);if(status)return status;
    if(record.player_occupied)return RF_FORMAT;
    status=scene_driller_physics_initialize(s->driller,record.position,record.orientation,scene_gravity.acceleration,&physics);if(status)return status;
    status=scene_driller_checkpoint_stage_rigid(&physics.state,&record,&staged);if(status)return status;
    *out=record;return RF_OK;
}
static int scene_vehicle_checkpoint_fit(scene_stream *s,scene_authored_collection_stage *stage,scene_authored_checkpoint_stage *single,
    const rf_vehicle_checkpoint *record,const rf_checkpoint_placement *player)
{
    scene_driller_physics physics;uint32_t i,j,k,q;int status;
    if(!s||(!stage&&!single)||!record||!player||!s->driller)return RF_RANGE;
    status=scene_driller_physics_initialize(s->driller,record->position,record->orientation,scene_gravity.acceleration,&physics);if(status)return status;
    status=stage?scene_driller_checkpoint_placement(stage,&physics,record):scene_driller_checkpoint_placement_single(single,&physics,record);if(status)return status;
    /* The saved on-foot body must also clear the restored chassis. Terrain
     * checks do not include this separately registered rigid vehicle. */
    for(i=0;i<physics.sphere_count;i++)for(j=0;j<player->count;j++){
        double distance=0,radius=physics.spheres[i].radius+player->spheres[j].radius-.002;
        for(k=0;k<3;k++){
            double d=(double)record->position[k]-player->position[k];
            for(q=0;q<3;q++)d+=(double)physics.spheres[i].center[q]*record->orientation[q*3+k]-(double)player->spheres[j].center[q]*player->basis[q*3+k];
            distance+=d*d;
        }
        if(distance<radius*radius)return RF_FORMAT;
    }
    return RF_OK;
}
