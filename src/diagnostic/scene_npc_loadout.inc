#ifndef RF_SCENE_NPC_LOADOUT_INC
#define RF_SCENE_NPC_LOADOUT_INC
#include "rf/entity_assets.h"
#include <string.h>
static int scene_npc_loadout_equal(const char *a,const char *b)
{
    unsigned char x,y;
    do{x=(unsigned char)*a++;y=(unsigned char)*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 0;}while(x);
    return 1;
}
/* Original464010 applies these instance strings after422360 class startup:
 * unknown/empty inherit; "none" clears ownership in the selected category;
 * valid overrides acquire(-1), preserving other owned defaults. Primary then
 * fills reserve and gives the rocket special-case3 (4c659f resolves872458 to
 * Rocket Launcher); secondary only acquires.
 * The caller rebinds motion selection to the resulting primary (42ab20).
 * No player inventory, view resources, handles or scene globals are touched. */
static int scene_npc_loadout_apply(const rf_level_entity *record,rf_weapon_inventory *inventory,
    rf_weapon_startup_state *state,const rf_weapon_supply_catalog *supply)
{
    rf_weapon_inventory candidate;rf_weapon_startup_state selected;uint32_t side,i;int status;
    const char *names[2];int32_t rocket;
    if(!record||!inventory||!state||!supply||supply->names.count>64||supply->names.primary_count>supply->names.count)return RF_RANGE;
    names[0]=record->primary_weapon;names[1]=record->secondary_weapon;
    for(side=0;side<2;side++)if(!memchr(names[side],0,RF_LEVEL_NAME_CAPACITY))return RF_FORMAT;
    candidate=*inventory;selected=*state;rocket=rf_weapon_name_find(&supply->names,"Rocket Launcher");
    for(side=0;side<2;side++){
        int32_t *held=side?&selected.secondary:&selected.primary,weapon;
        if(scene_npc_loadout_equal(names[side],"none")){
            uint32_t begin=side?supply->names.primary_count:0,end=side?supply->names.count:supply->names.primary_count;
            for(i=begin;i<end;i++)candidate.owned[i]=0;
            *held=-1;continue;
        }
        /* Original's deprecated glock spelling resolves to12mm handgun. */
        weapon=rf_weapon_name_find(&supply->names,!side&&scene_npc_loadout_equal(names[side],"glock")?"12mm handgun":names[side]);
        if(weapon<0)continue;
        status=rf_weapon_acquire_sp(&candidate,supply->definitions+weapon,weapon,-1);if(status)return status;
        if(!side){int32_t ammo=supply->definitions[weapon].ammo_type;
            if(ammo>=32)return RF_RANGE;
            if(ammo>=0)candidate.reserve[ammo]=weapon==rocket?3:supply->definitions[weapon].capacity;
        }
        *held=weapon;
    }
    *inventory=candidate;*state=selected;return RF_OK;
}
#endif
