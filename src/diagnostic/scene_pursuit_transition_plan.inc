#ifndef RF_SCENE_PURSUIT_TRANSITION_PLAN_INCLUDED
#define RF_SCENE_PURSUIT_TRANSITION_PLAN_INCLUDED
/* Uncalled parent staging, 405e7b..4060bc, ordinary ranged domain only.
 * No actor, timer, RNG, route, animation or save state is mutated here.
 * Caller must prove nonvehicle/nonfighter and nonmelee ownership, prepare
 * 406b70 eligibility and 4062c0 stop feasibility, and revalidate publication.
 * The melee state3 special admission path is intentionally not represented. */
#define SCENE_PURSUIT_TRANSITION_KEEP 0u
#define SCENE_PURSUIT_TRANSITION_ROUTE 1u
#define SCENE_PURSUIT_TRANSITION_STOP 2u
#define SCENE_PURSUIT_TRANSITION_RETREAT 3u
static inline int scene_pursuit_transition_plan(int32_t behavior,uint32_t pursue,
    uint32_t retry_expired,uint32_t stop_feasible,uint32_t *decision)
{
    uint32_t result=SCENE_PURSUIT_TRANSITION_KEEP;
    if(!decision)return RF_RANGE;
    if(pursue>1||retry_expired>1||stop_feasible>1)return RF_FORMAT;
    if(behavior==3)return RF_NOT_FOUND; /* Separate source melee branch. */
    if(behavior!=11){
        if(pursue){
            if(behavior!=1&&retry_expired)result=SCENE_PURSUIT_TRANSITION_ROUTE;
        }else if(behavior==1){
            result=stop_feasible?SCENE_PURSUIT_TRANSITION_STOP:
                                 SCENE_PURSUIT_TRANSITION_RETREAT;
        }
    }
    *decision=result;return RF_OK;
}
/* 406036..406061: consume one caller-owned nonnegative CRT-domain sample
 * only after accepting a ranged state1 stop. Both STOP and RETREAT clear
 * the old route and restart this tactical clock before their final branch.
 * The caller preserves the independent 500-ms primary recovery gate. */
static inline int scene_pursuit_stop_interval(uint32_t style,uint32_t random15,
    uint32_t *milliseconds)
{
    if(!milliseconds)return RF_RANGE;
    if(random15>32767u)return RF_FORMAT;
    *milliseconds=(style==1?2000u:1000u)+random15%2000u;return RF_OK;
}
/* 4062c0: caller resolves the target actor and supplies full 3D distance
 * and the PRIMARY authored weapon limit (not the max route limit).
 * The flags530 visibility override is a distinct input, not inferred from
 * Blind age or RFNC40 expired state. All query work remains caller-owned. */
static int scene_pursuit_stop_feasible(uint32_t body_visible,uint32_t eye_visible,
    uint32_t visibility_override,uint32_t target_actor,float distance,
    float primary_limit,uint32_t *feasible)
{
    if(!feasible)return RF_RANGE;
    if(body_visible>1||eye_visible>1||visibility_override>1||target_actor>1||
       !isfinite(distance)||distance<0||!isfinite(primary_limit)||primary_limit<0)
        return RF_FORMAT;
    *feasible=(body_visible||eye_visible||visibility_override)&&target_actor&&
              primary_limit>=distance;
    return RF_OK;
}
/* 4022d8..40234a: pure refresh proposal for independent retry+230.
 * Input visibility is normalized from source exact byte==1 samples.
 * Equality with max weapon limit does not rearm the 500-ms gate.
 * arm=0 carries milliseconds=0 canonically; no caller clock is changed. */
static inline int scene_pursuit_retry_refresh(int32_t behavior,uint32_t visible,
    float distance,float route_limit,uint32_t *arm,uint32_t *milliseconds)
{
    uint32_t a=0,ms=0;
    if(!arm||!milliseconds||arm==milliseconds)return RF_RANGE;
    if(visible>1||!isfinite(distance)||distance<0||
       !isfinite(route_limit)||route_limit<0)return RF_FORMAT;
    if(visible){
        if(behavior==3)a=1;
        else if(route_limit>distance){a=1;ms=500;}
    }
    *arm=a;*milliseconds=ms;return RF_OK;
}
/* Ordinary ranged 405fa6..40601b and running movement406a72..406a91.
 * This describes an already completed bounded route request. Caller supplies
 * the random sample only for failed admission; running failure is fixed1000.
 * State publication, flag8, route clearing and retry arming must be applied
 * together after complete owner/target/route revalidation. Failure never
 * authorizes the generic direct-movement fallback. */
typedef struct scene_pursuit_route_result {
    uint32_t set_pursuit_state,clear_route,arm_retry,retry_ms;
    uint32_t write_flag8,flag8,allow_route_advance;
} scene_pursuit_route_result;
static inline int scene_pursuit_route_outcome(uint32_t admission,uint32_t success,
    uint32_t special_visible,uint32_t body_visible,uint32_t eye_visible,
    uint32_t random15,scene_pursuit_route_result *result)
{
    scene_pursuit_route_result out={0};
    if(!result)return RF_RANGE;
    if(admission>1||success>1||special_visible>1||body_visible>1||eye_visible>1||
       random15>32767u)return RF_FORMAT;
    if(success){
        if(admission){
            out.set_pursuit_state=1;out.write_flag8=1;
            out.flag8=special_visible||body_visible||eye_visible;
        }else out.allow_route_advance=1;
    }else{
        out.arm_retry=1;out.retry_ms=admission?500u+random15%501u:1000u;
        out.clear_route=admission;
    }
    *result=out;return RF_OK;
}
#endif
