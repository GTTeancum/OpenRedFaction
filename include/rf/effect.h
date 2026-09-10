#ifndef RF_EFFECT_H
#define RF_EFFECT_H
/* Original 4c1d00 scans all 64 vclip name slots; for ASCII names, returns
 * the first match. Empty/unknown names return -1. Names are NUL-terminated;
 * null slots represent unused empty names. Caller owns definition storage. */
int rf_vclip_name_lookup(const char *const names[64],const char *name);
#include "rf/timer.h"
typedef struct rf_effect_switch {
    uint8_t enabled,reserved[3]; /* Original +140; reserved bytes preserved. */
    int32_t started; /* +154 deadline. */
} rf_effect_switch;
typedef struct rf_effect_pair {
    rf_effect_switch *objects[2][2]; /* Default / override pair. */
} rf_effect_pair;
/* 48f130 with 4973b0/4973d0. Nonzero low byte of override selects pair 1.
 * Both objects must exist or neither is changed. enabled is an int boolean;
 * enabling stamps now only when the object's byte was not exactly one.
 * Disabling leaves timestamps intact. Table ownership/rendering are separate.
 * Bounds checks reject original out-of-table indices. */
int rf_effect_set_enabled(rf_effect_pair *pairs,uint32_t count,int32_t index,
    uint32_t override_mode,int32_t enabled,int32_t now_ms);
#endif
