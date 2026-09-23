#ifndef RF_SCENE_WORLD_PLAYER_RESTORE_INCLUDED
#define RF_SCENE_WORLD_PLAYER_RESTORE_INCLUDED
/* Standing player candidate only; no gameplay publication. Boot integration
 * must restore inventory/equipped resources after initialization, before input. */
typedef struct scene_world_player_stage {
    rf_player_checkpoint player;rf_weapon_modes_checkpoint modes;
    rf_physics_body_state body;rf_entity_room_state room;rf_physics_support_contact support;
    float basis[9],eye[9];rf_checkpoint_placement placement;
    const scene_npc_checkpoint_restore_stage *npcs;
} scene_world_player_stage;
static int scene_world_player_pair(void *context,uint32_t index,rf_checkpoint_placement *p,uint32_t *retired)
{
    const scene_world_player_stage *s=context;
    if(!index){*p=s->placement;*retired=0;return RF_OK;}
    return scene_npc_checkpoint_restore_placement((void*)s->npcs,index-1,p,retired);
}
static inline int scene_world_player_prepare(scene_stream *stream,const scene_world_restore_stage *world,scene_world_player_stage *out)
{
    scene_world_player_stage s={0};rf_player_checkpoint_catalog catalog;scene_checkpoint_world_context context;
    rf_collision_room_location location;const rf_world_checkpoint_slice *v;int status;
    if(!stream||!world||!world->npcs||!out||!scene_actor_body.allocated_bytes||!scene_actor_body.spheres.count)return RF_RANGE;
    if(world->envelope.sections[RF_WORLD_VEHICLE-1].bytes)return RF_NOT_FOUND;
    status=scene_checkpoint_player_catalog_mode(&catalog,1);if(status)return status;
    v=world->envelope.sections+RF_WORLD_PLAYER-1;status=rf_player_checkpoint_decode(v->data,v->bytes,&catalog,&s.player);if(status)return status;
    status=rf_player_checkpoint_validate(&s.player,&catalog,s.basis,s.eye);if(status)return status;
    v=world->envelope.sections+RF_WORLD_WEAPON_MODES-1;
    status=rf_weapon_modes_checkpoint_decode(v->data,v->bytes,scene_remote_checkpoint_catalog_hash(),&s.modes);if(status)return status;
    status=scene_weapon_modes_checkpoint_prepare(stream,&s.player.player,&s.modes);if(status)return status;
    scene_checkpoint_placement(stream,&s.player,s.basis,&s.placement);
    context=world->world;context.allow_no_contact=0;context.authored_position=NULL;context.authored_basis=NULL;
    context.authored_props=NULL;context.authored_static_unchanged=0;
    context.dt=scene_step_seconds;context.class_speed=rf_scene_actor_movement_values.speed;
    status=rf_geometry_collision_world_locate(context.world,s.player.position,&location);if(status)return status;
    if(location.room==UINT32_MAX||location.room>=context.world->room_count)return RF_NOT_FOUND;
    context.reference_room=location.room;
    status=scene_checkpoint_world_place(&context,&s.placement,&s.room,&s.support);if(status)return status;
    s.npcs=world->npcs;status=scene_npc_checkpoint_pair_fit(s.npcs->count+1,scene_world_player_pair,&s);if(status)return status;
    s.body=scene_actor_body.state;memcpy(s.body.position,s.player.position,12);memcpy(s.body.next_position,s.player.position,12);
    memcpy(s.body.orientation,s.basis,36);memcpy(s.body.next_orientation,s.basis,36);memcpy(s.body.velocity,s.player.velocity,12);
    status=rf_physics_spheres_bounds(scene_actor_body.spheres.items,scene_actor_body.spheres.count,s.body.position,&s.body.bounds);if(status)return status;
    status=rf_physics_tensor_world(s.body.local_tensor,s.basis,s.body.world_tensor);if(status)return status;
    *out=s;return RF_OK;
}
#endif
