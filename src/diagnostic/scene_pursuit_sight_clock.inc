#ifndef RF_SCENE_PURSUIT_SIGHT_CLOCK_INCLUDED
#define RF_SCENE_PURSUIT_SIGHT_CLOCK_INCLUDED
/* Pure clock policy. Actor identity, admission, capture, restoration,
 * callback closure and lifecycle composition are not supplied by this file.
 * Original constructor 40e684 initializes the clock to disarmed (-1), not due.
 * 405d43..405d6c arms inclusive 2000..5000 ms only on visible -> hidden.
 * The scene supplies its existing bounded visibility sampler. This helper never performs queries or seeds global RNG. */
#define SCENE_PURSUIT_SEEN 1u
#define SCENE_PURSUIT_ARMED 2u
#define SCENE_PURSUIT_EXPIRED 4u
typedef struct scene_pursuit_sight_clock {
    uint32_t flags;
    int32_t due;
    rf_random_state random;
} scene_pursuit_sight_clock;
static int scene_pursuit_sight_admit(const scene_pursuit_sight_clock *r)
{
    if(!r)return RF_RANGE;
    if(r->flags&~7u)return RF_FORMAT;
    if((r->flags&SCENE_PURSUIT_EXPIRED)&&!(r->flags&SCENE_PURSUIT_ARMED))return RF_FORMAT;
    if((r->flags&SCENE_PURSUIT_ARMED)&&!(r->flags&SCENE_PURSUIT_EXPIRED)){
        if(r->due<0||r->due>RF_TIMER_PERIOD)return RF_FORMAT;
    }else if(r->due)return RF_FORMAT;
    return RF_OK;
}
/* Called by ordinary clock service even while query/update is suppressed.
 * Saturate expiry explicitly, so a long pause cannot wrap back into grace.
 * Capture is read-only and must never call this helper to repair live state. */
static int scene_pursuit_sight_age(scene_pursuit_sight_clock *r,int32_t now)
{
    int expired,status=scene_pursuit_sight_admit(r);if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!(r->flags&SCENE_PURSUIT_ARMED)||(r->flags&SCENE_PURSUIT_EXPIRED))return RF_OK;
    status=rf_timer_expired(r->due,now,&expired);if(status)return status;
    if(expired){r->flags|=SCENE_PURSUIT_EXPIRED;r->due=0;}
    return RF_OK;
}
/* No state or RNG changes on a validation/timer failure. Seeing again keeps
 * the old clock, matching the source; the next genuine loss rearms it. */
static int scene_pursuit_sight_sample(scene_pursuit_sight_clock *r,int32_t now,uint32_t visible)
{
    scene_pursuit_sight_clock next;int status=scene_pursuit_sight_admit(r);if(status)return status;
    if(visible>1)return RF_RANGE;
    next=*r;status=scene_pursuit_sight_age(&next,now);if(status)return status;
    if(!visible&&(next.flags&SCENE_PURSUIT_SEEN)){
        status=rf_timer_set_random(&next.due,now,2000,5000,&next.random);if(status)return status;
        next.flags=(next.flags|SCENE_PURSUIT_ARMED)&~SCENE_PURSUIT_EXPIRED;
    }
    next.flags=(next.flags&~SCENE_PURSUIT_SEEN)|(visible?SCENE_PURSUIT_SEEN:0u);
    *r=next;return RF_OK;
}
#endif
