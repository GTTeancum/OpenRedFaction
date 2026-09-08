#ifndef RF_PREVIEW_H
#define RF_PREVIEW_H
#include "rf/geometry.h"
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
void rf_preview_close(rf_preview_mesh *mesh);
#endif
