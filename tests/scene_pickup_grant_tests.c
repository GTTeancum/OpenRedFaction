#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static scene_stream stream;
static rf_geometry_collision_world world;
static scene_pickup_resource resources[SCENE_PICKUP_CLASSES-1];
int main(void)
{
    rf_vpp tables={0};rf_level_item items[5]={{0}};uint8_t taken[5]={0};uint32_t slots[5]={0},i;
    const char *names[]={"Remote Charges","Sniper Rifle",".50cal_ammo","rail gun","railgun_bolts"};
    const int quantities[]={3,6,6,8,8};float eye[3]={0,1,0};rf_weapon_inventory before;
    CHECK(rf_vpp_open(&tables,"Installed_Game/tables.vpp")==RF_OK);
    CHECK(rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply)==RF_OK);
    campaign_pistol_id=rf_weapon_name_find(&campaign_weapon_supply.names,"12mm handgun");
    campaign_rifle_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Assault Rifle");
    campaign_riot_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Riot Stick");
    campaign_shotgun_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Shotgun");
    campaign_rocket_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Rocket Launcher");
    campaign_grenade_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Grenade");
    campaign_sniper_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Sniper Rifle");
    campaign_rail_id=rf_weapon_name_find(&campaign_weapon_supply.names,"rail_gun");
    campaign_remote_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Remote Charge");
    CHECK(campaign_remote_id>=0 && campaign_sniper_id>=0 && campaign_rail_id>=0);
    stream.collision=&world;stream.pickups.items=items;stream.pickups.count=5;
    stream.pickup_taken=taken;stream.pickup_slots=slots;stream.pickup_resources=resources;
    for(i=0;i<5;i++) {
        int kind=pickup_class(names[i]);CHECK(kind>0);
        strcpy(items[i].class_name,names[i]);items[i].uid=100+i;items[i].quantity=quantities[i];items[i].position[0]=1;
        CHECK(rf_item_definition_load(&tables,names[i],128*1024,&resources[kind-1].definition)==RF_OK);
    }
    rf_vpp_close(&tables);campaign_player_damage.state.effects.health=100;
    strcpy(campaign_current_level,"pickup_test.rfl");campaign_equipped_slot=0;
    CHECK(campaign_pickups_restore(&stream)==RF_OK);
    CHECK(campaign_pickups_tick(&stream,eye)==RF_OK);
    CHECK(campaign_player_inventory.owned[campaign_remote_id]);
    CHECK(campaign_player_inventory.reserve[campaign_weapon_supply.definitions[campaign_remote_id].ammo_type]==3);
    CHECK(campaign_player_inventory.owned[campaign_sniper_id] && campaign_player_inventory.loaded[campaign_sniper_id]==6);
    CHECK(campaign_player_inventory.reserve[campaign_weapon_supply.definitions[campaign_sniper_id].ammo_type]==6);
    CHECK(campaign_player_inventory.owned[campaign_rail_id] && campaign_player_inventory.loaded[campaign_rail_id]==1);
    CHECK(campaign_player_inventory.reserve[campaign_weapon_supply.definitions[campaign_rail_id].ammo_type]==campaign_weapon_supply.definitions[campaign_rail_id].capacity);
    for(i=0;i<5;i++)CHECK(taken[i] && rf_scene_campaign_pickups.items[slots[i]].retired);
    before=campaign_player_inventory;
    CHECK(campaign_pickups_tick(&stream,eye)==RF_OK && !memcmp(&before,&campaign_player_inventory,sizeof(before)));
    memset(taken,0,sizeof(taken));CHECK(campaign_pickups_restore(&stream)==RF_OK);
    for(i=0;i<5;i++)CHECK(taken[i]);
    CHECK(campaign_pickups_tick(&stream,eye)==RF_OK && !memcmp(&before,&campaign_player_inventory,sizeof(before)));
    puts("Actual scene pickup collection grants remote3, sniper/rail weapon+ammo, retires once and restores retirement");return 0;
}
