#ifndef RF_SCENE_EXTRA_PICKUPS_INCLUDED
#define RF_SCENE_EXTRA_PICKUPS_INCLUDED
#include "scene_pickup_shared_ammo.inc"
/* Append these names to the existing pickup resource catalog; do not assign
 * fake player-view slots. Installed Undercover has no dedicated pickup and
 * uses handgun 12mm reserve. Machine Pistol Special uses Assault Rifle's AP
 * reserve, already supplied by the authored 5.56mm_ammo class. */
#define SCENE_EXTRA_PICKUP_COUNT 6
#define SCENE_EXTRA_PICKUP_NAMES "Machine Pistol","heavy machine gun","scope assault rifle","7.62mm_ammo","explosive 5.56mm rounds","shoulder cannon"
static const char *scene_extra_pickup_names[SCENE_EXTRA_PICKUP_COUNT]={SCENE_EXTRA_PICKUP_NAMES};
static const char *scene_extra_pickup_weapons[SCENE_EXTRA_PICKUP_COUNT]={
    "Machine Pistol","heavy_machine_gun","scope_assault_rifle","heavy_machine_gun","scope_assault_rifle","shoulder_cannon"};
/* Installed Fusion pickup gives one shell and its gun; no separate Fusion
 * ammo pickup class is invented. Existing indices remain stable. */
static const uint8_t scene_extra_pickup_gives_weapon[SCENE_EXTRA_PICKUP_COUNT]={1,1,1,0,0,1};
static int scene_extra_pickup_equal(const char *a,const char *b)
{
    unsigned char x,y;if(!a || !b)return 0;
    do{x=(unsigned char)*a++;y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;
    }while(x);return 1;
}
static int scene_extra_pickup_index(const char *name)
{
    int i;for(i=0;i<SCENE_EXTRA_PICKUP_COUNT;i++)if(scene_extra_pickup_equal(name,scene_extra_pickup_names[i]))return i;
    return -1;
}
/* Resolve scene slots by name, independently of compact supply IDs. Feed the
 * optional mask into scene_weapon_resources_plan, then expand its resulting
 * demand mask. MP's alternate owns a distinct view at present, so either MP
 * mode requires both resources; HMG/scoped AR never require the intervening
 * Undercover slot. Undercover is independently demand-loaded from imported
 * ownership/script metadata; no fictional pickup is added. */
static int scene_extra_pickup_demand_slots(const char *const *slots,uint32_t count,
    uint32_t *optional_mask,uint32_t *machine_pair)
{
    const char *names[8]={"Machine Pistol","heavy_machine_gun","scope_assault_rifle","Machine Pistol Special","Undercover 12mm handgun","shoulder_cannon","Sniper Rifle","rail_gun"};
    uint32_t i,j,bits[8]={0},optional=0;
    if(!slots || !count || count>32 || !optional_mask || !machine_pair)return RF_RANGE;
    for(i=0;i<count;i++){
        if(!slots[i])return RF_RANGE;
        for(j=0;j<8;j++)if(scene_extra_pickup_equal(slots[i],names[j])){
            if(bits[j])return RF_FORMAT;
            bits[j]=1u<<i;optional|=bits[j];
        }
    }
    for(j=0;j<8;j++)if(!bits[j])return RF_NOT_FOUND;
    *optional_mask=optional;*machine_pair=bits[0]|bits[3];return RF_OK;
}
static int scene_extra_pickup_demand_expand(const char *const *slots,uint32_t count,
    uint32_t demand,uint32_t *expanded)
{
    uint32_t optional,pair,valid;int status;
    if(!expanded)return RF_RANGE;
    status=scene_extra_pickup_demand_slots(slots,count,&optional,&pair);if(status)return status;
    valid=count==32?UINT32_MAX:((1u<<count)-1);
    if(demand&~valid)return RF_RANGE;
    *expanded=demand|((demand&pair)?pair:0);return RF_OK;
}
/* Return NOT_FOUND for a class outside this extension; the caller then uses
 * legacy pickup handling. Success grants finite inventory only, never equips
 * or loads a view model. Existing retirement occurs only on acquired/rounds. */
static int scene_extra_pickup_grant(const char *name,const rf_item_definition *item,
    const rf_weapon_supply_catalog *supply,rf_weapon_inventory *inventory,int32_t quantity,
    rf_weapon_pickup_grant *grant)
{
    int index=scene_extra_pickup_index(name);int32_t weapon;
    if(index<0)return RF_NOT_FOUND;
    if(!item || !supply || !inventory || !grant || supply->names.count>64)return RF_RANGE;
    if((item->flags&1) || !scene_extra_pickup_equal(item->weapon,scene_extra_pickup_weapons[index]) ||
       item->gives_weapon!=scene_extra_pickup_gives_weapon[index])return RF_FORMAT;
    weapon=rf_weapon_name_find(&supply->names,item->weapon);
    if(weapon<0)return RF_NOT_FOUND;
    if(weapon>=64 || (uint32_t)weapon>=supply->names.count)return RF_RANGE;
    return scene_pickup_shared_ammo_grant(inventory,supply,weapon,quantity,item->gives_weapon,grant);
}
#endif
