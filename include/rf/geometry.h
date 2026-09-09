#ifndef RF_GEOMETRY_H
#define RF_GEOMETRY_H
#include "rf/level.h"
#include "rf/collision.h"

typedef struct rf_geometry {
    unsigned char *data;
    uint32_t bytes, allocated_bytes;
    uint32_t textures, rooms, vertices, faces, corners, mappings;
    uint32_t vertices_offset, mapping_offset, tail_offset;
    uint32_t *texture_offsets, *room_offsets, *face_offsets;
} rf_geometry;
typedef struct rf_geometry_face {
    float plane[4];
    uint32_t texture, lightmap_mapping, room, portal, flags, corners;
} rf_geometry_face;
typedef struct rf_geometry_corner {
    uint32_t vertex;
    float uv[2], lightmap_uv[2];
} rf_geometry_corner;
/* New file-format implementation. Retains unknown bytes for later reconstruction.
 * budget covers requested payload/index allocations, excluding allocator metadata.
 * Close before reusing an already-open object; failures leave it empty. */
int rf_geometry_open(rf_geometry *geometry, const rf_level *level, uint32_t budget);
void rf_geometry_close(rf_geometry *geometry);
int rf_geometry_vertex(const rf_geometry *geometry, uint32_t index, float position[3]);
int rf_geometry_texture_name(const rf_geometry *geometry, uint32_t index, char *name, uint32_t capacity);
/* Resolve a mapping record's first word; remaining 92 bytes stay opaque. */
int rf_geometry_lightmap(const rf_geometry *geometry, uint32_t mapping, uint32_t image_count, uint32_t *image);
int rf_geometry_get_face(const rf_geometry *geometry, uint32_t index, rf_geometry_face *face);
int rf_geometry_get_corner(const rf_geometry *geometry, uint32_t face, uint32_t corner, rf_geometry_corner *result);
/* Bind a loaded file face to borrowed collision vertices with exact corner
 * order and original 0.0001-expanded bounds. filter MUST be supplied from resolved runtime
 * metadata; file flag bytes are not assumed equivalent to runtime flags.
 * No allocation. capacity is vertices, not bytes. Output is unchanged on
 * failure; scratch may be partially written. Borrow ends when scratch changes.
 * Geometry must be an unmodified, successfully opened object. */
int rf_geometry_collision_face(const rf_geometry *geometry,uint32_t index,
    const rf_collision_face_filter *filter,float (*scratch)[3],uint32_t capacity,
    rf_collision_face *face);
#endif
