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
typedef struct scene_ai_reeper_claw_impact_definition {
    uint32_t primary_count;int32_t primary_ms[2];
} scene_ai_reeper_claw_impact_definition;
typedef struct scene_ai_reeper_claw_catalog {
    rf_weapon_primary_definition primary[SCENE_AI_REEPER_CLAWS];
    scene_ai_reeper_claw_impact_definition impacts[SCENE_AI_REEPER_CLAWS];
    int32_t ids[SCENE_AI_REEPER_CLAWS];uint32_t ready;
} scene_ai_reeper_claw_catalog;
static scene_ai_reeper_claw_catalog scene_ai_reeper_claws;

static int campaign_enemy_reeper_claws_open(rf_vpp *tables,
    const rf_weapon_supply_catalog *supply,const rf_weapon_reset_catalog *reset)
{
    static const struct {
        float damage,fire_seconds,range,seconds[2];
        uint32_t count;int32_t milliseconds[2];
    } authored[SCENE_AI_REEPER_CLAWS]={
        {40.f,2.f,3.8f,{.4f,.8f},2,{400,800}},
        {10.f,2.f,2.5f,{.3f,.6f},2,{300,600}},
        {40.f,1.5f,2.5f,{.15f,.5f},2,{150,500}},
        {40.f,1.5f,2.5f,{.15f,0.f},1,{150,0}}};
    scene_ai_reeper_claw_catalog next={0};uint32_t i,j;int status;
    if(!tables || !supply || !reset || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count ||
       reset->names.count!=supply->names.count ||
       reset->names.primary_count!=supply->names.primary_count)return RF_RANGE;
    for(i=0;i<SCENE_AI_REEPER_CLAWS;i++) {
        const rf_weapon_descriptor *flags;rf_weapon_impact_delays delays;
        rf_weapon_primary_definition *primary=next.primary+i;
        next.ids[i]=rf_weapon_name_find(&supply->names,scene_ai_reeper_claw_names[i]);
        if(next.ids[i]<0)return RF_NOT_FOUND;
        if((uint32_t)next.ids[i]>=supply->names.primary_count ||
           rf_weapon_name_find(&reset->names,scene_ai_reeper_claw_names[i])!=next.ids[i])return RF_FORMAT;
        flags=reset->definitions+next.ids[i];
        /* Installed flags: melee/underwater/from_eye and no_fire_through.
         * Do not reinterpret a changed continuous/projectile definition. */
        if(flags->flags_264!=0x620u || flags->flags_268!=8u ||
           supply->definitions[next.ids[i]].magazine!=0)return RF_FORMAT;
        status=rf_weapon_primary_load(tables,scene_ai_reeper_claw_names[i],128*1024,primary);
        if(status)return status;
        status=rf_weapon_impact_delays_load(tables,scene_ai_reeper_claw_names[i],128*1024,&delays);
        if(status)return status;
        if(primary->magazine!=0 || primary->reload_seconds!=0 ||
           primary->projectiles!=1 || primary->damage_kind!=0 ||
           primary->burst_count!=1 || primary->burst_seconds!=0 ||
           primary->damage!=authored[i].damage || primary->fire_seconds!=authored[i].fire_seconds ||
           primary->ai_attack_range!=authored[i].range || primary->ai_damage_scale[0]!=1.f ||
           primary->piercing_enabled || delays.primary_count!=authored[i].count ||
           delays.alt_count!=0)return RF_FORMAT;
        next.impacts[i].primary_count=delays.primary_count;
        for(j=0;j<2;j++) {
            if(delays.primary_seconds[j]!=authored[i].seconds[j])return RF_FORMAT;
            next.impacts[i].primary_ms[j]=(int32_t)(delays.primary_seconds[j]*1000.f+.5f);
            if(next.impacts[i].primary_ms[j]!=authored[i].milliseconds[j])return RF_FORMAT;
        }
    }
    /* Stage the whole finite catalog: failed admission preserves all existing
     * metadata. Original42607d..4260c6 schedules each authored impact through
     * 4fa360 from current game time, not from the preceding impact deadline. */
    next.ready=1;scene_ai_reeper_claws=next;return RF_OK;
}

/* Read-only startup metadata for the shared delayed owner. Milliseconds are
 * independent offsets from the same accepted attack time; unused slots are
 * zero. This helper does not grant ownership or authorize an attack. */
static const scene_ai_reeper_claw_impact_definition *campaign_enemy_reeper_claw_impacts(int32_t weapon)
{
    uint32_t i;if(!scene_ai_reeper_claws.ready)return NULL;
    for(i=0;i<SCENE_AI_REEPER_CLAWS;i++)
        if(scene_ai_reeper_claws.ids[i]==weapon)return scene_ai_reeper_claws.impacts+i;
    return NULL;
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
    /* Shared melee retains cadence/range/aim; the delayed owner consumes the
     * impact metadata above for intended-body contact, cover and damage/death.
     * No firearm readiness/debit: an empty nominal12mm reserve cannot suppress. */
    selected.primary=scene_ai_reeper_claws.primary+i;selected.melee=1;
    selected.ammo=NULL;selected.penetrates_world=0;selected.reserve_fed=0;
    selected.slot=UINT32_MAX;*out=selected;return RF_OK;
}
#endif
