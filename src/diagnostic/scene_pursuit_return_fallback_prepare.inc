#ifndef RF_SCENE_PURSUIT_RETURN_FALLBACK_PREPARE_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_FALLBACK_PREPARE_INCLUDED
/* Uncalled private suffix for the bounded4065d0 branches. Caller has admitted
 * standalone Direct/Evasive ordinary ranged actors, live target, non-class800,
 * no active weapon bytes, and non-11 behavior BEFORE enqueue and again before
 * effects. Pending fire/command exclusion and all identity seals are required.
 * Facing failure means a successful actual player-facing predicate returning
 * false, never an error. It precedes navigation and therefore has query tag0.
 * FOUND/EXHAUSTED require the matching successful leased query and its advanced
 * private RNG. Empty/no-start results are refusals with no completion authority.
 * No queries, live writes, lease changes, callbacks or allocation occur here.
 * Outputs do not prove that live route cleanup/publication has happened. */
#define SCENE_PURSUIT_FALLBACK_ROUTE_KEEP 0u
#define SCENE_PURSUIT_FALLBACK_ROUTE_INSTALL 1u
#define SCENE_PURSUIT_FALLBACK_ROUTE_CLEAR 2u
typedef struct scene_pursuit_return_fallback_prepared {
    scene_tactical_receipt tactical;
    rf_entity_ai_transition_state ai;
    scene_pursuit_return_completion completion;
    uint32_t route_effect;
} scene_pursuit_return_fallback_prepared;
static int scene_pursuit_return_fallback_prepare(
    const scene_pursuit_return_receipt *before,
    const scene_tactical_receipt *tactical,const rf_entity_ai_transition_state *ai,
    uint32_t player_not_facing,uint32_t query_outcome,const rf_random_state *query_random,
    int32_t now,float clock,scene_pursuit_return_fallback_prepared *result)
{
    scene_pursuit_return_fallback_prepared out={0};int status;
    if(!before||!tactical||!ai||!result)return RF_RANGE;
    status=scene_pursuit_return_tactical_admit(before,tactical);if(status)return status;
    if(before->phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK||player_not_facing>1||
       ai->action_280!=3)return RF_FORMAT;
    if(now<0||now>RF_TIMER_PERIOD||!isfinite(clock)||
       (double)clock < -2147483648.0||(double)clock>=2147483648.0)return RF_RANGE;
    /* Exhaustion preserves existing11 in the source, whose route-free service
     * is not represented by this bounded adapter. Never silently publish2. */
    if(ai->state_2b4==11)return RF_NOT_FOUND;
    if(player_not_facing){if(query_outcome||query_random)return RF_FORMAT;}
    else {
        if(query_outcome==SCENE_TACTICAL_QUERY_EMPTY_GRAPH||
           query_outcome==SCENE_TACTICAL_QUERY_NO_START)return RF_NOT_FOUND;
        if((query_outcome!=SCENE_TACTICAL_QUERY_FOUND&&
            query_outcome!=SCENE_TACTICAL_QUERY_EXHAUSTED)||!query_random)return RF_FORMAT;
    }
    out.tactical=*tactical;out.ai=*ai;
    if(player_not_facing){
        status=rf_timer_set_random(&out.tactical.phase_due,now,200,500,&out.tactical.random);
        if(status)return status;
        out.route_effect=SCENE_PURSUIT_FALLBACK_ROUTE_KEEP;
    }else {
        out.tactical.random=*query_random;
        if(query_outcome==SCENE_TACTICAL_QUERY_FOUND){
            status=scene_tactical_receipt_relocate(&out.tactical,now);if(status)return status;
            out.ai.flags_530|=8u;
            status=rf_entity_ai_set_state(&out.ai,11,clock);if(status)return status;
            out.route_effect=SCENE_PURSUIT_FALLBACK_ROUTE_INSTALL;
        }else {
            /* Four completed attempts: preserve phase deadline and recovery.
             * Existing ordinary tactical200..500 retry is not this branch. */
            out.route_effect=SCENE_PURSUIT_FALLBACK_ROUTE_CLEAR;
        }
    }
    /* The private branch has reached its source return boundary. The actual
     * transaction must still realize route_effect and publish these together. */
    status=scene_pursuit_return_fallback_complete(before,1,out.ai.state_2b4,0,&out.completion);
    if(status)return status;
    if(out.completion.publish_behavior){
        status=rf_entity_ai_set_state(&out.ai,out.completion.behavior,clock);if(status)return status;
    }
    status=scene_tactical_receipt_admit(&out.tactical);if(status)return status;
    *result=out;return RF_OK;
}
#endif
