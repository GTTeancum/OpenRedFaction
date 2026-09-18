#ifndef RF_HITSCAN_SELECT_H
#define RF_HITSCAN_SELECT_H
#include "rf/object_registry.h"
#include "rf/physics.h"
typedef struct rf_hitscan_candidate {
    uint32_t handle,eligible;
    const void *identity; /* Exactly the object pointer registered for handle. */
    const rf_physics_body *body; /* Default trace; optional with callback. */
    const void *geometry; /* Opaque callback data, borrowed. */
} rf_hitscan_candidate;
typedef struct rf_hitscan_selection {
    uint32_t matched,index,handle;
    float fraction;
} rf_hitscan_selection;
typedef int (*rf_hitscan_trace)(void *context,const rf_hitscan_candidate *,
    const float start[3],const float delta[3],float limit,float *fraction,uint32_t *matched);
/* Stable ordered borrowed snapshot, no allocation. First candidate wins exact
 * equal fractions, consistent with original49c690 actor/clutter distance gates
 * (21 original cases). Caller supplies actors then clutter in insertion order
 * to match that wrapper; this is NOT reconstruction of full SP shot dispatch.
 * Body-sphere default is the existing practical port shape, not mesh fidelity.
 * Optional pure callback permits precise clutter models without scene globals.
 * World obstruction and gameplay eligibility (dead/hidden/self/class policies)
 * are caller-owned. Initial limit inclusive, within0..1. No match returns OK,
 * matched0/index/handle UINT32_MAX and fraction=limit. Errors preserve output.
 * Ineligible, null identity, invalid/stale handle or pointer mismatch skip
 * before borrowed body/geometry is inspected. Live malformed shapes or callback
 * fractions/status fail atomically. Registry/snapshot/geometry must remain
 * stable for the whole call; callbacks must not mutate owners or registry.
 * Revalidate the selected handle at later damage publication. */
int rf_hitscan_select(const rf_object_registry *,const rf_hitscan_candidate *,
    uint32_t count,const float start[3],const float delta[3],float limit,
    rf_hitscan_trace trace,void *context,rf_hitscan_selection *result);
#endif
