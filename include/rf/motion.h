#ifndef RF_MOTION_H
#define RF_MOTION_H
#include "rf/vpp.h"
typedef struct rf_motion_position_key {
    int32_t tick;
    float position[3], incoming[3], outgoing[3];
} rf_motion_position_key;
/* Reconstructs 0x53a130 on decoded keys. Tick units remain caller-defined.
 * Empty tracks return zero, endpoints clamp; keys must have increasing ticks.
 * Non-finite fields and overflow fail without changing output. */
int rf_motion_sample_position(const rf_motion_position_key *keys, uint32_t count, int32_t tick, float out[3]);
#endif
