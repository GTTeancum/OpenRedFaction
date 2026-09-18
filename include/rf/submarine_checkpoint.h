#ifndef RF_SUBMARINE_CHECKPOINT_H
#define RF_SUBMARINE_CHECKPOINT_H
#include "rf/vehicle_checkpoint.h"
#define RF_SUBMARINE_CHECKPOINT_BYTES 128u
/* RFVC4/profile4, installed class "sub" only. Fixed LE128 bytes:
 * common vehicle0..103, torpedo reserve104, cooldown seconds108,
 * reserved zero112..127. Authored HP700, armor0, ammo20, firewait3s.
 * No turret aim, raw registry handles, projectile pointers or hidden grants.
 * Common drill fields must be zero; dead occupied state is structurally valid.
 * Linear/angular velocities survive; caller reconstructs momentum with actual
 * mass2500 and initializes matching submerged physics. No live integration here.
 * Capture must reject active torpedoes and unsupported pending effects; restore
 * requires valid liquid/world/seat placement and fresh registry handles. The
 * finite remaining cooldown is retained, not discarded. Held input is transient
 * and must be released by the caller before capture/restore publication.
 * Errors preserve output. No allocation; input and output owners disjoint. */
typedef struct rf_submarine_checkpoint {
    rf_vehicle_checkpoint vehicle;
    uint32_t torpedo_reserve;
    float torpedo_cooldown;
} rf_submarine_checkpoint;
int rf_submarine_checkpoint_validate(const rf_submarine_checkpoint *);
int rf_submarine_checkpoint_encode(const rf_submarine_checkpoint *,void *,uint32_t);
int rf_submarine_checkpoint_decode(const void *,uint32_t,rf_submarine_checkpoint *);
#endif
