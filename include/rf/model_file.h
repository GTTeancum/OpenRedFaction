#ifndef RF_MODEL_FILE_H
#define RF_MODEL_FILE_H
#include "rf/vpp.h"
#include "rf/model.h"
#include "rf/physics.h"
#define RF_MODEL_MAX_SECTIONS 128
#define RF_MODEL_MAX_LODS 128
typedef struct rf_model_lod {
    uint32_t offset, size, attachment_offset, attachment_count;
    uint32_t batch_offset, batch_count, flags, auxiliary;
    uint32_t texture_offset,texture_count,section_index;
    float threshold;
} rf_model_lod;
/* File-relative regions in original 0x569920 order: positions, normals, UV,
 * indices, planes, extra, bone links, auxiliary. Encoding remains separate. */
typedef struct rf_model_batch {
    uint32_t vertices, triangles, format_bits;
    uint32_t offsets[8], sizes[8];
} rf_model_batch;
typedef struct rf_model_vertex {
    float position[3],normal[3],uv[2];
    uint8_t weights[4],bones[4];
} rf_model_vertex;
typedef struct rf_model_triangle { uint16_t indices[3],flags; } rf_model_triangle;
typedef struct rf_model_attachment {
    char name[69];
    float rotation[4], position[3];
    int32_t parent;
} rf_model_attachment;
typedef struct rf_model_collision_sphere {
    char name[25];
    int32_t parent;
    float center[3],radius;
} rf_model_collision_sphere;
typedef struct rf_model_section { uint32_t type, offset, size, material_offset, material_count; } rf_model_section;
typedef struct rf_model_file {
    rf_vpp *archive;
    rf_vpp_entry entry;
    uint32_t section_count, submeshes;
    rf_model_section sections[RF_MODEL_MAX_SECTIONS];
    uint32_t lod_count;
    rf_model_lod lods[RF_MODEL_MAX_LODS];
} rf_model_file;
/* Structural V3C v0x40000 traversal; LOD payloads stay on disc. Caller retains
 * archive ownership. Result is cleared on failure. No mesh/animation decoding. */
int rf_model_file_open(rf_model_file *model, rf_vpp *archive, const char *name);
/* First animated submesh sphere (5032d0/501610/504510). Does not combine
 * submeshes or collision spheres. Bounded16-byte read; errors preserve output. */
int rf_model_file_bound_sphere(const rf_model_file *model,float sphere[4]);
/*48a0b2..48a0c0: distance of sphere center from origin plus its radius.
 * Finite center and nonnegative radius required; errors preserve result. */
int rf_model_origin_radius(const float sphere[4],float *radius);
/* Read one raw 100-byte attachment without loading its LOD blob. Fields remain
 * local to the referenced bone; parent validation needs the loaded skeleton. */
int rf_model_file_attachment(const rf_model_file *model, uint32_t lod, uint32_t index, rf_model_attachment *attachment);
/* Stream the indexed CSPH record (44 serialized bytes). Center is bone-local
 * when parent is nonnegative; pose transformation is a separate runtime step.
 * NOT_FOUND past the last sphere; malformed data/errors preserve output. */
int rf_model_file_collision_sphere(const rf_model_file *model,uint32_t index,rf_model_collision_sphere *sphere);
/* Animated 503270/501500 sphere placement with already evaluated bone matrices.
 * Parent -1 uses identity. Returns center[3],radius in model space; radius is
 * copied without scaling. Bone/tag evaluation and class overrides are separate.
 * Invalid input leaves output unchanged. */
int rf_model_collision_sphere_pose(const rf_model_collision_sphere *sphere,
    const float (*matrices)[12],uint32_t bones,float result[4]);
/*4164c0 animated cached-pose path. Updates first model_count physics sphere
 * centers, deliberately preserving class-adjusted radii/other sphere words.
 * Rebuilds bounds over every physics sphere and copies radius to object78.
 * Model count may be smaller than physics count. All storage stays borrowed;
 * no allocation. Initial argument errors preserve everything; later query or
 * bounds errors can leave earlier centers updated, but preserve outputs.
 * physics_position is the physics body position, not an inferred render pose. */
int rf_model_corpse_spheres_refresh(const rf_model_collision_sphere *models,uint32_t model_count,
    const float (*matrices)[12],uint32_t bones,rf_physics_sphere *spheres,uint32_t sphere_count,
    const float physics_position[3],rf_physics_bounds *bounds,float *object_radius);
/* Stream one 84-byte serialized SUBM material. This is not the 200-byte
 * runtime material layout; conversion is separate. Output unchanged on error. */
