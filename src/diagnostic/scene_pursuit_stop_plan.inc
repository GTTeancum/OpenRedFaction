#ifndef RF_SCENE_PURSUIT_STOP_PLAN_INCLUDED
#define RF_SCENE_PURSUIT_STOP_PLAN_INCLUDED
/* UNCALLED ranged405e7b..4060bc stop composition. Call only after a selected
 * live state1 owner has actually ceased406b70 eligibility. Denied scheduling
 * must not call this. The route clear and prepared tactical receipt publish
 * together before the final stop/retreat branch. No actor/route mutation here.
 * Private tactical RNG is the port adaptation; no global random draw.
 */
typedef struct scene_pursuit_stop_plan {
    scene_tactical_receipt tactical;
    uint32_t clear_route,publish_behavior,retreat_milliseconds;
} scene_pursuit_stop_plan;
static int scene_pursuit_stop_prepare(
    const scene_pursuit_return_receipt *pending,
    const scene_tactical_receipt *current,int32_t now,uint32_t style,
    uint32_t stop_feasible,scene_pursuit_stop_plan *result)
{
    scene_pursuit_stop_plan next={0};uint32_t draw,milliseconds;int status;
    if(!result)return RF_RANGE;
    if(stop_feasible>1||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=scene_pursuit_return_tactical_admit(pending,current);
    if(status)return status;
    if(pending->phase||!(current->flags&SCENE_TACTICAL_PRESENT)||
       current->phase!=SCENE_TACTICAL_STATIONARY)return RF_FORMAT;
    next.tactical=*current;
    status=rf_random_next(&next.tactical.random,&draw);if(status)return status;
    status=scene_pursuit_stop_interval(style,draw,&milliseconds);
    if(status)return status;
    status=rf_timer_set(&next.tactical.phase_due,now,(int32_t)milliseconds);
    if(status)return status;
    /* Preserve the independent recovery deadline/flag exactly, even when
     * due. Ordinary maintenance owns aging; stopping must not re-arm it. */
    next.clear_route=1;
    next.publish_behavior=stop_feasible;
    next.retreat_milliseconds=stop_feasible?0:3000;
    /* publish_behavior authorizes ranged state2 only. A requested retreat
     * never authorizes state2, even if its later admission refuses:4060ab
     * returns immediately. Prior state1 can therefore survive route clearing.
     * Retreat target/no-flee/reset/entry/continuation remain caller-owned. */
    *result=next;return RF_OK;
}
#endif
