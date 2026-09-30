#ifndef RF_GRENADE_FLIGHT_H
#define RF_GRENADE_FLIGHT_H
#include "rf/weapon.h"
typedef struct rf_grenade_flight {
    float position[3],velocity[3],radius;
    uint32_t resting;
    rf_grenade_lifecycle lifecycle;
} rf_grenade_flight;
typedef struct rf_grenade_flight_event {
    uint32_t detonate,contacts,limited;
    float position[3];
    rf_weapon_flight_contact contact;
    uint32_t object_contact; /* Consumed contact; caller owns direct/blast effects. */
} rf_grenade_flight_event;
typedef uint32_t (*rf_grenade_object_contact)(void *,const rf_weapon_flight_contact *);
/* Practical swept-sphere, semi-implicit gravity/bounce policy, not retail rigid
 * body reconstruction. Zero initialize before launch; active slots are rejected.
 * Flags are forwarded to retained grenade lifecycle. Life initializes to10.
 * Direction is normalized; inherited thrower velocity may be added by caller. */
int rf_grenade_flight_launch(rf_grenade_flight *,const float position[3],
    const float direction[3],float speed,float radius,float fuse,
    uint32_t class_flags,uint32_t instance_flags);
/* At most4 contact sweeps, dt in[0,.25], restitution in[0,1]. Full tick fuse
 * update continues when resting; movement stops at fuse expiry. Alternate
 * contact marks life=-1, freezes motion, detonates on NEXT lifecycle step.
 * Ordinary floor impacts settle below speed0.5; tangent speed is damped by.8.
 * Contact-cap exhaustion retains the safe last pose and reports limited.
 * No host input/allocation. Sweep uses existing center-path fraction contract;
 * normals are normalized here. Callback must not mutate state/output.
 * Errors preserve state/output, not callback scratch. Detonation position is
 * grenade center; explosion effects/damage/terrain publication belong to caller.
 * Resting support removal/rotating attachment/liquids are later refinements. */
int rf_grenade_flight_step(rf_grenade_flight *,float dt,const float gravity[3],
    float restitution,rf_weapon_flight_sweep,void *,rf_grenade_flight_event *);
/* Optional pure classifier:1 consumes an object contact before bounce/fuse
 * publication,0 retains world response. Exactly one object contact is returned;
 * the retired flight cannot also emit a later fuse explosion. */
int rf_grenade_flight_step_objects(rf_grenade_flight *,float dt,const float gravity[3],
    float restitution,rf_weapon_flight_sweep,rf_grenade_object_contact,void *,rf_grenade_flight_event *);
#endif