int rf_model_file_material(const rf_model_file *model,uint32_t submesh,uint32_t index,uint8_t raw[84]);
/* Bounded batch directory; no LOD blob allocation. Output unchanged on error. */
int rf_model_file_batch(const rf_model_file *model,uint32_t lod,uint32_t index,rf_model_batch *batch);
/* Installed 0x518c41 layout only. Preserves bone bytes/triangle flags without
 * assuming their runtime interpretation. Source non-finite normal bits are
 * preserved; positions/UV must be finite. No allocation; output unchanged on error. */
int rf_model_file_vertex(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,rf_model_vertex *vertex);
int rf_model_file_triangle(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,rf_model_triangle *triangle);
/* Signed backward distance from the extra-data stream. Nonpositive means
 * deform afresh; positive must refer to an earlier vertex in this batch.
 * Preserves the original signed value; output unchanged on failure. */
int rf_model_file_vertex_reuse(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,int32_t *distance);
/* Resolve batch texture slot through LOD table to the flattened material
 * index used by rf_model_materials. Negative original slots return NOT_FOUND. */
int rf_model_file_batch_material(const rf_model_file *model,uint32_t lod,uint32_t batch,uint32_t *material);
typedef struct rf_model_draw_batch {
    uint32_t first_vertex,vertices,first_triangle,triangles,material;
} rf_model_draw_batch;
typedef struct rf_model_geometry {
    rf_model_draw_batch *batches;rf_model_vertex *vertices;
    rf_model_triangle *triangles;int32_t *reuse;
    uint32_t batch_count,vertex_count,triangle_count,accounted_bytes;
} rf_model_geometry;
/* Port-owned geometry for one LOD. Triangle indices/reuse remain batch-local;
 * material is flattened, or UINT32_MAX for an original negative draw slot.
 * Budget includes this struct and arrays, not allocator metadata or caller
 * archive/model/texture state. Zero-initialize; close before reuse. */
int rf_model_geometry_open(rf_model_geometry *geometry,const rf_model_file *model,uint32_t lod,uint32_t budget);
void rf_model_geometry_close(rf_model_geometry *geometry);
typedef struct rf_model_render_buffers {
    rf_model_render_cache *cache;float (*clip)[3],(*second)[3];uint8_t (*vertices)[40];uint32_t capacity;
} rf_model_render_buffers;
/* Port diagnostic policy after render_batch: classify z<near_depth as bit 1,
 * replacing the original nonpositive-Z bit 128. Uses fresh clip coordinates
 * and follows backward reuse entries. Does not alter original projection or
 * clipping helpers. Positive finite near depth required; failure preserves cache. */
int rf_model_geometry_clip_near(const rf_model_geometry *geometry,uint32_t batch,
    rf_model_render_buffers *buffers,float near_depth);
/* Process one resident batch into caller-owned buffers, no allocation or I/O.
 * Buffers use batch-local indices and retain fields the original does not write.
 * Invalid ranges/reuse/bone indices fail before writes; initialize buffers before
 * first use. Triangle clipping/submission and material selection are external. */
int rf_model_geometry_render_batch(const rf_model_geometry *geometry,uint32_t batch,
    const float (*matrices)[12],uint32_t bones,const rf_model_projection *view,
    const rf_model_lighting *lights,const rf_model_render_output *output,rf_model_render_buffers *buffers);
/* Original 0x52f5b2..0x52f699 clipping inputs: position/RGB from the fresh
 * cache entry, own clip mask and UV, corner ordinal and zero generated flags.
 * Records are 48 original-layout bytes; unrelated bytes stay untouched.
 * Invalid indices/reuse leave all records unchanged. */
int rf_model_prepare_clip_triangle(const rf_model_vertex *vertices,const int32_t *reuse,
    const rf_model_render_cache *cache,const float (*clip)[3],uint32_t count,
    const uint16_t indices[3],const rf_model_render_output *output,uint8_t records[3][48]);
/* Assemble recovered triangle routing/clipping/emission for one resident batch.
 * Output already contains this batch's processed vertices; indices are local
 * plus base. Caller owns initialized pool/output storage. No allocation or I/O.
 * Invalid source ranges fail before writes; capacity/runtime failures may retain
 * earlier triangles, so discard the batch output on failure. No GPU submission. */
int rf_model_geometry_emit_batch(const rf_model_geometry *geometry,uint32_t batch,
    const rf_model_render_buffers *buffers,const rf_model_projection *view,
    const rf_model_clip_planes *planes,const rf_model_clip_projection *projection,
    const rf_model_render_output *attributes,uint16_t base,rf_model_clip_pool *pool,
    rf_model_triangle_output *output);
/* Select a flattened LOD index within one SUBM using its serialized thresholds.
 * Metric and render gates have the same meaning as rf_model_select_lod.
 * No allocation; output unchanged on failure. */
int rf_model_file_select_lod(const rf_model_file *model,uint32_t submesh,uint32_t flags,
    int alternate,int32_t minimum,int scaled,int animated,double metric,uint32_t *out);
#endif
