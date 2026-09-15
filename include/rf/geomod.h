#ifndef RF_GEOMOD_H
#define RF_GEOMOD_H
#include "rf/vpp.h"
#define RF_GEOMOD_POLYGON_LIMIT 64
typedef struct rf_geomod_vertex {float position[3],uv[2];} rf_geomod_vertex;
/* Practical port CSG primitive, not an original executable binding.
 * Split a planar convex polygon by unit plane n.xyz*p+d=0. Positive is front.
 * Vertices within 1e-5 are coplanar; a wholly coplanar polygon belongs to front.
 * UVs interpolate with position. Winding is retained. No allocation. Inputs
 * and output buffers must not overlap. NULL buffers query required counts.
 * Capacity/numeric errors leave both outputs and counts unchanged. Convexity
 * and planarity are caller requirements. At most64 vertices per result. */
int rf_geomod_polygon_split(const rf_geomod_vertex *vertices,uint32_t count,
    const float plane[4],rf_geomod_vertex *front,uint32_t front_capacity,
    rf_geomod_vertex *back,uint32_t back_capacity,uint32_t *front_count,uint32_t *back_count);
typedef struct rf_geomod_fragment {uint32_t first,count;} rf_geomod_fragment;
/* Subtract a convex cutter (interior is negative for every plane) from one
 * convex surface polygon. Returns disjoint surviving convex fragments. This
 * does not generate a solid's interior caps. Source winding must face out of
 * its solid: same-facing coplanar cutter boundaries remove the overlap;
 * opposite-facing boundaries only touch and survive. Up to32 planes. NULL outputs
 * query sizes; otherwise both arrays are required, disjoint from inputs.
 * Errors preserve output arrays/counts. No allocation. */
int rf_geomod_polygon_subtract(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count);
/* Clip an outward cutter face to a convex source solid's negative half-spaces
 * and reverse winding to form an exposed interior face. A face wholly on a
 * source boundary yields no cap. Same bounded/atomic buffer contract as split.
 * This primitive is not a complete Boolean-solid owner or coplanar-face policy. */
int rf_geomod_interior_face(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*source_planes)[4],uint32_t plane_count,rf_geomod_vertex *out,
    uint32_t capacity,uint32_t *out_count);
typedef struct rf_geomod_face {uint32_t first,count,material,source_face;} rf_geomod_face;
typedef struct rf_geomod_mesh_view {
    const rf_geomod_vertex *vertices;const rf_geomod_face *faces;
    uint32_t vertex_count,face_count,generation;
} rf_geomod_mesh_view;
typedef struct rf_geomod_storage rf_geomod_storage;
/* Owns original/reset data and two bounded working banks in one allocation.
 * Budget includes the owner, excludes allocator overhead. Open requires *out
 * NULL. Append never allocates. Begin builds a replacement, not an in-place edit.
 * Abort leaves current data untouched; commit swaps banks. A borrowed view is
 * valid only until the next begin/reset/close. Caller must validate topology
 * and prepare dependent render/collision resources before commit. This owner
 * validates storage/numbers, not solid closure or material-resource existence. */
int rf_geomod_storage_open(const rf_geomod_mesh_view *source,uint32_t vertex_capacity,
    uint32_t face_capacity,uint32_t budget,rf_geomod_storage **out);
void rf_geomod_storage_close(rf_geomod_storage **storage);
int rf_geomod_storage_view(const rf_geomod_storage *storage,rf_geomod_mesh_view *out);
uint32_t rf_geomod_storage_bytes(const rf_geomod_storage *storage);
int rf_geomod_storage_begin(rf_geomod_storage *storage);
int rf_geomod_storage_pending(const rf_geomod_storage *storage,rf_geomod_mesh_view *out);
int rf_geomod_storage_append(rf_geomod_storage *storage,const rf_geomod_vertex *vertices,
    uint32_t count,uint32_t material,uint32_t source_face);
int rf_geomod_storage_commit(rf_geomod_storage *storage);
void rf_geomod_storage_abort(rf_geomod_storage *storage);
int rf_geomod_storage_reset(rf_geomod_storage *storage);
#endif
