#ifndef RF_COLLISION_H
#define RF_COLLISION_H
#include "rf/vpp.h"
typedef struct rf_collision_bounds {
    float minimum[3],maximum[3],radius,center[3],origin_radius;
} rf_collision_bounds;
/* 4cf9a0/4cf500 bounds plus 46b075 creation radius. Preserve input vertex
 * order, including unused and duplicate vertices. No allocation. Empty input
 * returns NOT_FOUND; errors preserve output. Nonfinite/overflow is FORMAT.
 * Exact x87 arithmetic on supported x86 PC/NXDK; other targets unverified. */
int rf_collision_vertex_bounds(const float (*vertices)[3],uint32_t count,
    rf_collision_bounds *result);
/* Input preparation of 4df1c0. Matrix rows dot (point-origin), with original
 * endpoint and vector stores; do not replace with a direct rotated delta.
 * Flag 4 copies local inputs and ignores origin/matrix. Zero displacement
 * sets active=0 and preserves vectors; errors preserve all outputs.
 * Other query flags are passed through here without interpretation. */
int rf_collision_query_local(const float start[3],const float displacement[3],
    const float origin[3],const float matrix[3][3],uint32_t flags,
    float local_start[3],float local_displacement[3],uint32_t *active);
/* Complete 508b70 with 508dc0: inclusive axis-aligned box / segment test.
 * If start is inside, copies start; otherwise an inside end is copied before
 * testing planes. This is NOT a nearest-hit calculation. Failed plane attempts
 * still overwrite point, matching the original; trivial rejection preserves it.
 * Finite, ordered bounds and nonoverlapping inputs/outputs required. Invalid
 * data leaves point/hit untouched. No allocation or world traversal. */
int rf_collision_segment_box(const float minimum[3],const float maximum[3],
    const float start[3],const float end[3],float point[3],uint32_t *hit);
/* Complete 506550: start plus displacement against plane (normal,d).
 * Rejects a start behind the plane or insufficient front-to-back travel.
 * Misses preserve fraction. Coplanar parallel input returns hit=1 and NaN,
 * matching the original; callers must resolve that before using the fraction.
 * Finite inputs required; invalid inputs preserve fraction/hit. */
int rf_collision_segment_plane(const float start[3],const float displacement[3],
    const float plane[4],float *fraction,uint32_t *hit);
/* Complete 5071b0 sphere against plane. Requires approach toward the front
 * side; a center behind the plane is rejected even if radius overlaps it.
 * Initial front-side overlap returns fraction zero and a projected contact.
 * Misses preserve fraction/point; invalid finite/radius inputs preserve all
 * outputs. This is a plane test, not polygon/edge or world collision. */
int rf_collision_sphere_plane(const float start[3],const float displacement[3],
    float radius,const float plane[4],float *fraction,float point[3],uint32_t *hit);
/* 5072e0 finite edge with starting-endpoint fallback only. Strict limit,
 * tangent rejection and original small-negative-time clamp. Local scratch.
 * Misses preserve fraction/point; malformed or overflowing terms preserve all
 * outputs. Finite inputs, nonnegative radius and limit [0,1] required. */
int rf_collision_sphere_edge(const float start[3],const float delta[3],float radius,
    const float a[3],const float b[3],float limit,float *fraction,float point[3],uint32_t *hit);
/* 4e1f50 + projection-axis selection 4fa6d0, using an ordered closed
 * vertex array instead of the original circular edge list. Exact half-open
 * crossing rule; no epsilon or generic on-edge override. Tests projected
 * containment only, not coplanarity. Finite inputs and count 1..65536 required;
 * errors preserve inside. No allocation. */
int rf_collision_polygon_contains(const float normal[3],const float point[3],
    const float (*vertices)[3],uint32_t count,uint32_t *inside);
typedef struct rf_collision_face_filter {
    uint32_t query_flags,face_flags; /* Query +50, face +28. */
    int32_t property_34; /* Signed 16-bit face +34. */
    uint32_t owner_present,owner_kind,owner_state; /* Face +44, owner +0/+98 bytes. */
} rf_collision_face_filter;
/* 4dec10..4deced, including all seven original predicates. Names of unknown
 * bits remain unresolved. accepted=1 proceeds to geometric testing. Invalid
 * signed-word/byte/boolean views preserve accepted. */
