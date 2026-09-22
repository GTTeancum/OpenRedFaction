#ifndef RF_SCENE_AI_DROP_SUPPLY_INCLUDED
#define RF_SCENE_AI_DROP_SUPPLY_INCLUDED
/* First-pass corpse drop policy: preserve the existing one-magazine maximum,
 * but do not manufacture rounds that the NPC has already spent. Only held,
 * owned, implemented weapons participate; no mutation before persistent drop
 * publication. An exhausted held weapon retains ownership value: quantity zero is
 * persisted without manufacturing ammunition. */
/* Stable scene slots are a resource policy, not weapon-table ordinals. */
static int32_t scene_enemy_drop_weapon_slot(const rf_weapon_names *names,int32_t weapon)
{
    static const char *supported[17]={"12mm handgun","Assault Rifle","Riot Stick","Shotgun","Rocket Launcher","Grenade",
        "Sniper Rifle","rail_gun","Remote Charge",NULL,"Flamethrower",NULL,"shoulder_cannon",
        "Machine Pistol","heavy_machine_gun","scope_assault_rifle","Undercover 12mm handgun"};uint32_t i;
    if(!names||names->count>64||weapon<0||(uint32_t)weapon>=names->count)return -1;
    for(i=0;i<17;i++)if(supported[i]&&rf_weapon_name_find(names,supported[i])==weapon)return (int32_t)i;
    return -1;
}
static int campaign_enemy_drop_supply(const rf_weapon_inventory *inventory,
    const rf_weapon_supply_catalog *supply,int32_t held,int32_t *quantity)
{
    int32_t slot;const rf_weapon_acquire_definition *definitions;
    int32_t loaded,reserve,type,magazine;int64_t available;
    if(!inventory || !supply || !quantity || supply->names.count>64)return RF_RANGE;
    if(held<0 || held>=64 || (uint32_t)held>=supply->names.count || !inventory->owned[held])return RF_NOT_FOUND;
    slot=scene_enemy_drop_weapon_slot(&supply->names,held);if(slot<0)return RF_NOT_FOUND;
    definitions=supply->definitions;
    type=definitions[held].ammo_type;magazine=definitions[held].magazine;
    if(slot==2){if(type!=-1 || magazine || inventory->loaded[held])return RF_RANGE;*quantity=0;return RF_OK;}
    if(type<0 || type>=32 || magazine<0)return RF_RANGE;
    loaded=inventory->loaded[held];reserve=inventory->reserve[type];
    if(loaded<0 || loaded>magazine || reserve<0)return RF_RANGE;
    /* Practical first pass: throwable pickups expose one remaining unit,
     * never a default pack or the NPC's entire carried explosive supply. */
    if(slot==5 || slot==8){if(magazine)return RF_RANGE;*quantity=reserve>0?1:0;return RF_OK;}
    if(!magazine)return RF_RANGE;
    available=(int64_t)loaded+reserve;
    *quantity=available>magazine?magazine:(int32_t)available;
    return RF_OK;
}
#endif
