#include "rf/entity_assets.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/diagnostic/scene_ai_weapon_selection.inc"
#include "../src/diagnostic/scene_ai_extra_weapons.inc"
#include "../src/diagnostic/scene_ai_gameplay.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"extra AI weapons line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    const char *names[4]={"Machine Pistol","heavy_machine_gun","scope_assault_rifle","Undercover 12mm handgun"};
    const int32_t clips[4]={30,99,20,16};
    const float damage[4]={40,75,125,20},wait[4]={.09f,.10f,.5f,.5f};
    rf_vpp tables={0};rf_weapon_supply_catalog supply;uint32_t i;
    CHECK(argc==2 && !rf_vpp_open(&tables,argv[1]));CHECK(!rf_weapon_supply_load(&tables,128*1024,&supply));
    CHECK(!campaign_enemy_extra_weapons_open(&tables,&supply));rf_vpp_close(&tables);
    for(i=0;i<4;i++){
        campaign_enemy_weapon_selection s;rf_weapon_inventory inventory={0};uint32_t due=0,ready,event,remaining=0,shot_due=0;int32_t reloading=-1;
        int32_t weapon=rf_weapon_name_find(&supply.names,names[i]),ammo;
        CHECK(weapon>=0 && weapon<64 && (uint32_t)weapon<supply.names.count);
        CHECK(!campaign_enemy_extra_weapon_select(weapon,&supply,&s));
        ammo=s.ammo->ammo_type;
        CHECK(s.ammo==supply.definitions+weapon && s.ammo->magazine==clips[i] && ammo>=0 && ammo<32 && !s.melee && !s.penetrates_world && s.slot==UINT32_MAX);
        CHECK(s.primary->damage==damage[i] && s.primary->fire_seconds==wait[i]);
        CHECK(!campaign_enemy_cadence(s.primary,100,&remaining,&shot_due) && shot_due>100);
        CHECK(isfinite(s.primary->reload_seconds) && s.primary->reload_seconds>0 && s.primary->reload_seconds<=60);
        inventory.owned[weapon]=1;inventory.reserve[ammo]=3;
        CHECK(!campaign_enemy_ammo_ready(&inventory,s.ammo,s.primary,weapon,100,&due,&reloading,&ready,&event) && !ready && event==1);
        CHECK(due>100 && reloading==weapon);
        CHECK(!campaign_enemy_ammo_ready(&inventory,s.ammo,s.primary,weapon,due,&due,&reloading,&ready,&event) && ready && event==2);
        CHECK(inventory.loaded[weapon]==3 && inventory.reserve[ammo]==0);
        inventory.loaded[weapon]=0;
        CHECK(!campaign_enemy_ammo_ready(&inventory,s.ammo,s.primary,weapon,500,&due,&reloading,&ready,&event) && !ready && event==3);
    }
    {campaign_enemy_weapon_selection s={0};int32_t weapon=rf_weapon_name_find(&supply.names,"Machine Pistol Special");
     CHECK(weapon>=0 && weapon<64);CHECK(campaign_enemy_extra_weapon_select(weapon,&supply,&s)==RF_NOT_FOUND);CHECK(!s.primary);}
    puts("installed four additional NPC guns: authored selection/cadence, finite reload and exhaustion passed");return 0;
}
