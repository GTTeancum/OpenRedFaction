#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_extra_pickups.inc"
#include "../src/diagnostic/scene_weapon_resource_demand.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"extra pickup line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int resolve_item(void *context,const char *name,rf_item_definition *item)
{return rf_item_definition_load((rf_vpp *)context,name,128*1024,item);}
static int demand_test(rf_vpp *tables,const rf_weapon_supply_catalog *supply)
{
    /* Deliberately nonproduction ordering proves masks are scene-slot based,
     * never weapon-table indices or a prefix that loads intervening guns. */
    const char *slots[]={"Undercover 12mm handgun","heavy_machine_gun","Machine Pistol Special","scope_assault_rifle","Machine Pistol","shoulder_cannon","Sniper Rifle","rail_gun","Rocket Launcher","Grenade","Remote Charge Detonator","Remote Charge","Flamethrower"};
    rf_level_item item={0};rf_level_owned_items pickups={0};rf_weapon_inventory inventory={0};
    scene_weapon_resource_demand demand;uint32_t optional=0,pair=0,remote=0,expanded=0;
    int32_t weapon=rf_weapon_name_find(&supply->names,"Machine Pistol Special");
    CHECK(!scene_extra_pickup_demand_slots(slots,13,&optional,&pair,&remote));
    CHECK(optional==8191 && pair==20 && remote==3072);
    pickups.items=&item;pickups.count=1;
    strcpy(item.class_name,"heavy machine gun");
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
    CHECK(demand.pickup_mask==2 && demand.mask==2);
    CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==2);
    strcpy(item.class_name,"scope assault rifle");
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
    CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==8);
    strcpy(item.class_name,"Machine Pistol");
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
    CHECK(demand.mask==16);
    CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==20);
    strcpy(item.class_name,"7.62mm_ammo");
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
    CHECK(!demand.mask); /* Ammo alone cannot select or load an unowned gun. */
    CHECK(weapon>=0 && weapon<64);inventory.owned[weapon]=1;
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,NULL,NULL,NULL,&demand));
    CHECK(demand.owned_mask==4);
    CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==20);
    {rf_level_owned_event event={0};rf_level_owned_events events={0};
     memset(&inventory,0,sizeof(inventory));events.items=&event;events.count=1;
     strcpy(event.record.type,"Give_Item_To_Player");strcpy(event.record.texts[0],"scope assault rifle");
     CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,&events,resolve_item,tables,&demand));
     CHECK(demand.event_mask==8 && demand.mask==8);}
    weapon=rf_weapon_name_find(&supply->names,"Undercover 12mm handgun");CHECK(weapon>=0);
    memset(&inventory,0,sizeof(inventory));inventory.owned[weapon]=1;
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,NULL,NULL,NULL,&demand));
    CHECK(demand.owned_mask==1 && demand.mask==1);
    CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==1);
    memset(&inventory,0,sizeof(inventory));strcpy(item.class_name,"shoulder cannon");
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
    CHECK(demand.pickup_mask==32 && demand.mask==32);
    CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==32);
    weapon=rf_weapon_name_find(&supply->names,"shoulder_cannon");CHECK(weapon>=0 && weapon<64);
    inventory.owned[weapon]=1;
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,NULL,NULL,NULL,&demand));
    CHECK(demand.owned_mask==32 && demand.mask==32);
    {const char *classes[4]={"Sniper Rifle","rail gun","Rocket Launcher","grenades"};
     const char *weapons[4]={"Sniper Rifle","rail_gun","Rocket Launcher","Grenade"};uint32_t i;
     for(i=0;i<4;i++) {
        uint32_t expected=1u<<(6+i);rf_level_owned_event event={0};rf_level_owned_events events={0};
        memset(&inventory,0,sizeof(inventory));strcpy(item.class_name,classes[i]);
        CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
        CHECK(demand.pickup_mask==expected && demand.mask==expected);
        CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==expected);
        weapon=rf_weapon_name_find(&supply->names,weapons[i]);CHECK(weapon>=0 && weapon<64);
        inventory.owned[weapon]=1;
        CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,NULL,NULL,NULL,&demand));
        CHECK(demand.owned_mask==expected && demand.mask==expected);
        memset(&inventory,0,sizeof(inventory));events.items=&event;events.count=1;
        strcpy(event.record.type,"Give_Item_To_Player");strcpy(event.record.texts[0],classes[i]);
        CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,&events,resolve_item,tables,&demand));
        CHECK(demand.event_mask==expected && demand.mask==expected);
     }}
    {const char *classes[2]={"Remote Charge","Remote Charges"};uint32_t i;
     for(i=0;i<2;i++) {
        rf_level_owned_event event={0};rf_level_owned_events events={0};
        memset(&inventory,0,sizeof(inventory));strcpy(item.class_name,classes[i]);
        CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
        CHECK(demand.mask==2048);
        CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==remote);
        events.items=&event;events.count=1;strcpy(event.record.type,"Give_Item_To_Player");strcpy(event.record.texts[0],classes[i]);
        CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,&events,resolve_item,tables,&demand));
        CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==remote);
     }
     for(i=10;i<12;i++) {
        weapon=rf_weapon_name_find(&supply->names,slots[i]);CHECK(weapon>=0 && weapon<64);
        memset(&inventory,0,sizeof(inventory));inventory.owned[weapon]=1;
        CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,NULL,NULL,NULL,&demand));
        CHECK(demand.mask==(1u<<i));
        CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==remote);
     }
     CHECK(!scene_extra_pickup_demand_expand(slots,13,2048|16,&expanded) && expanded==(remote|pair));}
    memset(&inventory,0,sizeof(inventory));strcpy(item.class_name,"flamethrower");
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
    CHECK(demand.mask==4096);
    CHECK(!scene_extra_pickup_demand_expand(slots,13,demand.mask,&expanded) && expanded==4096);
    strcpy(item.class_name,"Napalm");
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,&pickups,NULL,resolve_item,tables,&demand));
    CHECK(!demand.mask); /* Fuel alone grants no unowned gun/view. */
    weapon=rf_weapon_name_find(&supply->names,"Flamethrower");CHECK(weapon>=0 && weapon<64);
    inventory.owned[weapon]=1;
    CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,NULL,NULL,NULL,&demand));
    CHECK(demand.owned_mask==4096 && demand.mask==4096);
    {rf_level_owned_event event={0};rf_level_owned_events events={0};
     memset(&inventory,0,sizeof(inventory));events.items=&event;events.count=1;
     strcpy(event.record.type,"Give_Item_To_Player");strcpy(event.record.texts[0],"flamethrower");
     CHECK(!scene_weapon_resources_plan(0,optional,slots,13,&supply->names,&inventory,NULL,&events,resolve_item,tables,&demand));
     CHECK(demand.event_mask==4096 && demand.mask==4096);}
    return 0;
}
int main(int argc,char **argv)
{
    const int32_t counts[6]={32,99,20,99,20,1};const uint32_t gives[6]={1,1,1,0,0,1};rf_vpp tables={0};rf_weapon_supply_catalog supply;
    rf_item_definition item;int i;rf_weapon_inventory inventory={0},saved;rf_weapon_pickup_grant grant;
    CHECK(!rf_vpp_open(&tables,argc>1?argv[1]:"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&supply));
    CHECK(!demand_test(&tables,&supply));
    for(i=0;i<SCENE_EXTRA_PICKUP_COUNT;i++){
        int32_t weapon,ammo;const rf_weapon_acquire_definition *d;
        memset(&inventory,0,sizeof(inventory));
        CHECK(!rf_item_definition_load(&tables,scene_extra_pickup_names[i],128*1024,&item));
        CHECK(item.count==counts[i] && item.mesh_kind==1 && item.gives_weapon==gives[i]);
        weapon=rf_weapon_name_find(&supply.names,item.weapon);CHECK(weapon>=0 && weapon<64);
        d=supply.definitions+weapon;ammo=d->ammo_type;CHECK(ammo>=0 && ammo<32);
        CHECK(!scene_extra_pickup_grant(scene_extra_pickup_names[i],&item,&supply,&inventory,item.count,&grant));
        CHECK(grant.rounds==(uint32_t)item.count && grant.acquired==gives[i]);
        CHECK(inventory.owned[weapon]==gives[i]);
        CHECK(inventory.loaded[weapon]+inventory.reserve[ammo]==item.count);
        CHECK(!scene_extra_pickup_grant(scene_extra_pickup_names[i],&item,&supply,&inventory,INT32_MAX,&grant));
        CHECK(inventory.reserve[ammo]==d->capacity);saved=inventory;
        CHECK(!scene_extra_pickup_grant(scene_extra_pickup_names[i],&item,&supply,&inventory,item.count,&grant));
        CHECK(!grant.rounds && !grant.acquired && !memcmp(&inventory,&saved,sizeof(saved)));
    }
    /* Actual authored ammo-only classes feed both alternate users without
     * granting the base gun or aliasing 12mm and AP pools. */
    {int32_t mp=rf_weapon_name_find(&supply.names,"Machine Pistol Special");
     int32_t undercover=rf_weapon_name_find(&supply.names,"Undercover 12mm handgun"),base,ammo;
     CHECK(mp>=0 && mp<64 && undercover>=0 && undercover<64);
     CHECK(!rf_item_definition_load(&tables,"5.56mm_ammo",128*1024,&item));
     base=rf_weapon_name_find(&supply.names,item.weapon);CHECK(base>=0 && base<64 && !item.gives_weapon);
     ammo=supply.definitions[base].ammo_type;CHECK(ammo>=0 && ammo<32 && ammo==supply.definitions[mp].ammo_type);
     memset(&inventory,0,sizeof(inventory));inventory.owned[mp]=1;
     CHECK(!rf_weapon_pickup_grant_sp(&inventory,supply.definitions+base,base,item.count,0,&grant));
     CHECK(inventory.reserve[ammo]==item.count && !inventory.owned[base]);
     CHECK(!rf_item_definition_load(&tables,"12mm_ammo",128*1024,&item));
     base=rf_weapon_name_find(&supply.names,item.weapon);CHECK(base>=0 && base<64 && !item.gives_weapon);
     CHECK(supply.definitions[base].ammo_type==supply.definitions[undercover].ammo_type && supply.definitions[base].ammo_type!=ammo);
    }
    rf_vpp_close(&tables);puts("installed extra firearm pickups: real catalog IDs, finite supply, saturation and shared AP/12mm pools passed");return 0;
}
