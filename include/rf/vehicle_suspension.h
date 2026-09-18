#ifndef RF_VEHICLE_SUSPENSION_H
#define RF_VEHICLE_SUSPENSION_H
#include "rf/vehicle_rigid.h"
#define RF_VEHICLE_SUSPENSION_CAPACITY 6u
typedef struct rf_vehicle_spring {float center[3],radius,constant,length;} rf_vehicle_spring;
typedef struct rf_vehicle_spring_hit {
    uint32_t matched,material_id,support_handle;
    float fraction,material,velocity[3]; /* UINT32_MAX handle means static. */
} rf_vehicle_spring_hit;
typedef int (*rf_vehicle_spring_query)(void *,const float start[3],const float end[3],float radius,rf_vehicle_spring_hit *);
typedef struct rf_vehicle_suspension_result {
    rf_vehicle_rigid_support support;
    uint32_t hits,last_material,support_handle;
} rf_vehicle_suspension_result;
/* Prepared4a04d7..4a0729 spring loop, maximum six authored Driller supports.
 * Caller applies rf_physics_surface_probe_gate before this phase. Query uses
 * half sphere radius and WORLD-down length; force is world-up, torque uses
 * cross(contact lever,BODY-up), matching recovered code. Last hit material
 * wins; earlier dynamic support survives a later static hit. No damping or
 * assumed ground plane. Provider owns material lookup and dynamic velocity.
 * Result is additive force/torque for rigid support, not a state mutation.
 * Failures preserve output; no callbacks for nonpositive spring constants. */
int rf_vehicle_suspension_sample(const rf_vehicle_rigid_state *,float mass,
    const rf_vehicle_spring *,uint32_t count,rf_vehicle_spring_query,void *,rf_vehicle_suspension_result *);
#endif
