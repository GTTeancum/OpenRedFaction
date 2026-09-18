#ifndef RF_VEHICLE_SUBMERSIBLE_H
#define RF_VEHICLE_SUBMERSIBLE_H
#include "rf/vehicle_rigid.h"
typedef struct rf_vehicle_submersible_parameters {
    float mass,maximum_speed,acceleration,maximum_rotation,rotation_acceleration,drag;
} rf_vehicle_submersible_parameters;
typedef struct rf_vehicle_submersible_command {
    float throttle,strafe,rise,yaw,pitch;uint32_t controlled;
} rf_vehicle_submersible_command;
typedef struct rf_vehicle_submersible_result {uint32_t wet,moved,water_blocked;} rf_vehicle_submersible_result;
typedef struct rf_vehicle_submersible_backend {
    void *context;
    /* Mandatory: admit entire hull AND path between current and proposed,
     * including any liquid-room transition; center-only tests are insufficient.
     * Also called with current==proposed pose to establish initial wetness. */
    int (*water_path)(void *,const rf_vehicle_rigid_state *,const rf_vehicle_rigid_proposal *,uint32_t *allowed);
    /* Same contract as rigid resolve: full host world/mover collision. */
    int (*resolve)(void *,const rf_vehicle_rigid_state *,const rf_vehicle_rigid_proposal *,rf_vehicle_rigid_proposal *);
} rf_vehicle_submersible_backend;
/* Installed sub: mass2500, speed6, acceleration8, rotation4/acceleration2.
 * First-pass policies: neutral buoyancy, world-up rise/yaw, local-forward
 * throttle/right strafe/pitch; combined input normalized, no diagonal boost.
 * Input targets velocity with bounded acceleration; release uses exponential
 * drag (caller policy, suggested2/s). Angular velocity targets bounded rates,
 * reconstructs momentum using current inertia; no ground spring dependency.
 * dt[0,.1], finite inputs, unit axes, drag>=0. No heap. All callback errors and
 * malformed provider outputs preserve state/result. Callbacks must be pure.
 * Dry start or rejected wet path parks at previous pose and zeros velocities;
 * this is a stranded/boundary policy, not a dry flight or sinking simulation.
 * A collision-adjusted pose is checked again against water before publishing.
 */
int rf_vehicle_submersible_step(rf_vehicle_rigid_state *,const rf_vehicle_submersible_parameters *,
    const rf_vehicle_submersible_command *,float,const rf_vehicle_submersible_backend *,rf_vehicle_submersible_result *);
#endif
