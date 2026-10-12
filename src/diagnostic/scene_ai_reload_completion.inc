#ifndef RF_SCENE_AI_RELOAD_COMPLETION_INCLUDED
#define RF_SCENE_AI_RELOAD_COMPLETION_INCLUDED
/* Include after ordinary/extra weapon selectors and before the owner tick.
 * Original41e7ef calls419bc0 from ordinary entity update, before any AI target
 * requirement;419cc1 and419d27 retain living/same-reload-weapon gates.
 * This bounded adapter only finishes the port's existing magazine reload.
 * Completion belongs to the ordinary inventory even while seat/order/AI
 * suppression prevents firing. It neither admits nor reloads mounted weapons.
 * No new state: completed_weapon is a receipt for this owner iteration only. */
static int campaign_enemy_reload_legacy_service(campaign_npc_body *owner,uint32_t frame,
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
    if(status==RF_NOT_FOUND)status=campaign_enemy_flame_select(owner,&campaign_weapon_supply,&selected);
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
/* The bounded new owner is independent of target/mode/awareness. Legacy
 * positive timers are serviced first and never retimed or adopted. Due0 is
 * represented by pending1; temporarily suppressed completion retains it. */
static int campaign_enemy_reload_service(campaign_npc_body *owner,uint32_t frame,
    int32_t *completed_weapon,uint32_t *qualified,uint32_t initiate)
{
    uint32_t slot,completed=0,deadline,ready;int32_t weapon;int status;
    const rf_weapon_acquire_definition *ammo;
    if(!owner||!completed_weapon||!qualified||!campaign_npc_bodies||
       owner<campaign_npc_bodies||owner>=campaign_npc_bodies+campaign_npc_body_count)return RF_RANGE;
    *completed_weapon=-1;*qualified=0;slot=(uint32_t)(owner-campaign_npc_bodies);
    if(owner->combat_reload_pending>1)return RF_FORMAT;
    weapon=owner->view.weapons[0];*qualified=scene_npc_reload_named(slot,weapon);
    if(owner->combat_reload_pending){
        /* Resolve the same full registration before cancellation or transfer.
         * Known death/mismatch ends this episode before its future-due test. */
        status=scene_npc_reload_identity(slot,owner);if(status)return status;
        if(!isfinite(owner->damage.effects.health))return RF_FORMAT;
        if(owner->damage.effects.health<=0||(owner->view.flags_810&1u)||(owner->object_flags&2u)||
           owner->combat_reload_weapon!=weapon){
            owner->combat_reload_pending=owner->combat_reload_due=0;owner->combat_reload_weapon=-1;
            return RF_OK;
        }
        if(!*qualified)return RF_FORMAT;
        if(owner->object_flags&0x4000u)return RF_OK;
        if(frame<owner->combat_reload_due)return RF_OK;
        /* Completion owns current finite inventory, not the model clock or
         * residency of a presentation that may already have ended. Preserve
         * ordinary seat/order suppression behavior: neither starts a mounted
         * reload nor requires a target merely to finish a paid episode. */
        if(!campaign_weapon_supply.names.count||campaign_weapon_supply.names.count>64||
           (uint32_t)weapon>=campaign_weapon_supply.names.count)return RF_RANGE;
        ammo=campaign_weapon_supply.definitions+weapon;
        if(ammo->capacity<0||ammo->magazine<=0||campaign_primary[0].magazine!=(uint32_t)ammo->magazine)return RF_RANGE;
        status=campaign_enemy_reload_complete_present(&owner->inventory,ammo,campaign_primary,weapon,frame,1,
            &owner->combat_reload_due,&owner->combat_reload_weapon,&completed);if(status)return status;
        if(completed){owner->combat_reload_pending=0;*completed_weapon=weapon;}
        return RF_OK;
    }
    /* An admitted guard cannot own legacy pending0/due>0. Keep every other
     * actor's legacy service, and preserve the completion receipt above. */
    if(!initiate)return owner->combat_reload_due?RF_FORMAT:RF_OK;
    status=campaign_enemy_reload_legacy_service(owner,frame,completed_weapon);if(status)return status;
    if(*completed_weapon>=0||owner->combat_reload_due||!*qualified)return RF_OK;
    /* Persistent retired rows keep class/loadout scalars but no registration.
     * They are not candidate starters and must never be adopted or revived. */
    if(!owner->registration.view||!owner->body.allocated_bytes||
       (owner->object_flags&(2u|0x4000u))||(owner->view.flags_810&1u))return RF_OK;
    status=scene_npc_reload_identity(slot,owner);if(status)return status;
    if(!isfinite(owner->damage.effects.health))return RF_FORMAT;
    if(owner->damage.effects.health<=0||(owner->object_flags&(2u|0x4000u))||
       (owner->view.flags_810&1u)||!scene_npc_reload_standalone(owner))return RF_OK;
    if((uint32_t)weapon>=campaign_weapon_supply.names.count)return RF_FORMAT;
    ammo=campaign_weapon_supply.definitions+weapon;
    status=campaign_enemy_ammo_validate(&owner->inventory,ammo,campaign_primary,weapon);if(status)return status;
    if(owner->inventory.loaded[weapon]||!owner->inventory.reserve[ammo->ammo_type])return RF_OK;
    status=scene_npc_reload_start_preflight(slot,frame,&deadline,&ready);
    if(status)return status;if(!ready)return RF_OK;
    owner->combat_reload_pending=1;owner->combat_reload_weapon=weapon;owner->combat_reload_due=deadline;
    owner->combat_burst_remaining=0;
    /* The fixed Xbox/no-device Foley adapters are non-invalidating. Optional
     * sound failure is nonfatal. Mandatory postacceptance failure retains the
     * episode, so no subsequent service repeats the start presentation. */
    status=campaign_enemy_reload_presentation(slot);if(status)return status;
    if(rf_scene_combat_trace)printf("ENEMY_RELOAD_TRACE %u %u 1 %d\n",frame,
        (uint32_t)campaign_seeds.records.items[slot].record.uid,owner->inventory.loaded[weapon]);
    return RF_OK;
}
#endif
