#ifndef RF_EYE_H
#define RF_EYE_H
#include "rf/vpp.h"
typedef struct rf_eye_input {
    float position[3], orientation[3][3], standing_offset[3], crouching_offset[3];
    uint32_t flags;
    int32_t eye_tag, current_state, previous_state;
    float transition_duration, transition_elapsed;
} rf_eye_input;
/* Reconstructs the non-linked branch of RF.exe 0x4194e0. Caller supplies
 * entity/model data; RF_NOT_FOUND means the animated eye-tag branch is needed.
 * State ids 8/9/10 select crouching; transitions are not clamped. */
int rf_eye_position(const rf_eye_input *input, float result[3]);
#endif
