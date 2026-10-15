#ifndef RF_SCENE_PURSUIT_ROUTE_COMPLETION_PLAN_INCLUDED
#define RF_SCENE_PURSUIT_ROUTE_COMPLETION_PLAN_INCLUDED
/* Pure, uncalled composition of transition_plan.inc and retry_clock.inc.
 * Call only for a completed RF_OK graph request: errors are not route misses.
 * One private draw supplies BOTH the admission-failure proposal and deadline.
 * Do not also call scene_pursuit_retry_failure for this same completion.
 * Caller owns clock aging/refresh, identity, route geometry, scheduling and
 * atomic publication. This does not admit any live actor or save profile. */
typedef struct scene_pursuit_route_completion_plan {
    scene_pursuit_route_result outcome;
    scene_pursuit_retry_clock retry;
} scene_pursuit_route_completion_plan;
static int scene_pursuit_route_completion_prepare(
    const scene_pursuit_retry_clock *current,int32_t now,
    uint32_t admission,uint32_t success,uint32_t special_visible,
    uint32_t body_visible,uint32_t eye_visible,
    scene_pursuit_route_completion_plan *result)
{
    scene_pursuit_route_completion_plan next;
    uint32_t draw=0;int status;
    if(!result)return RF_RANGE;
    status=scene_pursuit_retry_admit(current);if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(admission>1||success>1||special_visible>1||body_visible>1||eye_visible>1)
        return RF_FORMAT;
    next.retry=*current;
    if(admission&&!success){
        status=rf_random_next(&next.retry.random,&draw);if(status)return status;
    }
    status=scene_pursuit_route_outcome(admission,success,special_visible,
        body_visible,eye_visible,draw,&next.outcome);if(status)return status;
    if(next.outcome.arm_retry){
        status=rf_timer_set(&next.retry.due,now,(int32_t)next.outcome.retry_ms);
        if(status)return status;
        next.retry.flags=SCENE_PURSUIT_RETRY_ARMED;
    }
    /* Success retains the retry clock/RNG exactly; running miss retains RNG.
     * No caller storage changes until all proposal/deadline checks succeed. */
    *result=next;return RF_OK;
}
#endif
