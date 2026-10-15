#ifndef RF_SCENE_PURSUIT_POLICY_INCLUDED
#define RF_SCENE_PURSUIT_POLICY_INCLUDED
/* Uncalled parent staging. 406b70 ordinary standalone nonvehicle policy.
 * Caller proves source cohort and supplies current body distance, authored
 * max(primary,secondary) route limit, and actual current owner gates.
 * Vehicle distance adjustment and melee horizontal metric are NOT inferred
 * here: their caller-side preparation must be explicit before any use.
 * Source state 11 suppresses transitions outside this predicate. */
static int scene_pursuit_policy(float distance,float route_limit,uint32_t behavior,
    uint32_t stand_ground,uint32_t reload_active,uint32_t pain_pending,
    uint32_t physically_crouched,uint32_t visible,uint32_t delay_expired,
    uint32_t *pursue)
{
    float limit;
    if(!pursue)return RF_RANGE;
    if(!isfinite(distance)||distance<0||!isfinite(route_limit)||route_limit<0||
       stand_ground>1||reload_active>1||pain_pending>1||physically_crouched>1||
       visible>1||delay_expired>1)return RF_FORMAT;
    limit=route_limit;
    if(behavior==2)limit*=1.5f;
    if(!isfinite(limit))return RF_FORMAT;
    *pursue=0;
    if(stand_ground||reload_active||pain_pending||physically_crouched)return RF_OK;
    if(distance>limit)*pursue=1;
    else if(!visible&&delay_expired)*pursue=1;
    return RF_OK;
}
#endif
