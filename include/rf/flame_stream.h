#ifndef RF_FLAME_STREAM_H
#define RF_FLAME_STREAM_H
#include "rf/entity_assets.h"
typedef struct rf_flame_cadence {uint32_t due,armed;} rf_flame_cadence;
/* One simulation-frame call,60 Hz. No catch-up burst; release retains cooldown.
 * Gas consumption and start-delay admission remain caller-owned. Request is
 * written only for pulse1 and feeds the existing entity damage pipeline. */
int rf_flame_pulse(rf_flame_cadence *,const rf_weapon_primary_definition *,
    uint32_t frame,uint32_t admitted,uint32_t source,uint32_t *pulse,rf_damage_request *);
/* Explicit port expanding-volume policy. Width increases linearly from zero
 * to end_radius over range. Sphere bounds are conservative near cone corners.
 * Caller invokes once per target (union its body spheres), preventing duplicate
 * damage. cover returns blocked for world/rubble; callback receives target
 * center. Persistent burn, flame rendering and resource ownership are separate. */
int rf_flame_contact(const float origin[3],const float forward[3],float range,float end_radius,
    const float center[3],float radius,int (*cover)(void *,const float *,const float *,uint32_t *),
    void *context,uint32_t *hit);
#endif
