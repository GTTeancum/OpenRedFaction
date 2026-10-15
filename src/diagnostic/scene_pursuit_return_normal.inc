#ifndef RF_SCENE_PURSUIT_RETURN_NORMAL_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_NORMAL_INCLUDED
/* UNCALLED ordinary return-prelude adapter for427fb0/42a020/4280b0.
 * Include after pursuit source qualification and scripted speed/ground helpers.
 * Caller must already own this frame's shared expensive-work slot. This is
 * not a scheduler, return admission, or permission to replay the prelude.
 * Only the existing ordinary tactical cohort is admitted; action7 retreat
 * and other stronger owners require their separate complete transition.
 *
 * The concrete NULL player callback and campaign_scripted_speed_ground path
 * use retained collision/material/support data and internal callbacks only.
 * No resource demand, audio callback or gameplay dispatch occurs. The shared
 * workspace guard excludes nested graph/collision scratch use through normal
 * publication. Caller retains simulation-thread and section ownership.
 * Existing normal's partial-error effects are intentionally not rolled back.
 * RF_OK also covers falling or blocked standing; it does not certify standing.
 */
static int scene_pursuit_return_normal(scene_stream *stream,uint32_t slot)
{
    campaign_npc_body *owner;uint32_t cls,target,melee_only;int status;
    rf_scene_npc_stance_services services={NULL,campaign_scripted_speed_ground,stream};
    if(!stream||stream!=scene_actor_collision_owner||!stream->collision)
        return RF_RANGE;
    /* Denial precedes every actor/output mutation. The dispatcher must retain
     * its logical demand, without RNG/timer restart or a miss fallback. */
    if(scene_flight_search_busy)return RF_NOT_FOUND;
    status=scene_pursuit_return_owner(slot,&target,&melee_only);if(status)return status;
    owner=campaign_npc_bodies+slot;
    cls=campaign_seeds.items[slot].class_index;
    /* Qualification proves the base class walk bit, but retain the explicit
     * source gate. Physical crouch is not the427fb0 predicate. */
    if(!(campaign_seeds.classes[cls].physics.flags&1u))return RF_FORMAT;
    if(rf_entity_falling((int32_t)campaign_modes[owner->movement_slot].index,
        campaign_seeds.classes[cls].physics.use_kind,owner->support.material))
        return RF_OK;
    scene_flight_search_busy=1;
    status=rf_scene_npc_normal(stream->collision,owner->registration.handle,&services);
    scene_flight_search_busy=0;
    return status;
}
#endif