int rf_collision_face_accept(const rf_collision_face_filter *filter,uint32_t *accepted);
typedef struct rf_collision_face {
    float plane[4],minimum[3],maximum[3];
    const float (*vertices)[3];uint32_t count;
    rf_collision_face_filter filter;
} rf_collision_face;
typedef struct rf_collision_ray_hit {float fraction,point[3],normal[3];} rf_collision_ray_hit;
/* Geometric output conversion of 498e80: matrix columns dot local point and
 * normal, then translate the stored point. No normal normalization. Caller
 * supplies output pose (moving solid +e4/+fc), distinct from query input pose.
 * Finite inputs required; errors preserve output. Input/output may alias. */
int rf_collision_contact_world(const rf_collision_ray_hit *local,const float origin[3],
    const float matrix[3][3],rf_collision_ray_hit *world);
/* Thin, zero-radius geometric path of 4dec10. Includes filters, box, plane,
 * nearest-fraction gate and polygon containment. Accepted hits replace result;
 * misses/errors preserve it. matched is set only on success. Texture-check
 * flags 0x80/0x100 are currently unsupported (RF_NOT_FOUND after filtering).
 * The crouch visibility mask 0x27 maps to supported internal flags 0x461.
 * Finite data and fraction limit [0,1] required. Coplanar NaN is a port FORMAT
 * error. This does not walk the world, increment original counters or own faces. */
int rf_collision_thin_face(const rf_collision_face *face,const float start[3],
    const float displacement[3],float limit,rf_collision_ray_hit *result,uint32_t *matched);
typedef struct rf_collision_sweep_hit {
    rf_collision_ray_hit hit;uint32_t edge,hits;
} rf_collision_sweep_hit;
/* Geometric 4dec10 sweep with fresh hit count. Query +60 displacement drives
 * contact; separate +40 normal_displacement drives edge response normals.
 * Radius below 0.0001 uses thin path. Ordered vertices close the edge loop.
 * Unsupported texture flags, errors and misses follow thin_face conventions.
 * No world traversal, transforms or actor response; no allocation. */
int rf_collision_sweep_face(const rf_collision_face *face,const float start[3],
    const float displacement[3],const float normal_displacement[3],float radius,
    float limit,rf_collision_sweep_hit *result,uint32_t *matched);

typedef struct rf_collision_node {
    float minimum[3],maximum[3];
    uint32_t first_face,face_count,left,right; /* UINT32_MAX means no child. */
} rf_collision_node;
typedef struct rf_collision_tree_hit {
    rf_collision_ray_hit hit;uint32_t face_index,hits;
} rf_collision_tree_hit;
/* 4deab0 zero-radius traversal: node faces first, right child before left;
 * query bit 0 returns first accepted hit, otherwise retain nearest (ties replace).
 * Nodes form a tree rooted at zero and reference ordered ranges in faces.
 * Caller provides node_count stack entries. No allocation or world room
 * selection. Errors preserve result/matched; scratch may change. */
int rf_collision_thin_tree(const rf_collision_node *nodes,uint32_t node_count,
    const rf_collision_face *faces,uint32_t face_count,uint32_t query_flags,
    const float start[3],const float displacement[3],float limit,
    uint32_t *stack,uint32_t capacity,rf_collision_tree_hit *result,uint32_t *matched);
typedef struct rf_collision_sweep_tree_hit {
    rf_collision_ray_hit hit;uint32_t face_index,hits,edge;
} rf_collision_sweep_tree_hit;
/* Swept 4deab0: expand every node by radius, then ordered finite-face queries.
 * Same stack/ownership and first-hit rules as thin_tree; count every improving
 * contact (including multiple edges within one face). Inputs are solid-local;
 * the separate normal displacement retains original query +40 semantics. */
int rf_collision_sweep_tree(const rf_collision_node *nodes,uint32_t node_count,
    const rf_collision_face *faces,uint32_t face_count,uint32_t query_flags,
    const float start[3],const float displacement[3],const float normal_displacement[3],float radius,float limit,
    uint32_t *stack,uint32_t capacity,rf_collision_sweep_tree_hit *result,uint32_t *matched);
/* 4f9050 split decision before allocation. Upper half is tested first;
 * labels are 0=parent, 1=upper, 2=lower. Both child counts must be nonzero.
 * Caller supplies count labels. No mutation of faces or node; invalid inputs
 * preserve outputs. Node bounds must enclose all face bounds. */
int rf_collision_partition(const rf_collision_node *node,const rf_collision_face *faces,
    uint32_t count,uint8_t *labels,uint32_t *axis,uint32_t counts[3]);
