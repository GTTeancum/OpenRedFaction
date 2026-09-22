#ifndef RF_SCENE_CLUTTER_POSE_INC
#define RF_SCENE_CLUTTER_POSE_INC
#include "rf/clutter.h"
#include <math.h>
#include <string.h>
/* Caller owns registry validation, collision admission and world room lookup.
 * Basis rows are local axes in world coordinates, as in clutter creation and
 * rf_physics_body_segment. Room is runtime one-based, with zero unknown.
 * This teleports a retained owner; it does not simulate carrying or velocity.
 * All state publishes atomically; position/basis may alias this owner. */
static int scene_clutter_pose_publish(rf_clutter_base_owner *owner,
    const float position[3],const float basis[9],uint32_t room)
{
    rf_physics_body_state body;float published[3];uint32_t flags,i,j,k;double det;int status;
    if(!owner || !position || !basis)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(position[i]))return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(basis[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=i;j<3;j++){
        double dot=0;for(k=0;k<3;k++)dot+=(double)basis[i*3+k]*basis[j*3+k];
        if(fabs(dot-(i==j?1.0:0.0))>.001)return RF_FORMAT;
    }
    det=(double)basis[0]*((double)basis[4]*basis[8]-(double)basis[5]*basis[7])-
        (double)basis[1]*((double)basis[3]*basis[8]-(double)basis[5]*basis[6])+
        (double)basis[2]*((double)basis[3]*basis[7]-(double)basis[4]*basis[6]);
    if(fabs(det-1.0)>.001)return RF_FORMAT;
    body=owner->body.state;
    if(!isfinite(body.bounds.radius) || body.bounds.radius<0)return RF_RANGE;
    memcpy(body.position,position,12);memcpy(body.orientation,basis,36);memcpy(body.next_orientation,basis,36);
    status=rf_physics_tensor_world(body.local_tensor,body.orientation,body.world_tensor);if(status)return status;
    flags=owner->state.flags;
    status=rf_physics_publish_position(&body,published,&flags);if(status)return status;
    /* Publication API normally marks physics scheduling flags. This explicit
     * pose service leaves scheduling and all gameplay flags with its caller. */
    body.flags=owner->body.state.flags;
    owner->body.state=body;
    memcpy(owner->state.position,published,12);memcpy(owner->query_position,published,12);
    memcpy(owner->matrix,body.orientation,36);owner->state.first_word=room;
    return RF_OK;
}
#endif
