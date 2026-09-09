#ifndef RF_PREVIEW_H
#define RF_PREVIEW_H
#include "rf/geometry.h"
#include "rf/material.h"
typedef struct rf_preview_vertex {
    float position[3], color[3];
    /* Perspective texture coordinates: u/z, v/z, 1/z after clipping. */
    float texture[3];
    uint32_t material;
    float lightmap_texture[3];
    uint32_t lightmap;
} rf_preview_vertex;
typedef struct rf_preview_mesh { rf_preview_vertex *vertices; uint32_t count, bytes; } rf_preview_mesh;
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
#endif
