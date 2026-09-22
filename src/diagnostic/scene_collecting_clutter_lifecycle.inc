#ifndef RF_SCENE_COLLECTING_CLUTTER_LIFECYCLE_INCLUDED
#define RF_SCENE_COLLECTING_CLUTTER_LIFECYCLE_INCLUDED
#include "rf/clutter.h"
#include "rf/timer.h"

/* 405460: the caller filters the live clutter list by distance/visibility,
 * then reserves the chosen object synchronously. These flags are not an
 * ownership token: the caller must retain its generation-bearing handle and
 * release only its own reservation on cancellation. No attachment implied. */
static uint32_t scene_collecting_clutter_eligible(const rf_clutter_state *state)
{
    return state && (state->flags&0x1000u) && !(state->flags&0x2000u);
}
static int scene_collecting_clutter_reserve(rf_clutter_state *state)
{
    if(!state || state->handle==UINT32_MAX)return RF_RANGE;
    if(!scene_collecting_clutter_eligible(state))return RF_NOT_FOUND;
    state->flags|=0x2000u;return RF_OK;
}

/* Only AFTER successful detach from the owning actor: 40599b..4059dd enables
 * physics and calls410cf0(0.5 seconds, even handle10/odd11). 410c70 only checks
 * object type4; this typed state is supplied by the caller's live lookup.
 * Original collection leaves flags100/2000 intact: do not make the dropped
 * prop collectible again. Delayed kill/retirement must consume timer_a4 and
 * word_a8 elsewhere. This helper does not detach, destroy, play sound or
 * clear the actor's target. Existing unrelated state is preserved. */
static int scene_collecting_clutter_released(rf_clutter_state *state,int32_t now)
{
    int32_t deadline;int status;
    if(!state || state->handle==UINT32_MAX)return RF_RANGE;
    status=rf_timer_set(&deadline,now,500);if(status)return status;
    state->physics_flags|=0x80000001u;
    state->timer_a4=deadline;state->word_a8=(state->handle&1u)?11:10;
    return RF_OK;
}
#endif
