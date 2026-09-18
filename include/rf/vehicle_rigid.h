#ifndef RF_VEHICLE_RIGID_H
#define RF_VEHICLE_RIGID_H
#include "rf/vpp.h"
typedef struct rf_vehicle_rigid_state {
    float position[3],orientation[9],velocity[3],momentum[3];
    float inverse_inertia[9],force[3],torque[3]; /* inertia is body-local */
    uint32_t skip_forces;
} rf_vehicle_rigid_state;
typedef struct rf_vehicle_rigid_parameters {
    float mass,maximum_speed,acceleration,maximum_turn,turn_acceleration,gravity;
} rf_vehicle_rigid_parameters;
typedef struct rf_vehicle_rigid_command {float throttle,turn;uint32_t controlled;} rf_vehicle_rigid_command;
typedef struct rf_vehicle_rigid_support {
    uint32_t grounded;float material,velocity[3],force[3],torque[3];
} rf_vehicle_rigid_support;
typedef struct rf_vehicle_rigid_proposal {
    float position[3],orientation[9],velocity[3],momentum[3],angular_velocity[3];
} rf_vehicle_rigid_proposal;
/* Providers own suspension contacts/material and full host-volume collision.
 * resolve receives an uncommitted proposal and returns its corrected pose,
 * linear velocity and angular momentum. No callback may mutate state. */
typedef struct rf_vehicle_rigid_backend {
    void *context;
    int (*support)(void *,const rf_vehicle_rigid_state *,rf_vehicle_rigid_support *);
    int (*resolve)(void *,const rf_vehicle_rigid_state *,const rf_vehicle_rigid_proposal *,rf_vehicle_rigid_proposal *);
} rf_vehicle_rigid_backend;
/* First-pass rigid integration, not an ordinary actor controller. Recovered
 *49e180 exponential steering/torque ownership and49e9f0 rest/coast/gravity
 * behavior guide it. Explicit policies: dt<=.1, authored speed cap, smooth
 * lateral damping, semi-implicit translation, axis-angle world rotation.
 * Terrain/spring fidelity and impact impulses belong to providers. */
int rf_vehicle_rigid_propose(const rf_vehicle_rigid_state *,const rf_vehicle_rigid_parameters *,
    const rf_vehicle_rigid_command *,const rf_vehicle_rigid_support *,float,rf_vehicle_rigid_proposal *);
/* Requires both providers; failed queries/resolution preserve the whole state.
 * Successful commit clears force/torque unless skip_forces is set. */
int rf_vehicle_rigid_step(rf_vehicle_rigid_state *,const rf_vehicle_rigid_parameters *,
    const rf_vehicle_rigid_command *,float,const rf_vehicle_rigid_backend *);
#endif
