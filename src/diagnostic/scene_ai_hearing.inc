#ifndef RF_SCENE_AI_HEARING_INCLUDED
#define RF_SCENE_AI_HEARING_INCLUDED
/* Port first-pass gunfire awareness. The caller emits one event per real
 * player firearm shot, outside pellet loops; melee/dry clicks do not call it.
 * Hearing is omnidirectional and crosses walls within a supplied bounded
 * radius. Acoustic portals, attenuation and original AI noise rules are not
 * reconstructed. Existing combat owns turning, pursuit, LOS and firing.
 * No per-frame scan, allocation, target override or repeated alert delay. */
static int campaign_enemy_hear_shot(const float position[3],float radius,uint32_t frame,
    uint32_t *alerted)
{
    uint32_t i,j,count=0;float radius_squared;int status;
    if(!position || !alerted || !isfinite(radius) || radius<=0 || radius>100 ||
       frame>UINT32_MAX-30)return RF_RANGE;
    for(j=0;j<3;j++)if(!isfinite(position[j]))return RF_RANGE;
    radius_squared=radius*radius;
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *owner=campaign_npc_bodies+i;float distance=0;
        if(!owner->registration.view || owner->damage.effects.health<=0 ||
           (owner->object_flags&(2|0x4000)) || (owner->view.flags_810&1) ||
           owner->view.weapons[0]<0 || owner->damage.effects.affiliation!=0 ||
           owner->combat_alert || owner->combat_scripted || !campaign_enemy_mode_admits(owner,0,1))continue;
        for(j=0;j<3;j++){float d=owner->eye_position[j]-position[j];distance+=d*d;}
        if(!isfinite(distance) || distance>radius_squared)continue;
        status=campaign_animation_cancel(owner);if(status)return status;
        campaign_pursuit_stop(owner);owner->script_move.active=0;owner->script_move.stop=1;
        owner->combat_target=campaign_player_object.handle;owner->combat_alert=1;
        owner->combat_navigation_due=0;owner->combat_burst_remaining=0;owner->combat_due=frame+30;
        ++count;
    }
    *alerted=count;return RF_OK;
}
#endif
