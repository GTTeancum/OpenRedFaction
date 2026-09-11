#ifndef RF_MODEL_H
#define RF_MODEL_H
#include "rf/vpp.h"
#include "rf/motion_file.h"
typedef struct rf_model_motion_registry {
    uint32_t *identities;uint8_t *flags;uint32_t count,capacity;
} rf_model_motion_registry;
/* Registry portion of 0x51cc42..0x51cc93: first matching resolved identity/flag
 * wins, otherwise append. Identity is a nonzero caller-owned stable token for
 * a resolved motion (the original uses a skeleton pointer). Flag is an exact
 * byte, not normalized to bool. Caller resolves/loads the motion separately.
 * Capacity/error handling is port-owned; all failures preserve registry/output.
 * added is 1 for a new entry, 0 for a reused entry. No allocation or I/O. */
int rf_model_register_motion(rf_model_motion_registry *registry,uint32_t identity,
    uint8_t flag,int32_t *index,int *added);

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
/*51d690: first case-sensitive substring match in the bone group only.
 * Empty query matches the first bone. Unlike find_tag, no case folding or
 * attachment/miscellaneous groups. Same bounded-name/error contracts. */
int rf_model_find_bone_substring(const rf_model_name *bones,uint32_t count,
    rf_model_name query,int32_t *index);
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
/* 0x51ba00 after pose evaluation: compose stored bone transforms with pose,
 * refreshing only entries whose 16-bit generation differs. Count <=256.
 * On malformed matrices, earlier successful entries remain refreshed. */
int rf_model_prepare_skinning(const float (*stored)[12],const float (*pose)[12],uint32_t count,
    uint16_t generation,float (*prepared)[12],uint16_t *generations,uint32_t capacity);
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
/* Fresh visible vertex 0x52f31e..0x52f34d with lighting enabled: normalize
 * deformed second stream via 0x4faaf0, then light it. No singular fallback.
 * Normalized output may alias input; source-vector non-finites are preserved
 * through arithmetic, not repaired. Camera clipping is external. */
int rf_model_render_vertex_lighting(const float vector[3],const float lights[3][6],const float ambient[3],
    float normalized[3],uint8_t rgb[3]);
/* Port-owned view of original separate renderer cache arrays. The historical
 * 'world' field is model-local when used with rf_model_local_view. Projected Z is
 * reciprocal depth. Duplicate processing only updates world and clip. */
typedef struct rf_model_render_cache {
    float world[3],projected[3];uint8_t clip,depth,rgb[3],reserved[3];
} rf_model_render_cache;
typedef struct rf_model_render_output {
    uint32_t lighting;uint8_t rgb[3],alpha;float depth_scale,reciprocal_scale;
} rf_model_render_output;
typedef struct rf_model_projection {
    float camera[3],rotation[9],fixed_depth,depth_factor;
    float screen[4]; /* X/Y scales, X/Y offsets. */
    float bounds[4]; /* left, top, right, bottom; strict comparisons. */
    float far_depth;
    uint32_t perspective,compute_clip,clipping,far_clip,screen_clip;
} rf_model_projection;
/* Primary view update in 0x547485..0x5474cc: transform world camera into
 * entity-local space and compose view rotation with transposed orientation.
 * Other projection fields are preserved; aliasing input/output is supported.
 * Does not reproduce the original global view stack or secondary view state. */
int rf_model_local_view(const rf_model_projection *world,const float position[3],
    const float orientation[9],rf_model_projection *local);
/* Fresh projection 0x52f154..0x52f31e/0x52f3cc. Updates projected/clip/depth
 * cache fields and vertex byte 23. clip_position updates only with compute_clip.
 * visible describes the screen-rejection branch, not whether clip bits are zero.
 * Caller supplies original view globals; other cache/vertex bytes are retained. */
int rf_model_project_vertex(const float world[3],const rf_model_projection *view,
    rf_model_render_cache *cache,float clip_position[3],uint8_t vertex[40],uint32_t *visible);
