#ifndef RF_TURN_H
#define RF_TURN_H
#include "rf/motion.h"
#include "rf/movement.h"
#include "rf/timer.h"

/* Historical rf_turn names refer to candidate helper 0x41f9f0. The original
 * action-name initializer 0x4181d0 identifies 17/18 as sidesteps and 19/20 as
 * rolls; the precise meaning of entity flag +7bc remains unrecovered. */
typedef struct rf_turn_effects {
    rf_movement_settings movement;
    int32_t deadlines[5]; /* Entity +79c,+4d0,+4d4,+744,+798. */
    uint32_t turning; /* +7bc. */
    int32_t move_candidate, alternate_candidate;
} rf_turn_effects;
typedef struct rf_turn_context {
    rf_movement_config movement;
    int32_t forced_action;
    float entity_scale;
    uint32_t override_enabled;
    int32_t now_ms;
} rf_turn_context;
typedef struct rf_turn_direction_input {
    uint32_t info_flags, entity_flags; /* Info +728, entity +7d0. */
    int32_t count; /* Entity +588. */
    float vector[3], orientation[9]; /* Entity +7a0 and +48. */
} rf_turn_direction_input;
typedef struct rf_turn_direction_result { float local[3]; uint32_t eligible; } rf_turn_direction_result;
/* Direction gate within 0x41f9f0: normalize for the third-basis dot test, but
 * transform the original vector to local coordinates. Length >=.1 and dot
 * in [-.5,.5] are accepted. Outputs zero when gated off. Finite inputs only;
 * unsafe original zero-vector normalization is avoided. No entity mutations. */
int rf_turn_direction(const rf_turn_direction_input *input, rf_turn_direction_result *result);
/* Selected-turn effects 0x41fbdc..0x41fc83. Positive local_x chooses action
 * 20, otherwise 19; candidates become 9/9. Starts weight 1 without freezing,
 * requests its sound class, marks turning, sets five 1200ms deadlines, applies
 * movement request 1. Sound selection/playback is caller-owned. Decision
 * predicates and the preceding state-reset helper are not included.
 * Invalid input preserves effects, playback, references and sound output. */
int rf_turn_apply_selected(rf_turn_effects *effects, rf_motion_playback_state *playback,
                           rf_motion_playback_resource *resources, uint32_t resource_count,
                           const int32_t actions[45], const int32_t sounds[45],
                           const rf_turn_context *context, float local_x, int32_t *sound_class);
typedef struct rf_turn_finish_input {
    float local_x;
    uint32_t eligible;
    int32_t weapon, preferred_weapon, behavior; /* +2a4, global 872114, +554. */
    uint32_t network_mode; /* Byte global 6fc4d8. */
} rf_turn_finish_input;
/* Remaining branches 0x41fc84 onward, when the selected-turn branch was not
 * taken. Eligible directions with both action mappings 17/18 use movement
 * request zero and candidates 3/5; start a side action only if neither is
 * active. Otherwise use movement request one and select 2/4 or 3/5 from the
 * weapon/behavior/global test. Timers and turning flag stay unchanged.
 * Returned sound requests are caller-owned, as in rf_turn_apply_selected. */
int rf_turn_finish_candidates(rf_turn_effects *effects, rf_motion_playback_state *playback,
                              rf_motion_playback_resource *resources, uint32_t resource_count,
                              const int32_t actions[45], const int32_t sounds[45],
                              const rf_turn_context *context, const rf_turn_finish_input *input,
                              int32_t *sound_class);
typedef struct rf_turn_actor {
    rf_turn_direction_input direction;
    int32_t mode;
    uint32_t info_flags; /* Info +724, consistent with movement config. */
    int32_t weapon, preferred_weapon, behavior;
    uint32_t network_mode;
    float source[3], target[3]; /* Entity +7d4 and +6fc. */
    uint32_t target_valid, trigger_a, trigger_b; /* Bytes +6f8,+53c,+53d. */
} rf_turn_actor;
typedef int (*rf_turn_reset_fn)(void *user);
/* Candidate helper 0x41f9f0 control flow. The required reset callback implements
 * 0x41ae70 when reached and may update actor/playback/context through user.
 * A missing required callback returns RF_NOT_FOUND, not implicit success.
 * Local direction is captured before reset; target/trigger fields are read
 * afterward. Callback side effects are not rolled back on subsequent failure.
 * Audio requests remain deferred to caller; this is not a complete entity
 * runtime until reset and audio adapters are supplied. */
int rf_turn_update(rf_turn_effects *effects, rf_motion_playback_state *playback,
                    rf_motion_playback_resource *resources, uint32_t resource_count,
                    const int32_t actions[45], const int32_t sounds[45],
                    const rf_turn_context *context, const rf_turn_actor *actor,
                    rf_turn_reset_fn reset, void *user, int32_t *sound_class);
#endif
