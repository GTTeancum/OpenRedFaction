#ifndef RF_SCENE_PURSUIT_RETURN_FOUND_PUBLISH_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_FOUND_PUBLISH_INCLUDED
/* Uncalled FOUND-only suffix. The caller has consumed one RETURN_ROUTE slot,
 * completed the query, and retained the same graph AND collision lease through
 * this call. A set busy flag alone does not authenticate that ownership.
 * Caller seals section/array lifetime, full command identity, graph identity,
 * selected geometry and authenticated live/disjoint allocations before entry.
 * No callbacks or competing writers may intervene through final publication.
 * Snapshot arguments and candidate are private immutable disjoint storage.
 * This is not a query executor, miss adapter, scheduler or feature admission.
 */
static int scene_pursuit_return_found_publish(uint32_t slot,uint32_t handle,
    const scene_pursuit_return_receipt *expected_return,
    const scene_tactical_receipt *expected_tactical,
    const rf_entity_ai_transition_state *expected_ai,
    const scene_pursuit_route_candidate *candidate,float clock)
{
    campaign_npc_body *o;rf_entity_ai_transition_state next_ai;
    scene_pursuit_return_receipt empty={0};
    const float *target_position;uint32_t target,melee,target_slot;int status;
    if(!expected_return||!expected_tactical||!expected_ai||!candidate||
       !campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    if(!scene_flight_search_busy)return RF_NOT_FOUND;
    status=scene_pursuit_return_tactical_admit(expected_return,expected_tactical);
    if(status)return status;
    if(expected_return->phase!=SCENE_PURSUIT_RETURN_PENDING_ROUTE)return RF_FORMAT;
    status=scene_pursuit_return_owner(slot,&target,&melee);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(o->registration.handle!=handle||target!=expected_return->target||
       o->combat_scripted!=expected_return->combat_mode||
       memcmp(&o->pursuit_return,expected_return,sizeof(*expected_return))||
       memcmp(&o->tactical,expected_tactical,sizeof(*expected_tactical))||
       memcmp(&o->ai_mode,expected_ai,sizeof(*expected_ai))||
       o->ai_mode.action_280!=3||o->view.action_520!=3||
       o->script_move.origin!=SCENE_MOVE_ORIGIN_NONE||o->script_move.event||
       o->script_move.path.count||
       (o->script_move.active&&o->script_move.follow!=2))return RF_FORMAT;
    if(target==campaign_player_object.handle)target_position=scene_actor_body.state.position;
    else {
        if(!scene_blind_target_slot(target,&target_slot))return RF_NOT_FOUND;
        target_position=campaign_npc_bodies[target_slot].body.state.position;
    }
    for(uint32_t axis=0;axis<3;axis++){
        if(!isfinite(o->body.state.position[axis])||!isfinite(target_position[axis])||
           o->body.state.position[axis]!=candidate->start.position[axis]||
           target_position[axis]!=candidate->goal.position[axis])return RF_FORMAT;
    }
    if(campaign_navigation_workspace.count!=campaign_navigation.count)return RF_FORMAT;
    next_ai=o->ai_mode;
    status=rf_entity_ai_set_state(&next_ai,1,clock);if(status)return status;
    /* A completed successful request selects behavior1 without evaluating
     * the missing miss-only stop/primary-limit bindings. The route helper
     * validates all of its destinations before its first live assignment. */
    status=scene_pursuit_route_publish(candidate,campaign_navigation_workspace.references,
        campaign_navigation_workspace.count,&o->navigation.start,&o->navigation.goal,
        &o->navigation.retained,&o->navigation.cost,&o->script_move.route_index);
    if(status)return status;
    /* Assignment-only suffix: no fallible work after navigation changes.
     * Preserve tactical stationary phase, clocks, RNG and recovery verbatim.
     * view.action_520 remains the action3 mirror, not behavior1. */
    memset(&o->script_move.path,0,sizeof(o->script_move.path));
    o->script_move.path_index=o->script_move.path_mode=o->script_move.path_reverse=0;
    o->script_move.active=1;o->script_move.event=0;o->script_move.follow=2;
    o->script_move.retry=0;o->script_move.stop=0;o->script_move.origin=SCENE_MOVE_ORIGIN_NONE;
    memcpy(o->script_move.target,candidate->goal.position,sizeof(o->script_move.target));
    o->ai_mode=next_ai;o->pursuit_return=empty;
    return RF_OK;
}
#endif
