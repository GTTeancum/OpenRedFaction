#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_extra_pickups.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"shared ammo line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_weapon_supply_catalog supply;rf_item_definition mp_item;
    rf_weapon_inventory inventory={0},saved;rf_weapon_pickup_grant grant;int32_t pistol,mp,ammo;
    CHECK(!rf_vpp_open(&tables,argc>1?argv[1]:"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&supply));
    CHECK(!rf_item_definition_load(&tables,"Machine Pistol",128*1024,&mp_item));rf_vpp_close(&tables);
    pistol=rf_weapon_name_find(&supply.names,"12mm handgun");mp=rf_weapon_name_find(&supply.names,"Machine Pistol");
    CHECK(pistol>=0 && pistol<64 && mp>=0 && mp<64);
    ammo=supply.definitions[pistol].ammo_type;CHECK(ammo>=0 && ammo<32 && supply.definitions[mp].ammo_type==ammo);
    CHECK(supply.definitions[pistol].capacity==125 && supply.definitions[mp].capacity==200 && supply.definitions[mp].magazine==30);
    inventory.owned[pistol]=1;inventory.reserve[ammo]=125;
    /* Unowned MP must not enlarge a handgun-only reserve. */
    CHECK(!scene_pickup_shared_ammo_grant(&inventory,&supply,pistol,32,0,&grant));
    CHECK(!grant.rounds && inventory.reserve[ammo]==125 && !inventory.owned[mp]);
    CHECK(!scene_extra_pickup_grant("Machine Pistol",&mp_item,&supply,&inventory,32,&grant));
    CHECK(grant.acquired && grant.rounds==32 && inventory.loaded[mp]==30 && inventory.reserve[ammo]==127);
    CHECK(!scene_pickup_shared_ammo_grant(&inventory,&supply,pistol,32,0,&grant));
    CHECK(!grant.acquired && grant.rounds==32 && inventory.reserve[ammo]==159);
    CHECK(!scene_pickup_shared_ammo_grant(&inventory,&supply,pistol,99,0,&grant));
    CHECK(grant.rounds==41 && inventory.reserve[ammo]==200 && inventory.loaded[mp]==30);
    saved=inventory;
    CHECK(!scene_pickup_shared_ammo_grant(&inventory,&supply,pistol,32,0,&grant));
    CHECK(!grant.rounds && !grant.acquired && !memcmp(&saved,&inventory,sizeof(saved)));
    CHECK(supply.definitions[pistol].capacity==125 && supply.definitions[mp].capacity==200);
    puts("installed shared 12mm pickup:125 -> MP32 gives127 -> ammo32 gives159 -> bounded200, no fabricated ownership");return 0;
}
