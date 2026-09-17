#ifndef RF_DEBRIS_VISIBILITY_H
#define RF_DEBRIS_VISIBILITY_H
#include "rf/visibility.h"
/* Original4d4708 visible-list membership +4d4c60/5189a0 room AABB gate.
 * Supply the CURRENT active world clipping planes, including the room's
 * rectangle adjustment. Camera frustum alone is not an established substitute.
 * room bounds belong to the retained original room, not the debris position or
 * expanded reconstruction collision overlay. Plane.corner is the original
 * minimum-distance selector (5398a0); use rf_visibility_plane_* constructors.
 * Reject iff any minimum-corner signed distance >0; tangency is admitted.
 * 0..6 finite planes, ordered finite bounds, membership0/1. No allocation.
 * Output unchanged on error. Caller owns list order and draw-time aging. */
int rf_debris_room_render_admit(const rf_visibility_plane *active_planes,uint32_t plane_count,
    const float minimum[3],const float maximum[3],uint32_t visible_list_member,uint32_t *admitted);
/* Perspective room rectangle adapter (546f60/5184e0). Pixel rectangle is
 * left/top/right/bottom in a zero-origin viewport. Replaces four side planes,
 * retaining near/far slots, count and distances. No flat/special-room modes.
 * Output unchanged on error; caller supplies the current camera frustum. */
int rf_debris_room_rectangle(const rf_visibility_view *view,float width,float height,
    const float rectangle[4],rf_visibility_frustum *active);
#endif
