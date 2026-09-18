#ifndef SCENE_SUBMARINE_CHECKPOINT_INC
#define SCENE_SUBMARINE_CHECKPOINT_INC
#include "rf/submarine_checkpoint.h"
/* Include after scene_driller_checkpoint_adapter.inc (math-only is sufficient).
 * Pure staging: caller supplies a validated live registration snapshot and
 * pure current-world/owner admission callbacks. No handles are serialized,
 * no ground support is required, and publication remains the transaction owner.
 */
typedef struct scene_submarine_checkpoint_live {
    const rf_vehicle_rigid_state *physics;
    float health,armor,torpedo_cooldown;
    int32_t torpedo_reserve;
    uint32_t alive,occupied,active_torpedoes,unsupported_effects,owner_valid;
} scene_submarine_checkpoint_live;
typedef struct scene_submarine_checkpoint_candidate {
    scene_driller_checkpoint_candidate vehicle;
    uint32_t torpedo_reserve;
    float torpedo_cooldown;
} scene_submarine_checkpoint_candidate;
typedef struct scene_submarine_checkpoint_backend {
    void *context;
    /* Fresh class/host/player registrations and possession links. */
    int (*owner)(void *,const rf_submarine_checkpoint *,uint32_t *);
    /* Entire actual hull in water + restored terrain/mover clearance. */
    int (*hull)(void *,const scene_submarine_checkpoint_candidate *,uint32_t *);
    /* Joint saved player placement/host exclusion or semantic occupied seat;
     * actual player water/collision admission, never a ground-floor test. */
    int (*player)(void *,const scene_submarine_checkpoint_candidate *,uint32_t *);
} scene_submarine_checkpoint_backend;
static int scene_submarine_checkpoint_capture_state(const scene_submarine_checkpoint_live *live,
    rf_submarine_checkpoint *out)
{
    rf_submarine_checkpoint value={0};float tensor[9];int status;
    if(!live || !live->physics || !out)return RF_RANGE;
    if(live->owner_valid!=1 || live->active_torpedoes || live->unsupported_effects)return RF_NOT_FOUND;
    if(live->torpedo_reserve<0)return RF_FORMAT;
    memcpy(value.vehicle.position,live->physics->position,12);
    memcpy(value.vehicle.orientation,live->physics->orientation,36);
    memcpy(value.vehicle.velocity,live->physics->velocity,12);
    status=scene_driller_checkpoint_inertia(live->physics->inverse_inertia,tensor);if(status)return status;
    status=scene_driller_checkpoint_rotate_tensor(live->physics->orientation,
        live->physics->inverse_inertia,live->physics->momentum,value.vehicle.angular_velocity);if(status)return status;
    value.vehicle.health=live->health;value.vehicle.armor=live->armor;
    value.vehicle.alive=live->alive;value.vehicle.player_occupied=live->occupied;
    value.torpedo_reserve=(uint32_t)live->torpedo_reserve;value.torpedo_cooldown=live->torpedo_cooldown;
    status=rf_submarine_checkpoint_validate(&value);if(status)return status;
    *out=value;return RF_OK;
}
static int scene_submarine_checkpoint_stage(const scene_submarine_checkpoint_live *live,
    const rf_submarine_checkpoint *record,const scene_submarine_checkpoint_backend *backend,
    scene_submarine_checkpoint_candidate *out)
{
    scene_submarine_checkpoint_candidate value={0};float tensor[9];uint32_t allowed=0;int status;
    if(!live || !live->physics || !record || !backend || !out ||
       !backend->owner || !backend->hull || !backend->player)return RF_RANGE;
    /* Stage only from an inactive live owner. Saved occupancy is retained for
     * a subsequent ordinary possession restore, not silently discarded here. */
    if(live->owner_valid!=1 || live->occupied || live->active_torpedoes || live->unsupported_effects)return RF_NOT_FOUND;
    status=rf_submarine_checkpoint_validate(record);if(status)return status;
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
    value.torpedo_reserve=record->torpedo_reserve;value.torpedo_cooldown=record->torpedo_cooldown;
    status=backend->owner(backend->context,record,&allowed);if(status)return status;
    if(allowed!=1)return RF_NOT_FOUND;
    allowed=0;status=backend->hull(backend->context,&value,&allowed);if(status)return status;
    if(allowed!=1)return RF_NOT_FOUND;
    allowed=0;status=backend->player(backend->context,&value,&allowed);if(status)return status;
    if(allowed!=1)return RF_NOT_FOUND;
    *out=value;return RF_OK;
}
#endif
