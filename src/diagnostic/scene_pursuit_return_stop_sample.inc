#ifndef RF_SCENE_PURSUIT_RETURN_STOP_SAMPLE_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_STOP_SAMPLE_INCLUDED
/* Uncalled fresh-sampling port adaptation for a resolved living actor target.
 * Original4062c0 reads cached AI29c/29d visibility bytes;
 * these body-then-eye rays do not claim the original cached-byte timing.
 * Caller supplies copied immutable points and authenticated live/disjoint
 * world/mover storage, holding exclusion for ALL actual ray traversal scratch.
 * Complete owner/target/command/weapon/geometry seals surround this operation;
 * it neither acquires a lease nor proves those identities. No actor access,
 * Blind-age/delay/timer/RNG mutation or live gameplay publication occurs.
 * Missing current-frame player eye must be refused before this call.
 */
typedef struct scene_pursuit_return_stop_sample_result {
    float distance;
    uint32_t feasible;
} scene_pursuit_return_stop_sample_result;
static int scene_pursuit_return_stop_sample(
    const rf_geometry_collision_world *world,
    const rf_geometry_collision_movers *movers,
    const float source_body[3],const float source_eye[3],
    const float target_body[3],const float target_eye[3],
    uint32_t hidden,uint32_t visibility_override,float primary_limit,
    scene_pursuit_return_stop_sample_result *result)
{
    scene_pursuit_return_stop_sample_result out={0};
    double squared=0,distance;uint32_t body_visible=0,eye_visible=0,blocked;int status;
    if(!world||!movers||!source_body||!source_eye||!target_body||!target_eye||!result)
        return RF_RANGE;
    if(hidden>1||visibility_override>1||!isfinite(primary_limit)||primary_limit<0)
        return RF_FORMAT;
    for(uint32_t axis=0;axis<3;axis++){
        double delta;
        if(!isfinite(source_body[axis])||!isfinite(source_eye[axis])||
           !isfinite(target_body[axis])||!isfinite(target_eye[axis]))return RF_FORMAT;
        delta=(double)target_body[axis]-(double)source_body[axis];squared+=delta*delta;
    }
    /* Bounded finite domain; double intermediates avoid accidental float
     * overflow before the final float distance is checked. No x87-bit claim. */
    distance=sqrt(squared);
    if(!isfinite(distance)||distance>FLT_MAX)return RF_FORMAT;
    out.distance=(float)distance;
    if(!hidden){
        status=rf_geometry_collision_ray(world,movers,source_eye,target_body,1,NULL,&blocked);
        if(status)return status;if(blocked>1)return RF_FORMAT;
        body_visible=!blocked;
        if(blocked){
            status=rf_geometry_collision_ray(world,movers,source_eye,target_eye,1,NULL,&blocked);
            if(status)return status;if(blocked>1)return RF_FORMAT;
            eye_visible=!blocked;
        }
    }
    /* Hidden targets suppress rays, not the independent flags530 bit1000.
     * An unqueried eye remains false; only the visibility OR is consumed. */
    status=scene_pursuit_stop_feasible(body_visible,eye_visible,visibility_override,
        1,out.distance,primary_limit,&out.feasible);if(status)return status;
    *result=out;return RF_OK;
}
#endif
