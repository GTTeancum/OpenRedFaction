#ifndef RF_SCENE_AI_PROJECTILE_DEMAND_INC
#define RF_SCENE_AI_PROJECTILE_DEMAND_INC
/* Include after scene_precision_drop_demand.inc and the campaign scene globals.
 * Borrow the current constructed NPC loadout before campaign_actors_restore
 * binds persistent slots. Bits4/5 mean shared rocket/grenade resources, NOT
 * player ownership or first-person-view demand. No allocation or mutation. */
static uint32_t scene_ai_projectile_resource_mask(void)
{
    int32_t rocket,grenade;uint32_t i,mask=0;
    if(!campaign_npc_bodies || campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS ||
       !campaign_weapon_supply.names.count || campaign_weapon_supply.names.count>64 ||
       rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS ||
       rf_scene_defeated_actors.level_count>RF_CAMPAIGN_PICKUP_LEVELS)return 0;
    rocket=rf_weapon_name_find(&campaign_weapon_supply.names,"Rocket Launcher");
    grenade=rf_weapon_name_find(&campaign_weapon_supply.names,"Grenade");
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *owner=campaign_npc_bodies+i;
        int32_t held=owner->view.weapons[0];uint32_t slot;
        if(!owner->registration.view || (owner->object_flags&2u) || (owner->view.flags_810&1u) ||
           owner->damage.effects.health<=0 || held<0 || (uint32_t)held>=campaign_weapon_supply.names.count ||
           !owner->inventory.owned[held])continue;
        if(owner->persistence_registered && owner->persistence_slot>=rf_scene_defeated_actors.count)continue;
        if(scene_precision_owner_persistence_slot(owner,&slot) && rf_scene_defeated_actors.items[slot].retired)continue;
        /* Hidden live actors can be revealed by events. Empty ammunition is
         * gameplay state, not proof the authored weapon will never be used. */
        if(rocket>=0 && held==rocket)mask|=1u<<4;
        if(grenade>=0 && held==grenade)mask|=1u<<5;
    }
    return mask;
}
#endif
