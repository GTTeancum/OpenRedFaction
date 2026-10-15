#ifndef RF_SCENE_PURSUIT_ROUTE_SELECT_PREPARE_INCLUDED
#define RF_SCENE_PURSUIT_ROUTE_SELECT_PREPARE_INCLUDED
/* UNCALLED staging, composing the current ordinary endpoint selector with
 * route_prepare.inc. Only the positive authored-clearance, mode0 cohort is
 * represented. Caller supplies already prepared physical height/endpoints.
 * Retains the current adapter's actor-radius visibility adaptation, rather
 * than the selector's historical2.5 callback argument. One clearance pass;
 * no added centerline retry, nearest-start query, or direct movement fallback.
 * Caller holds graph AND collision traversal scratch exclusively across
 * selection, preparation and eventual publication; these calls mutate scratch.
 * Inputs, outputs and scratch must be distinct, non-overlapping storage.
 * Source/actor/full-target/resource/scheduler/save joins remain caller-owned. */
typedef struct scene_pursuit_select_context {
    const rf_collision_solid_view *solid;
    float radius;
} scene_pursuit_select_context;
static int scene_pursuit_select_visible(void *context,const float start[3],
    const float end[3],float historical_radius,uint32_t *blocked)
{
    const scene_pursuit_select_context *c=context;
    rf_collision_sweep_room_hit hit;float delta[3];
    (void)historical_radius;
    if(!c||!c->solid||!start||!end||!blocked)return RF_RANGE;
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(start[i])||!isfinite(end[i]))return RF_FORMAT;
        delta[i]=end[i]-start[i];if(!isfinite(delta[i]))return RF_FORMAT;
    }
    return rf_collision_solid_preferred(c->solid,NULL,0x45,start,delta,c->radius,1,&hit,blocked);
}
static int scene_pursuit_route_select_prepare(
    const rf_entity_navigation_graph_request *parameters,
    const rf_entity_navigation_candidate *start,
    const rf_entity_navigation_candidate *goal,
    scene_pursuit_route_candidate *result,uint32_t *found)
{
    rf_entity_navigation_graph_request q;
    rf_entity_navigation_selection begin,end;
    scene_pursuit_select_context c;int status;
    if(!parameters||!start||!goal||!result||!found)return RF_RANGE;
    q=*parameters;
    if(!q.references||!q.adjacency||!q.scratch||!q.solid||
       q.global_count>65534||q.scratch_capacity<q.global_count+2||
       q.mode!=0||q.search_mode!=1)return RF_RANGE;
    if(!isfinite(q.radius)||q.radius<=0||!isfinite(q.height)||q.height<0)
        return RF_FORMAT;
    for(uint32_t axis=0;axis<3;axis++){
        if(!isfinite(start->position[axis])||!isfinite(goal->position[axis])||
           !isfinite(start->query_point[axis])||!isfinite(goal->query_point[axis]))return RF_FORMAT;
    }
    c.solid=q.solid;c.radius=q.radius;
    status=rf_entity_navigation_select(q.references,q.global_count,start->position,
        q.radius,q.height,0,1,scene_pursuit_select_visible,&c,&begin);if(status)return status;
    status=rf_entity_navigation_select(q.references,q.global_count,goal->position,
        q.radius,q.height,0,1,scene_pursuit_select_visible,&c,&end);if(status)return status;
    if(begin.first==UINT32_MAX||end.first==UINT32_MAX){
        memset(result,0,sizeof(*result));*found=0;return RF_OK;
    }
    q.first_start=begin.first;q.second_start=begin.second;
    q.first_end=end.first;q.second_end=end.second;
    return scene_pursuit_route_prepare(&q,start,goal,result,found);
}
#endif
