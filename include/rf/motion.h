#ifndef RF_MOTION_H
#define RF_MOTION_H
#include "rf/vpp.h"
typedef struct rf_motion_active_slot { int32_t motion, tick; float weight; } rf_motion_active_slot;
typedef struct rf_motion_slot_state {
    uint32_t count;
    rf_motion_active_slot slots[16];
    int32_t freeze_slot, primary_slot, dominant_slot;
} rf_motion_slot_state;
/* Original 0x51c090: remove the first matching motion, compact slots and
 * repair selected indices. references belongs to that motion; decrement clamps
 * at zero. Absent motions are successful no-ops. Invalid state is unchanged. */
int rf_motion_remove_slot(rf_motion_slot_state *state, int32_t motion, int32_t *references);
typedef struct rf_motion_completion_state {
    rf_motion_slot_state active;
    uint32_t frozen, primary_flag, primary_words[2];
    float primary_vectors[2][3]; /* Original +1d20/+1d2c; semantics pending. */
} rf_motion_completion_state;
/* Original update completion pass, before zero-weight slot removal. End ticks
 * and looping bits are indexed by active slot. Invalid state stays unchanged. */
int rf_motion_complete_slots(rf_motion_completion_state *state, const int32_t *end_ticks, uint32_t looping_mask);
/* Update 0x51ba80: truncate elapsed * 30 * 160 with no intermediate float spill.
 * Rejects non-finite input and results outside int32; preserves output on error. */
int rf_motion_elapsed_ticks(float elapsed, int32_t *out);
typedef struct rf_motion_phase_slot {
    int32_t duration;
    float weight;
    uint32_t looping;
} rf_motion_phase_slot;
typedef struct rf_motion_phase_result {
    float phase;
    int32_t dominant_slot;
    uint32_t wrapped;
} rf_motion_phase_result;
/* Shared loop phase from 0x51ba80. Forward playback, <=16 active slots;
 * non-loop slots do not affect phase. Does not update cursors/events. */
int rf_motion_advance_phase(const rf_motion_phase_slot *slots, uint32_t count, float phase,
                            int32_t delta_ticks, rf_motion_phase_result *out);
typedef struct rf_motion_loop_result { int32_t tick; uint32_t event_mask; } rf_motion_loop_result;
/* Original 0x51bc78..0x51bd1b: floor phase*duration, add start, test two
 * markers. Apply event_mask only for the dominant looping slot; OR into flags. */
int rf_motion_map_loop(int32_t start, int32_t end, float phase, int32_t previous,
                       const int32_t markers[2], int wrapped, rf_motion_loop_result *out);
typedef struct rf_motion_weight_envelope {
    float weight;
    int32_t start_tick, end_tick, fade_in, fade_out;
} rf_motion_weight_envelope;
/* Original 0x539e10. Bypass disables time fades but retains the small-weight
 * cutoff. Invalid durations, non-finite weights and overflow preserve output. */
int rf_motion_sample_weight(const rf_motion_weight_envelope *envelope, int32_t tick, int bypass, float *out);
/* Non-loop slot advance/primary selection within the mixed-loop update branch.
 * Candidate bypass comes from descriptor flags[active slot index], while
 * primary bypass comes from flags[primary motion ID], matching original code.
 * Envelopes describe the descriptor-selected comparison bone. */
int rf_motion_advance_candidate(rf_motion_completion_state *state, uint32_t index, int32_t delta,
                                const rf_motion_weight_envelope *candidate_envelope, int candidate_bypass,
                                const rf_motion_weight_envelope *primary_envelope, int primary_bypass);
typedef struct rf_motion_playback_state {
    rf_motion_completion_state completion;
    float phase;
    uint32_t generation, event_mask;
} rf_motion_playback_state;
typedef struct rf_motion_playback_resource {
    rf_motion_weight_envelope comparison;
    uint32_t looping;
    int32_t markers[2], references;
} rf_motion_playback_resource;
/* Loaded type-two query 0x51c270: max(end-cursor,0) * binary32(1/160)
 * * binary32(1/30), with no intermediate float spill. Absent slots return
 * zero; weight, frozen state and loop flags do not affect this query. */
int rf_motion_remaining(const rf_motion_playback_state *state, const rf_motion_playback_resource *resources,
                         uint32_t resource_count, int32_t motion, float *seconds);
/* Entity action predicate 0x428d10, type-two character path. Actions [0,44]
 * map to registered motions; -1 is absent. Out-of-range actions return false.
 * This tests remaining time, not weight. Invalid data preserves output. */
int rf_motion_action_active(const rf_motion_playback_state *state, const rf_motion_playback_resource *resources,
                             uint32_t resource_count, const int32_t actions[45], int32_t action, int *active);
