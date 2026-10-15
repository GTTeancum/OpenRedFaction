#ifndef RF_SCENE_PURSUIT_RETREAT_CONTINUATION_INCLUDED
#define RF_SCENE_PURSUIT_RETREAT_CONTINUATION_INCLUDED
/* Uncalled source-only projection of 4086e1..408775, AFTER the caller has
 * completed 4085f0 admission and its animation/pain/reset side effects.
 * This is not a replacement for those earlier side effects or a permission
 * to reuse damage-flight's lottery/profile. All target identities are full
 * resolved handles. Caller owns timer conversion, resources and publication.
 * An ordinary pursuit-stop request supplies exactly 3000 milliseconds. */
typedef struct scene_pursuit_retreat_continuation {
    uint32_t arm_deadline,set_action7,set_target,request_route;
} scene_pursuit_retreat_continuation;
static int scene_pursuit_retreat_continue_plan(int32_t current_action,
    uint32_t same_target,int32_t remaining_ms,int32_t requested_ms,
    scene_pursuit_retreat_continuation *result)
{
    scene_pursuit_retreat_continuation out={0};
    if(!result)return RF_RANGE;
    if(same_target>1||requested_ms<0)return RF_FORMAT;
    if(current_action!=7){
        out.arm_deadline=out.set_action7=out.set_target=out.request_route=1;
    }else if(remaining_ms<requested_ms){
        out.arm_deadline=1;
        if(!same_target){
            /* Source 408729 writes target before the timer; 408759 writes
             * it again after action7. This pure plan does not collapse or
             * reorder those calls: caller must preserve callback semantics. */
            out.set_action7=out.set_target=out.request_route=1;
        }
    }
    /* Equal/longer remaining time retains even a DIFFERENT current target.
     * Same-target extension never selects a fresh route at this branch. */
    *result=out;return RF_OK;
}
#endif
