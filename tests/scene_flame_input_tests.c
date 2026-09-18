#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static scene_stream stream;
int main(void)
{
    rf_vpp tables={0};rf_weapon_primary_definition definition;uint32_t frame,active;int32_t weapon,ammo;
    float position[3]={0},forward[3]={0,0,1};
    CHECK(rf_vpp_open(&tables,"Installed_Game/tables.vpp")==RF_OK);
    CHECK(rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply)==RF_OK);
    CHECK(rf_weapon_primary_load(&tables,"Flamethrower",128*1024,&definition)==RF_OK);rf_vpp_close(&tables);
    weapon=campaign_flame_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Flamethrower");CHECK(weapon>=0);
    ammo=campaign_weapon_supply.definitions[weapon].ammo_type;CHECK(ammo>=0);
    campaign_equipped_slot=10;campaign_player_inventory.owned[weapon]=1;
    campaign_player_inventory.loaded[weapon]=100;campaign_player_inventory.reserve[ammo]=200;
    scene_flame_input_reset();CHECK(!scene_flame_input_pending());
    for(frame=0;frame<6;frame++) {
        CHECK(scene_flame_input_tick(&stream,frame,position,forward,weapon,&definition,.1f,1,0,1,0,&active)==RF_OK);
        CHECK(!active && campaign_player_inventory.loaded[weapon]==100 && scene_flame_input_pending());
    }
    for(;frame<126;frame++) {
        CHECK(scene_flame_input_tick(&stream,frame,position,forward,weapon,&definition,.1f,1,0,1,0,&active)==RF_OK && active);
    }
    CHECK(campaign_player_inventory.loaded[weapon]==0 && scene_flame_input_pending());
    CHECK(scene_flame_input_tick(&stream,126,position,forward,weapon,&definition,.1f,1,0,1,0,&active)==RF_OK && !active);
    CHECK(scene_flame_input_pending());
    CHECK(scene_flame_input.reload_left==(uint32_t)ceilf(definition.reload_seconds*60));
    for(frame=127;scene_flame_input.reload_left;frame++)
        CHECK(scene_flame_input_tick(&stream,frame,position,forward,weapon,&definition,.1f,1,0,1,0,&active)==RF_OK && !active);
    CHECK(campaign_player_inventory.loaded[weapon]==100 && campaign_player_inventory.reserve[ammo]==100);
    CHECK(!scene_flame_input_pending()); /* elapsed/drained retained, operation complete */
    CHECK(scene_flame_input_tick(&stream,frame++,position,forward,weapon,&definition,.1f,1,0,1,0,&active)==RF_OK && !active);
    CHECK(scene_flame_input_tick(&stream,frame++,position,forward,weapon,&definition,.1f,0,0,1,0,&active)==RF_OK && !active);
    CHECK(!scene_flame_input.ignition && !scene_flame_input.reload_left && campaign_player_inventory.loaded[weapon]==100);
    CHECK(scene_flame_input_tick(&stream,frame++,position,forward,weapon,&definition,.1f,1,1,1,0,&active)==RF_OK && !active);
    CHECK(!scene_flame_input.ignition && !scene_flame_input_pending());
    scene_flame_gas_remainder=1;CHECK(scene_flame_input_pending());
    scene_flame_input_reset();CHECK(!scene_flame_input_pending());
    puts("Flame real-scene ignition delay, continuous single drain, empty reload and deselect/inhibit cancellation passed");return 0;
}
