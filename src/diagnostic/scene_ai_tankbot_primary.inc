#ifndef RF_SCENE_AI_TANKBOT_PRIMARY_INCLUDED
#define RF_SCENE_AI_TANKBOT_PRIMARY_INCLUDED
/* Exact authored NPC primary admission, not a generic clipless-gun fallback.
 * Original Tankbot Chaingun has continuous_fire, no clip/reload pair and a
 * finite turret_ammo reserve. Startup/default and placed loadout already own
 * and supply it. Keep that inventory and the separate Missile/Smash intact.
 * Include after scene_ai_weapon_selection.inc. No runtime asset reads. */
static struct {
    rf_weapon_primary_definition primary;
    int32_t weapon;uint32_t ready;
} scene_ai_tankbot_primary;

static int campaign_enemy_tankbot_primary_open(rf_vpp *tables,
    const rf_weapon_supply_catalog *supply)
{
    rf_weapon_primary_definition primary;const rf_weapon_acquire_definition *ammo;
    int32_t weapon;int status;
    memset(&scene_ai_tankbot_primary,0,sizeof(scene_ai_tankbot_primary));
    scene_ai_tankbot_primary.weapon=-1;
    if(!tables || !supply || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count)return RF_RANGE;
    weapon=rf_weapon_name_find(&supply->names,"Tankbot Chaingun");
    if(weapon<0)return RF_NOT_FOUND;
    if((uint32_t)weapon>=supply->names.primary_count)return RF_FORMAT;
    ammo=supply->definitions+weapon;
    if(ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine!=0 ||
       ammo->capacity<=0)return RF_FORMAT;
    status=rf_weapon_primary_load(tables,"Tankbot Chaingun",128*1024,&primary);
    if(status)return status;
    if(primary.magazine!=0 || primary.reload_seconds!=0 ||
       primary.projectiles!=1 || primary.damage_kind!=1)return RF_FORMAT;
    scene_ai_tankbot_primary.primary=primary;
    scene_ai_tankbot_primary.weapon=weapon;scene_ai_tankbot_primary.ready=1;
    return RF_OK;
}

static uint32_t campaign_enemy_tankbot_primary_weapon(int32_t weapon)
{return scene_ai_tankbot_primary.ready && weapon==scene_ai_tankbot_primary.weapon;}

static int campaign_enemy_tankbot_primary_select(int32_t weapon,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected={0};
    if(!supply || !out || supply->names.count>64)return RF_RANGE;
    if(!campaign_enemy_tankbot_primary_weapon(weapon))return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.count)return RF_RANGE;
    selected.primary=&scene_ai_tankbot_primary.primary;
    selected.ammo=supply->definitions+weapon;
    if(selected.ammo->ammo_type<0 || selected.ammo->ammo_type>=32 ||
       selected.ammo->magazine!=0 || selected.ammo->capacity<=0)return RF_RANGE;
    /* No player slot or world penetration. The accepted-shot consumer may
     * apply authored actor power; this selector owns only fire admission. */
    selected.slot=UINT32_MAX;selected.reserve_fed=1;*out=selected;return RF_OK;
}

/* Clipless primary readiness cancels any previous gun's magazine reload;
 * it never starts one or transfers ammo. Exhaustion uses existing event3
 * fallback policy. Accepted shots debit the mapped reserve through the
 * original rf_weapon_consume_shot in the ordinary NPC fire owner. */
static int campaign_enemy_tankbot_primary_ready(const rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *ammo,int32_t weapon,uint32_t *reload_due,
    int32_t *reload_weapon,uint32_t *ready,uint32_t *event)
{
    uint32_t available;
    if(!inventory || !ammo || !reload_due || !reload_weapon || !ready || !event ||
       !campaign_enemy_tankbot_primary_weapon(weapon) || weapon<0 || weapon>=64 ||
       !inventory->owned[weapon] || ammo->ammo_type<0 || ammo->ammo_type>=32 ||
       ammo->magazine!=0 || ammo->capacity<=0 || inventory->loaded[weapon]!=0 ||
       inventory->reserve[ammo->ammo_type]<0)return RF_RANGE;
    available=inventory->reserve[ammo->ammo_type]>0;
    *reload_due=0;*reload_weapon=-1;*ready=available;*event=available?0:3;
    return RF_OK;
}
#endif
