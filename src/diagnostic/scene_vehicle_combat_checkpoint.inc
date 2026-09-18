#ifndef SCENE_VEHICLE_COMBAT_CHECKPOINT_INC
#define SCENE_VEHICLE_COMBAT_CHECKPOINT_INC
#include "rf/apc_checkpoint.h"
#include "rf/jeep_checkpoint.h"
/* Include after live vehicle/fire runtime and checkpoint inertia helpers.
 * Typed capture only; no publication, health normalization or disk format. */
static int scene_vehicle_combat_checkpoint_common(const scene_stream *s,const char *model,rf_vehicle_checkpoint *out)
{
    const scene_driller_runtime *r;rf_vehicle_checkpoint value={0};float tensor[9];int status;
    if(!s || !out || !model)return RF_RANGE;r=s->driller_runtime;
    if(!s->driller || strcmp(s->driller->model,model) || !r || !scene_driller_damage_runtime_valid(&r->damage) ||
       r->damage.entry!=&r->entry || r->entry.physics!=&r->physics || r->entry.resource!=s->driller ||
       !r->entry.player_view)return RF_NOT_FOUND;
    if(r->damage.damage.state.effects.burn || r->damage.damage.destruction_pending ||
       player_input.fire || player_input.alt_fire || player_input.use || r->entry.held ||
       scene_apc_primary_pending(s) || scene_apc_secondary_pending(s) ||
       s->apc_primary.scheduler.held || s->apc_primary.scheduler.warmup!=0 ||
       !isfinite(s->apc_primary.scheduler.cooldown) || s->apc_primary.scheduler.cooldown>0 ||
       !isfinite(s->apc_secondary.cooldown) || s->apc_secondary.cooldown>0)return RF_NOT_FOUND;
    if(r->entry.session.active &&
       (r->entry.session.host!=r->entry.host.handle || r->entry.session.driver!=r->entry.player.handle ||
        r->entry.host.driver!=r->entry.player.handle || r->entry.player.host!=r->entry.host.handle ||
        r->entry.occupant!=(int32_t)r->entry.player.handle ||
        r->entry.player_view->linked_handle!=(int32_t)r->entry.host.handle))return RF_NOT_FOUND;
    memcpy(value.position,r->physics.state.position,12);memcpy(value.orientation,r->physics.state.orientation,36);
    memcpy(value.velocity,r->physics.state.velocity,12);
    status=scene_driller_checkpoint_inertia(r->physics.state.inverse_inertia,tensor);if(status)return status;
    status=scene_driller_checkpoint_rotate_tensor(value.orientation,r->physics.state.inverse_inertia,
        r->physics.state.momentum,value.angular_velocity);if(status)return status;
    value.health=r->damage.damage.state.effects.health;value.armor=r->damage.damage.state.effects.armor;
    value.alive=r->entry.host.alive;value.player_occupied=!!r->entry.session.active;
    *out=value;return RF_OK;
}
static int scene_apc_checkpoint_capture(const scene_stream *s,rf_apc_checkpoint *out)
{
    rf_apc_checkpoint value={0};int status;if(!s || !out)return RF_RANGE;
    status=scene_vehicle_combat_checkpoint_common(s,"APC.v3m",&value.vehicle);if(status)return status;
    if(s->apc_primary.reserve<0 || s->apc_secondary.reserve<0)return RF_FORMAT;
    value.primary_reserve=(uint32_t)s->apc_primary.reserve;value.secondary_reserve=(uint32_t)s->apc_secondary.reserve;
    value.primary_random=s->apc_primary.random.value;
    if(value.vehicle.player_occupied){
        if(!s->apc_aim_active || s->apc_aim.angles[2]!=0)return RF_NOT_FOUND;
        value.aim_pitch=s->apc_aim.angles[0];value.aim_yaw=s->apc_aim.angles[1];
    }
    status=rf_apc_checkpoint_validate(&value);if(status)return status;*out=value;return RF_OK;
}
static int scene_jeep_checkpoint_capture(const scene_stream *s,rf_jeep_checkpoint *out)
{
    rf_jeep_checkpoint value={0};const scene_driller_runtime *r;int status;
    if(!s || !out)return RF_RANGE;
    status=scene_vehicle_combat_checkpoint_common(s,"Jeep01.v3m",&value.vehicle);if(status)return status;r=s->driller_runtime;
    if(r->jeep_seat.held || player_input.cycle_weapon)return RF_NOT_FOUND;
    if(s->apc_primary.reserve<0)return RF_FORMAT;
    value.primary_reserve=(uint32_t)s->apc_primary.reserve;value.primary_random=s->apc_primary.random.value;
    memcpy(value.aim_reference,value.vehicle.orientation,36);
    if(value.vehicle.player_occupied){
        int32_t seat;
        if(!s->apc_aim_active || s->apc_aim.angles[2]!=0)return RF_NOT_FOUND;
        status=scene_jeep_seat_index(s->driller,r->jeep_seat.role,&seat);if(status)return status;
        if(s->driller->seat!=seat)return RF_NOT_FOUND;
        value.role=r->jeep_seat.role;value.aim_pitch=s->apc_aim.angles[0];value.aim_yaw=s->apc_aim.angles[1];
        memcpy(value.aim_reference,scene_actor_body.state.orientation,36);
    }
    status=rf_jeep_checkpoint_validate(&value);if(status)return status;*out=value;return RF_OK;
}
#endif
