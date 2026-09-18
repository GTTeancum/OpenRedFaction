#ifndef RF_VEHICLE_CHECKPOINT_H
#define RF_VEHICLE_CHECKPOINT_H
#include "rf/vpp.h"
#define RF_VEHICLE_CHECKPOINT_BYTES 128u
/* RFVCv1: one authored Driller, no raw handles/pointers, resources or force
 * accumulators. Does not implement live save/restore, world identity, terrain
 * history, body clearance, registry creation or player/host binding. Caller
 * validates those together before publication. A dead occupied host is valid
 * while its driver awaits a clear exit. Not the original game's save ABI. */
typedef struct rf_vehicle_checkpoint {
    float position[3],orientation[9],velocity[3],angular_velocity[3];
    float health,armor,drill_spin;
    uint32_t alive,player_occupied,accepted_drill_cuts;
} rf_vehicle_checkpoint;
/* Fixed little-endian128-byte record; FNV-1a checksum with bytes12..15 treated
 * as zero detects accidental corruption, not authentication. No allocation.
 * All failures preserve output. Buffers/owners must be disjoint.
 * Explicit codec safety ranges: position +/-1e6, velocity +/-1000 per axis,
 * angular velocity +/-100 per axis; proper orthonormal basis tolerance1e-3.
 * Authored health<=900, armor0, spin0..floatbits412fede0, acceptedcuts<=25.
 * Dead health may retain finite overkill down to-1e6; alive iff health>0.
 * Runtime angular MOMENTUM must be converted using reconstructed inertia;
 * do not serialize that vector as angular_velocity or restore inverse inertia
 * from this payload. Rebuild resource/physics parameters from the class. */
int rf_vehicle_checkpoint_validate(const rf_vehicle_checkpoint *);
int rf_vehicle_checkpoint_encode(const rf_vehicle_checkpoint *,void *,uint32_t);
int rf_vehicle_checkpoint_decode(const void *,uint32_t,rf_vehicle_checkpoint *);
#endif
