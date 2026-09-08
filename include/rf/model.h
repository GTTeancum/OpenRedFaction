#ifndef RF_MODEL_H
#define RF_MODEL_H
#include "rf/vpp.h"
#include "rf/motion_file.h"

/* Fixed-width material record used by original model-instance arrays.
 * Unknown fields remain bytes; pointer-valued slots are not native pointers. */
typedef struct rf_model_material_record { uint8_t bytes[200]; } rf_model_material_record;
/* Original 54a7c0 constructor: initializes only fields it writes, preserving
 * other bytes. Call on newly owned storage, never to release a live material. */
int rf_model_material_initialize(rf_model_material_record *material);
/* Fixed-field portion of 503950. Copies names through their terminator and
 * preserves destination tails/owned-array fields. array_counts reports the
 * subsequent allocation plan: kind 3 retains positive source counts, other
 * kinds retain one element of each nonempty array. Caller must finish that
 * ownership step before publishing the material. Rejects unterminated names
 * or identical source/destination without mutation. All supplied storage
 * regions must be disjoint; partial overlap is unsupported. */
int rf_model_material_prepare_copy(rf_model_material_record *destination,
    const rf_model_material_record *source,int32_t kind,uint32_t array_counts[3]);

typedef struct rf_model_material_instance {
    rf_model_material_record record;
    uint32_t *storage,*arrays[3],counts[3],accounted_bytes;
} rf_model_material_instance;
/* Zero-initialize instance before first use and close before reuse. Creates
 * independent arrays using the recovered copy plan, in one bounded allocation.
 * Budget includes sizeof(instance) and array payload, not allocator metadata.
 * Raw pointer slots stay zero; use arrays/counts for native ownership.
 * Invalid source views, overflow, or budget failures leave output unchanged. */
int rf_model_material_instance_open(rf_model_material_instance *instance,
    const rf_model_material_record *source,int32_t kind,const uint32_t *const arrays[3],
    const uint32_t capacities[3],uint32_t budget);
void rf_model_material_instance_close(rf_model_material_instance *instance);
/* 53ae5f material-record conversion after texture resolution. Exactly 84
 * disk bytes; primary name must be nonempty, both names NUL-terminated in
 * 32 bytes. primary_transparent is resolved 510710. Texture handles are
 * caller-owned; close releases only the material's copied scalar array. */
int rf_model_material_from_disk(rf_model_material_instance *instance,
    const uint8_t *raw,size_t size,int32_t primary_texture,int32_t secondary_texture,
    uint32_t primary_transparent,uint32_t budget);

/* 503690 over resolved model views. Kind 1 returns static_count when
 * static_lods<=1; kind 2 sums mesh counts; kind 3 returns direct_count;
 * unknown kinds return zero. Signed counts and original wrapping addition
 * are preserved. This is a query, not an allocation size validation. */
int rf_model_material_count(int32_t kind,int32_t static_lods,int32_t static_count,
    int32_t mesh_count,const int32_t *mesh_counts,uint32_t mesh_capacity,
    int32_t direct_count,int32_t *result);

/* Bounded names supplied by a model loader; length excludes the terminator.
 * Groups retain the original 0x51d5b0 search order, without original pointers
 * or object layout. Their semantic names await loader reconstruction. */
typedef struct rf_model_name { const char *data; size_t length; } rf_model_name;
typedef struct rf_model_name_group {
    const rf_model_name *names;
    uint32_t count;
} rf_model_name_group;
/* Default-locale ASCII case folding; output is untouched on failure.
 * RF_NOT_FOUND denotes absence, RF_FORMAT an embedded NUL or invalid name,
 * RF_RANGE invalid arguments or a combined index exceeding signed 32 bits. */
int rf_model_find_tag(const rf_model_name_group groups[3],
                      rf_model_name query, int32_t *index);
/* Raw BONE payload fields, before the original quaternion conversion.
 * Caller locates the section and owns output storage; no allocation is made.
 * Input/output storage must not overlap. Error returns leave output untouched. */
typedef struct rf_model_bone {
    char name[25];
    float rotation[4], position[3];
    int32_t parent;
} rf_model_bone;
int rf_model_decode_bones(const void *payload, size_t bytes,
                          rf_model_bone *bones, uint32_t capacity, uint32_t *count);
/* Reconstructs normalization (0x519720) and local transform (0x4fe900).
 * Output is nine rotation floats followed by three translation floats.
 * Zero-length/non-finite rotations are rejected; output is unchanged on error.
 * This is a local transform, before parent composition or animation. */
int rf_model_bone_transform(const float rotation[4], const float position[3], float transform[12]);
/* Attachment setup calls 0x4fe900 without bone normalization. Zero quaternions
 * therefore produce an identity rotation; non-finite/overflow results fail. */
