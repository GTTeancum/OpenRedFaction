#ifndef RF_SCENE_PURSUIT_RETURN_EXECUTE_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_EXECUTE_INCLUDED
/* Uncalled synchronous phase1/2 executor. The sole scene writer invokes this
 * immediately after consuming one matching return scheduler selection. Caller
 * owns authenticated live/disjoint actor, graph, sphere and collision backing;
 * no concurrent writer, resource replacement or foreign query is permitted.
 * The fixed query chain has no gameplay/resource/audio dispatch. Therefore no
 * command writer can intervene between fresh capture and immediate publication.
 * This scope excludes phase3 prelude and its fallible resource/audio boundaries.
 * These flags prevent same-thread nested acquisition; they are not a mutex or
 * allocation authenticator. Integration must establish the stated ownership.
 * Refusal consumes the selection, preserves the pending receipt and does not
 * authorize ordinary fire/chase, reselection or fabricated fallback completion.
 * RF_OK means progress: a route miss may publish phase2, still pending.
 * Classified EMPTY_GRAPH/NO_START can instead cancel only phase2; cancelled
 * is a private disjoint output requiring same-frame full-owner suppression.
 */
static int scene_pursuit_return_execute(scene_stream *stream,uint32_t slot,
    uint32_t frame,int32_t now,float clock,uint32_t *cancelled)
{
    scene_pursuit_return_input input;
    union {
        scene_pursuit_return_route_prepared route;
        scene_pursuit_return_fallback_query_result fallback;
    } prepared;
    uint64_t epoch;int status;
    if(!cancelled)return RF_RANGE;
    *cancelled=0;
    if(!stream||!stream->collision)return RF_RANGE;
    status=scene_pursuit_execution_boundary();if(status)return status;
    if(scene_flight_search_busy)return RF_NOT_FOUND;
    if(now<0||now>RF_TIMER_PERIOD||!isfinite(clock)||
       (double)clock < -2147483648.0||(double)clock>=2147483648.0)return RF_RANGE;
    epoch=scene_robot_death_epoch;
    scene_pursuit_execution_active=1;scene_flight_search_busy=1;
    status=scene_pursuit_return_input_capture(slot,frame,&input);if(status)goto done;
    if(input.query.before.phase==SCENE_PURSUIT_RETURN_PENDING_ROUTE){
        status=scene_pursuit_return_route_prepare(&campaign_navigation_workspace,
            &campaign_navigation,&campaign_npc_bodies[slot].body.spheres,
            stream->collision,&campaign_movers,&input.query,&prepared.route);
        if(status)goto done;
        status=scene_pursuit_return_input_matches(&input);if(status)goto done;
        if(prepared.route.route_status==SCENE_PURSUIT_RETURN_ROUTE_SUCCESS)
            status=scene_pursuit_return_found_publish(slot,input.handle,
                &input.query.before,&input.tactical,&input.ai,&prepared.route.route,clock);
        else if(prepared.route.route_status==SCENE_PURSUIT_RETURN_ROUTE_MISS)
            status=scene_pursuit_return_miss_publish(slot,input.handle,&input.query,
                &input.tactical,&input.ai,&prepared.route,clock);
        else status=RF_FORMAT;
    }else if(input.query.before.phase==SCENE_PURSUIT_RETURN_PENDING_FALLBACK){
        status=scene_pursuit_return_fallback_query(stream,&input,now,clock,&prepared.fallback);
        if(status)goto done;
        if(prepared.fallback.cancellation_tag){
            status=scene_pursuit_return_geometry_cancel(&input,prepared.fallback.cancellation_tag);
            if(!status)*cancelled=1;
            goto done;
        }
        status=scene_pursuit_return_fallback_publish(slot,input.handle,&input.query.before,
            &input.tactical,&input.ai,&prepared.fallback.prepared,
            prepared.fallback.prepared.route_effect==SCENE_PURSUIT_FALLBACK_ROUTE_INSTALL?
                &prepared.fallback.route:NULL);
    }else status=RF_FORMAT;
done:
    /* A replaced section owns its own reset flags. Never release a new scope.
     * The callback-free interval excludes this case under the caller contract.
     * Query failures change scratch only, so no phase4 fault latch is needed. */
    if(epoch!=scene_robot_death_epoch)return RF_NOT_FOUND;
    scene_flight_search_busy=0;scene_pursuit_execution_active=0;
    return status;
}
#endif