/* 0x428c90 through loaded type-two start. sound_class receives the requested
 * sound-class ID when play_sound's low byte is exactly one, otherwise -1.
 * The caller resolves/plays that class after a successful call; sound-class
 * selection and audio output are not performed here. Sound may be requested
 * even when weight zero or loop byte one prevents the motion from starting.
 * Invalid/unmapped actions are no-ops. Errors preserve output and playback. */
int rf_motion_start_action(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                           uint32_t resource_count, const int32_t actions[45], const int32_t sounds[45],
                           int32_t action, float weight, int freeze, int play_sound, int32_t *sound_class);
typedef struct rf_motion_controller {
    int32_t current, next;
    float duration, elapsed;
    int32_t override_state;
    uint32_t override_enabled;
} rf_motion_controller;
/* 0x42a580: request a logical state, falling back to state zero when absent
 * or out of range. Retargeting retains the nearer existing endpoint and
 * rescales progress; it does not suppress repeated requests. Finite forward
 * durations and progress within the existing transition are required. */
int rf_motion_request_state(rf_motion_controller *controller, const int32_t motions[23],
                            int32_t requested, float duration);
/* 0x42a650: current OR next matches, regardless of transition duration. */
int rf_motion_has_state(const rf_motion_controller *controller, int32_t requested);
typedef struct rf_motion_movement {
    float vector[3]; /* Entity +714, compared componentwise with zero. */
    int32_t mode, direction; /* *(entity+858)+4 and entity+8c4. */
    int32_t idle_state, move_state, alternate_state; /* Selected earlier. */
} rf_motion_movement;
/* Selector tail 0x41f7c1..0x41f94f, after physics side effects. Modes 4/7
 * select logical 18/19; other modes use caller-supplied candidates. Only
 * requests states absent from current AND next, using a .25-second blend.
 * Upstream priority selection, AI/physics effects and candidate selection
 * are not included. Finite vectors and candidate indices [0,22] required. */
int rf_motion_select_movement(rf_motion_controller *controller, const int32_t motions[23],
                              const rf_motion_movement *movement);
enum { RF_MOTION_STANCE_NONE=0, RF_MOTION_STANCE_CROUCH=1, RF_MOTION_STANCE_STAND=2 };
enum { RF_MOTION_CROUCH_NO=0, RF_MOTION_CROUCH_YES=1, RF_MOTION_CROUCH_QUERY=2 };
typedef struct rf_motion_crouch_input {
    int32_t motion_9, weapon, excluded_weapons[2], mode, action;
    int32_t state_740, stance_7bc, behavior_554, property_4ec;
    uint32_t flags_7d0, global_64ecb9, global_6fc4d8, target_present;
} rf_motion_crouch_input;
/* 402ab0 and 429ae0 through its visibility-query boundary. target_present
 * describes an already resolved 40a0e0 object. Global fields are byte values.
 * QUERY requires the original target eye / transformed class offset ray test
 * (498e80, mask 0x27); only a return byte exactly one denies crouch.
 * Do not pass QUERY as stance eligibility. No collision query is fabricated.
 * Invalid byte/boolean views leave result unchanged. */
int rf_motion_crouch_eligibility(const rf_motion_crouch_input *input,uint32_t *result);
typedef struct rf_motion_stance_decision {int32_t handled;uint32_t effect;} rf_motion_stance_decision;
/* 0x41f743..0x41f7c1 after eligibility predicates: eligible is the resolved
 * 402ab0 && 429ae0 result. entity_flags is +810; bit 400 is physical crouch.
 * Requests the supplied special state, then asks for crouch geometry only once
 * that state is current with zero transition duration. Ineligible actors ask
 * to stand when physically crouched, then continue movement selection.
 * Caller MUST apply a requested effect through the corresponding physics path
 * (4289d0/428a60) before continuing; this function does not change colliders.
 * Finite controller times, eligible 0/1 and state [0,22] required. Errors leave
 * controller/decision unchanged. handled=1 stops the remaining selector. */
int rf_motion_select_stance(rf_motion_controller *controller,const int32_t motions[23],
    int32_t special_state,uint32_t eligible,uint32_t entity_flags,rf_motion_stance_decision *decision);
typedef struct rf_motion_priority {
    int32_t forced_state;
    uint32_t flags, physics_flags;
    int32_t mode, class_type, action, linked_class_type;
    uint32_t linked_flags;
    int32_t linked_occupant_handle, entity_handle;
    float velocity[3];
    uint32_t linked_present;
} rf_motion_priority;
/* 0x41f400..0x41f5ad priority selection. Linked fields describe an already
 * resolved valid type-zero entity; handle lookup remains the caller's job.
 * class_type values are resolved 0x486c90 classifications. handled=0 means
 * continue with candidate/physics selection; handled=1 stops selection even
 * when the requested motion is absent. Finite velocities required. */
