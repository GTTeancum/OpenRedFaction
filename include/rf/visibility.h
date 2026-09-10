#ifndef RF_VISIBILITY_H
#define RF_VISIBILITY_H
#include "rf/vpp.h"
typedef struct rf_room_visibility {
    uint32_t visible,visited,depth;
    float rectangle[4];
} rf_room_visibility;
/* Borrowed arrays, capacity count; initialize rooms to zero before first use.
 * Begin-render clears prior render eligibility (4d2f80). Begin-view clears
 * traversal state (4d4be0/4d4760), retaining eligibility across player views.
 * Visit is the nonrecursive bookkeeping in 4d4860, after room predicates.
 * Caller performs portal clipping/eligibility. Rect order: left, top, right,
 * bottom. Repeated visits union bounds and move the room to the list tail.
 * No allocation. Errors preserve state. Arrays must not overlap. */
typedef struct rf_visibility {
    rf_room_visibility *rooms;
    uint32_t *order,count,visible_count;
} rf_visibility;
int rf_visibility_begin_render(rf_visibility *state);
int rf_visibility_begin_view(rf_visibility *state);
int rf_visibility_visit(rf_visibility *state,uint32_t room,const float rectangle[4],uint32_t depth);
typedef struct rf_visibility_room_links {
    uint32_t first,count,blocked,detail;
} rf_visibility_room_links;
typedef struct rf_visibility_portal {
    uint32_t rooms[2],rejected;
    float rectangle[4];
} rf_visibility_portal;
typedef struct rf_visibility_plane {float normal[3],distance;uint32_t corner;} rf_visibility_plane;
enum {RF_PORTAL_PROJECT=0,RF_PORTAL_FULL_VIEW=1,RF_PORTAL_REJECT=2};
/* 4d4860 preprojection branch using 507ba0 (inclusive one-unit expanded box)
 * and 518750 (strict positive plane distance at supplied extreme corner).
 * Planes/corner selectors come from the original view-plane setup; this does
 * not derive them or calculate the projected rectangle. Errors preserve action. */
int rf_visibility_portal_classify(const float camera[3],const float minimum[3],const float maximum[3],
    const rf_visibility_plane *planes,uint32_t count,uint32_t *action);
typedef struct rf_visibility_frame {
    uint32_t room,cursor,depth;float rectangle[4];
} rf_visibility_frame;
/* 4d4860 traversal using caller-resolved portal screen rectangles. Begin-view
 * first. Scratch has 257 frames (7196 bytes); no recursion or allocation.
 * Room links and adjacency list preserve authored order. This does not compute
 * portal projection/cache results. Flags bit 1 disables traversal. The special
 * room bypasses the detail-room stop. Errors may follow earlier visits. */
int rf_visibility_traverse(rf_visibility *state,const rf_visibility_room_links *rooms,
    const uint32_t *links,uint32_t link_count,const rf_visibility_portal *portals,
    uint32_t portal_count,uint32_t start,uint32_t special,uint32_t flags,
    const float rectangle[4],rf_visibility_frame scratch[257]);
#endif
