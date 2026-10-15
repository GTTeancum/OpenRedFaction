#ifndef RF_SCENE_PURSUIT_ROUTE_PUBLISH_INCLUDED
#define RF_SCENE_PURSUIT_ROUTE_PUBLISH_INCLUDED
/* UNCALLED callback-free writer. The caller must revalidate its complete
 * actor/full-target/section/graph receipt, scheduler authority and private
 * candidate before this call, then jointly publish its retry/state receipt.
 * The preparation workspace lease must still be held: global query points
 * are mutable search scratch. No intervening graph request/callback is allowed
 * without a separate complete selected-geometry seal and revalidation.
 * Inputs, destinations and the graph must be distinct, non-overlapping
 * caller-owned storage. This helper is not a save-restore join by itself. */
static int scene_pursuit_route_publish(const scene_pursuit_route_candidate *candidate,
    const rf_entity_navigation_reference *references,uint32_t global_count,
    rf_entity_navigation_candidate *start,rf_entity_navigation_candidate *goal,
    rf_entity_navigation_retained_route *retained,float *cost,uint32_t *route_index)
{
    rf_entity_navigation_retained_route next={0};
    if(!candidate||!references||!start||!goal||start==goal||!retained||!cost||!route_index)
        return RF_RANGE;
    if(global_count>65534||candidate->count<2||candidate->count>4||
       candidate->node[0]!=global_count+1||!isfinite(candidate->cost)||candidate->cost<0)
        return RF_FORMAT;
    for(uint32_t axis=0;axis<3;axis++){
        if(!isfinite(candidate->start.position[axis])||!isfinite(candidate->start.query_point[axis])||
           !isfinite(candidate->goal.position[axis])||!isfinite(candidate->goal.query_point[axis]))return RF_FORMAT;
    }
    for(uint32_t i=0;i<candidate->count;i++){
        uint32_t token=candidate->node[i];
        if(token>global_count+1)return RF_FORMAT;
        for(uint32_t j=0;j<i;j++)if(candidate->node[j]==token)return RF_FORMAT;
        if(token==global_count){
            if(i+1!=candidate->count)return RF_FORMAT;
            next.nodes[i]=goal;
        }else if(token==global_count+1)next.nodes[i]=start;
        else{
            if(!references[token].candidate||references[token].candidate==start||
               references[token].candidate==goal)return RF_FORMAT;
            next.nodes[i]=references[token].candidate;
        }
    }
    /* The retained prefix need not contain goal: core4cebb0 retains only
     * its first four nodes. Never append a fabricated direct goal segment. */
    next.count=candidate->count;
    /* Assignment-only success boundary; no lookup/query/resource operation
     * or other fallible call occurs after the first destination changes. */
    *start=candidate->start;*goal=candidate->goal;*retained=next;
    *cost=candidate->cost;*route_index=1;
    return RF_OK;
}
#endif
