#ifndef RF_SCENE_AI_REEPER_CLAW_INCLUDED
#define RF_SCENE_AI_REEPER_CLAW_INCLUDED
/* Shared finite creature-melee registry; retain the existing Reeper hook names.
 * Admit only the two Reeper claws and the two exact spaced Mutant Attack names.
 * Original425b9a bypasses ammo
 * availability for melee;426000..426196 returns from noncontinuous melee
 * without the ordinary4257c0 debit. The nominal12mm supply remains intact.
 * Include after scene_ai_weapon_selection.inc. No per-shot reads, inventory
 * grants, fallback candidates, player slots or persistent gameplay state. */
#define SCENE_AI_REEPER_CLAWS 4u
static const char *const scene_ai_reeper_claw_names[SCENE_AI_REEPER_CLAWS]={
    "Reeper Claw","Baby Reeper Claw","Mutant Attack 1","Mutant Attack 2"};
static struct {
    rf_weapon_primary_definition primary[SCENE_AI_REEPER_CLAWS];
    int32_t ids[SCENE_AI_REEPER_CLAWS];uint32_t ready;
} scene_ai_reeper_claws;

static int campaign_enemy_reeper_claws_open(rf_vpp *tables,
    const rf_weapon_supply_catalog *supply,const rf_weapon_reset_catalog *reset)
{
    rf_weapon_primary_definition primary[SCENE_AI_REEPER_CLAWS];
    int32_t ids[SCENE_AI_REEPER_CLAWS];uint32_t i;int status;
    memset(&scene_ai_reeper_claws,0,sizeof(scene_ai_reeper_claws));
    for(i=0;i<SCENE_AI_REEPER_CLAWS;i++)scene_ai_reeper_claws.ids[i]=-1;
    if(!tables || !supply || !reset || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count ||
       reset->names.count!=supply->names.count ||
       reset->names.primary_count!=supply->names.primary_count)return RF_RANGE;
    for(i=0;i<SCENE_AI_REEPER_CLAWS;i++) {
        const rf_weapon_descriptor *flags;
        ids[i]=rf_weapon_name_find(&supply->names,scene_ai_reeper_claw_names[i]);
        if(ids[i]<0)return RF_NOT_FOUND;
        if((uint32_t)ids[i]>=supply->names.primary_count ||
           rf_weapon_name_find(&reset->names,scene_ai_reeper_claw_names[i])!=ids[i])return RF_FORMAT;
        flags=reset->definitions+ids[i];
        /* Installed flags: melee/underwater/from_eye and no_fire_through.
         * Do not reinterpret a changed continuous/projectile definition. */
        if(flags->flags_264!=0x620u || flags->flags_268!=8u ||
           supply->definitions[ids[i]].magazine!=0)return RF_FORMAT;
        status=rf_weapon_primary_load(tables,scene_ai_reeper_claw_names[i],128*1024,primary+i);
        if(status)return status;
        if(primary[i].magazine!=0 || primary[i].reload_seconds!=0 ||
           primary[i].projectiles!=1 || primary[i].damage_kind!=0 ||
           primary[i].burst_count!=1 || primary[i].burst_seconds!=0 ||
           !(primary[i].ai_attack_range>0))return RF_FORMAT;
    }
    memcpy(scene_ai_reeper_claws.primary,primary,sizeof(primary));
    memcpy(scene_ai_reeper_claws.ids,ids,sizeof(ids));
    scene_ai_reeper_claws.ready=1;return RF_OK;
}

static int campaign_enemy_reeper_claw_select(const campaign_npc_body *owner,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected={0};uint32_t i,handle;int32_t weapon;
    if(!owner || !supply || !out || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count)return RF_RANGE;
    if(!scene_ai_reeper_claws.ready)return RF_NOT_FOUND;
    weapon=owner->view.weapons[0];
    for(i=0;i<SCENE_AI_REEPER_CLAWS;i++)if(scene_ai_reeper_claws.ids[i]==weapon)break;
    if(i==SCENE_AI_REEPER_CLAWS)return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.primary_count)return RF_RANGE;
    handle=owner->registration.handle;
    if(!owner->inventory.owned[weapon] || !handle || handle==UINT32_MAX ||
       owner->registration.view!=&owner->view || !owner->body.allocated_bytes ||
       (uint32_t)owner->view.handle!=handle || owner->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0)return RF_NOT_FOUND;
    /* Shared first-playable riot melee owns cadence/range/aim, intended-body
     * contact, world/rubble cover and damage/death. No firearm readiness or
     * debit: even an empty nominal12mm reserve cannot suppress these attacks. */
    selected.primary=scene_ai_reeper_claws.primary+i;selected.melee=1;
    selected.ammo=NULL;selected.penetrates_world=0;selected.reserve_fed=0;
    selected.slot=UINT32_MAX;*out=selected;return RF_OK;
}
#endif
