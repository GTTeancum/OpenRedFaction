#ifndef RF_SCENE_AI_RELOAD_COMPLETION_INCLUDED
#define RF_SCENE_AI_RELOAD_COMPLETION_INCLUDED
/* Include after ordinary/extra weapon selectors and before the owner tick.
 * Original41e7ef calls419bc0 from ordinary entity update, before any AI target
 * requirement;419cc1 and419d27 retain living/same-reload-weapon gates.
 * This bounded adapter only finishes the port's existing magazine reload.
 * Completion belongs to the ordinary inventory even while seat/order/AI
 * suppression prevents firing. It neither admits nor reloads mounted weapons.
 * No new state: completed_weapon is a receipt for this owner iteration only. */
static int campaign_enemy_reload_service(campaign_npc_body *owner,uint32_t frame,
    int32_t *completed_weapon)
{
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,campaign_shotgun_id,
        campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    campaign_enemy_weapon_selection selected={0};uint32_t completed;int32_t weapon;int status;
    if(!owner || !completed_weapon)return RF_RANGE;
    *completed_weapon=-1;weapon=owner->view.weapons[0];
    if(!owner->combat_reload_due || frame<owner->combat_reload_due ||
       owner->combat_reload_weapon!=weapon || owner->registration.view!=&owner->view ||
       rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)!=&owner->view ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       weapon<0 || weapon>=64 || !owner->inventory.owned[weapon])return RF_OK;
    if(!campaign_weapon_supply.names.count || campaign_weapon_supply.names.count>64 ||
       (uint32_t)weapon>=campaign_weapon_supply.names.count)return RF_RANGE;
    status=campaign_enemy_weapon_select(weapon,ids,campaign_primary,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,&selected);
    if(status==RF_NOT_FOUND)status=campaign_enemy_extra_weapon_select(weapon,&campaign_weapon_supply,&selected);
    /* Rocket uses the same finite magazine gate, but has separate projectile
     * admission in the ordinary scheduler. Preserve that existing boundary. */
    if(status==RF_NOT_FOUND && weapon==campaign_rocket_id && scene_rocket_resources) {
        selected.primary=campaign_primary+4;selected.ammo=campaign_weapon_supply.definitions+weapon;
        status=RF_OK;
    }
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(!selected.primary || !selected.ammo || selected.melee || selected.reserve_fed)return RF_OK;
    if(selected.ammo->capacity<0 || selected.ammo->magazine<=0 ||
       selected.primary->magazine!=(uint32_t)selected.ammo->magazine)return RF_RANGE;
    status=campaign_enemy_reload_complete(&owner->inventory,selected.ammo,selected.primary,weapon,frame,
        &owner->combat_reload_due,&owner->combat_reload_weapon,&completed);if(status)return status;
    if(completed)*completed_weapon=weapon;
    return RF_OK;
}
#endif
