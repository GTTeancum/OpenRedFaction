#ifndef RF_PLAYER_CHECKPOINT_H
#define RF_PLAYER_CHECKPOINT_H
#include "rf/campaign.h"
#define RF_PLAYER_CHECKPOINT_BYTES 544u
/* Same-level, living, standing, settled player only. No transient actions,
 * velocities, timers, resources, attachments, world/mission state or physical
 * fit test. Caller must validate supported gameplay mode and restored-world
 * body clearance before publication. Not the original game's save ABI. */
typedef struct rf_player_checkpoint {
    rf_campaign_player_state player;
    float position[3],body_angles[3],eye_angles[3];
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
 * Derived bases optional (both may be NULL), each9 row-major floats. */
int rf_player_checkpoint_validate(const rf_player_checkpoint *,const rf_player_checkpoint_catalog *,float body[9],float eye[9]);
int rf_player_checkpoint_encode(const rf_player_checkpoint *,const rf_player_checkpoint_catalog *,void *,uint32_t);
int rf_player_checkpoint_decode(const void *,uint32_t,const rf_player_checkpoint_catalog *,rf_player_checkpoint *);
#endif
