#ifndef RF_EFFECT_H
#define RF_EFFECT_H
#include "rf/random.h"
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
/* 496f60/504db0: consume one CRT draw, use draw/32768, select on/off
 * timings by nonzero low byte, then clamp rounded duration to 0.1 seconds.
 * Finite authored timing domain. Errors preserve RNG and output. */
int rf_particle_cycle_duration(const rf_particle_cycle *cycle,unsigned enabled,
    rf_random_state *random,float *duration);
typedef struct rf_particle_emitter_clock {
    uint32_t flags,enabled;float elapsed,duration;
} rf_particle_emitter_clock;
typedef struct rf_particle_emitter_actions {uint32_t toggled,timer_checked,emit;} rf_particle_emitter_actions;
/* 4972f0 timing/emission decision, before parent attachment update. Supplied
 * timer_due is the result of the original timer query; continuous emitters
 * bypass it. Finite clock domain. Global enable and enabled use low bytes.
 * One toggle at most per update, discarding excess elapsed time. No emission
 * callback runs here; RNG advances only for a new phase. Errors preserve all
 * outputs/state. Parent motion, spawn timer reset and particles are separate. */
int rf_particle_emitter_tick(const rf_particle_cycle *cycle,uint32_t global_enabled,float dt,
    uint32_t timer_due,rf_random_state *random,rf_particle_emitter_clock *clock,
    rf_particle_emitter_actions *actions);
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
/* Prepare the authored direction through original 4faaf0 normalization.
 * Copies all other metadata unchanged; supports in-place preparation.
 * Zero/nonfinite vectors retain original IEEE NaN behavior, not a fallback
 * direction. No bitmap resolution, allocation or emitter creation. */
int rf_particle_definition_prepare(const rf_particle_definition *authored,
    rf_particle_definition *result);
/* Runtime spawn packet recovered from 496840. Resource/room/emitter values
 * are caller-owned 32-bit handles, never host pointers. copied_48 semantics
 * remain unknown and are deliberately preserved without interpretation. */
typedef struct rf_particle_spawn {
    float position[3],velocity[3],radius,growth,acceleration,gravity_scale,life;
    uint32_t bitmap,frame_count,color,color_destination,flags,secondary;
    float age_to_finish_vbm;
    uint32_t copied_48;
} rf_particle_spawn;
typedef struct rf_particle {
    uint32_t next,previous,owner;
    float position[3],velocity[3],age;
    uint32_t color,color_destination,color_current;
    float life,radius,growth,acceleration,gravity;
    uint32_t bitmap;
    uint16_t frame_count,secondary;
    uint8_t pool,reserved_51[3];
    float orientation;
    uint32_t flags;
    float age_to_finish_vbm;
    uint32_t copied_48,room,emitter;
    float previous_position[3];
} rf_particle;
/* 496840 record initialization after a free node has been obtained. Preserves
 * list links and untouched bytes. Pool 0/1 only; caller supplies resolved
 * handles. No allocation, linking or count mutation. Null/invalid arguments
 * preserve output/RNG. Source and output must not overlap. */
int rf_particle_initialize(const rf_particle_spawn *spawn,uint32_t pool,
    uint32_t owner,uint32_t room,uint32_t emitter,rf_random_state *random,
    rf_particle *particle);

/* 494baf..494c8f frame selection, before bitmap binding/drawing. Returns a
 * zero-based frame; original signed frame counts <=1 select zero. Finite,
 * nonnegative age and valid positive denominators required for animation.
 * Unsupported conversion range preserves output. No resource access. */
int rf_particle_frame_index(const rf_particle *particle,uint32_t *frame);

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
