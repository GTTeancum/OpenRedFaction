#ifndef RF_TURN_H
#define RF_TURN_H
#include "rf/motion.h"
#include "rf/movement.h"
#include "rf/timer.h"
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
#endif
