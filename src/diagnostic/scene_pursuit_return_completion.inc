#ifndef RF_SCENE_PURSUIT_RETURN_COMPLETION_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_COMPLETION_INCLUDED
/* UNCALLED logical completion composition. No route, actor, timer or RNG is
 * published here. Caller must separately join any prepared route and tactical
 * relocation, then perform one assignment-only publication after all checks.
 */
typedef struct scene_pursuit_return_completion {
    scene_pursuit_return_receipt receipt;
    uint32_t publish_behavior;
    int32_t behavior;
} scene_pursuit_return_completion;
static int scene_pursuit_return_route_complete(
    const scene_pursuit_return_receipt *before,uint32_t melee_only,
    uint32_t route_status,uint32_t stop_feasible,float distance,float primary_limit,
    scene_pursuit_return_completion *out)
{
    scene_pursuit_return_completion next={0};scene_pursuit_return_outcome decision;
    int status;if(!out)return RF_RANGE;
    status=scene_pursuit_return_receipt_admit(before);if(status)return status;
    if(before->phase!=SCENE_PURSUIT_RETURN_PENDING_ROUTE)return RF_FORMAT;
    next.receipt=*before;
    /* An active receipt has a target. Target loss is cancellation by the full
     * owner, never a fabricated completed miss against an absent target. */
    status=scene_pursuit_return_outcome_plan(melee_only,1,route_status,
        stop_feasible,distance,primary_limit,&decision);if(status)return status;
    if(decision.disposition==SCENE_PURSUIT_RETURN_FALLBACK){
        status=scene_pursuit_return_receipt_fallback(&next.receipt);if(status)return status;
    }else if(decision.disposition==SCENE_PURSUIT_RETURN_PUBLISH){
        next.publish_behavior=1;next.behavior=decision.behavior;
        scene_pursuit_return_receipt_clear(&next.receipt);
    }else if(decision.disposition!=SCENE_PURSUIT_RETURN_DEFER)return RF_FORMAT;
    *out=next;return RF_OK;
}
/* completed is an explicit backend result, never inferred from RF_OK alone.
 * False means the existing fallback still owns pending work; no new prelude
 * or primary-recovery/phase timer mutation is permitted by this proposal.
 */
static int scene_pursuit_return_fallback_complete(
    const scene_pursuit_return_receipt *before,uint32_t completed,
    int32_t current_behavior,uint32_t current_melee_only,
    scene_pursuit_return_completion *out)
{
    scene_pursuit_return_completion next={0};scene_pursuit_return_outcome decision;
    int status;if(!out)return RF_RANGE;
    status=scene_pursuit_return_receipt_admit(before);if(status)return status;
    if(before->phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK||completed>1||
       current_melee_only>1)return RF_FORMAT;
    next.receipt=*before;
    if(!completed){*out=next;return RF_OK;}
    status=scene_pursuit_return_after_fallback(current_behavior,current_melee_only,&decision);
    if(status)return status;
    if(decision.disposition==SCENE_PURSUIT_RETURN_PUBLISH){
        next.publish_behavior=1;next.behavior=decision.behavior;
    }else if(decision.disposition!=SCENE_PURSUIT_RETURN_RETAIN)return RF_FORMAT;
    scene_pursuit_return_receipt_clear(&next.receipt);
    *out=next;return RF_OK;
}
#endif
