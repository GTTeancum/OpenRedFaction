#ifndef RF_SCENE_PURSUIT_RETURN_ROUTE_PREPARE_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_ROUTE_PREPARE_INCLUDED
/* Uncalled phase1 composition, no gameplay publication or lease management.
 * Caller consumes one scheduler selection, qualifies range/cohort before
 * prelude, then snapshots these inputs under the same owner/target/command.
 * The exclusive graph AND collision traversal lease spans this entire call,
 * subsequent full identity/geometry revalidation and eventual publication.
 * All borrowed allocations must remain authenticated, live and disjoint.
 * Queries mutate graph/collision scratch; failure is not scratch rollback.
 * RF_NOT_FOUND is refusal, never a completed MISS. No retry or reselection.
 */
typedef struct scene_pursuit_return_route_frame {
    scene_pursuit_return_receipt before;
    float source_body[3],source_eye[3],target_body[3],target_eye[3];
    float authored_radius,primary_limit;
    uint32_t melee_only,hidden,visibility_override;
} scene_pursuit_return_route_frame;
typedef struct scene_pursuit_return_route_prepared {
    uint32_t route_status; /* SUCCESS or MISS only; zero is not a result. */
    scene_pursuit_route_candidate route;
    scene_pursuit_return_completion completion;
    scene_pursuit_return_stop_sample_result miss_sample; /* valid only on MISS */
} scene_pursuit_return_route_prepared;
static int scene_pursuit_return_route_prepare(
    const rf_level_navigation_workspace *workspace,
    const rf_level_owned_navigation *navigation,const rf_physics_spheres *spheres,
    const rf_geometry_collision_world *world,const rf_geometry_collision_movers *movers,
    const scene_pursuit_return_route_frame *frame,scene_pursuit_return_route_prepared *result)
{
    scene_pursuit_return_route_prepared out={0};
    scene_pursuit_route_request request;rf_collision_solid_view solid={0};
    uint32_t found=0;int status;
    if(!workspace||!navigation||!spheres||!world||!movers||!frame||!result)return RF_RANGE;
    status=scene_pursuit_return_receipt_admit(&frame->before);if(status)return status;
    if(frame->before.phase!=SCENE_PURSUIT_RETURN_PENDING_ROUTE||frame->melee_only>1||
       frame->hidden>1||frame->visibility_override>1||
       !isfinite(frame->primary_limit)||frame->primary_limit<0)return RF_FORMAT;
    for(uint32_t axis=0;axis<3;axis++)if(!isfinite(frame->source_body[axis])||
        !isfinite(frame->source_eye[axis])||!isfinite(frame->target_body[axis])||
        !isfinite(frame->target_eye[axis]))return RF_FORMAT;
    if(!scene_flight_search_busy)return RF_NOT_FOUND;
    solid.rooms=world->views;solid.room_count=world->room_count;
    solid.primary=world->primary;solid.primary_count=world->primary_count;
    solid.children=world->children;solid.child_count=world->child_count;
    /* Construct directly at its final address. request.graph.solid is a
     * self-pointer; request must not be copied or returned. */
    status=scene_pursuit_route_request_prepare(workspace,navigation,spheres,
        frame->authored_radius,frame->source_body,frame->target_body,&solid,&request);
    if(status)return status;
    status=scene_pursuit_route_select_prepare(&request.graph,&request.start,&request.goal,
        &out.route,&found);if(status)return status;
    if(found>1)return RF_FORMAT;
    if(found){
        out.route_status=SCENE_PURSUIT_RETURN_ROUTE_SUCCESS;
        /* Exact successful-request branch: no fabricated miss inputs.
         * Behavior clock and actual writes remain with found_publish. */
        out.completion.publish_behavior=1;out.completion.behavior=1;
    }else{
        out.route_status=SCENE_PURSUIT_RETURN_ROUTE_MISS;
        status=scene_pursuit_return_stop_sample(world,movers,frame->source_body,frame->source_eye,
            frame->target_body,frame->target_eye,frame->hidden,frame->visibility_override,
            frame->primary_limit,&out.miss_sample);if(status)return status;
        status=scene_pursuit_return_route_complete(&frame->before,frame->melee_only,
            SCENE_PURSUIT_RETURN_ROUTE_MISS,out.miss_sample.feasible,out.miss_sample.distance,
            frame->primary_limit,&out.completion);if(status)return status;
    }
    /* Route holds values/ordinals only. MISS may request phase2; it never
     * means the fallback itself completed. Caller must inspect the tag. */
    *result=out;return RF_OK;
}
#endif