/* Original positive-reuse branch 0x52edbc..0x52ee9d plus UV tail 0x52f3bd.
 * Distance is batch-local, positive and <=index. vertex holds 40 writable
 * original-format bytes; untouched fields remain intact, and clipped copies
 * do not write it at all. Caller owns separate cache/vertex storage. */
int rf_model_render_reuse_vertex(rf_model_render_cache *cache,uint32_t count,uint32_t index,int32_t distance,
    const rf_model_render_output *output,const float uv[2],uint8_t vertex[40]);
/* Visible fresh-vertex tail: normalize second in place, optionally cache RGB,
 * emit projected attributes and this vertex's UVs. Caller has projected it. */
int rf_model_finish_render_vertex(rf_model_render_cache *cache,float second[3],
    const rf_model_render_output *output,const float lights[3][6],const float ambient[3],
    const float uv[2],uint8_t vertex[40]);
/* Original 0x52f4e8 facing test. Flag 0x20 bypasses culling; otherwise
 * normal=(b-a) cross (c-b). Perspective accepts dot(camera-a,normal)>0;
 * nonperspective accepts !(dot(forward,normal)>0), including unordered. */
int rf_model_triangle_facing(const float a[3],const float b[3],const float c[3],uint16_t flags,
    uint32_t perspective,const float camera[3],const float forward[3],uint32_t *accepted);
typedef enum rf_model_triangle_route { RF_MODEL_TRIANGLE_REJECT=0,RF_MODEL_TRIANGLE_DIRECT=1,RF_MODEL_TRIANGLE_CLIP=2 } rf_model_triangle_route;
/* Original 0x52f473 routing around facing and clipping. Does not generate a
 * clipped polygon. All indices must fit original signed-short cache addressing. */
int rf_model_route_triangle(const rf_model_render_cache *cache,uint32_t count,const uint16_t indices[3],
    uint16_t flags,const rf_model_projection *view,uint32_t *route);
/* 0x54954c..0x549626: interpolate UV0 (bit1), UV1 (bit2), RGB (bit4).
 * Original model clipping passes flags=5. Caller supplies intersection factor
 * in [0,1]; position, clip flags and alpha are untouched. Aliasing supported. */
int rf_model_clip_attributes(const uint8_t inside[48],const uint8_t outside[48],double factor,
    uint32_t flags,uint8_t result[48]);
typedef struct rf_model_clip_planes {float near_depth,far_depth,point[3],normal[3];} rf_model_clip_planes;
/* 0x549324..0x54954c intersection for one plane bit 1..0x40. Double factor
 * approximates x87; custom-plane factor and intermediate vectors use float
 * stores. Singular arithmetic is retained. No attributes/pool allocation. */
int rf_model_clip_intersection(uint32_t plane,const float inside[3],const float outside[3],
    const rf_model_clip_planes *planes,float position[3],double *factor);
/* 0x518320 -> 0x518bd0 -> 0x5475d0. Mode 0x66 updates record byte24;
 * other modes preserve it. Plane checks consume camera-space position. */
int rf_model_classify_clip_vertex(uint32_t mode,const rf_model_projection *view,uint8_t record[48]);
typedef struct rf_model_clip_pool {
    uint8_t records[48][48];uint32_t order[48],used;uint64_t live;
} rf_model_clip_pool;
/* Original 0x549270 reset retains record bytes. Allocation 0x5496e0 allows
 * 47 live slots; attempt 48 increments used then fails. After failure reset
 * before reuse. Release 0x5492d0 pushes the slot back onto the free list.
 * Port live tracking rejects invalid/double releases without writes. */
