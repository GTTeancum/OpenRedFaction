#ifndef RF_SCENE_AI_TARGET_LIVENESS_INCLUDED
#define RF_SCENE_AI_TARGET_LIVENESS_INCLUDED
/* First-pass combat ownership policy: player death stops player-directed
 * combat pursuit, not independent scripted/retaliatory NPC-vs-NPC orders.
 * Call before weapon selection in enemy tick; remove the loop-wide player
 * health guard. Repeat after scripted target resolution, which can release
 * a finished NPC order back to ordinary player awareness. Death must also
 * prevent unalerted guards acquiring the corpse.
 * Authored Goto/animation and reload/ammunition state remain separate owners. */
static uint32_t campaign_enemy_target_living(campaign_npc_body *owner,
    uint32_t player_handle,float player_health)
{
    if(player_health>0)return 1;
    if(owner->combat_scripted && owner->combat_target!=player_handle)return 1;
    campaign_pursuit_stop(owner);
    owner->combat_alert=owner->combat_scripted=owner->combat_target=0;
    owner->combat_burst_remaining=owner->combat_due=owner->combat_navigation_due=0;
    return 0;
}
#endif
