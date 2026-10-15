#ifndef RF_SCENE_PURSUIT_RETURN_OUTCOME_PLAN_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_OUTCOME_PLAN_INCLUDED
/* UNCALLED pure post-prelude plan, 408c65..408d88. The caller must already
 * complete the source reset/recovery/stand-up/notice and action3 ownership
 * sequence. Route success here means an actually completed source-equivalent
 * request, never an RF_OK scheduler refusal. Inputs require a fresh resolved
 * target snapshot after all callbacks. Finite limits are this bounded domain;
 * no fallback branch is removed by converting invalid numbers to zero. */
#define SCENE_PURSUIT_RETURN_PUBLISH 0u
#define SCENE_PURSUIT_RETURN_FALLBACK 1u
#define SCENE_PURSUIT_RETURN_RETAIN 2u
#define SCENE_PURSUIT_RETURN_DEFER 3u
#define SCENE_PURSUIT_RETURN_ROUTE_NONE 0u
#define SCENE_PURSUIT_RETURN_ROUTE_MISS 1u
#define SCENE_PURSUIT_RETURN_ROUTE_SUCCESS 2u
#define SCENE_PURSUIT_RETURN_ROUTE_DEFERRED 3u
typedef struct scene_pursuit_return_outcome {
    uint32_t disposition;
    int32_t behavior;
} scene_pursuit_return_outcome;
static int scene_pursuit_return_outcome_plan(uint32_t melee_only,
    uint32_t target_present,uint32_t route_status,
    uint32_t stop_feasible,float distance,float primary_limit,
    scene_pursuit_return_outcome *result)
{
    scene_pursuit_return_outcome out={SCENE_PURSUIT_RETURN_PUBLISH,2};
    if(!result)return RF_RANGE;
    if(melee_only>1||target_present>1||route_status>SCENE_PURSUIT_RETURN_ROUTE_DEFERRED||
       stop_feasible>1||(!target_present&&route_status!=SCENE_PURSUIT_RETURN_ROUTE_NONE)||
       !isfinite(distance)||distance<0||
       !isfinite(primary_limit)||primary_limit<0)return RF_FORMAT;
    /* Explicit bounded-work adaptation: denied/pending service is neither
     * a miss nor no-request. It has no source state publication authority. */
    if(route_status==SCENE_PURSUIT_RETURN_ROUTE_DEFERRED){
        out.disposition=SCENE_PURSUIT_RETURN_DEFER;out.behavior=0;
        *result=out;return RF_OK;
    }
    if(melee_only)out.behavior=3;
    if(target_present){
        if(route_status==SCENE_PURSUIT_RETURN_ROUTE_SUCCESS)out.behavior=1;
        if(!stop_feasible&&(out.behavior==2||out.behavior==3)&&
           primary_limit<distance)out.disposition=SCENE_PURSUIT_RETURN_FALLBACK;
    }
    *result=out;return RF_OK;
}
/* Invoke only after the requested4065d0 fallback really completed. Deferred
 * work retains its own continuation without calling this helper or consuming
 * another reset/notice/timer/random sequence. A completed fallback that leaves
 * state11 returns immediately; otherwise the CURRENT melee predicate is read
 * again before the source's final state2 publication. */
static int scene_pursuit_return_after_fallback(int32_t current_behavior,
    uint32_t current_melee_only,scene_pursuit_return_outcome *result)
{
    scene_pursuit_return_outcome out={SCENE_PURSUIT_RETURN_PUBLISH,2};
    if(!result)return RF_RANGE;
    if(current_melee_only>1)return RF_FORMAT;
    if(current_behavior==11||current_melee_only){
        out.disposition=SCENE_PURSUIT_RETURN_RETAIN;out.behavior=current_behavior;
    }
    *result=out;return RF_OK;
}
#endif