int rf_motion_select_priority(rf_motion_controller *controller, const int32_t motions[23],
                              const rf_motion_priority *priority, int *handled);
/* Post-selector block 0x41f2b6..0x41f3f3 only. The caller selects logical
 * states and passes their 23 registered motion IDs (-1 means missing).
 * Stops looping weights before assigning the current/blended/override motion.
 * Does not run locomotion selection, gating, or advance playback cursors.
 * Forward finite time; invalid input leaves controller/playback/references intact. */
int rf_motion_apply_controller(rf_motion_controller *controller, const int32_t motions[23],
                               float elapsed, rf_motion_playback_state *state,
                               rf_motion_playback_resource *resources, uint32_t resource_count);
/* Complete 0x51ba80 forward update. Resources are indexed by registered motion
 * ID, with the comparison envelope for the descriptor-selected bone. Active
 * IDs must be unique, as enforced by the original motion insertion path;
 * durations must be positive and active weights finite and nonnegative.
 * Generation wraps at 16 bits; marker events remain sticky until caller clears
 * them. Invalid input leaves state and resource reference counts unchanged. */
int rf_motion_update(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                     uint32_t resource_count, float elapsed);
/* Loaded-resource control paths 0x51c190/0x51c1c0 with shared insertion
 * 0x51bfd0. The caller opens archives before registration; lazy loading is not
 * performed here. Existing slots are reused without another reference.
 * A full table rejects a new slot with RF_RANGE and leaves state unchanged. */
int rf_motion_set_weight(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                         uint32_t resource_count, int32_t motion, float weight);
/* Restart only when weight>0 and loop flag !=1, matching the original exact
 * byte comparison. A low byte of one in freeze designates end freezing. */
int rf_motion_start(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                    uint32_t resource_count, int32_t motion, float weight, int freeze);
/* 0x51c340/0x51c390: zero weights for exact loop byte 1 / byte 0.
 * Both clear freeze designation/frozen; non-loop stop also clears primary.
 * Slots, references, cursors, generation and primary auxiliaries remain until
 * later update/control work. No immediate slot removal occurs. */
int rf_motion_stop_looping(rf_motion_playback_state *state, const rf_motion_playback_resource *resources, uint32_t resource_count);
int rf_motion_stop_nonlooping(rf_motion_playback_state *state, const rf_motion_playback_resource *resources, uint32_t resource_count);
/* 0x51c3f0: zero the first matching slot and clear primary unconditionally
 * when found. Absent motions leave everything untouched, including primary. */
int rf_motion_stop_slot(rf_motion_playback_state *state, int32_t motion);
/* 0x51b500 per-bone contribution selection. Envelopes and looping bits are
 * indexed by active slot; output retains slot order, with zero for excluded
 * contributions. Positive weights are normalized by the original sum. */
int rf_motion_bone_weights(const rf_motion_slot_state *active, const rf_motion_weight_envelope *envelopes,
                           uint32_t looping_mask, float weights[16]);
/* Original 0x417e90: signed packed components scaled without normalization.
 * Input is eight little-endian bytes; output is quaternion x, y, z, w. */
int rf_motion_decode_rotation(const void *packed, size_t bytes, float out[4]);
/* Original packed interpolation 0x51a000, with caller-normalized t in [0,1].
 * Components remain signed 16-bit values, including original wrap and truncation. */
int rf_motion_interpolate_rotation(const int16_t a[4], const int16_t b[4], float t, int16_t out[4]);
typedef struct rf_motion_rotation_key {
    int32_t tick;
    int16_t packed[4];
    int8_t incoming, outgoing;
    uint8_t reserved[2];
} rf_motion_rotation_key;
/* Reconstructs 0x539ed0 and easing 0x53a040. Empty tracks return identity.
 * Single keys decode directly, avoiding the original's read past the last key
 * at/before its tick. Increasing ticks and nonnegative easing are required. */
int rf_motion_sample_rotation(const rf_motion_rotation_key *keys, uint32_t count, int32_t tick, float out[4]);
typedef struct rf_motion_position_key {
    int32_t tick;
    float position[3], incoming[3], outgoing[3];
} rf_motion_position_key;
/* Evaluate an interior segment, including its t=0 boundary, without endpoint
 * copying. This preserves signed zero when selecting keys from an archive. */
int rf_motion_interpolate_position(const rf_motion_position_key *previous, const rf_motion_position_key *next, float t, float out[3]);
/* Reconstructs 0x53a130 on decoded keys. Tick units remain caller-defined.
 * Empty tracks return zero, endpoints clamp; keys must have increasing ticks.
 * Non-finite fields and overflow fail without changing output. */
int rf_motion_sample_position(const rf_motion_position_key *keys, uint32_t count, int32_t tick, float out[3]);
#endif
