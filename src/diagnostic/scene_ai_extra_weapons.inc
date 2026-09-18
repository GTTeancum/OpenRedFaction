#ifndef RF_SCENE_AI_EXTRA_WEAPONS_INC
#define RF_SCENE_AI_EXTRA_WEAPONS_INC
/* Include after scene_ai_weapon_selection.inc. These four installed ordinary
 * firearms use existing direct-ray/spread/finite-ammo AI behavior; scoped AR's
 * armor-piercing DAMAGE TYPE does not imply rail-like world penetration.
 * No player slots/resources are added. Names resolve actual catalog IDs. */
#define SCENE_AI_EXTRA_WEAPONS 4u
static const char *scene_ai_extra_weapon_names[SCENE_AI_EXTRA_WEAPONS]={
    "Machine Pistol","heavy_machine_gun","scope_assault_rifle","Undercover 12mm handgun"};
static struct {
    rf_weapon_primary_definition primary[SCENE_AI_EXTRA_WEAPONS];
    int32_t ids[SCENE_AI_EXTRA_WEAPONS];uint32_t ready;
} scene_ai_extra_weapons;
static int campaign_enemy_extra_weapons_open(rf_vpp *tables,const rf_weapon_supply_catalog *supply)
{
    rf_weapon_primary_definition primary[SCENE_AI_EXTRA_WEAPONS];int32_t ids[SCENE_AI_EXTRA_WEAPONS];uint32_t i;int status;
    if(!tables || !supply || supply->names.count>64)return RF_RANGE;
    for(i=0;i<SCENE_AI_EXTRA_WEAPONS;i++){
        const rf_weapon_acquire_definition *ammo;
        ids[i]=rf_weapon_name_find(&supply->names,scene_ai_extra_weapon_names[i]);
        if(ids[i]<0)return RF_NOT_FOUND;if((uint32_t)ids[i]>=supply->names.count)return RF_RANGE;
        ammo=supply->definitions+ids[i];
        if(ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine<=0 || ammo->capacity<0)return RF_FORMAT;
        status=rf_weapon_primary_load(tables,scene_ai_extra_weapon_names[i],128*1024,primary+i);if(status)return status;
        if(primary[i].magazine!=(uint32_t)ammo->magazine || primary[i].projectiles!=1 ||
           primary[i].damage_kind<0 || primary[i].damage_kind>8)return RF_FORMAT;
    }
    memcpy(scene_ai_extra_weapons.primary,primary,sizeof(primary));memcpy(scene_ai_extra_weapons.ids,ids,sizeof(ids));
    scene_ai_extra_weapons.ready=1;return RF_OK;
}
static int campaign_enemy_extra_weapon_select(int32_t weapon,const rf_weapon_supply_catalog *supply,
    campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected={0};uint32_t i;
    if(!supply || !out || supply->names.count>64)return RF_RANGE;
    if(!scene_ai_extra_weapons.ready || weapon<0)return RF_NOT_FOUND;
    for(i=0;i<SCENE_AI_EXTRA_WEAPONS;i++)if(scene_ai_extra_weapons.ids[i]==weapon)break;
    if(i==SCENE_AI_EXTRA_WEAPONS)return RF_NOT_FOUND;
    if(weapon>=64 || (uint32_t)weapon>=supply->names.count)return RF_RANGE;
    selected.primary=scene_ai_extra_weapons.primary+i;selected.ammo=supply->definitions+weapon;
    if(selected.ammo->ammo_type<0 || selected.ammo->ammo_type>=32 || selected.ammo->magazine<=0)return RF_RANGE;
    /* No player slot equivalence. Consumers use primary/ammo/melee/penetration;
     * this sentinel prevents accidentally applying a legacy slot special case. */
    selected.slot=UINT32_MAX;*out=selected;return RF_OK;
}
/* Optional ordinary exhausted-weapon fallback extension. Existing explicit
 * Attack veto remains. Parent may try these after legacy ranged choices and
 * before the legacy riot-stick fallback. No inventory grants or mutation. */
static int32_t campaign_enemy_extra_fallback_pick(const rf_weapon_inventory *inventory,
    const rf_weapon_supply_catalog *supply,int32_t current,uint32_t scripted)
{
    static const uint32_t order[SCENE_AI_EXTRA_WEAPONS]={1,2,0,3};uint32_t i;
    if(!inventory || !supply || scripted==1)return -1;
    for(i=0;i<SCENE_AI_EXTRA_WEAPONS;i++){
        campaign_enemy_weapon_selection selected;int32_t weapon=scene_ai_extra_weapons.ids[order[i]];
        if(weapon<0 || weapon>=64 || weapon==current || !inventory->owned[weapon])continue;
        if(campaign_enemy_extra_weapon_select(weapon,supply,&selected))continue;
        if(inventory->loaded[weapon]>0 || inventory->reserve[selected.ammo->ammo_type]>0)return weapon;
    }return -1;
}
#endif
