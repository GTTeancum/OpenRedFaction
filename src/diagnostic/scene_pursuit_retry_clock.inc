#ifndef RF_SCENE_PURSUIT_RETRY_CLOCK_INCLUDED
#define RF_SCENE_PURSUIT_RETRY_CLOCK_INCLUDED
/* Pure, uncalled staging for source retry+230. Construction40e668 first
 * disarms; actor initialization402ce2 then arms offset0. Neither operation
 * belongs to a repeated target setter409050. Caller owns actor admission,
 * target/route binding, saved identity and callback-safe publication.
 * Private RNG is an explicit bounded-port adaptation, not global CRT state. */
#define SCENE_PURSUIT_RETRY_ARMED 1u
#define SCENE_PURSUIT_RETRY_EXPIRED 2u
typedef struct scene_pursuit_retry_clock {
    uint32_t flags;
    int32_t due;
    rf_random_state random;
} scene_pursuit_retry_clock;
static int scene_pursuit_retry_admit(const scene_pursuit_retry_clock *r)
{
    if(!r)return RF_RANGE;
    if((r->flags&~3u)||
       ((r->flags&SCENE_PURSUIT_RETRY_EXPIRED)&&!(r->flags&SCENE_PURSUIT_RETRY_ARMED)))return RF_FORMAT;
    if((r->flags&SCENE_PURSUIT_RETRY_ARMED)&&!(r->flags&SCENE_PURSUIT_RETRY_EXPIRED)){
        if(r->due<0||r->due>RF_TIMER_PERIOD)return RF_FORMAT;
    }else if(r->due)return RF_FORMAT;
    return RF_OK;
}
static int scene_pursuit_retry_age(scene_pursuit_retry_clock *r,int32_t now)
{
    int expired,status=scene_pursuit_retry_admit(r);if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!(r->flags&SCENE_PURSUIT_RETRY_ARMED)||(r->flags&SCENE_PURSUIT_RETRY_EXPIRED))return RF_OK;
    status=rf_timer_expired(r->due,now,&expired);if(status)return status;
    if(expired){r->flags|=SCENE_PURSUIT_RETRY_EXPIRED;r->due=0;}
    return RF_OK;
}
/* Only the proved ordinary fixed offsets: initialization/melee refresh0,
 * visible in-range refresh500, running route failure1000. RNG is retained.
 * A fixed0 is already expired and canonicalizes immediately to due0. */
static int scene_pursuit_retry_fixed(scene_pursuit_retry_clock *r,int32_t now,
    uint32_t milliseconds)
{
    scene_pursuit_retry_clock next;int status=scene_pursuit_retry_admit(r);
    if(status)return status;
    if(milliseconds!=0&&milliseconds!=500&&milliseconds!=1000)return RF_RANGE;
    next=*r;
    status=rf_timer_set(&next.due,now,(int32_t)milliseconds);if(status)return status;
    next.flags=SCENE_PURSUIT_RETRY_ARMED;
    if(!milliseconds){next.flags|=SCENE_PURSUIT_RETRY_EXPIRED;next.due=0;}
    *r=next;return RF_OK;
}
/* Admission failure alone consumes a private draw for inclusive500..1000.
 * Running failure consumes none and uses exactly1000. No route/state write
 * is implied; the caller must commit both outcome and clock atomically. */
static int scene_pursuit_retry_failure(scene_pursuit_retry_clock *r,int32_t now,
    uint32_t admission)
{
    scene_pursuit_retry_clock next;int status=scene_pursuit_retry_admit(r);
    if(status)return status;if(admission>1)return RF_RANGE;
    if(!admission)return scene_pursuit_retry_fixed(r,now,1000);
    next=*r;
    status=rf_timer_set_random(&next.due,now,500,1000,&next.random);if(status)return status;
    next.flags=SCENE_PURSUIT_RETRY_ARMED;*r=next;return RF_OK;
}
#endif
