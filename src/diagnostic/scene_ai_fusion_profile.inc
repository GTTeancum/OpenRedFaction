#ifndef RF_SCENE_AI_FUSION_PROFILE_INCLUDED
#define RF_SCENE_AI_FUSION_PROFILE_INCLUDED
/* Installed shoulder_cannon primary. Source damage is1000 * SP AI scale.16;
 * the player retains its separate1000-damage consumer. Only the shared Fusion
 * resources are required, never a first-person view or a player grant. */
static int campaign_enemy_fusion_definition(int32_t weapon,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected={0};const rf_weapon_primary_definition *p;
    const rf_weapon_acquire_definition *ammo;
    if(!supply || !out || supply->names.count>64)return RF_RANGE;
    if(!scene_fusion_resources || campaign_fusion_id<0 || weapon!=campaign_fusion_id)return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.count)return RF_RANGE;
    p=campaign_primary+12;ammo=supply->definitions+weapon;
    if(ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine!=1 || ammo->capacity!=5 ||
       p->magazine!=1 || p->fire_seconds!=.8f || p->reload_seconds!=1.f ||
       p->damage!=1000.f || p->damage_kind!=3 || p->ai_attack_range!=30.f ||
       p->ai_damage_scale[0]!=.16f || p->projectiles!=1 || p->burst_count!=1 ||
       p->burst_seconds!=0 || p->piercing_enabled ||
       campaign_fusion.speed!=20.f || campaign_fusion.lifetime!=15.f ||
       campaign_fusion.collision_radius!=.051f || campaign_fusion.damage_radius!=25.f ||
       campaign_fusion.crater_radius!=7.f || campaign_fusion.impact_count!=1 ||
       campaign_fusion.impact_radius[0]!=9.f)return RF_FORMAT;
    selected.primary=p;selected.ammo=ammo;selected.slot=12;*out=selected;return RF_OK;
}
/* Full live registration is checked independently of descriptor-only saved
 * selection qualification. No candidate ever borrows the current held gun. */
static int campaign_enemy_fusion_select(const campaign_npc_body *owner,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected;uint32_t handle;int32_t weapon;int status;
    if(!owner || !supply || !out)return RF_RANGE;
    weapon=owner->view.weapons[0];
    status=campaign_enemy_fusion_definition(weapon,supply,&selected);if(status)return status;
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
