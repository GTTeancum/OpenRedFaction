#ifndef RF_GEOMOD_H
#define RF_GEOMOD_H
#include "rf/vpp.h"
#include "rf/collision.h"
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
/* Caller-owned scratch, normally retained on heap, not the Xbox thread stack. */
typedef struct rf_geomod_cut_work {
    rf_geomod_vertex vertices[RF_GEOMOD_POLYGON_LIMIT*32];
    rf_geomod_fragment fragments[32];
} rf_geomod_cut_work;
/* Prepare a complete convex-source minus convex-cutter replacement. Caller
 * supplies closed, outward-wound convex meshes (up to32 faces each); finite
 * data, face bounds/planes and convex half-space containment are checked.
 * Non-convex live results must be handled by the future repeated-cut layer,
 * not passed back as a convex source. No allocation. Failure aborts this new
 * edit and preserves live data. Success leaves pending data for validation
 * and dependent render/collision preparation, then explicit commit/abort.
 * Old surface material/source IDs survive; interior materials come from the
 * cutter and interior source_face is UINT32_MAX. Work must not alias inputs. */
int rf_geomod_storage_prepare_convex_cut(rf_geomod_storage *storage,
    const rf_geomod_mesh_view *cutter,rf_geomod_cut_work *work);
/* Bounded scratch for rebuilding original convex terrain minus a union of
 * cutters. Retain on heap and include sizeof(*work) in the Xbox memory budget. */
#define RF_GEOMOD_CUT_LIMIT 8
#define RF_GEOMOD_WORK_VERTICES 4096
#define RF_GEOMOD_WORK_FRAGMENTS 512
typedef struct rf_geomod_multi_work {
    rf_geomod_cut_work split;
    rf_geomod_vertex vertices[2][RF_GEOMOD_WORK_VERTICES];
    rf_geomod_fragment fragments[2][RF_GEOMOD_WORK_FRAGMENTS];
    float source_planes[32][4],cut_planes[RF_GEOMOD_CUT_LIMIT][32][4];
} rf_geomod_multi_work;
/* Rebuild from immutable original data, never from a concave working result.
 * The caller supplies the COMPLETE ordered cutter history (0..8), including
 * prior committed cuts. Original and cutters must be closed outward convex
 * meshes as above. Same-facing coincident interiors belong to the earliest
 * cutter; internal faces between touching cutters are removed. Success leaves
 * an uncommitted replacement; overflow/invalid data preserves the live bank.
 * Zero cutters prepares the original. No allocation or retained input pointers.
 * Work and cutter inputs must not alias storage or each other. */
int rf_geomod_storage_prepare_cuts(rf_geomod_storage *storage,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work);
/* Bind a prepared mesh to the existing collision tree/query implementation.
 * Caller provides one explicit filter per face and persistent position/face
 * arrays sized to mesh counts. Faces borrow the output positions, never mesh
 * storage. Output order preserves mesh face/material lookup indices. Validates
 * finite planar convex polygons; errors preserve both arrays. No allocation.
 * Arrays/filters/mesh must be disjoint and stable throughout the call. This
 * does not publish a tree or alter the live world. */
int rf_geomod_collision_faces(const rf_geomod_mesh_view *mesh,
    const rf_collision_face_filter *filters,float (*positions)[3],uint32_t vertex_capacity,
    rf_collision_face *faces,uint32_t face_capacity);
#endif
