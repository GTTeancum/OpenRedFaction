#ifndef RF_FIGHTER_CHECKPOINT_H
#define RF_FIGHTER_CHECKPOINT_H
#include "rf/vehicle_checkpoint.h"
#define RF_FIGHTER_CHECKPOINT_BYTES 160u
/* RFVC5/profile5: common vehicle0..103; primary/rocket reserve104/108;
 * cooldown seconds112/116; shot counters120/124; reserved zero128..159.
 * Fighter01 HP900, armor0; ammo0..900/0..20; cooldown[-.05,.05]/[-.05,3].
 * Negative cooldown is the existing scheduler's valid <=50ms residual.
 * No aim/held/warmup/active projectiles serialized. Caller must capture only
 * settled held/warmup and no active projectiles/unsupported pending effects.
 * The codec preserves semantic occupancy, never registry handles. Restore
 * requires fresh class/host/player ownership, complete hull clearance and
 * validated player seat/exit placement before transactional publication.
 * Dead occupied records are structurally valid but need safe live handling.
 * First member is the common vehicle record for typed profile unions.
 * No allocation; failures preserve output; disjoint buffers/owners required. */
typedef struct rf_fighter_checkpoint {
    rf_vehicle_checkpoint vehicle;
    uint32_t primary_reserve,rocket_reserve;
    float primary_cooldown,rocket_cooldown;
    uint32_t primary_shots,rocket_shots;
} rf_fighter_checkpoint;
int rf_fighter_checkpoint_validate(const rf_fighter_checkpoint *);
int rf_fighter_checkpoint_encode(const rf_fighter_checkpoint *,void *,uint32_t);
int rf_fighter_checkpoint_decode(const void *,uint32_t,rf_fighter_checkpoint *);
#endif
