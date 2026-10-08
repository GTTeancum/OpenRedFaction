#ifndef RF_SCENE_SECONDARY_VEHICLE_RESTORE_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_RESTORE_INCLUDED
/* World, actors and every secondary row remain candidates until all owners
 * pass. No seated actor or authored-overlap exception is borrowed. */
uint32_t rf_scene_secondary_vehicle_clearance[6]; /* calls,phase,UID,room,pairs,status */
static int scene_secondary_vehicle_restore_one(scene_stream *stream,
    const scene_world_restore_stage *world,const scene_world_player_stage *player,
    const scene_vehicle_checkpoint_record *vehicle,const scene_passive_vehicle_checkpoint_stage *passive,
    const scene_secondary_vehicle_runtime *r,uint32_t active_hidden)
{
    uint32_t *probe=rf_scene_secondary_vehicle_clearance,retired;int status;
    rf_checkpoint_placement hull={0},other={0};rf_physics_sphere other_spheres[8];
    scene_checkpoint_world_context context;rf_collision_room_location location;
    rf_entity_room_state room;rf_physics_support_contact support;
    if(!stream||!world||!world->npcs||!player||!passive||!r)return RF_FORMAT;
    if(r->slot>=passive->count)return RF_FORMAT;
    /* RFEN/RFNC support was admitted against this exact RFSV candidate and
     * actual roof contact. Keep the complete actor/hull pair checks below;
     * support never grants an overlap or world-clearance exemption. */
    const scene_passive_vehicle_saved *saved=passive->rows+r->slot;
    ++probe[0];probe[1]=1;probe[2]=r->uid;probe[5]=0;
    /* Hidden and retired candidates have no hull. Their ownership, wire
     * state and RFVA visibility agreement were already admitted. */
    if(saved->object_flags&0x4000u){probe[1]=6;return RF_OK;}
    hull.spheres=r->physics.spheres;hull.count=r->physics.sphere_count;hull.query_flags=4;
    memcpy(hull.position,r->physics.state.position,12);memcpy(hull.basis,r->physics.state.orientation,36);
    if(scene_secondary_vehicle_aircraft_profile(r->profile)){
        scene_fighter_motion air=r->fighter;uint32_t dry=0;
        /* Liquid metadata is immutable level data. Geometry and pose belong
         * to this candidate transaction, including any admitted overlay. */
        air.collision.world=world->world.world;air.collision.movers=world->world.movers;
        probe[1]=7;
        status=scene_fighter_dry_body_path(&air,r->physics.collision,r->physics.sphere_count,
            hull.position,hull.basis,hull.position,hull.basis,&dry);if(status)goto fail;
        if(!dry){status=RF_NOT_FOUND;goto fail;}
    }else if(r->profile==4){
        scene_submarine_motion water=r->submarine;uint32_t wet=0;
        water.collision.world=world->world.world;water.collision.movers=world->world.movers;
        probe[1]=7;
        status=scene_submarine_water_body_path(&water,r->physics.collision,r->physics.sphere_count,
            hull.position,hull.basis,hull.position,hull.basis,&wet);if(status)goto fail;
        if(!wet){status=RF_NOT_FOUND;goto fail;}
    }else if(r->profile!=2){status=RF_FORMAT;goto fail;}
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
static int scene_secondary_vehicle_restore_admit(scene_stream *stream,
    const scene_world_restore_stage *world,const scene_world_player_stage *player,
    const scene_vehicle_checkpoint_record *vehicle,const scene_passive_vehicle_checkpoint_stage *passive,
    const scene_secondary_vehicle_checkpoint_stage *stage,uint32_t active_hidden)
{
    int status;
    if(!stage)return RF_OK;
    if(!stage->admitted||!stage->count||!stage->entries||!stage->pool||
       stage->capacity!=campaign_passive_vehicle_count||stage->count>stage->capacity)return RF_FORMAT;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_secondary_vehicle_runtime *r=stage->entries[i].runtime;
        if(!r||r->slot>=stage->capacity||stage->pool[r->slot]!=r)return RF_FORMAT;
        status=scene_secondary_vehicle_restore_one(stream,world,player,vehicle,passive,r,active_hidden);
        if(status)return status;
    }
    return RF_OK;
}
#endif
