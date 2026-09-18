#ifndef RF_REMOTE_CHARGE_H
#define RF_REMOTE_CHARGE_H
#include "rf/grenade_flight.h"
#define RF_REMOTE_CHARGE_CAPACITY 32u
typedef struct rf_remote_charge {
    rf_grenade_flight flight;uint32_t owner,type,object_flags,attached;
    rf_weapon_flight_contact contact;
    float authored_fuse,orientation[9],local_offset[3],local_orientation[9];
    uint32_t host,host_bound,attachment_initialized,physics_flags;
} rf_remote_charge;
typedef struct rf_remote_charge_event {
    uint32_t attached,detonate;float position[3];rf_weapon_flight_contact contact;
} rf_remote_charge_event;
/* Zero initialize. Authored motion fields supplied by caller, not duplicated.
 * Class0x40 suppresses countdown (4c69a0). Owner remains distinct from hit object.
 * Active slots rejected; no allocation. */
int rf_remote_charge_launch(rf_remote_charge *,uint32_t owner,uint32_t type,
    const float position[3],const float direction[3],float speed,float radius,float fuse);
/* Practical semi-implicit gravity with one swept segment until first contact.
 * World contact settles at hit+.05normal (4c5140); object contact stops at hit
 * point and retains hit object identity (4c5b75). Launch fuse stays unchanged.
 * This first pass processes a queued detonation before movement/contact; the
 * original contact-before-request fuse-reset ordering is not reconstructed.
 * Object contact must be followed by bind_host with the resolved host pose.
 * Call update_host before subsequent step calls for bound charges.
 * Requested fuse-1 emits detonation once on next tick, including airborne.
 * Attached charges skip movement but still check lifecycle. Errors atomic. */
int rf_remote_charge_step(rf_remote_charge *,float dt,const float gravity[3],
    rf_weapon_flight_sweep,void *,rf_remote_charge_event *);
/*4c9dxx owner/class selector: all matching records get fuse-1, regardless of
 * dead/active/attachment/type. Caller supplies only registered projectile slots.
 * count<=32, any is boolean. Retirement remains next typed step's job. */
int rf_remote_charge_request(rf_remote_charge *,uint32_t count,uint32_t owner,uint32_t *any);
/* Availability differs: exact type/owner, positive fuse/life, no dead flag2. */
int rf_remote_charge_available(const rf_remote_charge *,uint32_t count,uint32_t owner,uint32_t type,uint32_t *any);
typedef struct rf_remote_charge_host {
    uint32_t handle,found,entity,entity_flags,class_flags;
    float position[3],orientation[9]; /* Orthonormal basis rows, local to world. */
} rf_remote_charge_host;
/* Bind first admitted object contact. Computes local offset/basis, resets fuse
 * to authored lifetime once and sets instance0x40. Repeated bind is a no-op.
 * Orientation faces -contact normal using stable port-owned up-axis selection.
 * Caller resolves contact tags to actual registry handle; owner is untouched. */
int rf_remote_charge_bind_host(rf_remote_charge *,const rf_remote_charge_host *);
/* Original4c83c0 policy: action0 unchanged,1 pose transported,2 detached from
 * dying/special entity (810&1 or724&04000000),3 missing host retired without
 * explosion. Detached instance0x40 remains set and cannot bind again.
 * Host handle must match; found0 needs no pose. Pure borrowed host snapshot.
 * Caller publishes new flight.position/orientation into scene collision/model.
 * Error preserves state/action; unbound charges return action0. */
int rf_remote_charge_update_host(rf_remote_charge *,const rf_remote_charge_host *,uint32_t *action);
#endif
