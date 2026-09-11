#ifndef RF_MOVEMENT_H
#define RF_MOVEMENT_H
#include "rf/vpp.h"
typedef struct rf_movement_descriptor {
    uint32_t enabled,index,translation[3],rotation[3];
} rf_movement_descriptor;
/* Original4339d0 selection plus422dfa creation flag adjustment. Disabled
 * low-byte enabled or out-of-range requested index selects slot0 (even when
 * slot0 is disabled). Descriptor mode10 clears body flag10. Returns slot;
 * NULL descriptors/body_flags returns0 without mutation. */
uint32_t rf_movement_start(const rf_movement_descriptor descriptors[16],int32_t requested,uint32_t *body_flags);

/* Original4281a0 descriptor/flag operation: set body bit1, choose slot8
 * when class724 bit400 is set, otherwise3; disabled low byte falls back0.
 * Caller installs the returned descriptor and identity orientation85c.
 * No creation-mode10 flag adjustment. NULL inputs return0 unchanged. */
uint32_t rf_movement_fall(const rf_movement_descriptor descriptors[16],uint32_t class_flags,uint32_t *body_flags);

typedef struct rf_movement_settings {
    float response, speed; /* Entity +8c and +8c0. Response meaning pending. */
    int32_t mode; /* Entity +8c4, read by animation selection. */
} rf_movement_settings;
typedef struct rf_movement_config {
    uint32_t flags; /* Entity info +724. */
    float base_speed, slow_factor, alternate_factor, response; /* Info +50..5c. */
    float override_slow, override_normal; /* Globals 594590 / 59458c. */
} rf_movement_config;
/* Complete numeric 0x427450, including flag predicate 0x40a210.
 * A forced action other than -1 forces mode zero. Request zero/2 selects
 * modes zero/2; all other requests select mode one. override_enabled is
 * original byte 64ecb9; entity_scale is +98. Invalid data preserves state. */
int rf_movement_set_mode(rf_movement_settings *state, const rf_movement_config *config,
                         int32_t requested, int32_t forced_action, float entity_scale,
                         uint8_t override_enabled);
/* Complete 433a50 translation-axis selection: 1 eye, 2 body, 3 parent;
 * all other values select a zero axis. Three 3x3 matrices in original storage
 * order; no normalization. Output may alias input or matrices. */
int rf_movement_transform(const uint32_t reference[3],const float input[3],
    const float eye[9],const float body[9],const float parent[9],float output[3]);
/* 49f6cd..49f753: scale local input by class acceleration, transform it, then
 * clamp transformed length to acceleration. Repeated passes bypass this stage. */
int rf_movement_acceleration(const uint32_t reference[3],const float input[3],float acceleration,
    const float eye[9],const float body[9],const float parent[9],float output[3]);
#endif
