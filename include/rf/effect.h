#ifndef RF_EFFECT_H
#define RF_EFFECT_H
/* Original 4c1d00 scans all 64 vclip name slots; for ASCII names, returns
 * the first match. Empty/unknown names return -1. Names are NUL-terminated;
 * null slots represent unused empty names. Caller owns definition storage. */
int rf_vclip_name_lookup(const char *const names[64],const char *name);
typedef struct rf_particle_text_flags {
    unsigned emitter,particle,secondary;
} rf_particle_text_flags;
/* Original 497590 flag-string spans: case-insensitive ASCII substring matching.
 * Boolean and packed numeric fields are applied separately by the loader.
 * Both strings must be NUL-terminated; NULL arguments preserve output. */
int rf_particle_flags_read(const char *emitter,const char *particle,rf_particle_text_flags *result);
/* Original 497afe..497ba2: optional bounciness, stickiness, swirliness,
 * damage_factor values in that order. Presence bits 0..3 select fields;
 * each low nibble is ORed into existing flags, without clamping or clearing.
 * NULL arguments return RF_RANGE without modifying flags. */
int rf_particle_flags_pack(rf_particle_text_flags *flags,unsigned present,const int values[4]);
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
