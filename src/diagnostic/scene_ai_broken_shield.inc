#ifndef RF_SCENE_AI_BROKEN_SHIELD_INCLUDED
#define RF_SCENE_AI_BROKEN_SHIELD_INCLUDED
/* Called only after physical shield break retires its inventory ownership.
 * Unlike ordinary exhaustion, destruction invalidates the explicit Attack's
 * chosen weapon. Keep its target/order while choosing a real owned fallback.
 * Existing fallback/reset is synchronous and owns all selection/publication;
 * restore the order flag even when that operation returns an error. */
static int campaign_enemy_broken_shield_fallback(uint32_t slot,int32_t shield_weapon,uint32_t *changed)
{
    campaign_npc_body *owner;uint32_t scripted;int status;
    if(!changed || !campaign_npc_bodies || slot>=campaign_npc_body_count ||
       shield_weapon<0 || shield_weapon>=64)return RF_RANGE;
    owner=campaign_npc_bodies+slot;*changed=0;
    if(owner->view.weapons[0]!=shield_weapon || owner->inventory.owned[shield_weapon])return RF_OK;
    scripted=owner->combat_scripted;
    /* Only bypass the choice veto; no target/alert/affiliation changes. */
    owner->combat_scripted=0;
    status=campaign_enemy_ammo_fallback(slot,changed);
    owner->combat_scripted=scripted;
    return status;
}
#endif
