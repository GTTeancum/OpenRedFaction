#ifndef RF_SCENE_NPC_RETIREMENT_ORDERS_INCLUDED
#define RF_SCENE_NPC_RETIREMENT_ORDERS_INCLUDED
/* Original Remove_Object2 (4b9a10 -> 48ab40) marks removal, not damage/death.
 * Our terminal NPC retains its authored/persistence storage after unregister.
 * Reuse corpse expiry's cancellation for those retained, non-executable banks.
 * Include after campaign_npc_body; caller has successfully unregistered the
 * actor and admitted script_once.count<=16 before invoking this helper. */
static void campaign_script_single_pop(campaign_npc_body *owner,uint32_t fired);
static void scene_npc_retirement_cancel_orders(campaign_npc_body *owner)
{
    /* Queue pop preserves pending-request accounting without firing a shot.
     * The path/route/animation references borrow shared level resources. */
    while(owner->script_once.count)campaign_script_single_pop(owner,0);
    memset(&owner->script_move,0,sizeof(owner->script_move));
    memset(&owner->script_look,0,sizeof(owner->script_look));
    memset(&owner->script_shoot,0,sizeof(owner->script_shoot));
    memset(&owner->script_animation,0,sizeof(owner->script_animation));
    owner->navigation.retained.count=0;owner->controller_handle=0;
    owner->combat_alert=owner->combat_due=owner->combat_burst_remaining=0;
    owner->combat_reload_due=0;owner->combat_reload_weapon=-1;
    owner->combat_scripted=owner->combat_target=owner->combat_navigation_due=0;
    owner->ai_mode.action_280=owner->view.action_520=1;
    /* Do not erase firing/audio/effect tokens, inventory, damage attribution,
     * physics or owned pose storage. Their existing owners retain teardown. */
}
#endif
