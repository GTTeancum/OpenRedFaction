#ifndef RF_SCENE_WORLD_VEHICLE_RESTORE_INCLUDED
#define RF_SCENE_WORLD_VEHICLE_RESTORE_INCLUDED
/* Static-world vehicle admission. The world and player/NPC candidates
 * are already staged; no live owner is changed by this check. */
typedef struct scene_world_vehicle_pair_context {
    rf_checkpoint_placement vehicle;
    const scene_world_player_stage *player;
    const scene_npc_checkpoint_restore_stage *npcs;
    uint32_t occupied;
} scene_world_vehicle_pair_context;
static int scene_world_vehicle_pair(void *context,uint32_t index,rf_checkpoint_placement *out,uint32_t *retired)
{
    const scene_world_vehicle_pair_context *pairs=context;
    if(!index){*out=pairs->vehicle;*retired=0;return RF_OK;}
    if(index==1){*out=pairs->player->placement;*retired=0;return RF_OK;}
    return scene_npc_checkpoint_restore_placement((void*)pairs->npcs,index-2,out,retired);
}
static int scene_world_vehicle_other_pair(void *context,uint32_t i,uint32_t j,
    uint32_t a,uint32_t b,double distance2,double radius)
{
    const scene_world_vehicle_pair_context *pairs=context;
    (void)a;(void)b;(void)distance2;(void)radius;
    /* Only a seated player's overlap with their own host is intentional.
     * Player/NPC and NPC/NPC pairs were admitted separately. */
    return i!=0 || (j==1 && pairs->occupied);
}
static int scene_world_vehicle_prepare(scene_stream *stream,const scene_world_restore_stage *world,
    const scene_world_player_stage *player,const scene_vehicle_checkpoint_record *record)
{
    scene_driller_physics physics;scene_checkpoint_world_context context;
    scene_world_vehicle_pair_context pairs={0};rf_collision_room_location location;
    rf_entity_room_state room;rf_physics_support_contact support;int status;
    if(!stream||!world||!player||!record||!stream->driller||!stream->driller_runtime||
       !campaign_authored_vehicle_uid||!campaign_authored_vehicle_handle)return RF_NOT_FOUND;
    status=scene_vehicle_checkpoint_physics(stream,&record->value.vehicle,&physics);if(status)return status;
    pairs.vehicle.spheres=physics.spheres;pairs.vehicle.count=physics.sphere_count;
    pairs.vehicle.query_flags=4;
    memcpy(pairs.vehicle.position,record->value.vehicle.position,12);
    memcpy(pairs.vehicle.basis,record->value.vehicle.orientation,36);
    context=world->world;context.allow_no_contact=1;context.dt=scene_step_seconds;
    context.class_speed=0;context.authored_position=campaign_authored_vehicle_position;
    context.authored_basis=campaign_authored_vehicle_basis;
    context.authored_props=world->authored_prop_views;
    status=rf_geometry_collision_world_locate(context.world,pairs.vehicle.position,&location);if(status)return status;
    if(location.room==UINT32_MAX||location.room>=context.world->room_count)return RF_NOT_FOUND;
    context.reference_room=location.room;
    status=scene_checkpoint_world_place(&context,&pairs.vehicle,&room,&support);
    if(status==RF_NOT_FOUND && record->profile==1 && physics.sphere_count==8 &&
       scene_world_l1s2_authored_terrain(stream) && stream->terrain_source_count==2 &&
       record->value.vehicle.player_occupied && record->value.vehicle.accepted_drill_cuts>=2){
        rf_checkpoint_placement full=pairs.vehicle,chassis=pairs.vehicle;
        rf_checkpoint_placement_result reason={UINT32_MAX,UINT32_MAX,0};
        /* The last two Driller spheres are its forward cutter heads. A live
         * cutter may end a tick inside the surface it just excavated; the
         * chassis must still fit. Keep full-body mover/prop and pair checks. */
        full.world=context.world;full.replaced_room=location.room;
        int clearance=rf_checkpoint_world_placement_check(&full,&reason);
        if(clearance==RF_NOT_FOUND && reason.reason==RF_CHECKPOINT_PLACEMENT_SURFACE && reason.sphere>=6){
            chassis.count=6;
            status=scene_checkpoint_world_place(&context,&chassis,&room,&support);
            if(!status)status=scene_checkpoint_world_obstacles(&context,&full);
        }
    }
    if(status)return status;
    pairs.player=player;pairs.npcs=world->npcs;pairs.occupied=record->value.vehicle.player_occupied;
    return scene_npc_checkpoint_pair_fit_allowed(world->npcs->count+2,scene_world_vehicle_pair,&pairs,
        scene_world_vehicle_other_pair);
}
#endif
