#include "../src/diagnostic/scene_weapon_resource_demand.inc"
#include <assert.h>
#include <stdio.h>
static int resolve(void *context,const char *name,rf_item_definition *item)
{return rf_item_definition_load(context,name,128*1024,item);}
int main(void)
{
    const char *slots[12]={"12mm handgun","Assault Rifle","Riot Stick","Shotgun","Rocket Launcher","Grenade","Sniper Rifle","rail_gun","Remote Charge","Remote Charge Detonator","Flamethrower","riot shield"};
    rf_vpp tables={0};rf_weapon_supply_catalog supply;rf_weapon_inventory inventory={0};
    rf_level_item item={0};rf_level_owned_items pickups={&item,1,0};
    rf_level_owned_event event={0};rf_level_owned_events events={0};scene_weapon_resource_demand result;
    int32_t shield;events.items=&event;events.count=1;
    assert(rf_vpp_open(&tables,"Installed_Game/tables.vpp")==RF_OK);
    assert(rf_weapon_supply_load(&tables,128*1024,&supply)==RF_OK);
    shield=rf_weapon_name_find(&supply.names,"riot shield");assert(shield>=0);
    assert(scene_weapon_resources_plan(15,1u<<11,slots,12,&supply.names,&inventory,NULL,NULL,NULL,NULL,&result)==RF_OK && result.mask==15);
    inventory.owned[shield]=1;
    assert(scene_weapon_resources_plan(15,1u<<11,slots,12,&supply.names,&inventory,NULL,NULL,NULL,NULL,&result)==RF_OK && result.mask==0x80f && result.owned_mask==0x800);
    inventory.owned[shield]=0;strcpy(item.class_name,"riot shield");
    assert(scene_weapon_resources_plan(15,1u<<11,slots,12,&supply.names,&inventory,&pickups,NULL,resolve,&tables,&result)==RF_OK && result.mask==0x80f && result.pickup_mask==0x800);
    strcpy(item.class_name,"12mm_ammo");strcpy(event.record.type,"Give_Item_To_Player");strcpy(event.record.texts[0],"riot shield");
    assert(scene_weapon_resources_plan(15,1u<<11,slots,12,&supply.names,&inventory,&pickups,&events,resolve,&tables,&result)==RF_OK && result.mask==0x80f && !result.pickup_mask && result.event_mask==0x800);
    strcpy(event.record.type,"Set_Life");
    assert(scene_weapon_resources_plan(0x7ff,1u<<11,slots,12,&supply.names,&inventory,&pickups,&events,resolve,&tables,&result)==RF_OK && result.mask==0x7ff);
    {
        const char *form_slots[17]={"12mm handgun","Assault Rifle","Riot Stick","Shotgun","Rocket Launcher","Grenade","Sniper Rifle","rail_gun","Remote Charge","Remote Charge Detonator","Flamethrower","riot shield","shoulder_cannon","Machine Pistol","heavy_machine_gun","scope_assault_rifle","Undercover 12mm handgun"};
        strcpy(event.record.type,"Go_Undercover");
        assert(scene_weapon_resources_plan(15,1u<<16,form_slots,17,&supply.names,&inventory,NULL,&events,resolve,&tables,&result)==RF_OK && result.event_mask==(1u<<16) && result.mask==(15u|(1u<<16)));
        strcpy(event.record.type,"Set_Life");
    }
    strcpy(item.class_name,"not_an_authored_item");
    assert(scene_weapon_resources_plan(15,1u<<11,slots,12,&supply.names,&inventory,&pickups,NULL,resolve,&tables,&result)==RF_OK && result.mask==15 && result.unresolved_items==1);
    rf_vpp_close(&tables);puts("sparse shield resource demand: inventory, pickups, authored grants, base masks and unrelated items pass");return 0;
}
