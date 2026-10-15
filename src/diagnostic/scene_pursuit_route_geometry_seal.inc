#ifndef RF_SCENE_PURSUIT_ROUTE_GEOMETRY_SEAL_INCLUDED
#define RF_SCENE_PURSUIT_ROUTE_GEOMETRY_SEAL_INCLUDED
/* UNCALLED movement-geometry snapshot for an already selected route prefix.
 * Caller proves world/section epoch and owning reference-array lifetime before
 * every call; header equality alone cannot prove that freed storage is alive.
 * This seals movement positions, not mutable search scores/query points, and
 * never rechecks target clearance. It is not a search lease or full owner seal.
 * Endpoints are supplied privately, globals through the current graph header.
 * Inputs and result must be non-overlapping caller-owned storage.
 * No allocation, actor writes, collision queries or callbacks occur here. */
typedef struct scene_pursuit_route_geometry_seal {
    uint32_t count,global_count;
    const rf_entity_navigation_reference *references;
    uint32_t node[4];
    const rf_entity_navigation_candidate *global_node[4];
    float position[4][3];
} scene_pursuit_route_geometry_seal;
_Static_assert(sizeof(void*)!=4||sizeof(scene_pursuit_route_geometry_seal)==92,
    "Pursuit movement geometry seal Xbox32 source bound changed");
static int scene_pursuit_route_geometry_capture(
    const scene_pursuit_route_candidate *candidate,
    const rf_entity_navigation_reference *references,uint32_t global_count,
    scene_pursuit_route_geometry_seal *result)
{
    scene_pursuit_route_geometry_seal out={0};
    if(!candidate||!references||!result)return RF_RANGE;
    if(global_count>65534||candidate->count<2||candidate->count>4||
       candidate->node[0]!=global_count+1)return RF_FORMAT;
    out.count=candidate->count;out.global_count=global_count;out.references=references;
    for(uint32_t i=0;i<out.count;i++){
        uint32_t n=candidate->node[i];const rf_entity_navigation_candidate *point;
        if(n>global_count+1)return RF_FORMAT;
        for(uint32_t j=0;j<i;j++)if(candidate->node[j]==n)return RF_FORMAT;
        if(n==global_count){
            if(i+1!=out.count)return RF_FORMAT;
            point=&candidate->goal;
        }else if(n==global_count+1)point=&candidate->start;
        else{
            point=references[n].candidate;
            if(!point)return RF_FORMAT;
            out.global_node[i]=point;
        }
        out.node[i]=n;
        for(uint32_t axis=0;axis<3;axis++){
            if(!isfinite(point->position[axis]))return RF_FORMAT;
            out.position[i][axis]=point->position[axis];
        }
    }
    *result=out;return RF_OK;
}
static int scene_pursuit_route_geometry_matches(
    const scene_pursuit_route_geometry_seal *expected,
    const scene_pursuit_route_candidate *candidate,
    const rf_entity_navigation_reference *references,uint32_t global_count)
{
    scene_pursuit_route_geometry_seal current;int status;
    if(!expected)return RF_RANGE;
    /* Caller must check the section lifetime before reaching this function. */
    if(expected->references!=references||expected->global_count!=global_count)
        return RF_NOT_FOUND;
    status=scene_pursuit_route_geometry_capture(candidate,references,global_count,&current);
    if(status)return status;
    if(expected->count!=current.count||
       memcmp(expected->node,current.node,sizeof(current.node))||
       memcmp(expected->global_node,current.global_node,sizeof(current.global_node))||
       memcmp(expected->position,current.position,sizeof(current.position)))return RF_NOT_FOUND;
    return RF_OK;
}
#endif
