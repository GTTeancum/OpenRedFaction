#ifndef RF_SCENE_SECONDARY_VEHICLE_RESTORE_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_RESTORE_INCLUDED
/* World, actors and all vehicle rows are candidates. The secondary owner is
 * unoccupied, so no seated actor or authored overlap exception is borrowed. */
uint32_t rf_scene_secondary_vehicle_clearance[6]; /* calls,phase,UID,room,pairs,status */
static int scene_secondary_vehicle_restore_admit(scene_stream *stream,
    const scene_world_restore_stage *world,const scene_world_player_stage *player,
    const scene_vehicle_checkpoint_record *vehicle,const scene_passive_vehicle_checkpoint_stage *passive,
    const scene_secondary_vehicle_checkpoint_stage *stage,uint32_t active_hidden)
{
    uint32_t *probe=rf_scene_secondary_vehicle_clearance,retired;int status;
    rf_checkpoint_placement hull={0},other={0};rf_physics_sphere other_spheres[8];
    scene_checkpoint_world_context context;rf_collision_room_location location;
    rf_entity_room_state room;rf_physics_support_contact support;
    if(!stage)return RF_OK;
    if(!stream||!world||!world->npcs||!player||!passive||!stage->admitted||!stage->runtime)return RF_FORMAT;
    const scene_secondary_vehicle_runtime *r=stage->runtime;
    if(r->slot>=passive->count)return RF_FORMAT;
    if(player->support.handle==r->handle)return RF_NOT_FOUND;
    for(uint32_t i=0;i<world->npcs->count;i++)
        if(world->npcs->entries[i].saved.support_uid==r->uid)return RF_NOT_FOUND;
    const scene_passive_vehicle_saved *saved=passive->rows+r->slot;
    ++probe[0];probe[1]=1;probe[2]=r->uid;probe[5]=0;
    /* Hidden and retired candidates have no hull. Their ownership, wire
     * state and RFVA visibility agreement were already admitted. */
    if(saved->object_flags&0x4000u){probe[1]=6;return RF_OK;}
    hull.spheres=r->physics.spheres;hull.count=r->physics.sphere_count;hull.query_flags=4;
    memcpy(hull.position,r->physics.state.position,12);memcpy(hull.basis,r->physics.state.orientation,36);
    context=world->world;context.allow_no_contact=1;context.dt=scene_step_seconds;context.class_speed=0;
    context.authored_position=context.authored_basis=NULL;context.authored_props=NULL;
    context.authored_rotating_npc=context.authored_center_shift_npc=context.support_uid=0;
    context.npc_route_contact=context.player_route_contact=context.spawn_shell_uid=0;
    status=rf_geometry_collision_world_locate(context.world,hull.position,&location);if(status)goto fail;
    probe[3]=location.room;
    if(location.room==UINT32_MAX||location.room>=context.world->room_count){status=RF_NOT_FOUND;goto fail;}
    context.reference_room=location.room;probe[1]=2;
    status=scene_checkpoint_world_place(&context,&hull,&room,&support);if(status)goto fail;
    probe[1]=3;++probe[4];
    status=scene_vehicle_wreck_pair(&hull,player->placement.spheres,player->placement.count,
        player->placement.position,player->placement.basis);if(status)goto fail;
    for(uint32_t i=0;i<world->npcs->count;i++){
        status=scene_npc_checkpoint_restore_placement(world->npcs,i,&other,&retired);if(status)goto fail;
        if(retired)continue;++probe[4];
        status=scene_vehicle_wreck_pair(&hull,other.spheres,other.count,other.position,other.basis);if(status)goto fail;
    }
    probe[1]=4;
    if(vehicle&&!active_hidden){
        scene_driller_physics physics;
        status=scene_vehicle_checkpoint_physics(stream,&vehicle->value.vehicle,&physics);if(status)goto fail;
        ++probe[4];status=scene_vehicle_wreck_pair(&hull,physics.spheres,physics.sphere_count,
            vehicle->value.vehicle.position,vehicle->value.vehicle.orientation);if(status)goto fail;
    }
    probe[1]=5;
    for(uint32_t i=0;i<passive->count;i++){
        if(i==r->slot||(passive->rows[i].object_flags&0x4000u))continue;
        status=scene_passive_jeep_seat_restore_shape(stream,passive,i,&other,other_spheres);if(status)goto fail;
        ++probe[4];status=scene_vehicle_wreck_pair(&hull,other.spheres,other.count,other.position,other.basis);if(status)goto fail;
    }
    probe[1]=6;return RF_OK;
fail:probe[5]=(uint32_t)status;return status;
}
#endif
