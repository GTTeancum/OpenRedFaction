#ifndef RF_SCENE_WORLD_VEHICLE_RESTORE_INCLUDED
#define RF_SCENE_WORLD_VEHICLE_RESTORE_INCLUDED
/* Static-world vehicle admission. The world and player/NPC candidates
 * are already staged; no live owner is changed by this check. */
uint32_t rf_scene_vehicle_restore_trace[8]; /* stage,status,room,spheres,other UID,seat,corpse,other pair */
typedef struct scene_world_vehicle_pair_context {
    rf_checkpoint_placement vehicle;
    const scene_world_player_stage *player;
    const scene_npc_checkpoint_restore_stage *npcs;
    const scene_npc_seat_checkpoint_stage *seats;
    const scene_vehicle_checkpoint_record *record;
    uint32_t occupied,host_uid;
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
    /* Player/NPC and NPC/NPC pairs were admitted separately. Vehicle overlap
     * is allowed only for its saved player or an explicitly staged own NPC
     * seat whose exact tag pose was admitted by the seat/NPC transaction. */
    if(i!=0)return 1;
    if(j==1)return pairs->occupied;
    if(j<2||!pairs->npcs||j-2>=pairs->npcs->count)return 0;
    {uint32_t seat=scene_npc_seat_checkpoint_owns(pairs->seats,pairs->host_uid,
          pairs->npcs->entries[j-2].saved.uid);
     uint32_t corpse=scene_npc_jeep_seat_save_dead_contact(pairs->seats,pairs->npcs,
          pairs->record,pairs->host_uid,j-2);
     rf_scene_vehicle_restore_trace[4]=pairs->npcs->entries[j-2].saved.uid;
     rf_scene_vehicle_restore_trace[5]=seat;rf_scene_vehicle_restore_trace[6]=corpse;
     rf_scene_vehicle_restore_trace[7]=j;
     return seat||corpse;}
}
static int scene_world_vehicle_prepare(scene_stream *stream,const scene_world_restore_stage *world,
    const scene_world_player_stage *player,const scene_vehicle_checkpoint_record *record,
    const scene_npc_seat_checkpoint_stage *seats,uint32_t hidden)
{
    scene_driller_physics physics;scene_checkpoint_world_context context;
    scene_world_vehicle_pair_context pairs={0};rf_collision_room_location location;
    rf_entity_room_state room;rf_physics_support_contact support;uint32_t dev_submarine,wet=0;int status;
    rf_collision_body_sphere player_spheres[8];uint32_t i;
    if(!stream||!world||!player||!record||!stream->driller||!stream->driller_runtime||
       ((!campaign_authored_vehicle_uid||!campaign_authored_vehicle_handle)&&
        !(rf_scene_dev_room_enabled&&record->profile==4)))return RF_NOT_FOUND;
    memset(rf_scene_vehicle_restore_trace,0,sizeof(rf_scene_vehicle_restore_trace));
    rf_scene_vehicle_restore_trace[0]=1;
    dev_submarine=rf_scene_dev_room_enabled&&record->profile==4&&!campaign_authored_vehicle_uid;
    status=scene_vehicle_checkpoint_physics(stream,&record->value.vehicle,&physics);
    rf_scene_vehicle_restore_trace[0]=2;rf_scene_vehicle_restore_trace[1]=(uint32_t)status;
    if(status)return status;rf_scene_vehicle_restore_trace[3]=physics.sphere_count;
    /* Hidden owners have no live hull. Identity, finite rigid state and
     * hidden/occupancy consistency were admitted before this placement step. */
    if(hidden){
        if(hidden!=1||record->value.vehicle.player_occupied)return RF_FORMAT;
        rf_scene_vehicle_restore_trace[0]=6;return RF_OK;
    }
    pairs.vehicle.spheres=physics.spheres;pairs.vehicle.count=physics.sphere_count;
    pairs.vehicle.query_flags=4;
    memcpy(pairs.vehicle.position,record->value.vehicle.position,12);
    memcpy(pairs.vehicle.basis,record->value.vehicle.orientation,36);
    context=world->world;context.allow_no_contact=1;context.dt=scene_step_seconds;
    context.class_speed=0;context.authored_position=dev_submarine?stream->driller_position:campaign_authored_vehicle_position;
    context.authored_basis=dev_submarine?stream->driller_basis:campaign_authored_vehicle_basis;
    context.authored_props=world->authored_prop_views;
    status=rf_geometry_collision_world_locate(context.world,pairs.vehicle.position,&location);
    rf_scene_vehicle_restore_trace[0]=3;rf_scene_vehicle_restore_trace[1]=(uint32_t)status;
    if(status)return status;rf_scene_vehicle_restore_trace[2]=location.room;
    if(location.room==UINT32_MAX||location.room>=context.world->room_count)return RF_NOT_FOUND;
    context.reference_room=location.room;
    status=scene_checkpoint_world_place(&context,&pairs.vehicle,&room,&support);
    rf_scene_vehicle_restore_trace[0]=4;rf_scene_vehicle_restore_trace[1]=(uint32_t)status;
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
    if(record->profile==4){
        const scene_submarine_motion *motion=&stream->driller_runtime->submarine;
        if(motion->collision.world!=stream->collision||!motion->liquids||
           physics.sphere_count>8)return RF_NOT_FOUND;
        status=scene_submarine_water_body_path(motion,physics.collision,physics.sphere_count,
            pairs.vehicle.position,pairs.vehicle.basis,pairs.vehicle.position,pairs.vehicle.basis,&wet);
        if(status||!wet)return status?status:RF_NOT_FOUND;
        if(record->value.vehicle.player_occupied){
            if(player->placement.count>8)return RF_RANGE;
            for(i=0;i<player->placement.count;i++){
                memcpy(player_spheres[i].center,player->placement.spheres[i].center,12);
                player_spheres[i].radius=player->placement.spheres[i].radius;
            }
            status=scene_submarine_water_body_path(motion,player_spheres,player->placement.count,
                player->placement.position,player->placement.basis,
                player->placement.position,player->placement.basis,&wet);
            if(status||!wet)return status?status:RF_NOT_FOUND;
        }
    }
    pairs.player=player;pairs.npcs=world->npcs;pairs.occupied=record->value.vehicle.player_occupied;
    pairs.seats=seats;pairs.record=record;pairs.host_uid=(uint32_t)campaign_authored_vehicle_uid;
    status=scene_npc_checkpoint_pair_fit_allowed(world->npcs->count+2,scene_world_vehicle_pair,&pairs,
        scene_world_vehicle_other_pair);
    rf_scene_vehicle_restore_trace[0]=5;rf_scene_vehicle_restore_trace[1]=(uint32_t)status;
    return status;
}
#endif
