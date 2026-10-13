#ifndef RF_SCENE_TACTICAL_LOCAL_ROUTE_CANDIDATE_INCLUDED
#define RF_SCENE_TACTICAL_LOCAL_ROUTE_CANDIDATE_INCLUDED
/* Four bounded samples after the current start region was prepared once.
 * The result stores graph ordinals and endpoints, never pointers to locals.
 * The live owner must recheck identity and install its OWN start/goal nodes
 * with either [start,goal] or [start,first,selected,goal]. No general pursuit
 * fallback is allowed on a miss. No movement or shared RNG is changed here. */
typedef struct scene_tactical_route_candidate {
    uint32_t count,first,selected;
    float start[3],goal[3];
} scene_tactical_route_candidate;
static int scene_tactical_local_candidate(const rf_entity_navigation_reference *refs,
    uint32_t count,uint32_t first,float radius,const float start[3],const float target[3],
    rf_random_state *random,scene_tactical_route_candidate *out,uint32_t *found)
{
    scene_tactical_route_candidate next={0};scene_tactical_region_sample sample;
    rf_random_state working;float closest[3];double score;uint32_t i,attempt;int status;
    if(!refs||!start||!target||!random||!out||!found)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(start[i])||!isfinite(target[i]))return RF_FORMAT;
    working=*random;
    for(attempt=0;attempt<4;attempt++){
        status=scene_tactical_sample_region(refs,count,first,radius,&working,&sample);if(status)return status;
        /* A zero-length sampled segment is a rejected candidate, never stale
         * retained geometry. Finite arithmetic failures still reject the call. */
        if(sample.goal[0]==start[0]&&sample.goal[1]==start[1]&&sample.goal[2]==start[2])continue;
        status=scene_tactical_target_clearance(target,start,sample.goal,closest,&score);if(status)return status;
        if(score<9.0)continue;
        next.count=sample.count;next.first=sample.first;next.selected=sample.selected;
        memcpy(next.start,start,sizeof(next.start));memcpy(next.goal,sample.goal,sizeof(next.goal));
        *out=next;*random=working;*found=1;return RF_OK;
    }
    /* Deliberate exhausted-attempt publication advances only the private RNG.
     * Empty candidate requires the caller to publish its bounded retry phase. */
    *out=next;*random=working;*found=0;return RF_OK;
}
#endif
