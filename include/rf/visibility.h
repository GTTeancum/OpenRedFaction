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
#endif
