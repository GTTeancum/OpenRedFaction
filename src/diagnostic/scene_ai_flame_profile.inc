#ifndef RF_SCENE_AI_FLAME_PROFILE_INCLUDED
#define RF_SCENE_AI_FLAME_PROFILE_INCLUDED
/* Real authored Flamethrower primaries use the existing shared flame volume,
 * never generic hitscan. The first-pass NPC scheduler batches finite fuel at
 * each authored0.1-second pulse:100 units /2 seconds =5 units per pulse.
 * This intentionally has no fractional/ignition owner or new disk grammar.
 * Player continuous-charge scheduling is independent and unchanged. */
/* Descriptor-only form also supports candidate save validation without
 * consulting the current actor's weapon or mutating live ownership. */
static int campaign_enemy_flame_definition(int32_t weapon,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected={0};const rf_weapon_primary_definition *p;
    const rf_weapon_acquire_definition *ammo;
    if(!supply || !out || supply->names.count>64)return RF_RANGE;
    if(!scene_ai_flame_resources || campaign_flame_id<0 || weapon!=campaign_flame_id)return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.count)return RF_RANGE;
    p=campaign_primary+10;ammo=supply->definitions+weapon;
    /* Refuse changed assets rather than interpreting a different continuous
     * drain profile as this deliberately integer, stateless approximation. */
    if(ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine!=100 ||
       p->magazine!=100 || p->drain_seconds!=2.f || p->fire_seconds!=.1f ||
       p->reload_seconds!=2.6f || p->damage!=20.f || p->damage_kind!=4 ||
       p->ai_attack_range!=5.f || p->ai_damage_scale[0]!=1.f || p->projectiles!=1 ||
       p->burst_count!=1 || p->burst_seconds!=0 || p->piercing_enabled)return RF_FORMAT;
    selected.primary=p;selected.ammo=ammo;selected.slot=10;*out=selected;return RF_OK;
}
static int campaign_enemy_flame_select(const campaign_npc_body *owner,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected;uint32_t handle;int32_t weapon;int status;
    if(!owner || !supply || !out)return RF_RANGE;
    weapon=owner->view.weapons[0];
    status=campaign_enemy_flame_definition(weapon,supply,&selected);if(status)return status;
    handle=owner->registration.handle;
    if(!handle || handle==UINT32_MAX || !owner->inventory.owned[weapon] ||
       owner->registration.view!=&owner->view || !owner->body.allocated_bytes ||
       (uint32_t)owner->view.handle!=handle || owner->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0)return RF_NOT_FOUND;
    *out=selected;return RF_OK;
}
#endif
