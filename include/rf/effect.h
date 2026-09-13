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
/* Original 4fadb0 local +Z cone sampler. cosine_min must be in [-1,1].
 * Two CRT draws even for a zero-angle cone; caller rotates to world direction.
 * Errors preserve both output and RNG. This is not emitter execution. */
int rf_particle_cone_sample(float cosine_min,rf_random_state *random,float direction[3]);
/* Original 4fae00: 4fcfa0 basis, local sampler, then 4facb0 rotation.
 * Preserves the original near-vertical snap and nonunit-axis behavior; caller
 * supplies the resolved emitter direction. Errors preserve output and RNG. */
int rf_particle_cone_oriented(const float axis[3],float cosine_min,
    rf_random_state *random,float direction[3]);

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

typedef struct rf_particle_billboard_vertex {float position[3],uv[2];} rf_particle_billboard_vertex;
/* Original5590f0 world quad before558d40. kind:0 rejected,1 billboard
 * fallback,2 quad. Camera/forward are original world camera and third view
 * row. Only kind2 writes vertices. Finite inputs required; errors preserve outputs. */
int rf_corona_oriented_build(const float camera[3],const float forward[3],
    const float first[3],const float second[3],float size,
    rf_particle_billboard_vertex vertices[4],uint32_t *kind);
/* 558e30 world-space stretched diamond, before 558d40 transform/submission.
 * forward is the original view matrix third row. Previous-current displacement
 * squared below 0.001 requests the ordinary zero-angle billboard fallback.
 * Finite input and nonnegative radius required; errors preserve outputs. */
int rf_particle_stretch_build(const float position[3],const float previous[3],
    const float forward[3],float radius,rf_particle_billboard_vertex out[4],uint32_t *fallback);
/* 5552a0..555483 camera-space quad before clipping/depth bias. Positive bitmap
 * dimensions; finite center/angle/radius/scales and nonnegative radius. Output
 * follows original polygon submission order. Caller supplies camera scale.
 * No projection, clipping, blending or velocity-stretch path here. */
int rf_particle_billboard_build(const float center[3],float angle,float radius,
    uint32_t width,uint32_t height,const float scale[2],rf_particle_billboard_vertex out[4]);

typedef struct rf_particle_clip_environment {
    uint32_t enabled,depth_enabled,far_enabled;
    float far_distance;
} rf_particle_clip_environment;
typedef struct rf_particle_clipped_vertex {rf_particle_billboard_vertex vertex;uint32_t clip;} rf_particle_clipped_vertex;
typedef struct rf_particle_billboard_packet {
    rf_particle_clipped_vertex vertices[4];
    float depth;
    uint32_t clip_and,clip_or;
} rf_particle_billboard_packet;
/* 555230 through corner classification: original 5475d0 clip masks and
 * 518660 radius-dependent submission depth. Center is already camera-space.
 * Output retains unbiased corner Z for clipping/projection; depth is applied
 * only afterward. clip_and != 0 rejects the whole quad; clip_or != 0 needs
 * polygon clipping when enabled. No clipped vertices, projection or drawing.
 * Finite inputs required; errors preserve output. */
int rf_particle_billboard_prepare(const float center[3],float angle,float radius,
    uint32_t width,uint32_t height,const float scale[3],
    const rf_particle_clip_environment *clip,rf_particle_billboard_packet *packet);

/* Convex billboard clipping, original 549e00/549bd0 with UV-only draw flags.
 * Input is a convex quad classified with the same clip environment, from
 * billboard_prepare or the verified box projector. Caller rejects nonzero
 * output clip_and before projection, even when output count is nonzero.
 * Fixed index arrays and a 48-slot temporary pool bound scratch storage;
 * no heap or host pointers. The original reuse order is retained.
 * Output follows original vertex order. Nonzero clip_and rejects the result.
 * Near/user planes 1/64 are not generated by billboard classification and
 * are unsupported here. Bit 128 is preserved, as in the original clipper. */
typedef struct rf_particle_clipped_polygon {
    uint32_t count;
    rf_particle_clipped_vertex vertices[12];
    float depth;
    uint32_t clip_and,clip_or;
} rf_particle_clipped_polygon;
int rf_particle_billboard_clip(const rf_particle_clip_environment *environment,
    const rf_particle_billboard_packet *packet,rf_particle_clipped_polygon *polygon);

