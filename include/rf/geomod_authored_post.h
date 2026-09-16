#ifndef RF_GEOMOD_AUTHORED_POST_H
#define RF_GEOMOD_AUTHORED_POST_H
#include "rf/geometry.h"
#include "rf/geomod_publication.h"
typedef struct rf_geomod_authored_post rf_geomod_authored_post;
typedef struct rf_geomod_authored_post_view {
    rf_geomod_mesh_view source, windows, neighbors;
    const float (*source_planes)[4];
    const rf_geomod_publication_solid *solids;
    const rf_geomod_publication_origin *source_origins, *window_origins, *neighbor_origins;
    const rf_collision_face_filter *source_filters;
    const uint32_t *replaced_ids;
    uint32_t source_uid, room, solid_count, replaced_count, brush_count, authored_face_count;
    uint32_t resident_bytes, peak_bytes;
    rf_level_geomod_settings settings;
} rf_geomod_authored_post_view;
/* Scoped, proven ctf06/UID94 profile. Reads v180 editor brush section and real
 * compiled face+24 ownership. This is not a general editor CSG evaluator.
 * Required local neighborhood: earlier air66, solid neighbors71/95/70, no
 * other intersecting brush AABB. Source/neighbors must be closed outward convex
 * solids with <=32 faces. No caps, repaired topology or generated geometry.
 *
 * Owned meshes/planes/UVs/provenance survive closing level/geometry/archive.
 * Materials are COMPILED TEXTURE INDICES, not renderer slots; caller remaps them.
 * Source_face is AUTHORED identity. reference is a compiled metadata candidate,
 * not a lightmap assignment; absent compiled counterparts use UINT32_MAX.
 * Source hidden-cap collision filters inherit owning-room state from a visible
 * source face while retaining their actual authored flags/portal field.
 *
 * Budget includes output owner, parsing payload, index records and face-map
 * scratch; excludes caller-owned geometry, allocator overhead and small stack.
 * No scene mutation. *out must be NULL and remains unchanged on error.
 */
int rf_geomod_authored_post_open(const rf_level *, const rf_geometry *, uint32_t budget,
                                 rf_geomod_authored_post **);
/* Same v180/profile decoder for retained section bytes. Peak conservatively
 * includes supplied payload bytes even though decode borrows them. settings
 * must be read from the same level's section900. No payload borrow escapes. */
int rf_geomod_authored_post_decode(const void *, uint32_t, const rf_geometry *,
                                   const rf_level_geomod_settings *, uint32_t budget,
                                   rf_geomod_authored_post **);
int rf_geomod_authored_post_get(const rf_geomod_authored_post *, rf_geomod_authored_post_view *);
void rf_geomod_authored_post_close(rf_geomod_authored_post **);
#endif
