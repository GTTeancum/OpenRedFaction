#ifndef RF_MOTION_H
#define RF_MOTION_H
#include "rf/vpp.h"
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
/* Reconstructs 0x53a130 on decoded keys. Tick units remain caller-defined.
 * Empty tracks return zero, endpoints clamp; keys must have increasing ticks.
 * Non-finite fields and overflow fail without changing output. */
int rf_motion_sample_position(const rf_motion_position_key *keys, uint32_t count, int32_t tick, float out[3]);
#endif