typedef struct rf_particle_projection {
    uint32_t clamp;
    float depth_offset,half_width,half_height;
    int32_t origin_x,origin_y;
} rf_particle_projection;
typedef struct rf_particle_projected_point {
    float camera[3],screen[2],reciprocal_z;
    uint8_t clip,flags,reserved[2];
} rf_particle_projected_point;
/* 5477a0 center/corner projection. Flags 1/2 cache projected/rejected state.
 * Clamping rejects z<=0 and bounds normalized XY to [0,2]. Depth offset
 * affects reciprocal Z only above its original 20*offset threshold; screen
 * coordinates retain the unbiased reciprocal. Finite input domain, no clip
 * polygon construction or raster depth conversion. Errors preserve point. */
int rf_particle_project(const rf_particle_projection *projection,rf_particle_projected_point *point);

/* 54f160 render-state tail, after cache/texture-stage handling. State/value
 * numbers retain original D3D8 encoding; backends must translate explicitly.
 * No allocation. Unknown modes preserve selector values and emit no writes
 * for that category. Caller retains GPU state for omitted writes. This does
 * not implement texture stages, mode caching, batching or GPU submission. */
/* Original startup 50be10/50be40, and selection 494c8f..494cbc. Callers
 * may supply runtime mode replacements; flag 0x2000 clears only depth mode. */
#define RF_PARTICLE_NORMAL_MODE 0x00118c42u
#define RF_PARTICLE_GLOW_MODE 0x06110c42u
/* Original50f9cd rate conversion and50f500 frame clock. Unsigned millisecond
 * subtraction wraps; output is zero-based, -1 for finished nonlooping playback.
 * Count1..255, nonnegative signed fps domain and int32 phase required. Mode2
 * preserves the original odd-remainder reversal. Errors preserve output. */
int rf_bitmap_animation_frame(uint32_t now,uint32_t started,uint32_t fps,
    uint32_t count,uint32_t loop,int32_t *frame);
uint32_t rf_particle_render_mode(uint32_t flags,uint32_t normal_mode,uint32_t glow_mode);
typedef struct rf_particle_texture_states {
    uint32_t count;
    struct {uint32_t stage,state,value;} writes[12];
} rf_particle_texture_states;
/* Original texture-source cases 1 (coronas) and 2 (particle defaults). lod_bias
 * is the raw float bit pattern passed as D3D8 MIPMAPLODBIAS. Other texture
 * sources return RF_NOT_FOUND preserving output. No GPU or cache mutation. */
int rf_particle_texture_decode(uint32_t mode,uint32_t lod_bias,rf_particle_texture_states *states);

typedef struct rf_particle_render_environment {
    uint32_t blend_caps,depth_kind,fog_enabled,fog_kind;
} rf_particle_render_environment;
typedef struct rf_particle_render_states {
    uint32_t count;
    struct {uint32_t state,value;} writes[10];
    uint32_t vertex_color,vertex_alpha,vertex_fog;
} rf_particle_render_states;
int rf_particle_render_decode(uint32_t mode,const rf_particle_render_environment *environment,
    rf_particle_render_states *states);

typedef struct rf_particle_screen_vertex {
    float camera[3],screen[2],reciprocal_z,uv[2];
} rf_particle_screen_vertex;
typedef struct rf_particle_screen_polygon {
    uint32_t count;
    rf_particle_screen_vertex vertices[12];
} rf_particle_screen_polygon;
/* 5587c0 UV-only billboard path through 551900 submission: trivial rejection,
 * optional polygon clipping, point projection, then billboard depth override.
 * A rejected polygon returns RF_OK with count zero. Invalid inputs preserve
 * output. Zero override depth retains original IEEE infinity behavior.
 * No GPU call, texture binding, color conversion or batching occurs here. */
int rf_particle_billboard_project(const rf_particle_projection *projection,
    const rf_particle_clip_environment *environment,const rf_particle_billboard_packet *packet,
    rf_particle_screen_polygon *polygon);

typedef struct rf_particle_vertex_environment {
    uint32_t rgba,vertex_color,vertex_alpha,color_transform;
    float depth_scale,reciprocal_scale,uv_scale[2],fog_scale,color_scale[3];
} rf_particle_vertex_environment;
typedef struct rf_particle_draw_vertex {
    float screen[2],depth,reciprocal_w;
    uint32_t argb,fog;
    float uv[2];
} rf_particle_draw_vertex;
/* 551900 UV-only draw flag 1: current color, optional 550780 transform,
 * original 52fc70 fog rounding, reciprocal/depth and texture scales.
 * Represents the first 32 written bytes of the original 40-byte GPU record;
 * unused secondary UVs are excluded. No upload or renderer state changes. */
int rf_particle_vertex_encode(const rf_particle_vertex_environment *environment,
    const rf_particle_screen_vertex *vertex,rf_particle_draw_vertex *output);

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
