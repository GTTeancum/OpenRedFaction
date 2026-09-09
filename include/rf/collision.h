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
#endif
