#ifndef RF_EYE_H
#define RF_EYE_H
#include "rf/vpp.h"
#include "rf/random.h"
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
/* Resolved player portion of 416450/50cc40: packed bytes at player+10d0,
 * full alpha word at +10d4. 4a7520 supplies (255,0,0,128) for damage.
 * This sets state only; flash decay, compositing and player lifetime are separate. */
typedef struct rf_screen_flash {uint8_t rgba[4];uint32_t alpha;} rf_screen_flash;
int rf_screen_flash_set(rf_screen_flash *state,uint32_t red,uint32_t green,uint32_t blue,uint32_t alpha);
/* Resolved actor portion of 40e0b0: stores strength/duration and sets deadline
 * from trunc(duration*1000). Signed duration supported within one timer period.
 * Caller resolves view/player/actor ownership. Errors preserve state. */
int rf_camera_effect_start(rf_camera_effect_state *state,float strength,float duration,int32_t now_ms);
/* 41d980 per-player reset: deadline=now, not disabled (-1). */
int rf_camera_effect_reset(rf_camera_effect_state *state,int32_t now_ms);
/* 40db70 before 4fae00/4fc960. Returns the cone cosine from PRE-decay strength.
 * Active effects decay strength by original binary32 factor when <1000ms
 * remain. Expired effects preserve cosine and state. No random direction or
 * orientation rebuild is performed here. Errors preserve every output. */
int rf_camera_effect_step(rf_camera_effect_state *state,int32_t now_ms,float *cosine,uint32_t *active);
/* Complete resolved-entity 40db70 effect, including 4fae00 and 4fc960.
 * draw0/draw1 are the two original rand() outputs (0..32767), used only if
 * active. This does not own or advance a global RNG. Orientation is row-major
 * right/up/forward. Finite inputs required; nonfinite/degenerate generated
 * forward is a port error.
 * Errors preserve state/orientation/active. Expired effects ignore draws. */
int rf_camera_effect_apply(rf_camera_effect_state *state,int32_t now_ms,
    uint32_t draw0,uint32_t draw1,float orientation[9],uint32_t *active);
/* State-owning adapter: exactly two draws when active, none when expired.
 * Caller supplies the same stream used by other original-thread consumers.
 * No seed choice is inferred. Errors preserve state, RNG, orientation, active. */
int rf_camera_effect_apply_random(rf_camera_effect_state *state,int32_t now_ms,
    rf_random_state *random,float orientation[9],uint32_t *active);
/* Scalar portion of 49de50 through 49dfcd. Radians; positive finite dt.
 * Pending pitch/yaw are consumed; rotation command is cleared. Body pitch/
 * roll and eye yaw/roll are reset. Matrix rebuild and physics commit are caller
 * work; this API does not implement mouse/controller acquisition. */
typedef struct rf_look_state {
    float command[3],pending_pitch,pending_yaw;
    float body_angles[3],eye_angles[3],angular_velocity[3];
} rf_look_state;
int rf_look_update(rf_look_state *state,float angular_speed,float dt);
typedef struct rf_spawn_look_angles {float body[3],eye[3];} rf_spawn_look_angles;
/* 422e2c..422e82: original matrix angle extraction, yaw-only body, and
 * physics-frame projection filtered by rotation reference == 1. No command
 * clearing, pose construction or factory ownership. Finite inputs required;
 * errors preserve result. Matrices are row-major right/up/forward. */
int rf_look_spawn_angles(const float orientation[9],const float physics_orientation[9],
    const uint32_t rotation[3],rf_spawn_look_angles *result);
/* Original 4a0d70 eye matrix, including 4fc500 orthogonalization.
 * Pitch within +/-pi/2, yaw within +/-2pi; finite roll accepted but ignored.
 * Errors preserve output; angles/output may alias. */
int rf_look_orientation(const float angles[3],float orientation[9]);
typedef struct rf_look_pose {
    rf_look_state state;
    float body_orientation[9],eye_orientation[9];
} rf_look_pose;
/* 49de50 through 49e02c: angle update and distinct body/eye orientation.
 * Does not commit physics or auxiliary entity vectors. Error preserves result;
 * state may point into result. */
int rf_look_update_pose(const rf_look_state *state,float angular_speed,float dt,rf_look_pose *result);
#endif
