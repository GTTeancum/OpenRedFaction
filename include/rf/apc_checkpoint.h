#ifndef RF_APC_CHECKPOINT_H
#define RF_APC_CHECKPOINT_H
#include "rf/vehicle_checkpoint.h"
#define RF_APC_CHECKPOINT_BYTES 128u
/* RFVC2/profile2 APC only; existing Driller RFVC1 APIs remain unchanged.
 * Fixed LE128 bytes: common vehicle fields0..103, primary ammo104,
 * secondary ammo108, relative pitch112/yaw116, primary RNG120, reserved124.
 * Common wrapper's drill fields MUST be zero. Health[-1e6,5000], armor0,
 * ammo0..999/0..15, pitch[-20,+50]degrees, yaw0. No handles or pointers.
 * All failures preserve outputs. No allocation; buffers/owners disjoint.
 * Codec alone is not RFCP/live save integration. Caller must reject active
 * projectiles, pending fire/cooldown/held triggers, burns and destruction
 * callbacks; omitted transients must not be silently discarded. Restore
 * validates class/world/seat placement and fresh registry ownership before
 * publishing. Dead occupied state is structurally valid, not safe ejection. */
typedef struct rf_apc_checkpoint {
    rf_vehicle_checkpoint vehicle;
    uint32_t primary_reserve,secondary_reserve,primary_random;
    float aim_pitch,aim_yaw;
} rf_apc_checkpoint;
int rf_apc_checkpoint_validate(const rf_apc_checkpoint *);
int rf_apc_checkpoint_encode(const rf_apc_checkpoint *,void *,uint32_t);
int rf_apc_checkpoint_decode(const void *,uint32_t,rf_apc_checkpoint *);
#endif
