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
#endif
