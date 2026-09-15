#ifndef RF_PREVIEW_H
#define RF_PREVIEW_H
#include "rf/geometry.h"
#include "rf/material.h"
#include "rf/geomod.h"
#define RF_PREVIEW_VERTEX_LIT (UINT32_MAX-1u) /* lightmap tag: texture times vertex RGB */
typedef struct rf_preview_vertex {
    float position[3], color[3];
    /* Perspective texture coordinates: u/z, v/z, 1/z after clipping. */
    float texture[3];
    uint32_t material;
    float lightmap_texture[3];
    uint32_t lightmap;
} rf_preview_vertex;
typedef struct rf_preview_mesh { rf_preview_vertex *vertices; uint32_t count, bytes; } rf_preview_mesh;
/* Project generated terrain through the same world clipping/raster format.
 * bound must be the successful rf_geomod_collision_faces output for this mesh,
 * in unchanged source order. Materials are final shared table slots. Replaces
 * mesh contents in its existing capacity, with no allocation; capacity/input
 * errors preserve output bytes and counts. The basic entry point is unlit.
 * Caller keeps all inputs stable/disjoint from output across both passes.
 * This prepares draw data; it does not submit or publish a live world. */
int rf_preview_geomod(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    const rf_geomod_mesh_view *source,const rf_collision_face *bound,
    uint32_t material_count,const rf_level *camera);
/* Explicit linear RGB per face, validated in[0,1]; no preview tint. */
/* Per-source-corner colors survive frustum clipping; hard face normals remain
 * the caller's responsibility. Invalid RGB preserves the destination mesh. */
int rf_preview_geomod_vertex_lit(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    const rf_geomod_mesh_view *source,const rf_collision_face *bound,uint32_t material_count,
    const rf_level *level,const float (*vertex_colors)[3]);
/* Surviving authored faces retain their original lightmap image/projection;
 * newly exposed faces use vertex lighting. source_face IDs address authored. */
typedef struct rf_preview_surface_lightmap {
    rf_lightmap_projection projection;uint32_t image;
} rf_preview_surface_lightmap;
/* Per-face generated lightmap bindings; UINT32_MAX selects vertex lighting.
 * Surviving source faces retain authored mappings. Projections address final
 * image/atlas coordinates. Caller owns images and guarantees valid image IDs. */
int rf_preview_geomod_lightmapped(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    const rf_geomod_mesh_view *source,const rf_collision_face *bound,uint32_t material_count,
    const rf_level *level,const float (*vertex_colors)[3],const rf_geometry *authored,
    const rf_preview_surface_lightmap *bindings);
int rf_preview_geomod_world_lit(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    const rf_geomod_mesh_view *source,const rf_collision_face *bound,uint32_t material_count,
    const rf_level *level,const float (*vertex_colors)[3],const rf_geometry *authored);
int rf_preview_geomod_lit(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    const rf_geomod_mesh_view *source,const rf_collision_face *bound,
    uint32_t material_count,const rf_level *camera,const float (*face_colors)[3]);
/* Append a processed model batch using the recovered clipping/emission helpers.
 * Caller supplies batch-local render buffers, 4096 output vertex slots, 24576
 * index slots and an initialized clip pool; all can be reused between actors.
 * No allocation or archive I/O. NULL mesh performs clipping/counting only.
 * Non-NULL mesh appends to its existing allocation, preserving prior vertices;
 * errors may alter scratch and append bytes but preserve count/bytes. Material
 * remains the model-local index, for the caller's appearance mapping.
 * Screen/depth conversion is the current 640x480 diagnostic renderer policy,
 * not a claim of recovered world lighting or final draw-state parity. */
int rf_preview_model_emit(const rf_model_geometry *geometry,uint32_t batch,
    rf_model_render_buffers *buffers,uint16_t *indices,rf_model_clip_pool *pool,
    const rf_model_projection *view,const rf_model_clip_planes *planes,
    const rf_model_clip_projection *projection,const rf_model_render_output *attributes,
    rf_preview_mesh *mesh,uint32_t capacity_bytes,uint32_t *emitted);

/* Static equivalent with required batch-local stored face planes and optional
 * batch-local RGB rows. Uses static facing/clip records, never skeletal cached
 * world positions. Retains the diagnostic white vertex tint and depth policy. */
