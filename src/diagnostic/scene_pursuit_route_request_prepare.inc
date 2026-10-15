#ifndef RF_SCENE_PURSUIT_ROUTE_REQUEST_PREPARE_INCLUDED
#define RF_SCENE_PURSUIT_ROUTE_REQUEST_PREPARE_INCLUDED
/* Uncalled, allocation-free construction for the positive authored-clearance
 * return lane. Mirrors campaign_script_route_leased's physical height and
 * endpoint construction, using a freshly resolved target, never an old route.
 * Caller authenticates all allocation lifetimes/spans and owns graph AND
 * collision scratch exclusively through query, revalidation and publication.
 * Structural byte checks below do not authenticate allocations or a lease.
 * Inputs/output must be stable and disjoint. No queries or live writes here.
 * The result contains a self-pointer: retain at its final address, do not copy.
 * RF_NOT_FOUND is untouched admission refusal, never a completed route miss.
 * No caller/include, scheduler authority or feature admission is supplied. */
typedef struct scene_pursuit_route_request {
    rf_entity_navigation_graph_request graph;
    rf_collision_solid_view solid;
    rf_entity_navigation_candidate start,goal;
} scene_pursuit_route_request;

static int scene_pursuit_route_request_span(const void *base,uint64_t bytes,
    const void *pointer,uint64_t length)
{
    uintptr_t b=(uintptr_t)base,p=(uintptr_t)pointer;
    if(!base||!pointer||bytes>UINTPTR_MAX-b||p<b)return 0;
    return (uint64_t)(p-b)<=bytes&&length<=bytes-(uint64_t)(p-b);
}

static int scene_pursuit_route_request_prepare(
    const rf_level_navigation_workspace *workspace,
    const rf_level_owned_navigation *navigation,const rf_physics_spheres *spheres,
    float authored_radius,const float position[3],const float target[3],
    const rf_collision_solid_view *solid,scene_pursuit_route_request *result)
{
    scene_pursuit_route_request out={0};
    uint64_t bytes,n,sphere_bytes;float low=0,high=0;
    if(!workspace||!navigation||!spheres||!position||!target||!solid||!result)
        return RF_RANGE;
    if(!isfinite(authored_radius)||authored_radius<0)return RF_FORMAT;
    if(!authored_radius||!workspace->storage||!spheres->count)return RF_NOT_FOUND;
    sphere_bytes=(uint64_t)spheres->count*sizeof(*spheres->items);
    if(!spheres->items||sphere_bytes+sizeof(*spheres)!=spheres->allocated_bytes)
        return RF_FORMAT;
    if(workspace->count!=navigation->count||workspace->count>65534||
       workspace->allocated_bytes<sizeof(*workspace)||
       (navigation->count&&!navigation->references))return RF_FORMAT;
    bytes=workspace->allocated_bytes-sizeof(*workspace);n=(uint64_t)workspace->count+2;
    if(!scene_pursuit_route_request_span(workspace->storage,bytes,workspace->references,
            n*sizeof(*workspace->references))||
       !scene_pursuit_route_request_span(workspace->storage,bytes,workspace->adjacency,
            n*sizeof(*workspace->adjacency))||
       !scene_pursuit_route_request_span(workspace->storage,bytes,workspace->scratch,
            n*sizeof(*workspace->scratch)))return RF_FORMAT;
    /* Require the actual constructor layout, not merely overlapping spans. */
    if((uintptr_t)workspace->references!=(uintptr_t)workspace->storage||
       (uintptr_t)workspace->adjacency!=(uintptr_t)workspace->references+n*sizeof(*workspace->references)||
       (uintptr_t)workspace->scratch!=(uintptr_t)workspace->adjacency+n*sizeof(*workspace->adjacency))return RF_FORMAT;
    for(uint32_t i=0;i<workspace->count;i++){
        const rf_entity_navigation_reference *r=workspace->references+i;
        const rf_entity_navigation_token_list *a=workspace->adjacency+i;
        if(!r->candidate||r->candidate!=navigation->references[i].candidate||
           r->order_key!=i+1u||a->count>a->capacity||
           !scene_pursuit_route_request_span(workspace->storage,bytes,a->items,
               (uint64_t)a->capacity*sizeof(*a->items)))return RF_FORMAT;
    }
    for(uint32_t i=0;i<spheres->count;i++){
        const rf_physics_sphere *s=spheres->items+i;float bottom,top;
        if(!isfinite(s->radius)||s->radius<0)return RF_FORMAT;
        for(uint32_t j=0;j<3;j++)if(!isfinite(s->center[j]))return RF_FORMAT;
        bottom=s->center[1]-s->radius;top=s->center[1]+s->radius;
        if(!isfinite(bottom)||!isfinite(top))return RF_FORMAT;
        if(!i||bottom<low)low=bottom;if(!i||top>high)high=top;
    }
    out.graph.height=high-low;
    if(!isfinite(out.graph.height)||out.graph.height<0)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(position[i])||!isfinite(target[i]))return RF_FORMAT;
        out.start.position[i]=out.start.query_point[i]=position[i];
        out.goal.position[i]=out.goal.query_point[i]=target[i];
    }
    out.solid=*solid;out.graph.radius=authored_radius;
    out.graph.references=workspace->references;out.graph.adjacency=workspace->adjacency;
    out.graph.global_count=workspace->count;out.graph.scratch=workspace->scratch;
    out.graph.scratch_capacity=(uint32_t)n;out.graph.mode=0;out.graph.search_mode=1;
    /* Downstream private route preparation supplies its own retained route.
     * Bind the solid only after copying so no stack address escapes. */
    *result=out;result->graph.solid=&result->solid;return RF_OK;
}
#endif
