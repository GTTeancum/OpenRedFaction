#ifndef RF_SCENE_PURSUIT_NOTICE_DISTRIBUTION_INCLUDED
#define RF_SCENE_PURSUIT_NOTICE_DISTRIBUTION_INCLUDED
/* UNCALLED live-adapter staging for408c05..408c63. Requires the notice mirror
 * helper and actor identity definitions. No return/prelude calls this yet.
 * Registered scene NPCs are the port actor-list domain; generated/other lists
 * must be explicitly composed before a broader claim. No distance filter.
 * Two callback-free passes avoid partial writes on a later malformed peer.
 * Caller owns the simulation thread and stable section/actor array throughout;
 * no resource, audio, graph, collision or other gameplay callback occurs here.
 * Capture/restore and reached-cone consumers must be complete before enablement.
 */
static uint32_t scene_pursuit_notice_peer_selected(const campaign_npc_body *owner,
    const campaign_npc_body *peer)
{
    return peer!=owner&&peer->registration.view&&
        peer->damage.effects.affiliation==owner->damage.effects.affiliation&&
        (peer->ai_mode.action_280==2||peer->ai_mode.action_280==4);
}
static int scene_pursuit_notice_distribute(uint32_t slot,uint32_t network_a,
    uint32_t network_b,uint32_t *changed)
{
    campaign_npc_body *owner;uint32_t count=0;int status;
    if(!changed||!campaign_npc_bodies||slot>=campaign_npc_body_count)
        return RF_RANGE;
    if(campaign_npc_body_count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_FORMAT;
    owner=campaign_npc_bodies+slot;
    status=scene_npc_reload_identity(slot,owner);if(status)return status;
    /*40a110 tests the INITIATOR object+7c bit4000, not a peer predicate. */
    if(((network_a|network_b)&255u)||(owner->object_flags&0x4000u)){
        *changed=0;return RF_OK;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *peer=campaign_npc_bodies+i;
        scene_pursuit_notice_mirrors words;
        if(!scene_pursuit_notice_peer_selected(owner,peer))continue;
        status=scene_npc_reload_identity(i,peer);if(status)return status;
        if(peer->view.action_520!=peer->ai_mode.action_280)return RF_FORMAT;
        words=(scene_pursuit_notice_mirrors){peer->ai_mode.flags_530,
            peer->view.flags_7d0,peer->firing.flags_7d0};
        status=scene_pursuit_notice_mirrors_valid(&words);if(status)return status;
    }
    /* Validation above and these assignments have no intervening callback.
     * Preserve every unrelated flag separately in its existing port mirror. */
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *peer=campaign_npc_bodies+i;
        if(!scene_pursuit_notice_peer_selected(owner,peer))continue;
        count+=!(peer->ai_mode.flags_530&SCENE_PURSUIT_NOTICE_BIT);
        peer->ai_mode.flags_530|=SCENE_PURSUIT_NOTICE_BIT;
        peer->view.flags_7d0|=SCENE_PURSUIT_NOTICE_BIT;
        peer->firing.flags_7d0|=SCENE_PURSUIT_NOTICE_BIT;
    }
    *changed=count;return RF_OK;
}
#endif
