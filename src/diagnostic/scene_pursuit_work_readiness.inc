#ifndef RF_SCENE_PURSUIT_WORK_READINESS_INCLUDED
#define RF_SCENE_PURSUIT_WORK_READINESS_INCLUDED
/* UNCALLED scheduling proposal. This selects a cheap service category, never
 * permission to run a graph query. Complete owner/target/stronger-command and
 * source retry/visibility/range gates must still run after the shared slot is
 * selected. A later gate refusal does not replenish that frame's slot.
 * One existing persistent cursor and transient frame guard must cover ALL
 * categories; do not call the old tactical scheduler as a second authority.
 * No RNG, timer mutation, allocation, actor dereference or route publication. */
#define SCENE_PURSUIT_WORK_NONE 0u
#define SCENE_PURSUIT_WORK_RETURN_ROUTE 1u
#define SCENE_PURSUIT_WORK_RETURN_FALLBACK 2u
#define SCENE_PURSUIT_WORK_TACTICAL 3u
#define SCENE_PURSUIT_WORK_RETURN_PRELUDE 4u
static int scene_pursuit_work_ready(
    const scene_pursuit_return_receipt *pending,
    const scene_tactical_receipt *tactical,int32_t now,uint32_t *kind)
{
    uint32_t result=SCENE_PURSUIT_WORK_NONE;int expired,status;
    if(!kind)return RF_RANGE;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=scene_pursuit_return_tactical_admit(pending,tactical);
    if(status)return status;
    /*408ac0 may attempt the initial route immediately after the prelude.
     * Its independently armed tactical interval does not delay this category.
     * Actual route admission can still defer, without restarting the prelude. */
    if(pending->phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE)
        result=SCENE_PURSUIT_WORK_RETURN_PRELUDE;
    else if(pending->phase==SCENE_PURSUIT_RETURN_PENDING_ROUTE)
        result=SCENE_PURSUIT_WORK_RETURN_ROUTE;
    else if((tactical->flags&SCENE_TACTICAL_PRESENT)&&
            tactical->phase==SCENE_TACTICAL_STATIONARY){
        status=rf_timer_expired(tactical->phase_due,now,&expired);
        if(status)return status;
        if(expired)result=pending->phase==SCENE_PURSUIT_RETURN_PENDING_FALLBACK?
            SCENE_PURSUIT_WORK_RETURN_FALLBACK:SCENE_PURSUIT_WORK_TACTICAL;
    }
    *kind=result;return RF_OK;
}
#endif
