#ifndef RF_SCENE_PURSUIT_RETURN_FALLBACK_PUBLISH_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_FALLBACK_PUBLISH_INCLUDED
/* Uncalled bounded fallback transaction suffix. The dispatcher owns the full
 * section/array/command/target/geometry/resource seals and exclusive graph AND
 * collision lease through final publication. Busy alone is not ownership.
 * Prepared result is authenticated output of fallback_prepare for these exact
 * immutable expected snapshots and successful query/facing result. All inputs
 * are live/disjoint from destinations. No callback or competing writer may
 * intervene. This does not supply admission before enqueue or pending-fire
 * exclusion; both remain required in the enclosing executor. */
static int scene_pursuit_return_fallback_publish(uint32_t slot,uint32_t handle,
    const scene_pursuit_return_receipt *expected_return,
    const scene_tactical_receipt *expected_tactical,
    const rf_entity_ai_transition_state *expected_ai,
    const scene_pursuit_return_fallback_prepared *prepared,
    const scene_tactical_route_candidate *candidate)
{
    campaign_npc_body *o;uint32_t target,melee;float primary_limit;int status;
    if(!expected_return||!expected_tactical||!expected_ai||!prepared||
       !campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    if(!scene_flight_search_busy)return RF_NOT_FOUND;
    status=scene_pursuit_return_tactical_admit(expected_return,expected_tactical);
    if(status)return status;
    if(expected_return->phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK||
       expected_ai->action_280!=3||expected_ai->state_2b4==11)return RF_FORMAT;
    status=scene_pursuit_return_owner(slot,&target,&melee);if(status)return status;
    status=scene_pursuit_return_primary_limit(slot,&primary_limit);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(melee||o->view.weapons[0]<=0||o->view.weapons[0]>=64||
       !o->inventory.owned[o->view.weapons[0]])return RF_NOT_FOUND;
    for(uint32_t i=0;i<64;i++)if(o->firing.active[i])return RF_NOT_FOUND;
    if(((o->ai_mode.flags_530^o->view.flags_7d0)|
        (o->ai_mode.flags_530^o->firing.flags_7d0))&8u)return RF_FORMAT;
    if(o->registration.handle!=handle||target!=expected_return->target||
       o->combat_scripted!=expected_return->combat_mode||
       memcmp(&o->pursuit_return,expected_return,sizeof(*expected_return))||
       memcmp(&o->tactical,expected_tactical,sizeof(*expected_tactical))||
       memcmp(&o->ai_mode,expected_ai,sizeof(*expected_ai))||o->view.action_520!=3||
       o->script_move.origin!=SCENE_MOVE_ORIGIN_NONE||o->script_move.event||
       o->script_move.path.count||(o->script_move.active&&o->script_move.follow!=2)||
       (o->script_move.follow!=0&&o->script_move.follow!=2)||
       (o->navigation.retained.count&&o->script_move.follow!=2))return RF_FORMAT;
    status=scene_tactical_receipt_admit(&prepared->tactical);if(status)return status;
    status=scene_pursuit_return_receipt_admit(&prepared->completion.receipt);if(status)return status;
    if(prepared->completion.receipt.phase||
       prepared->tactical.flags!=expected_tactical->flags||
       prepared->tactical.target!=expected_tactical->target||
       prepared->tactical.recovery_due!=expected_tactical->recovery_due||
       prepared->ai.action_280!=expected_ai->action_280||
       prepared->ai.clock_288!=expected_ai->clock_288||
       prepared->ai.argument_28c!=expected_ai->argument_28c||
       prepared->ai.argument_290!=expected_ai->argument_290)return RF_FORMAT;
    if(prepared->route_effect==SCENE_PURSUIT_FALLBACK_ROUTE_INSTALL){
        if(!candidate||prepared->tactical.phase!=SCENE_TACTICAL_RELOCATING||
           prepared->ai.state_2b4!=11||prepared->completion.publish_behavior||
           prepared->ai.flags_530!=(expected_ai->flags_530|8u))return RF_FORMAT;
        /* This is the last fallible call. It validates geometry before stores
         * and consumes the already prepared timer without drawing again. */
        status=scene_tactical_route_publish_prepared(slot,handle,target,candidate,&prepared->tactical);
        if(status)return status;
    }else {
        if(candidate||(prepared->route_effect!=SCENE_PURSUIT_FALLBACK_ROUTE_KEEP&&
           prepared->route_effect!=SCENE_PURSUIT_FALLBACK_ROUTE_CLEAR)||
           prepared->tactical.phase!=SCENE_TACTICAL_STATIONARY||
           prepared->ai.state_2b4!=2||prepared->completion.publish_behavior!=1||
           prepared->completion.behavior!=2||
           prepared->ai.flags_530!=expected_ai->flags_530)return RF_FORMAT;
        if(prepared->route_effect==SCENE_PURSUIT_FALLBACK_ROUTE_CLEAR&&
           prepared->tactical.phase_due!=expected_tactical->phase_due)return RF_FORMAT;
        /* All fallible work is finished. Clear only our authenticated route;
         * do not invoke general pursuit cancellation or tactical release. */
        if(prepared->route_effect==SCENE_PURSUIT_FALLBACK_ROUTE_CLEAR){
            memset(&o->navigation.retained,0,sizeof(o->navigation.retained));
            o->navigation.cost=0;
            if(o->script_move.follow==2){
                o->script_move.active=0;o->script_move.follow=0;o->script_move.stop=1;
                o->script_move.retry=0;o->script_move.route_index=1;
            }
        }
    }
    /* Assignment-only suffix across every branch. Prepared inputs cannot
     * alias any of these live destinations. Independent recovery is retained. */
    if(prepared->route_effect==SCENE_PURSUIT_FALLBACK_ROUTE_INSTALL){
        o->view.flags_7d0|=8u;o->firing.flags_7d0|=8u;
    }
    o->ai_mode=prepared->ai;o->tactical=prepared->tactical;
    o->pursuit_return=prepared->completion.receipt;
    return RF_OK;
}
#endif
