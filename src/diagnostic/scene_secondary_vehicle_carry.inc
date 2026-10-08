#ifndef RF_SCENE_SECONDARY_VEHICLE_CARRY_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_CARRY_INCLUDED
/* Preflight only: actors are carried later by their existing support owner.
 * Use the same row-basis old-frame -> new-frame point math as moving groups.
 * A blocked full-body rider path vetoes this hull step; there is no new seat,
 * extra integration, hidden collision bypass or coupled/crush approximation. */
uint32_t rf_scene_secondary_vehicle_carry[8]; /* steps,riders,blocked,host UID,actor,point paths,pair gates,status */
static const rf_physics_body *scene_secondary_vehicle_rider_body(uint32_t host,uint32_t index,uint32_t *actor)
{
    if(!actor||index>campaign_npc_body_count)return NULL;
    *actor=index?campaign_npc_bodies[index-1].registration.handle:campaign_player_object.handle;
    if(!scene_secondary_vehicle_rider(host,*actor))return NULL;
    return index?&campaign_npc_bodies[index-1].body:&scene_actor_body;
}
/* A carry chord can meet an upward-facing sphere of its already published
 * host. Match the NPC tangent-support retry, but require the complete player
 * endpoint to be outside the actual host before omitting that self-contact. */
static int scene_secondary_vehicle_player_destination(uint32_t handle,const rf_physics_body_state *state,uint32_t *clear)
{
    const scene_secondary_vehicle_runtime *r;rf_checkpoint_placement p={0};int status;
    if(!state||!clear)return RF_RANGE;*clear=0;
    if(!scene_secondary_vehicle_rider(handle,campaign_player_object.handle))return RF_OK;
    r=scene_secondary_vehicle_find(handle);if(!r)return RF_NOT_FOUND;
    p.spheres=scene_actor_body.spheres.items;p.count=scene_actor_body.spheres.count;
    memcpy(p.position,state->next_position,12);memcpy(p.basis,state->orientation,36);
    status=scene_vehicle_wreck_pair(&p,r->physics.spheres,r->physics.sphere_count,
        r->physics.state.position,r->physics.state.orientation);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;*clear=1;return RF_OK;
}
static int scene_secondary_vehicle_carry_clear(scene_stream *stream,const scene_secondary_vehicle_runtime *r,
    const rf_vehicle_rigid_state *next,uint32_t *allowed)
{
    scene_mover_interval interval={0};uint32_t *probe=rf_scene_secondary_vehicle_carry;int status;
    if(!stream||!r||!next||!allowed)return RF_RANGE;*allowed=1;
    if(!scene_secondary_vehicle_has_rider(r->handle))return RF_OK;
    ++probe[0];probe[3]=r->uid;
    memcpy(interval.start,r->physics.state.position,12);memcpy(interval.end,next->position,12);
    memcpy(interval.matrix,r->physics.state.orientation,36);memcpy(interval.end_matrix,next->orientation,36);
    interval.handle=r->handle;interval.changed=!!memcmp(interval.start,interval.end,12)|
        ((!!memcmp(interval.matrix,interval.end_matrix,36))<<1);
    if(!interval.changed)return RF_OK;
    for(uint32_t i=0;i<=campaign_npc_body_count;i++){
        uint32_t actor,found=0;const rf_physics_body *body=scene_secondary_vehicle_rider_body(r->handle,i,&actor);
        rf_collision_body_sphere spheres[8];rf_collision_body_query query={0};rf_geometry_body_hit hit={0};
        rf_checkpoint_placement placement={0};float point[3],velocity[3];
        if(!body)continue;++probe[1];probe[4]=actor;
        if(!body->spheres.items||!body->spheres.count||body->spheres.count>8)return RF_FORMAT;
        status=rf_scene_npc_support_interval_target(&interval,body->state.position,scene_step_seconds,point,velocity);if(status)goto fail;
        for(uint32_t k=0;k<body->spheres.count;k++){
            memcpy(spheres[k].center,body->spheres.items[k].center,12);spheres[k].radius=body->spheres.items[k].radius;
        }
        memcpy(query.start,body->state.position,12);memcpy(query.end,point,12);memcpy(query.matrix,body->state.orientation,36);
        query.spheres=spheres;query.count=body->spheres.count;query.radius=body->state.bounds.radius;query.flags=0x460;query.limit=1;
        ++probe[5];status=scene_vehicle_owner_collision_query_ex(stream,actor,r->handle,&query,&hit,&found);if(status)goto fail;
        if(found&&hit.contact.fraction<1){*allowed=0;++probe[2];return RF_OK;}
        placement.spheres=body->spheres.items;placement.count=body->spheres.count;
        memcpy(placement.position,point,12);memcpy(placement.basis,body->state.orientation,36);
        /* The rider keeps its own orientation. A pitching host must still fit
         * below that real upright body at the accepted destination. */
        ++probe[6];status=scene_vehicle_wreck_pair(&placement,r->physics.spheres,r->physics.sphere_count,
            next->position,next->orientation);
        if(status==RF_NOT_FOUND){*allowed=0;++probe[2];return RF_OK;}if(status)goto fail;
        for(uint32_t j=i+1;j<=campaign_npc_body_count;j++){
            uint32_t other_actor;float other_point[3],other_velocity[3];
            const rf_physics_body *other=scene_secondary_vehicle_rider_body(r->handle,j,&other_actor);
            if(!other)continue;
            status=rf_scene_npc_support_interval_target(&interval,other->state.position,scene_step_seconds,other_point,other_velocity);if(status)goto fail;
            ++probe[6];status=scene_vehicle_wreck_pair(&placement,other->spheres.items,other->spheres.count,other_point,other->state.orientation);
            if(status==RF_NOT_FOUND){*allowed=0;++probe[2];return RF_OK;}if(status)goto fail;
        }
    }
    probe[7]=0;return RF_OK;
fail:probe[7]=(uint32_t)status;return status;
}
#endif
