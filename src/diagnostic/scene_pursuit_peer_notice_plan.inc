#ifndef RF_SCENE_PURSUIT_PEER_NOTICE_PLAN_INCLUDED
#define RF_SCENE_PURSUIT_PEER_NOTICE_PLAN_INCLUDED
/* UNCALLED pure proposals for 408c05..408c63 and 401f90..401fba.
 * Caller resolves current full identities and affiliations after callbacks.
 * The original peer traversal has no spatial filter. The receiving actor's
 * ordinary acquisition still owns range, scheduling and visibility gates.
 * No affiliation, target, action or live flags are changed by these helpers.
 * Neither helper supplies capture/restore or callback lifetime protection. */
typedef struct scene_pursuit_peer_notice_plan {
    uint32_t flags,write;
} scene_pursuit_peer_notice_plan;
static int scene_pursuit_peer_notice(uint32_t network_a,uint32_t network_b,
    uint32_t same_owner,uint32_t owner_affiliation,uint32_t peer_affiliation,
    int32_t peer_action,uint32_t initiating_predicate_40a110,uint32_t peer_flags,
    scene_pursuit_peer_notice_plan *result)
{
    scene_pursuit_peer_notice_plan out={peer_flags,0};
    if(!result)return RF_RANGE;
    if(same_owner>1)return RF_FORMAT;
    if(!((network_a|network_b)&255u)&&!same_owner&&
       owner_affiliation==peer_affiliation&&(peer_action==2||peer_action==4)&&
       (initiating_predicate_40a110&255u)!=1){
        out.flags|=0x20000u;out.write=1;
    }
    *result=out;return RF_OK;
}
typedef struct scene_pursuit_notice_cone_plan {
    uint32_t flags,bypass_facing;
} scene_pursuit_notice_cone_plan;
static int scene_pursuit_notice_at_cone(uint32_t current_flags,
    scene_pursuit_notice_cone_plan *result)
{
    scene_pursuit_notice_cone_plan out;
    if(!result)return RF_RANGE;
    /* Call only at the original reached-cone boundary, after earlier gates.
     * Publish the clear before visibility work even if that work later fails.
     * Duplicated player-priority preflight/final checks for the SAME candidate
     * need one shared decision. Distinct later candidates see the cleared live
     * bit; never spread this bypass over an entire candidate loop. */
    out.bypass_facing=!!(current_flags&0x20000u);
    out.flags=current_flags&~0x20000u;
    *result=out;return RF_OK;
}
#endif
