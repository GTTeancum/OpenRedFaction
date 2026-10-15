#ifndef RF_SCENE_PURSUIT_ROUTE_PREPARE_INCLUDED
#define RF_SCENE_PURSUIT_ROUTE_PREPARE_INCLUDED
/* UNCALLED staging. One ordinary search through the existing graph backend.
 * Requires exclusive synchronous use of the supplied bounded workspace
 * through its eventual publication; no intervening graph request or callback
 * may change global candidate query points. Deferred publication requires a
 * separate full geometry seal not supplied here.
 * Mutates graph search scratch and temporary goal-link backing bytes, as the
 * core normally does; does NOT mutate an actor, its route or its endpoints.
 * This is not a pure helper and does not grant scheduler/query authority.
 * Only core-owned solid search callbacks are invoked. The non-textured solid
 * path also borrows each collision tree traversal stack; exclusion must cover
 * those stacks as well as graph scratch. No texture/resource callback is used.
 * Caller must enforce non-reentrancy and close owner/target/resource joins. */
typedef struct scene_pursuit_route_candidate {
    rf_entity_navigation_candidate start,goal;
    uint32_t count,node[4]; /* global ordinals; n=goal, n+1=start */
    float cost;
} scene_pursuit_route_candidate;
static int scene_pursuit_route_prepare(const rf_entity_navigation_graph_request *parameters,
    const rf_entity_navigation_candidate *start,const rf_entity_navigation_candidate *goal,
    scene_pursuit_route_candidate *result,uint32_t *found)
{
    rf_entity_navigation_graph_request q;
    rf_entity_navigation_retained_route route={0};
    rf_entity_navigation_reference old_refs[2];
    rf_entity_navigation_token_list old_lists[2];
    scene_pursuit_route_candidate out={0};
    uint32_t local_tokens[2][2]={{0}},n,success=0;int status;
    if(!parameters||!start||!goal||!result||!found)return RF_RANGE;
    q=*parameters;n=q.global_count;
    if(!q.references||!q.adjacency||!q.scratch||!q.solid||n>65534||
       q.scratch_capacity<n+2||q.search_mode!=1||
       q.first_start>=n||q.first_end>=n)return RF_RANGE;
    /* Caller already selected ordinary endpoints. This wrapper intentionally
     * does not add nearest fallback or a second clearance attempt. */
    out.start=*start;out.goal=*goal;
    for(uint32_t i=0;i<2;i++){
        old_refs[i]=q.references[n+i];old_lists[i]=q.adjacency[n+i];
        q.adjacency[n+i]=(rf_entity_navigation_token_list){local_tokens[i],0,2};
    }
    q.references[n].candidate=&out.goal;
    q.references[n+1].candidate=&out.start;
    q.route=&route;
    status=rf_entity_navigation_graph_request_run(&q,&success);
    /* Resolve private pointers before restoring the borrowed endpoint slots.
     * No pointer into this stack may escape, on either success or failure. */
    if(!status&&success){
        if(route.count>4)status=RF_FORMAT;
        else if(route.count<2)success=0;
        else if(!isfinite(q.cost))status=RF_FORMAT;
        else{
            out.count=route.count;out.cost=q.cost;
            for(uint32_t i=0;!status&&i<route.count;i++){
                uint32_t j;
                if(route.nodes[i]==&out.goal)j=n;
                else if(route.nodes[i]==&out.start)j=n+1;
                else for(j=0;j<n&&q.references[j].candidate!=route.nodes[i];j++){}
                if(j==n&&route.nodes[i]!=&out.goal)status=RF_FORMAT;
                else out.node[i]=j;
            }
        }
    }
    /* Reserved lists have private backing, so prior endpoint counts/items
     * were never changed. Global goal links are closed by the core. Core
     * errors are fatal to this preparation, never a benign direct fallback. */
    for(uint32_t i=0;i<2;i++){
        q.adjacency[n+i]=old_lists[i];q.references[n+i]=old_refs[i];
    }
    if(status)return status;
    if(!success)memset(&out,0,sizeof(out));
    *result=out;*found=!!success;return RF_OK;
}
#endif
