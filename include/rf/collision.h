#ifndef RF_COLLISION_H
#define RF_COLLISION_H
#include "rf/vpp.h"
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
/* Thin, zero-radius geometric path of 4dec10. Includes filters, box, plane,
 * nearest-fraction gate and polygon containment. Accepted hits replace result;
 * misses/errors preserve it. matched is set only on success. Texture-check
 * flags 0x80/0x100 are currently unsupported (RF_NOT_FOUND after filtering).
 * The crouch visibility mask 0x27 maps to supported internal flags 0x461.
 * Finite data and fraction limit [0,1] required. Coplanar NaN is a port FORMAT
 * error. This does not walk the world, increment original counters or own faces. */
int rf_collision_thin_face(const rf_collision_face *face,const float start[3],
    const float displacement[3],float limit,rf_collision_ray_hit *result,uint32_t *matched);
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
#endif
