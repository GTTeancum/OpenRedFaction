#ifndef RF_SCENE_AI_WEAPON_SELECTION_INCLUDED
#define RF_SCENE_AI_WEAPON_SELECTION_INCLUDED
/* Scene slot policy: handgun0, rifle1, riot2, shotgun3, rocket4,
 * grenade5, sniper6, rail7. Only implemented NPC attacks are admitted;
 * projectile weapons must not accidentally become generic hitscan. */
typedef struct campaign_enemy_weapon_selection {
    const rf_weapon_primary_definition *primary;
    const rf_weapon_acquire_definition *ammo;
    uint32_t slot,melee,penetrates_world;
} campaign_enemy_weapon_selection;
static int campaign_enemy_weapon_select(int32_t weapon,const int32_t ids[8],
    const rf_weapon_primary_definition primary[8],
    const rf_weapon_acquire_definition *ammo,uint32_t ammo_count,
    campaign_enemy_weapon_selection *result)
{
    static const uint32_t supported[6]={0,1,2,3,6,7};uint32_t i,slot;
    campaign_enemy_weapon_selection selected;
    if(!ids || !primary || !ammo || !result)return RF_RANGE;
    if(weapon<0)return RF_NOT_FOUND;
    for(i=0;i<6;i++)if(ids[supported[i]]>=0 && weapon==ids[supported[i]])break;
    if(i==6)return RF_NOT_FOUND;
    slot=supported[i];
    if((uint32_t)weapon>=ammo_count || weapon>=64)return RF_RANGE;
    selected.primary=primary+slot;selected.ammo=slot==2?NULL:ammo+weapon;
    selected.slot=slot;selected.melee=slot==2;selected.penetrates_world=slot==7;
    if(selected.ammo && (selected.ammo->ammo_type<0 || selected.ammo->ammo_type>=32 ||
       selected.ammo->magazine<=0))return RF_RANGE;
    *result=selected;return RF_OK;
}
#endif
