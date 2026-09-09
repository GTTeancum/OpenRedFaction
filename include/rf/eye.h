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
typedef struct rf_first_person_pose {
    float position[3],body_orientation[3][3],eye_orientation[3][3];
} rf_first_person_pose;
/* 40d88c..40d8be: camera pose before 40db70 effects and 48a190 commit.
 * Copies distinct body/eye orientations. Caller supplies the computed eye;
 * this does not derive an eye height, resolve a player handle or apply effects.
 * Finite inputs only; errors preserve output, input/output may alias. */
int rf_first_person_pose_copy(const float eye[3],const float body_orientation[3][3],
    const float eye_orientation[3][3],rf_first_person_pose *result);
typedef struct rf_camera_effect_state {float strength,duration;int32_t deadline;} rf_camera_effect_state;
/* 41d980 per-player reset: deadline=now, not disabled (-1). */
int rf_camera_effect_reset(rf_camera_effect_state *state,int32_t now_ms);
/* 40db70 before 4fae00/4fc960. Returns the cone cosine from PRE-decay strength.
 * Active effects decay strength by original binary32 factor when <1000ms
 * remain. Expired effects preserve cosine and state. No random direction or
 * orientation rebuild is performed here. Errors preserve every output. */
int rf_camera_effect_step(rf_camera_effect_state *state,int32_t now_ms,float *cosine,uint32_t *active);
#endif