typedef struct rf_collision_tree {
    void *storage;rf_collision_node *nodes;rf_collision_face *faces;
    uint32_t *source_indices,*stack,node_count,face_count,node_capacity;
    uint32_t allocated_bytes,peak_bytes;
} rf_collision_tree;
/* Build original ordered partitions and child bounds without recursive stack
 * growth. Copies face views, but borrows their vertex arrays. Budget includes
 * this struct, retained storage and construction scratch (not allocator overhead).
 * Failure preserves output. Close an existing tree before reusing its output. */
int rf_collision_tree_open(const rf_collision_face *faces,uint32_t count,uint32_t budget,rf_collision_tree *tree);
void rf_collision_tree_close(rf_collision_tree *tree);
typedef struct rf_collision_room_view {
    float minimum[3],maximum[3];
    uint32_t skip,first_child,child_count; /* Original room +1 byte, +6c array. */
    const rf_collision_tree *tree;
} rf_collision_room_view;
typedef struct rf_collision_room_hit {rf_collision_tree_hit tree;uint32_t room;} rf_collision_room_hit;
/* Uncached zero-radius hierarchy branch of 4df1c0, in solid-local coordinates.
 * Ordered primary rooms and their ordered children; children are not recursive.
 * Primary skip byte is bypassed by query mask 8; children ignore that byte.
 * Requires prepared runtime room lists, no cached/preferred face or transforms.
 * Texture modes 80/100 and special room-face mode 1000 are unsupported.
 * No allocation; uses each tree's stack. Errors preserve result/matched. */
int rf_collision_thin_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],float limit,
    rf_collision_room_hit *result,uint32_t *matched);
typedef struct rf_collision_sweep_room_hit {rf_collision_sweep_tree_hit tree;uint32_t room;} rf_collision_sweep_room_hit;
/* Uncached local-coordinate hierarchy of 4df1c0 with radius-expanded sweep
 * bounds. Same primary/child selection, unsupported modes and owned tree
 * scratch rules as thin_rooms. Start/displacement are already solid-local. */
int rf_collision_sweep_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],float radius,float limit,
    rf_collision_sweep_room_hit *result,uint32_t *matched);
/* Uncached hierarchy query including 4df1c0 input transformation. Results
 * remain in the original function's local contact convention; edge normals
 * retain original +40 displacement semantics. World-output conversion is a
 * separate caller step. Flag 4 bypasses origin/matrix; no caches/special modes. */
int rf_collision_transformed_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],const float origin[3],
    const float matrix[3][3],float radius,float limit,rf_collision_sweep_room_hit *result,uint32_t *matched);
typedef struct rf_collision_solid_view {
    const rf_collision_room_view *rooms;uint32_t room_count;
    const uint32_t *primary;uint32_t primary_count;
    const uint32_t *children;uint32_t child_count;
    float minimum[3],maximum[3],input_origin[3],input_matrix[3][3];
    float output_origin[3],output_matrix[3][3];uint32_t object_id;
    const rf_collision_face *flat_faces;uint32_t flat_count; /* Used when room_count is zero. */
} rf_collision_solid_view;
typedef struct rf_collision_solid_hit {
    rf_collision_ray_hit hit;uint32_t object_id,solid_index,room,face_index;
} rf_collision_solid_hit;
/* Geometric 498e80 composition: ordered movers followed by static world.
 * External flags are translated by original 499190. Radius is zero. Original
 * retained fraction after shortening is intentional; not a generic closest ray.
 * Result may be NULL for visibility-only queries. No material lookup, caches,
 * preferred faces or world face-ID remapping. Scratch is shared/serialized. */
int rf_collision_ray_solids(const rf_collision_solid_view *moving,uint32_t count,
    const rf_collision_solid_view *stationary,const float start[3],const float end[3],
    uint32_t flags,rf_collision_solid_hit *result,uint32_t *matched);
/* Uncached no-room branch of 4df1c0: ordered solid face list (+70, next +54).
 * Includes query transformation. Unlike the hierarchy path, query bit 0 does
 * not stop face iteration. No hierarchy/preferred-face/cache handling. */
int rf_collision_flat_faces(const rf_collision_face *faces,uint32_t count,uint32_t flags,
    const float start[3],const float delta[3],const float origin[3],const float matrix[3][3],
    float radius,float limit,rf_collision_sweep_tree_hit *result,uint32_t *matched);
#endif
