#ifndef RF_MOTION_H
#define RF_MOTION_H
#include "rf/vpp.h"
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
