#ifndef RF_SCENE_PURSUIT_RETRY_CHECKPOINT_INCLUDED
#define RF_SCENE_PURSUIT_RETRY_CHECKPOINT_INCLUDED
/* UNCALLED bounded owner payload proposal, not an installed checkpoint row.
 * Outer ownership/profile grammar must distinguish old absence from a present
 * disarmed clock. RNG remains retained even when disarmed or expired.
 * Capture canonicalizes only the output and never ages the live actor.
 */
typedef struct scene_pursuit_retry_wire {
    uint32_t flags,remaining,random;
} scene_pursuit_retry_wire;
_Static_assert(sizeof(scene_pursuit_retry_wire)==12,
    "Pursuit retry source wire proposal changed");
static int scene_pursuit_retry_capture_wire(const scene_pursuit_retry_clock *r,
    int32_t now,scene_pursuit_retry_wire *out)
{
    scene_pursuit_retry_wire next={0};int32_t remaining;int status;
    if(!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=scene_pursuit_retry_admit(r);if(status)return status;
    next.flags=r->flags;next.random=r->random.value;
    if((r->flags&SCENE_PURSUIT_RETRY_ARMED)&&
       !(r->flags&SCENE_PURSUIT_RETRY_EXPIRED)){
        status=rf_timer_remaining(r->due,now,&remaining);if(status)return status;
        if(remaining>1000)return RF_FORMAT;
        if(remaining>0)next.remaining=(uint32_t)remaining;
        else next.flags|=SCENE_PURSUIT_RETRY_EXPIRED;
    }
    *out=next;return RF_OK;
}
static int scene_pursuit_retry_prepare_wire(const scene_pursuit_retry_wire *wire,
    int32_t now,scene_pursuit_retry_clock *out)
{
    scene_pursuit_retry_clock next={0};int status;
    if(!wire||!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if((wire->flags&~3u)||wire->remaining>1000||
       ((wire->flags&SCENE_PURSUIT_RETRY_EXPIRED)&&
        !(wire->flags&SCENE_PURSUIT_RETRY_ARMED)))return RF_FORMAT;
    next.flags=wire->flags;next.random.value=wire->random;
    if((wire->flags&SCENE_PURSUIT_RETRY_ARMED)&&
       !(wire->flags&SCENE_PURSUIT_RETRY_EXPIRED)){
        if(!wire->remaining)return RF_FORMAT;
        status=rf_timer_set(&next.due,now,(int32_t)wire->remaining);
        if(status)return status;
    }else if(wire->remaining)return RF_FORMAT;
    status=scene_pursuit_retry_admit(&next);if(status)return status;
    *out=next;return RF_OK;
}
#endif
