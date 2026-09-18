#ifndef RF_JEEP_CHECKPOINT_H
#define RF_JEEP_CHECKPOINT_H
#include "rf/vehicle_checkpoint.h"
#define RF_JEEP_CHECKPOINT_BYTES 160u
/* RFVC3/profile3 Jeep01 only, unchanged Driller1/APC2 codecs reject it.
 * LE160-byte common physical fields0..103;104 gun reserve,108 seat role,
 *112 relative aim pitch,116 yaw,120 primary RNG,124..159 independent aim-reference basis. FNV checksum
 * at12 treats its own four bytes as zero, as earlier RFVC profiles do.
 * Authored Jeep01 health<=400/armor0, Jeep Gun reserve<=999. No secondary.
 * Role0 driver=interface_1;role1 gunner=interface_2, no handles/pointers.
 * Unoccupied state must be role0 with zeroaim; driver relativeaim0 follows
 * actual class table. Gunner limits pitch+/-1.0471975512,yaw+/-3.1415926536 radians are explicit
 * first-pass live Jeep aiming policy, NOT recovered turret constraints.
 * Relative angles MUST be restored against aim_reference, never silently
 * rebound to the current host basis. Basis is proper orthonormal tolerance1e-3.
 * Alive iff health>0; deadoccupied retained for pending safe exit. Common
 * wrapper drill fields mustzero; body bounds match previous RFVC validation.
 * Capture must reject active flights, fire/cooldown/held input, pending seat
 * transition, burns/destruction callbacks; no transient state is serialized.
 * Caller validates world/seat placement and reconstructs role/registry links
 * before joint publication. Codec is not live/RFCP integration. No heap;
 * disjoint buffers/owners, failures preserve output. */
typedef struct rf_jeep_checkpoint {
    rf_vehicle_checkpoint vehicle;
    uint32_t primary_reserve,role,primary_random;
    float aim_pitch,aim_yaw,aim_reference[9];
} rf_jeep_checkpoint;
int rf_jeep_checkpoint_validate(const rf_jeep_checkpoint *);
int rf_jeep_checkpoint_encode(const rf_jeep_checkpoint *,void *,uint32_t);
int rf_jeep_checkpoint_decode(const void *,uint32_t,rf_jeep_checkpoint *);
#endif