int rf_model_attachment_transform(const float rotation[4], const float position[3], float transform[12]);
/* Reconstructed 0x51c620: row-vector local * parent, with implicit final column
 * (0,0,0,1). Aliased output is supported; errors leave it unchanged. */
int rf_model_compose_transform(const float local[12], const float parent[12], float result[12]);
/* Collision deformation block 0x54e344: stop at first zero weight, multiply
 * by byte/256, no normalization. Matrices are prepared bone transforms.
 * This is not yet verified as the rendering skinning path. */
int rf_model_collision_vertex(const float position[3],const uint8_t weights[4],const uint8_t bones[4],
    const float (*matrices)[12],uint32_t count,float result[3]);
/* Rendering block 0x52ee9d..0x52f154, freshly deformed vertex branch only.
 * Both streams receive translation; second-stream preparation is external.
 * Six outputs: position followed by second stream. No camera/projection. */
int rf_model_render_vertex_pair(const float position[3],const float second[3],
    const uint8_t weights[4],const uint8_t bones[4],const float (*matrices)[12],uint32_t count,float result[6]);
/* Original 0x52fcf0 lighting: three direction/RGB records and ambient RGB.
 * Vector is supplied as consumed by that helper, with no normalization. */
int rf_model_vertex_lighting(const float vector[3],const float lights[3][6],const float ambient[3],uint8_t rgb[3]);
typedef struct rf_model_local_light { float position[3],radius_squared;uint32_t enabled; } rf_model_local_light;
typedef struct rf_model_light_choice { int32_t index;float delta[3],distance_squared; } rf_model_light_choice;
/* 0x52dcaf selection block: nearest enabled containing light, first tie wins.
 * Output index -1 means none. Global disable gate and attenuation are external. */
int rf_model_choose_local_light(const float position[3],const rf_model_local_light *lights,uint32_t count,rf_model_light_choice *choice);
/* Selected-light tail 0x52dd9d: (1-sqrt(distance_squared/radius_squared))*255
 * times light RGB, with no clamp. Invalid arithmetic propagates as original. */
int rf_model_local_light_color(float distance_squared,float radius_squared,const float color[3],float result[3]);
/* Selected-light direction 0x52dd51: normalize delta; flag 0x400 replaces Y
 * with 0.5 and normalizes again; rotate through the supplied 3x3 matrix. */
int rf_model_local_light_direction(const float delta[3],uint32_t flags,const float rotation[9],float result[3]);
/* Tag placement 0x5034f0 after character tag evaluation: rotate then translate.
 * Preserves its separate rounding stages; no extra scale parameter is applied. */
int rf_model_place_tag(const float local[12], const float orientation[9], const float position[3], float out[12]);
/* Original 0x51cb50 orders byte indices by depth, stable within each depth.
 * Supports up to 256 bones; rejects cycles/invalid parents before output. */
int rf_model_bone_order(const rf_model_bone *bones, uint32_t count, uint8_t *order, uint32_t capacity);
/* Original 0x51b110. Caller supplies positive, normalized animation weights.
 * Supports the original evaluator's 1..16 contributing poses, no allocation. */
int rf_model_blend_pose(const float (*rotations)[4], const float (*positions)[3], const float *weights,
                        uint32_t count, float out[12]);
/* Initial single-motion pose: no root displacement, bone overrides or slot
 * transitions. Caller owns matrices; a failed read can leave partial results.
 * Skeleton and motion must have identical counts, at most 256 bones. */
int rf_model_sample_single_motion(const rf_model_bone *bones, uint32_t count, const rf_motion_file *motion,
                                  int32_t tick, int bypass_fades, float (*matrices)[12], uint32_t capacity);
/* Evaluate current playback slots from immutable archives, including per-bone
 * primary attenuation of looping motions. Motion handles/resources are indexed
 * by registered ID. The pending displacement is added to the first evaluated
 * root and consumed (cleared). No bone overrides or generation cache yet.
 * Failed reads may leave partial matrices and consumed displacement.
 * Displacement storage must not overlap the output matrices. */
int rf_model_sample_playback(const rf_model_bone *bones, uint32_t count, const rf_motion_playback_state *state,
                             const rf_motion_file *const *motions, const rf_motion_playback_resource *resources,
                             uint32_t resource_count, float root_displacement[3], float (*matrices)[12], uint32_t capacity);
/* Original per-bone generation cache in 0x51b500. Matrices and generation stamps
 * share capacity. Initialize stamps unequal to state->generation before first
 * evaluation, and keep them with their matrices. Matching stamps skip sampling
 * and displacement consumption; successful bones are stamped individually. */
int rf_model_evaluate_playback(const rf_model_bone *bones, uint32_t count, const rf_motion_playback_state *state,
                               const rf_motion_file *const *motions, const rf_motion_playback_resource *resources,
                               uint32_t resource_count, float root_displacement[3], float (*matrices)[12],
                               uint16_t *generations, uint32_t capacity);
#endif
