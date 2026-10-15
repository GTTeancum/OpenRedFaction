#ifndef RF_SCENE_PURSUIT_RETURN_FALLBACK_QUERY_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_FALLBACK_QUERY_INCLUDED
/* Uncalled phase2 preparation. The caller owns one consumed scheduler slot,
 * authenticated live/disjoint inputs, full command/resource/geometry seals,
 * and uninterrupted exclusive graph AND collision scratch through eventual
 * publication. Input equality and the busy flag are consistency checks only.
 * No gameplay publication, shared RNG update or lease release occurs here.
 * Errors may change query scratch but never publish a completed fallback.
 */
typedef struct scene_pursuit_return_fallback_query_result {
    uint32_t cancellation_tag; /* EMPTY_GRAPH/NO_START only; prepared invalid. */
    scene_tactical_route_candidate route; /* valid only for INSTALL */
    scene_pursuit_return_fallback_prepared prepared;
} scene_pursuit_return_fallback_query_result;
static int scene_pursuit_return_fallback_query(scene_stream *stream,
    const scene_pursuit_return_input *input,int32_t now,float clock,
    scene_pursuit_return_fallback_query_result *result)
{
    scene_pursuit_return_fallback_query_result out={0};
    rf_random_state random;uint32_t facing=1,found=0,tag=0;int status;
    if(!stream||!stream->collision||!input||!result)return RF_RANGE;
    if(input->query.before.phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK||
       input->target_player>1)return RF_FORMAT;
    if(now<0||now>RF_TIMER_PERIOD||!isfinite(clock)||
       (double)clock < -2147483648.0||(double)clock>=2147483648.0)return RF_RANGE;
    if(!scene_flight_search_busy)return RF_NOT_FOUND;
    status=scene_pursuit_return_input_matches(input);if(status)return status;
    if(input->target_player){
        status=scene_tactical_target_facing(input->query.target_body,
            input->target_forward,input->target_cosine,input->query.source_body,&facing);
        if(status)return status;
    }
    if(!facing){
        status=scene_pursuit_return_fallback_prepare(&input->query.before,
            &input->tactical,&input->ai,1,0,NULL,now,clock,&out.prepared);
        if(status)return status;
    }else {
        random=input->tactical.random;
        status=scene_tactical_navigation_leased(stream,input->slot,input->query.target_body,
            &random,&out.route,&found,&tag);if(status)return status;
        if(tag==SCENE_TACTICAL_QUERY_EMPTY_GRAPH||tag==SCENE_TACTICAL_QUERY_NO_START){
            out.cancellation_tag=tag;goto verify;
        }
        if((tag==SCENE_TACTICAL_QUERY_FOUND&&found!=1)||
           (tag==SCENE_TACTICAL_QUERY_EXHAUSTED&&found!=0)||
           (tag!=SCENE_TACTICAL_QUERY_FOUND&&tag!=SCENE_TACTICAL_QUERY_EXHAUSTED))return RF_FORMAT;
        status=scene_pursuit_return_fallback_prepare(&input->query.before,
            &input->tactical,&input->ai,0,tag,&random,now,clock,&out.prepared);
        if(status)return status;
    }
    /* This is sampled-value revalidation only. The enclosing executor still
     * proves command history, backing lifetime and complete query provenance. */
verify:
    status=scene_pursuit_return_input_matches(input);if(status)return status;
    *result=out;return RF_OK;
}
#endif
