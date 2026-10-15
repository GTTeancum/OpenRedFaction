#ifndef RF_SCENE_PURSUIT_RETURN_MISS_PUBLISH_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_MISS_PUBLISH_INCLUDED
/* Uncalled successful-query MISS suffix. Full current owner/command/target,
 * eye/visibility/range inputs and collision/graph/resource seals remain caller
 * owned, with uninterrupted exclusive query lease through these final stores.
 * frame/prepared are authenticated immutable disjoint private query outputs.
 * Failure/refusal before publication never becomes a completed MISS. */
static int scene_pursuit_return_miss_publish(uint32_t slot,uint32_t handle,
    const scene_pursuit_return_route_frame *frame,
    const scene_tactical_receipt *expected_tactical,
    const rf_entity_ai_transition_state *expected_ai,
    const scene_pursuit_return_route_prepared *prepared,float clock)
{
    campaign_npc_body *o;rf_entity_ai_transition_state next_ai;
    scene_pursuit_return_completion completion;
    const float *target_position;uint32_t target,melee,target_slot;float limit;int status;
    if(!frame||!expected_tactical||!expected_ai||!prepared||
       !campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    if(!scene_flight_search_busy)return RF_NOT_FOUND;
    status=scene_pursuit_return_tactical_admit(&frame->before,expected_tactical);if(status)return status;
    if(frame->before.phase!=SCENE_PURSUIT_RETURN_PENDING_ROUTE||frame->melee_only||
       prepared->route_status!=SCENE_PURSUIT_RETURN_ROUTE_MISS)return RF_FORMAT;
    status=scene_pursuit_return_live_admit(slot,&limit);if(status)return status;
    status=scene_pursuit_return_owner(slot,&target,&melee);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(melee||limit!=frame->primary_limit||o->registration.handle!=handle||
       target!=frame->before.target||o->combat_scripted!=frame->before.combat_mode||
       memcmp(&o->pursuit_return,&frame->before,sizeof(frame->before))||
       memcmp(&o->tactical,expected_tactical,sizeof(*expected_tactical))||
       memcmp(&o->ai_mode,expected_ai,sizeof(*expected_ai))||
       expected_ai->action_280!=3||o->view.action_520!=3||
       o->script_move.origin!=SCENE_MOVE_ORIGIN_NONE||o->script_move.event||
       o->script_move.path.count||(o->script_move.active&&o->script_move.follow!=2)||
       (o->script_move.follow!=0&&o->script_move.follow!=2)||
       (o->navigation.retained.count&&o->script_move.follow!=2))return RF_FORMAT;
    if(target==campaign_player_object.handle)target_position=scene_actor_body.state.position;
    else {
        if(!scene_blind_target_slot(target,&target_slot))return RF_NOT_FOUND;
        target_position=campaign_npc_bodies[target_slot].body.state.position;
    }
    for(uint32_t axis=0;axis<3;axis++)if(!isfinite(o->body.state.position[axis])||
        !isfinite(target_position[axis])||o->body.state.position[axis]!=frame->source_body[axis]||
        target_position[axis]!=frame->target_body[axis])return RF_FORMAT;
    status=scene_pursuit_return_route_complete(&frame->before,0,SCENE_PURSUIT_RETURN_ROUTE_MISS,
        prepared->miss_sample.feasible,prepared->miss_sample.distance,limit,&completion);
    if(status)return status;
    if(memcmp(&completion,&prepared->completion,sizeof(completion)))return RF_FORMAT;
    next_ai=*expected_ai;
    if(completion.publish_behavior){
        if(completion.behavior!=2||completion.receipt.phase)return RF_FORMAT;
        status=rf_entity_ai_set_state(&next_ai,2,clock);if(status)return status;
    }else if(completion.receipt.phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK)return RF_FORMAT;
    /* Assignment-only owned-route teardown and continuation publication.
     * Old embedded endpoint values become inert; fallback uses fresh inputs.
     * Do not run another work selection or generic chase refresh afterward. */
    memset(&o->navigation.retained,0,sizeof(o->navigation.retained));o->navigation.cost=0;
    if(o->script_move.follow==2){
        o->script_move.active=0;o->script_move.follow=0;o->script_move.stop=1;
        o->script_move.retry=0;o->script_move.route_index=1;
    }
    o->ai_mode=next_ai;o->pursuit_return=completion.receipt;
    return RF_OK;
}
#endif
