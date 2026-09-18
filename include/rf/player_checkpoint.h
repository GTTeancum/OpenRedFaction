#ifndef RF_PLAYER_CHECKPOINT_H
#define RF_PLAYER_CHECKPOINT_H
#include "rf/campaign.h"
#include "rf/vehicle_checkpoint.h"
#define RF_PLAYER_CHECKPOINT_BYTES 544u
/* Same-level, living, standing, settled player only. No transient actions,
 * timers, resources, attachments, world/mission state or physical
 * fit test. Caller must validate supported gameplay mode and restored-world
 * body clearance before publication. Not the original game's save ABI. */
typedef struct rf_player_checkpoint {
    rf_campaign_player_state player;
    float position[3],body_angles[3],eye_angles[3],velocity[3];
} rf_player_checkpoint;
typedef struct rf_player_checkpoint_catalog {
    uint32_t hash,count;
    uint8_t supported[64];
    rf_weapon_acquire_definition weapons[64];
    int32_t reserve_capacity[32];
    float health_capacity,armor_capacity;
} rf_player_checkpoint_catalog;
/* Caller catalog errors => RF_RANGE, invalid saved state => RF_FORMAT.
 * No allocation; errors leave outputs unchanged. All buffers/owners disjoint.
 * Canonical standing pose stores body yaw in[-2pi,2pi], eye pitch in[-pi/2,pi/2],
 * other angle components zero. No normalization/clamping of malformed input.
 * Residual standing velocity must be finite and <=.001 per axis. RFPL v2 uses
 * reserved bytes68..79 for it; v1 remains readable with zero velocity. The
 * encoder retains v1 for bitwise zero velocity, preserving existing fixtures.
 * Derived bases optional (both may be NULL), each9 row-major floats. */
int rf_player_checkpoint_validate(const rf_player_checkpoint *,const rf_player_checkpoint_catalog *,float body[9],float eye[9]);
int rf_player_checkpoint_encode(const rf_player_checkpoint *,const rf_player_checkpoint_catalog *,void *,uint32_t);
int rf_player_checkpoint_decode(const void *,uint32_t,const rf_player_checkpoint_catalog *,rf_player_checkpoint *);
/* RFPL3 driver-only representation: same544 bytes, version3 and word12=1.
 * Never contains a registry handle. Requires a structurally valid occupied
 * RFVC, including a dead host awaiting safe exit. Living player and existing
 * inventory rules still apply. Independent player velocity must be zero;
 * vehicle velocity belongs to RFVC. Stored canonical angles are PREBOARDING
 * EXIT LOOK, not seated orientation. Position is seated body origin: caller
 * MUST validate it against the actual authored seat in the restored RFVC pose
 * and restored-world placement before publishing either state. These codecs
 * do not establish safe dead-host restore, clearance or attachment identity.
 * Existing standing APIs continue rejecting RFPL3. Error atomic, no heap. */
int rf_player_checkpoint_seated_validate(const rf_player_checkpoint *,const rf_player_checkpoint_catalog *,const rf_vehicle_checkpoint *);
int rf_player_checkpoint_seated_encode(const rf_player_checkpoint *,const rf_player_checkpoint_catalog *,const rf_vehicle_checkpoint *,void *,uint32_t);
int rf_player_checkpoint_seated_decode(const void *,uint32_t,const rf_player_checkpoint_catalog *,const rf_vehicle_checkpoint *,rf_player_checkpoint *);
#endif
