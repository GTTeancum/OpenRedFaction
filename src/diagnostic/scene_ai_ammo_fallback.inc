#ifndef RF_SCENE_AI_AMMO_FALLBACK_INCLUDED
#define RF_SCENE_AI_AMMO_FALLBACK_INCLUDED
/* Port policy, called only on exhausted-ammo event3. Explicit Attack orders
 * retain their chosen gun; ordinary/retaliatory combat may switch. Prefer an
 * owned usable ranged gun, then owned riot melee. No grants or reload transfer. */
static int32_t campaign_enemy_fallback_pick(const rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *ammo,uint32_t count,const int32_t ids[8],
    int32_t current,uint32_t scripted)
{
    static const uint32_t order[6]={1,3,0,6,7,2};uint32_t i;int32_t weapon,type;
    if(!inventory || !ammo || !ids || scripted==1)return -1;
    for(i=0;i<6;i++) {
        weapon=ids[order[i]];
        if(weapon<0 || weapon>=64 || (uint32_t)weapon>=count || weapon==current || !inventory->owned[weapon])continue;
        if(order[i]==2)return weapon; /* Riot primary is melee; alternate charge is separate. */
        type=ammo[weapon].ammo_type;
        if(type<0 || type>=32 || ammo[weapon].magazine<=0)continue;
        if(inventory->loaded[weapon]>0 || inventory->reserve[type]>0)return weapon;
    }
    return -1;
}
static int campaign_enemy_ammo_fallback(uint32_t slot,uint32_t *changed)
{
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,campaign_shotgun_id,
        campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    campaign_npc_body *owner;rf_entity_motion_selection selection;int32_t replacement;int status;
    if(!changed || slot>=campaign_npc_body_count || !campaign_npc_bodies)return RF_RANGE;
    *changed=0;owner=campaign_npc_bodies+slot;
    replacement=campaign_enemy_fallback_pick(&owner->inventory,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,ids,owner->view.weapons[0],owner->combat_scripted);
    if(replacement<0)return RF_OK;
    if(!campaign_seeds.items || slot>=campaign_seeds.records.count)return RF_NOT_FOUND;
    selection=owner->selection;
    status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,
        campaign_seeds.items[slot].class_index,replacement,&selection);if(status)return status;
    /* Same retained reset as pain handling. Unsupported live effects prevent
     * switching rather than silently leaving stale sound/effect owners. */
    status=campaign_pain_reset_retained(NULL,owner->registration.handle,owner->view.weapons[0]);if(status)return status;
    owner->selection=selection;owner->view.weapons[0]=replacement;
    owner->combat_burst_remaining=owner->combat_due=owner->combat_reload_due=0;
    owner->combat_reload_weapon=-1;owner->combat_navigation_due=0;
    *changed=1;return RF_OK;
}
#endif