int rf_preview_static_model_emit(const rf_model_geometry *geometry,uint32_t batch,
    rf_model_render_buffers *buffers,uint16_t *indices,rf_model_clip_pool *pool,
    const rf_model_projection *view,const rf_model_clip_planes *planes,
    const rf_model_clip_projection *projection,const rf_model_render_output *attributes,
    rf_preview_mesh *mesh,uint32_t capacity_bytes,uint32_t *emitted,
    const float (*face_planes)[4],const uint8_t (*colors)[3]);

/* Last world capacity error: valid, face, fan corner, used/capacity vertices,
 * geometry face count, writing pass, required vertices. Diagnostic only. */
extern uint32_t rf_preview_failure[8];
/* 55f82f..55f844, with 4163a0/40a0b0: strict positive plane distance.
 * Plane and viewer share coordinates. NaN/zero distance is rejected. */
uint32_t rf_preview_plane_visible(const float plane[4],const float viewer[3]);
/* Initial geometry renderer input, not reconstructed original camera/materials.
 * 640x480, 90 degree horizontal FOV, +Z forward, 0.1..1000 depth range.
 * Temporary face shading keeps geometry inspectable until materials are restored. */
int rf_preview_build(rf_preview_mesh *mesh, const rf_geometry *geometry, const rf_level *level, uint32_t budget);
/* Same diagnostic projection/clipping for solid-local geometry at a supplied
 * committed pose. material_base offsets its local texture slots into a caller
 * material table; lightmap indices remain level-wide. Caller keeps inputs stable
 * through both passes; close before reuse. Not original renderer reconstruction. */
int rf_preview_build_transformed(rf_preview_mesh *mesh,const rf_geometry *geometry,
    const rf_level *level,const float origin[3],const float matrix[3][3],
    uint32_t material_base,uint32_t budget);
void rf_preview_close(rf_preview_mesh *mesh);
/* One bounded allocation for world followed by authored mover meshes. The
 * material bundle must describe world then movers in the same order. Optional
 * poses supplies one committed runtime pose per mover; NULL uses file poses.
 * Preserves output on failure; close before reuse. Diagnostic projection only. */
int rf_preview_build_world(rf_preview_mesh *mesh,const rf_geometry *world,
    const rf_geometry_movers *movers,const rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level,uint32_t budget);
/* Reproject into an existing caller-owned allocation with capacity_bytes.
 * Updates count/bytes to the used range, retaining the allocation address.
 * No allocation; inputs and buffer must not alias and must remain stable
 * through validation/counting and writing. Validation/capacity errors preserve
 * mesh and vertex bytes. Buffer capacity is tracked separately from mesh.bytes.
 * Close normally with rf_preview_close when the allocation came from malloc. */
int rf_preview_update_world(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    const rf_geometry *world,const rf_geometry_movers *movers,
    const rf_group_attached_pose *poses,const rf_geometry_materials *materials,
    const rf_level *level);
/* Single generation pass using caller-owned staging storage. Scratch must be
 * disjoint from the destination capacity range and at least capacity_bytes long.
 * Failure may alter scratch, but preserves mesh and destination vertex bytes.
 * Inputs cannot alias either output range. No allocation. */
int rf_preview_update_world_staged(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    rf_preview_vertex *scratch,uint32_t scratch_bytes,const rf_geometry *world,
    const rf_geometry_movers *movers,const rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level);
/* Transactional type9 mover dispatch: uses live flags, model-less outer gates,
 * and low-byte draw suppression. Publishes markers only after mesh success.
 * NULL scratch selects two passes; nonnull scratch obeys staged alias rules.
 * Poses must not alias mesh/scratch; all geometry solids must be present. */
int rf_preview_update_world_dispatch(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    rf_preview_vertex *scratch,uint32_t scratch_bytes,const rf_geometry *world,
    const rf_geometry_movers *movers,rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level,uint32_t disabled);
/* As dispatch, with borrowed primary-room visibility from this same geometry
 * and camera. Detail/unknown rooms and movers are conservatively retained.
 * NULL preserves unfiltered rendering; no allocation or visibility mutation. */
struct rf_visibility;
/* disabled bit0x100 omits static geometry for a retained platform backend;
 * the low byte keeps its existing mover suppression meaning. */
int rf_preview_update_world_visible(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    rf_preview_vertex *scratch,uint32_t scratch_bytes,const rf_geometry *world,
    const rf_geometry_movers *movers,rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level,uint32_t disabled,
    const struct rf_visibility *visibility);
#endif
