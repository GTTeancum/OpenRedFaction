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
typedef struct rf_particle_cycle {
    float on_time,on_variance,off_time,off_variance;
} rf_particle_cycle;
/* 49771b..4977c3: boolean low bytes must equal one. ORs emitter bits;
 * disabled alternation writes 1,0,1,0. Authored timing may alias output.
 * All pointers required; NULL arguments preserve outputs. */
int rf_particle_cycle_read(rf_particle_text_flags *flags,unsigned initially_on,
    unsigned alternate,const rf_particle_cycle *authored,rf_particle_cycle *result);
#include "rf/timer.h"
typedef struct rf_particle_definition {
    float position[3],direction[3]; /* Authored direction, before normalization. */
    float direction_random,min_velocity,max_velocity,spawn_radius;
    float min_spawn_delay,max_spawn_delay,min_life,max_life,min_radius,max_radius;
    float growth,acceleration,gravity_scale;
    rf_particle_cycle cycle;
    rf_particle_text_flags flags;
    char bitmap[64]; /* Owned filename; resource loading is separate. */
    uint8_t color[4],color_destination[4]; /* Authored RGBA components. */
    float age_to_finish_vbm;
    uint32_t has_age_to_finish_vbm;
} rf_particle_definition;
/* Bounded authored particle block ($pos through its last particle field).
 * No allocation; rejects unknown/duplicate fields, malformed or nonfinite
 * values and missing required fields. Output preserved on failure.
 * Accepts installed table labels with spaces/underscores, // comments.
 * This metadata reader is not the original general parser or resource loader.
 * Absent age is explicitly marked; no original runtime default is assumed. */
int rf_particle_definition_read(const void *text,uint32_t bytes,rf_particle_definition *result);
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