void rf_model_clip_pool_reset(rf_model_clip_pool *pool);
int rf_model_clip_pool_allocate(rf_model_clip_pool *pool,uint32_t *slot);
int rf_model_clip_pool_release(rf_model_clip_pool *pool,uint32_t slot);
/* Recovered 0x549e00 / 0x549bd0 model path (attribute flags 0..7).
 * Caller resets/initializes pool first. Lists and pool records must not alias
 * original records. Returned pointers refer to originals or caller pool until
 * next reset. On failure pool may be partially consumed; reset before retry.
 * Up to 46 original vertices; output capacity 48. mask[0]=union, [1]=common. */
int rf_model_clip_polygon(rf_model_clip_pool *pool,uint8_t *const *original,uint32_t count,
    const rf_model_clip_planes *planes,const rf_model_projection *view,uint32_t mode,uint32_t attributes,
    uint8_t *result[48],uint32_t *result_count,uint8_t mask[2]);
typedef struct rf_model_clip_projection {
    float scale[2];int32_t offset[2];float depth_bias;uint32_t clamp;
} rf_model_clip_projection;
/* Complete 0x5477a0: project a camera-space 48-byte clip record in place.
 * Flags 1/2 in byte 25 skip already processed records; generated flag 4 remains.
 * Updates projected XYZ at 12..23 and flags only. Bias changes reciprocal depth,
 * not screen XY. Caller supplies the original viewport scales/integer offsets. */
int rf_model_project_clip_vertex(const rf_model_clip_projection *view,uint8_t record[48]);
typedef struct rf_model_triangle_output {
    uint8_t (*vertices)[40];uint16_t *indices;
    uint32_t vertex_count,vertex_capacity,index_count,index_capacity;
} rf_model_triangle_output;
/* 0x52f6c1..0x52f84e: append generated vertices and a fan using original corner
 * indices for retained records. Uses the original strict capacity gates,
 * conservatively reserving count vertices. RF_RANGE means no output writes.
 * Counts below three/common masks reject without writes. base wraps to u16. */
int rf_model_emit_clip_polygon(uint8_t *const *records,uint32_t count,uint8_t common,
    const uint16_t triangle[3],uint16_t base,const rf_model_clip_projection *projection,
    const rf_model_render_output *attributes,float depth_factor,rf_model_triangle_output *output);
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
typedef struct rf_model_lighting_input {
    uint32_t flags,alternate,disable_local;
    uint8_t color[4];float ambient[3],gain,model_rotation[9],light_rotation[9],position[3];
} rf_model_lighting_input;
typedef struct rf_model_lighting { float lights[3][6],ambient[3]; } rf_model_lighting;
/* Complete 0x52dad0 setup with caller-owned global values and light list. */
int rf_model_lighting_setup(const rf_model_lighting_input *input,const rf_model_local_light *lights,
    const float (*colors)[3],uint32_t count,rf_model_lighting *result);
typedef struct rf_model_bone_query {float position[3],basis[9];} rf_model_bone_query;
/*503230/5012a0 kind2 over already evaluated51b2e0 bone matrices.
 * Index-1 supplies identity; other indices must be within count<=256.
 * Virtual bones/attachments and lazy animation advancement are external.
 * Finite matrix required; output unchanged on error, input/output may alias. */
int rf_model_query_bone(const float (*pose)[12],uint32_t count,int32_t index,
    rf_model_bone_query *result);
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
/* Detail selection from 0x52faae..0x52fb1d (and 0x52fbe2).
 * Metric is supplied by the caller (original 0x5182f0); thresholds retain file
 * order. No allocation/loading. Invalid count or minimum leaves output intact. */
int rf_model_select_lod(const float *thresholds,uint32_t count,uint32_t flags,
    int alternate,int32_t minimum,int scaled,int animated,double metric,uint32_t *out);
/* 0x5182f0 / 0x5479b0: mode 0x66 uses distance * numerator / denominator;
 * other modes return zero. Inputs represent original caller/global values.
 * Preserve non-finite arithmetic; double approximates the x87 return value. */
int rf_model_lod_metric(uint32_t mode,const float position[3],const float camera[3],
    float numerator,float denominator,double *out);
#endif
