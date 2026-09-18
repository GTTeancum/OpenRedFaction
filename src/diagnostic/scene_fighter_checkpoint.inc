#ifndef SCENE_FIGHTER_CHECKPOINT_INC
#define SCENE_FIGHTER_CHECKPOINT_INC
#include "scene_fighter_weapon.inc"
/* Include after scene_driller_checkpoint_adapter.inc (math-only is sufficient).
 * Pure staging: caller supplies a validated live registration snapshot and
 * pure current-world/owner admission callbacks. No handles are serialized,
 * no ground support is required, and publication remains the transaction owner.
 */
/* In-memory class5 record only; do not pass vehicle to the Driller wire codec.
 * A distinct fighter disk profile/RFCP admission remains unimplemented. Neutral
 * trigger capture preserves valid negative scheduler residuals (>=-50ms), both
 * authored cooldowns and shot counters; warmup/held transitions must settle. */
typedef struct scene_fighter_checkpoint_record {
    uint32_t class_id;rf_vehicle_checkpoint vehicle;
    uint32_t primary_reserve,rocket_reserve;
    scene_vehicle_primary_state primary_fire,rocket_fire;
} scene_fighter_checkpoint_record;
typedef struct scene_fighter_checkpoint_live {
    const rf_vehicle_rigid_state *physics;
    const scene_fighter_weapon_state *weapon;
    float health,armor;
    uint32_t alive,occupied,unsupported_effects,owner_valid;
} scene_fighter_checkpoint_live;
typedef struct scene_fighter_checkpoint_candidate {
    scene_driller_checkpoint_candidate vehicle;
    uint32_t primary_reserve,rocket_reserve;
    scene_vehicle_primary_state primary_fire,rocket_fire;
} scene_fighter_checkpoint_candidate;
static int scene_fighter_checkpoint_validate(const scene_fighter_checkpoint_record *r)
{
    const scene_vehicle_primary_state *s;uint32_t i;int status;
    if(!r)return RF_RANGE;
    if(r->class_id!=5 || r->vehicle.accepted_drill_cuts || r->vehicle.drill_spin!=0 ||
       r->primary_reserve>900 || r->rocket_reserve>20)return RF_FORMAT;
    /* Both classes have the same actual900HP bound; reuse only numeric pose
     * validation, never the Driller encoded identity or its gameplay fields. */
    status=rf_vehicle_checkpoint_validate(&r->vehicle);if(status)return status;
    for(i=0;i<2;i++){
        s=i?&r->rocket_fire:&r->primary_fire;
        if(!isfinite(s->cooldown) || s->cooldown<-.05f || s->cooldown>(i?3.f:.05f) ||
           s->held || s->warmup!=0)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_fighter_checkpoint_quiet(const scene_fighter_checkpoint_live *live)
{
    uint32_t i;
    if(!live || !live->physics || !live->weapon)return RF_RANGE;
    if(live->owner_valid!=1 || live->unsupported_effects)return RF_NOT_FOUND;
    for(i=0;i<48;i++)if(live->weapon->bullets[i].flight.active)return RF_NOT_FOUND;
    for(i=0;i<4;i++)if(live->weapon->rockets[i].flight.active)return RF_NOT_FOUND;
    if(live->weapon->primary_fire.held || live->weapon->primary_fire.warmup!=0 ||
       live->weapon->rocket_fire.held || live->weapon->rocket_fire.warmup!=0)return RF_NOT_FOUND;
    return RF_OK;
}
typedef struct scene_fighter_checkpoint_backend {
    void *context;
    /* Fresh class/host/player registrations and possession links. */
    int (*owner)(void *,const scene_fighter_checkpoint_record *,uint32_t *);
    /* Entire actual hull versus restored terrain/mover clearance; no floor requirement. */
    int (*hull)(void *,const scene_fighter_checkpoint_candidate *,uint32_t *);
    /* Joint saved player placement/host exclusion or semantic occupied seat;
     * actual player collision admission, never an invented ground-floor test. */
    int (*player)(void *,const scene_fighter_checkpoint_candidate *,uint32_t *);
} scene_fighter_checkpoint_backend;
static int scene_fighter_checkpoint_capture_state(const scene_fighter_checkpoint_live *live,
    scene_fighter_checkpoint_record *out)
{
    scene_fighter_checkpoint_record value={0};float tensor[9];int status;
    if(!live || !live->physics || !out)return RF_RANGE;
    status=scene_fighter_checkpoint_quiet(live);if(status)return status;
    if(live->weapon->primary_reserve<0 || live->weapon->rocket_reserve<0)return RF_FORMAT;
    value.class_id=5;
    memcpy(value.vehicle.position,live->physics->position,12);
    memcpy(value.vehicle.orientation,live->physics->orientation,36);
    memcpy(value.vehicle.velocity,live->physics->velocity,12);
    status=scene_driller_checkpoint_inertia(live->physics->inverse_inertia,tensor);if(status)return status;
    status=scene_driller_checkpoint_rotate_tensor(live->physics->orientation,
        live->physics->inverse_inertia,live->physics->momentum,value.vehicle.angular_velocity);if(status)return status;
    value.vehicle.health=live->health;value.vehicle.armor=live->armor;
    value.vehicle.alive=live->alive;value.vehicle.player_occupied=live->occupied;
    value.primary_reserve=(uint32_t)live->weapon->primary_reserve;value.rocket_reserve=(uint32_t)live->weapon->rocket_reserve;
    value.primary_fire=live->weapon->primary_fire;value.rocket_fire=live->weapon->rocket_fire;
    status=scene_fighter_checkpoint_validate(&value);if(status)return status;
    *out=value;return RF_OK;
}
static inline int scene_fighter_checkpoint_stage(const scene_fighter_checkpoint_live *live,
    const scene_fighter_checkpoint_record *record,const scene_fighter_checkpoint_backend *backend,
    scene_fighter_checkpoint_candidate *out)
{
    scene_fighter_checkpoint_candidate value={0};float tensor[9];uint32_t allowed=0;int status;
    if(!live || !live->physics || !record || !backend || !out ||
       !backend->owner || !backend->hull || !backend->player)return RF_RANGE;
    /* Stage only from an inactive live owner. Saved occupancy is retained for
     * a subsequent ordinary possession restore, not silently discarded here. */
    status=scene_fighter_checkpoint_quiet(live);if(status)return status;
    if(live->occupied)return RF_NOT_FOUND;
    status=scene_fighter_checkpoint_validate(record);if(status)return status;
    if(record->vehicle.player_occupied && !record->vehicle.alive)return RF_NOT_FOUND;
    status=scene_driller_checkpoint_inertia(live->physics->inverse_inertia,tensor);if(status)return status;
    memcpy(value.vehicle.physics.inverse_inertia,live->physics->inverse_inertia,36);
    memcpy(value.vehicle.physics.position,record->vehicle.position,12);
    memcpy(value.vehicle.physics.orientation,record->vehicle.orientation,36);
    memcpy(value.vehicle.physics.velocity,record->vehicle.velocity,12);
    status=scene_driller_checkpoint_rotate_tensor(record->vehicle.orientation,tensor,
        record->vehicle.angular_velocity,value.vehicle.physics.momentum);if(status)return status;
    value.vehicle.health=record->vehicle.health;value.vehicle.armor=record->vehicle.armor;
    value.vehicle.alive=record->vehicle.alive;value.vehicle.player_occupied=record->vehicle.player_occupied;
    value.primary_reserve=record->primary_reserve;value.rocket_reserve=record->rocket_reserve;
    value.primary_fire=record->primary_fire;value.rocket_fire=record->rocket_fire;
    status=backend->owner(backend->context,record,&allowed);if(status)return status;
    if(allowed!=1)return RF_NOT_FOUND;
    allowed=0;status=backend->hull(backend->context,&value,&allowed);if(status)return status;
    if(allowed!=1)return RF_NOT_FOUND;
    allowed=0;status=backend->player(backend->context,&value,&allowed);if(status)return status;
    if(allowed!=1)return RF_NOT_FOUND;
    *out=value;return RF_OK;
}
#endif
