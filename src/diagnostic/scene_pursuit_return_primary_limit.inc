#ifndef RF_SCENE_PURSUIT_RETURN_PRIMARY_LIMIT_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_PRIMARY_LIMIT_INCLUDED
/* Uncalled scalar binding for the five ordinary loaded primary slots only.
 * 401cc0 reads active weapon-row514: parser50c/510 and default4c2a20 bind
 * this supplied asset set's equal AI Attack Range columns to ai_attack_range.
 * 464563/46457c ->464c36/464c3c bind immutable placed override528/52c.
 * Caller owns successful catalog initialization and stable section/records.
 * This bounded scope assumes no admitted runtime override replacement.
 * Perform this admission BEFORE queuing/entering the return prelude, even
 * when an override is enabled. A refusal is never a completed route miss.
 * No queries, callbacks, allocation, timers or actor/output writes on error.
 */
static int scene_pursuit_return_primary_limit(uint32_t slot,float *result)
{
    const campaign_npc_body *o;campaign_enemy_weapon_selection selected={0};
    rf_level_entity_combat_range authored;uint32_t target,melee;float value;int status;
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,
        campaign_shotgun_id,campaign_rocket_id,campaign_grenade_id,
        campaign_sniper_id,campaign_rail_id};
    if(!result)return RF_RANGE;
    status=scene_pursuit_return_owner(slot,&target,&melee);if(status)return status;
    o=campaign_npc_bodies+slot;
    /* Original IDs<=0 select .5 absent an override. They are outside this
     * five-positive-ID live cohort; do not remap ID0 or grant it admission. */
    if(o->view.weapons[0]<=0)return RF_NOT_FOUND;
    status=campaign_enemy_weapon_select(o->view.weapons[0],ids,campaign_primary,
        campaign_weapon_supply.definitions,campaign_weapon_supply.names.count,&selected);
    if(status)return status;
    if(selected.melee||!selected.primary||
       (selected.slot!=0&&selected.slot!=1&&selected.slot!=3&&
        selected.slot!=6&&selected.slot!=7))return RF_NOT_FOUND;
    if(!campaign_seeds.records.items||slot>=campaign_seeds.records.count)return RF_FORMAT;
    status=rf_level_entity_combat_range_read(campaign_seeds.records.items+slot,&authored);
    if(status)return status;
    /* Source checks the low byte EXACTLY1. Other authored values do not
     * grant the override, even though the accessor parsed their float. */
    value=authored.enabled==1?authored.range:selected.primary->ai_attack_range;
    if(!isfinite(value)||value<=0||value>1000000.f)return RF_FORMAT;
    *result=value;return RF_OK;
}
#endif
