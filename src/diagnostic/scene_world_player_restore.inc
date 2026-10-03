#ifndef RF_SCENE_WORLD_PLAYER_RESTORE_INCLUDED
#define RF_SCENE_WORLD_PLAYER_RESTORE_INCLUDED
/* Player candidate with static or exact authored-mover support; no gameplay
 * publication. Boot integration restores inventory/equipment before input. */
typedef struct scene_world_player_stage {
    rf_player_checkpoint player;rf_weapon_modes_checkpoint modes;
    rf_physics_body_state body;rf_entity_room_state room;rf_physics_support_contact support;
    float support_velocity[3];
    float basis[9],eye[9];rf_checkpoint_placement placement;rf_physics_sphere seat_scratch;
    const scene_npc_checkpoint_restore_stage *npcs;
    const scene_npc_seat_checkpoint_stage *seats;
    const scene_vehicle_checkpoint_record *vehicle;
} scene_world_player_stage;
uint32_t rf_scene_world_player_probe[3]; /* prepare step, status, room */
static int scene_world_player_pair(void *context,uint32_t index,rf_checkpoint_placement *p,uint32_t *retired)
{
    const scene_world_player_stage *s=context;
    if(!index){*p=s->placement;*retired=0;return RF_OK;}
    return scene_npc_checkpoint_restore_placement((void*)s->npcs,index-1,p,retired);
}
static int scene_world_player_npc_overlap_allowed(void *context,uint32_t i,uint32_t j,
    uint32_t a,uint32_t b,double distance2,double radius)
{
    const scene_world_player_stage *s=context;
    if(!i&&j&&s&&s->npcs&&j-1<s->npcs->count){
        const rf_npc_checkpoint_record *npc=&s->npcs->entries[j-1].saved;
        double depth=radius-sqrt(distance2);
        /* RFNS2 proves this live NPC is the same Jeep's authored driver;
         * RFVC/RFPL already admitted the living player at interface_2. Their
         * broad body spheres may overlap without relaxing any other pair. */
        if(s->player.player.health>0&&!npc->retired&&!npc->dead_pose&&npc->health>0&&
           scene_jeep_gunner_checkpoint_pair(s->seats,s->vehicle,npc->uid))return 1;
#ifndef RF_IMAGE_XBOX_NATIVE
        {const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");
         if(enabled&&!strcmp(enabled,"1"))
             printf("WORLD_RESTORE_PLAYER_PAIR uid%u sphere%u/%u depth%.9g active%u follow%u health%.9g\n",
                 npc->uid,a,b,depth,npc->move.active,npc->move.follow,(double)npc->health);}
#endif
        /* Broad collision spheres can overlap during ordinary player/NPC
         * contact even before the NPC enters pursuit (L1S1 guard 8462).
         * Preserve only shallow live contact; an active pursuer may use the
         * same .75-unit allowance as an NPC/NPC pursuit pair. */
        if(s->player.player.health>0&&!npc->retired&&!npc->dead_pose&&npc->health>0&&
           (depth<=.5||(npc->move.active&&npc->move.follow==2&&depth<=.75)))return 1;
        return 0;
    }
    if(!i||!j)return 0;
    return scene_npc_checkpoint_restore_original_overlap((void*)s->npcs,i-1,j-1,a,b,distance2,radius);
}
static inline int scene_world_player_prepare(scene_stream *stream,const scene_world_restore_stage *world,
    uint32_t support_uid,const scene_vehicle_checkpoint_record *vehicle,
    const scene_passive_vehicle_checkpoint_stage *passive,scene_world_player_stage *out)
{
    scene_world_player_stage s={0};rf_player_checkpoint_catalog catalog;scene_checkpoint_world_context context;
    rf_collision_room_location location;const rf_world_checkpoint_slice *v;uint32_t passive_index=UINT32_MAX;int status;
    if(!stream||!world||!world->npcs||!out||!scene_actor_body.allocated_bytes||!scene_actor_body.spheres.count)return RF_RANGE;
    rf_scene_world_player_probe[0]=1;
    status=scene_checkpoint_player_catalog_mode(&catalog,1);if(status)return status;
    rf_scene_world_player_probe[0]=2;
    v=world->envelope.sections+RF_WORLD_PLAYER-1;status=rf_player_checkpoint_decode(v->data,v->bytes,&catalog,&s.player);if(status)return status;
    if(vehicle && vehicle->value.vehicle.player_occupied){
        status=rf_player_checkpoint_seated_profile_validate(&s.player,&catalog,vehicle->profile,
            scene_vehicle_checkpoint_typed(vehicle));
        if(status)return status;
    }else{status=rf_player_checkpoint_validate(&s.player,&catalog,s.basis,s.eye);if(status)return status;}
    rf_scene_world_player_probe[0]=3;
    v=world->envelope.sections+RF_WORLD_WEAPON_MODES-1;
    status=rf_weapon_modes_checkpoint_decode(v->data,v->bytes,scene_remote_checkpoint_catalog_hash(),&s.modes);if(status)return status;
    rf_scene_world_player_probe[0]=4;
    status=scene_weapon_modes_checkpoint_prepare(stream,&s.player.player,&s.modes);if(status)return status;
    if(vehicle && vehicle->value.vehicle.player_occupied){
        if(support_uid)return RF_FORMAT;
        status=scene_vehicle_checkpoint_player_placement(stream,vehicle,&s.player,&s.placement,&s.seat_scratch);if(status)return status;
        memcpy(s.basis,s.placement.basis,36);memcpy(s.eye,s.placement.basis,36);
    }else scene_checkpoint_placement(stream,&s.player,s.basis,&s.placement);
    context=world->world;context.allow_no_contact=0;context.authored_position=NULL;context.authored_basis=NULL;
    context.player_route_contact=1;
    context.authored_props=NULL;context.authored_static_unchanged=0;
    context.spawn_shell_uid=0;context.spawn_shell_position=stream->world_checkpoint_level?
        stream->world_checkpoint_level->player_position:NULL;context.support_uid=support_uid;
    context.spawn_basis=stream->world_checkpoint_level?stream->world_checkpoint_level->player_orientation[0]:NULL;
    if(world->world.authored_static_unchanged){
        context.authored_static_unchanged=1;context.authored_props=world->authored_prop_views;
    }
    if(stream->world_checkpoint_level&&
       ((scene_npc_checkpoint_level_equal(world->envelope.level,"L18S1.rfl")&&
         scene_npc_checkpoint_level_equal(campaign_current_level,"L18S1.rfl"))||
        (scene_npc_checkpoint_level_equal(world->envelope.level,"L17S4.rfl")&&
         scene_npc_checkpoint_level_equal(campaign_current_level,"L17S4.rfl")))&&
       !world->envelope.sections[RF_WORLD_DESTRUCTION-1].bytes){
        context.authored_static_unchanged=1;context.authored_props=world->authored_prop_views;
        context.spawn_shell_uid=scene_npc_checkpoint_level_equal(campaign_current_level,"L18S1.rfl")?10469:13627;
        context.spawn_shell_position=stream->world_checkpoint_level->player_position;
    }
    context.dt=scene_step_seconds;context.class_speed=rf_scene_actor_movement_values.speed;
    rf_scene_world_player_probe[0]=5;
    status=rf_geometry_collision_world_locate(context.world,s.player.position,&location);if(status)return status;
    rf_scene_world_player_probe[1]=location.room;
    if(location.room==UINT32_MAX||location.room>=context.world->room_count)return RF_NOT_FOUND;
    context.reference_room=location.room;
    if(vehicle && vehicle->value.vehicle.player_occupied){
        s.placement.world=context.world;s.placement.replaced_room=location.room;
        status=rf_checkpoint_world_placement_check(&s.placement,NULL);if(status)return status;
        s.room.room=location.room;
        memcpy(s.room.query_position,s.player.position,12);
    }else{rf_scene_world_player_probe[0]=6;
        if(support_uid)for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)
            if(campaign_passive_vehicles[i].uid==support_uid){passive_index=i;break;}
        status=passive_index!=UINT32_MAX?
            scene_world_passive_support_place(stream,&context,&s.placement,passive,
                passive_index,campaign_player_object.handle,(uint32_t)campaign_player_view.linked_handle,&s.room,&s.support):
            scene_checkpoint_world_place(&context,&s.placement,&s.room,&s.support);
        if(status)return status;}
    if(support_uid&&passive_index==UINT32_MAX){
        uint32_t i,found=0;
        for(i=0;i<context.movers->count;i++)if((uint32_t)context.movers->uids[i]==support_uid &&
            context.movers->views[i].object_id==s.support.handle){
            memcpy(s.support_velocity,context.movers->poses[i].velocity,12);++found;
        }
        if(found!=1)return RF_FORMAT;
    }
    rf_scene_world_player_probe[0]=7;
    s.npcs=world->npcs;s.seats=world->seats;s.vehicle=vehicle;
    status=scene_npc_checkpoint_pair_fit_allowed(s.npcs->count+1,scene_world_player_pair,
        &s,scene_world_player_npc_overlap_allowed);if(status)return status;
    rf_scene_world_player_probe[0]=8;
    s.body=scene_actor_body.state;memcpy(s.body.position,s.player.position,12);memcpy(s.body.next_position,s.player.position,12);
    memcpy(s.body.orientation,s.basis,36);memcpy(s.body.next_orientation,s.basis,36);memcpy(s.body.velocity,s.player.velocity,12);
    status=rf_physics_spheres_bounds(scene_actor_body.spheres.items,scene_actor_body.spheres.count,s.body.position,&s.body.bounds);if(status)return status;
    status=rf_physics_tensor_world(s.body.local_tensor,s.basis,s.body.world_tensor);if(status)return status;
    *out=s;
    if(vehicle && vehicle->value.vehicle.player_occupied && out->placement.spheres==&s.seat_scratch)
        out->placement.spheres=&out->seat_scratch;
    return RF_OK;
}
#endif
