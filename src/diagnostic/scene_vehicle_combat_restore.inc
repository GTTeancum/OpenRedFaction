#ifndef RF_SCENE_VEHICLE_COMBAT_RESTORE_INC
#define RF_SCENE_VEHICLE_COMBAT_RESTORE_INC
#include "rf/apc_checkpoint.h"
#include "rf/jeep_checkpoint.h"
/* Include after scene_driller_checkpoint_adapter.inc. Pure staging only:
 * parent admits current class/resources, joint terrain/player/seat geometry,
 * owner generation and weapon transient policy before atomic publication.
 * No disk handles, reconstructed registrations, or live resources are copied.
 */
typedef struct scene_vehicle_combat_candidate {
    scene_driller_checkpoint_candidate vehicle;
    uint32_t profile,primary_reserve,secondary_reserve,primary_random,role;
    float aim_pitch,aim_yaw,aim_reference[9];
} scene_vehicle_combat_candidate;
/* Internal only: caller has validated a typed public record with its own
 * health/weapon/role limits. Do NOT call legacy Driller health validation. */
static int scene_vehicle_combat_stage_validated(const rf_vehicle_rigid_state *current,
    const rf_vehicle_checkpoint *record,scene_driller_checkpoint_candidate *out)
{
    scene_driller_checkpoint_candidate value={0};float tensor[9];int status;
    status=scene_driller_checkpoint_inertia(current->inverse_inertia,tensor);if(status)return status;
    memcpy(value.physics.inverse_inertia,current->inverse_inertia,36);
    memcpy(value.physics.position,record->position,12);memcpy(value.physics.orientation,record->orientation,36);
    memcpy(value.physics.velocity,record->velocity,12);
    status=scene_driller_checkpoint_rotate_tensor(record->orientation,tensor,record->angular_velocity,value.physics.momentum);if(status)return status;
    value.health=record->health;value.armor=record->armor;value.alive=record->alive;value.player_occupied=record->player_occupied;
    *out=value;return RF_OK;
}
static int scene_apc_checkpoint_stage_rigid(const rf_vehicle_rigid_state *current,
    const rf_apc_checkpoint *record,scene_vehicle_combat_candidate *out)
{
    scene_vehicle_combat_candidate value={0};int status;
    if(!current||!record||!out)return RF_RANGE;
    status=rf_apc_checkpoint_validate(record);if(status)return status;
    status=scene_vehicle_combat_stage_validated(current,&record->vehicle,&value.vehicle);if(status)return status;
    value.profile=2;value.primary_reserve=record->primary_reserve;value.secondary_reserve=record->secondary_reserve;
    value.primary_random=record->primary_random;value.aim_pitch=record->aim_pitch;value.aim_yaw=record->aim_yaw;
    /* APC pitch is host-relative; unlike Jeep it has no independent turret
     * reference payload. Use the SAVED host basis, never the pre-load pose. */
    memcpy(value.aim_reference,record->vehicle.orientation,36);
    *out=value;return RF_OK;
}
static int scene_jeep_checkpoint_stage_rigid(const rf_vehicle_rigid_state *current,
    const rf_jeep_checkpoint *record,scene_vehicle_combat_candidate *out)
{
    scene_vehicle_combat_candidate value={0};int status;
    if(!current||!record||!out)return RF_RANGE;
    status=rf_jeep_checkpoint_validate(record);if(status)return status;
    status=scene_vehicle_combat_stage_validated(current,&record->vehicle,&value.vehicle);if(status)return status;
    value.profile=3;value.primary_reserve=record->primary_reserve;value.primary_random=record->primary_random;
    value.role=record->role;value.aim_pitch=record->aim_pitch;value.aim_yaw=record->aim_yaw;
    memcpy(value.aim_reference,record->aim_reference,36);
    *out=value;return RF_OK;
}
#endif
