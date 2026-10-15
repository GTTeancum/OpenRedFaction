#ifndef RF_SCENE_PURSUIT_NOTICE_ACQUISITION_INCLUDED
#define RF_SCENE_PURSUIT_NOTICE_ACQUISITION_INCLUDED
/* UNCALLED ordinary acquisition adapter; include after opposed range/facing,
 * registered-target helpers and notice mirrors. Outer caller has already
 * admitted the sample, affiliation, mode and candidate. It owns stable scene
 * lifetime and must retain this exact decision for duplicated checks of the
 * SAME candidate. A distinct candidate invokes this again and sees the clear.
 * The result is a local observation, not permission after a callback or on a
 * later frame. No global cache, actor allocation or target assignment.
 * Clear occurs at reached-cone, BEFORE later cover/LOS. Range refusal leaves
 * notice untouched. Alarm sight continues using its side-effect-free wrapper.
 */
typedef struct scene_pursuit_notice_decision {
    uint32_t owner,candidate,in_range,in_cone,consumed;
    float delta[3],distance_squared;
} scene_pursuit_notice_decision;
static int scene_pursuit_notice_acquire_cone(uint32_t slot,uint32_t candidate,
    const float point[3],scene_pursuit_notice_decision *result)
{
    scene_pursuit_notice_decision out={0};campaign_npc_body *owner;
    scene_pursuit_notice_mirrors before;
    scene_pursuit_notice_consume_plan plan;int status;
    if(!point||!result||!campaign_npc_bodies||slot>=campaign_npc_body_count)
        return RF_RANGE;
    owner=campaign_npc_bodies+slot;
    status=scene_npc_reload_identity(slot,owner);if(status)return status;
    if(!campaign_enemy_opposed_live(owner)||candidate==owner->registration.handle||
       !scene_blind_target_live(candidate))return RF_NOT_FOUND;
    out.owner=owner->registration.handle;out.candidate=candidate;
    if(!campaign_enemy_opposed_range(owner,point,out.delta,&out.distance_squared)){
        /* Do not export partial nonfinite deltas from a refused range. */
        memset(out.delta,0,sizeof(out.delta));*result=out;return RF_OK;
    }
    out.in_range=1;
    before=(scene_pursuit_notice_mirrors){owner->ai_mode.flags_530,
        owner->view.flags_7d0,owner->firing.flags_7d0};
    status=scene_pursuit_notice_consume(&before,&plan);if(status)return status;
    out.consumed=plan.bypass_facing;
    out.in_cone=plan.bypass_facing||
        campaign_enemy_opposed_facing(owner,out.delta,out.distance_squared);
    /* All checks and preparation above are callback-free. No fallible work
     * follows these joint assignments. Failed LOS must not restore this bit. */
    owner->ai_mode.flags_530=plan.words.ai_flags;
    owner->view.flags_7d0=plan.words.view_flags;
    owner->firing.flags_7d0=plan.words.firing_flags;
    *result=out;return RF_OK;
}
#endif
