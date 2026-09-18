/* Actual scene Give_Item/Strip_Weapons entry points, installed metadata,
 * ownership-only grant and retained shield durability; no render fixture. */
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Shield script grant line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    const char *path="Installed_Game/tables.vpp";
    rf_vpp tables={0};rf_vpp_entry entry;char *text;
    rf_weapon_inventory expected,empty={0};scene_player_shield_hit hit;
    float forward[3]={0,0,1},travel[3]={0,0,-1};float damaged;
    int32_t *ids[12]={&campaign_pistol_id,&campaign_rifle_id,&campaign_riot_id,
        &campaign_shotgun_id,&campaign_rocket_id,&campaign_grenade_id,
        &campaign_sniper_id,&campaign_rail_id,&campaign_remote_id,NULL,
        &campaign_flame_id,&campaign_shield_id};uint32_t i;
    CHECK(!rf_vpp_open(&tables,path));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply));
    for(i=0;i<12;i++)if(ids[i]){
        *ids[i]=rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[i]);
        CHECK(*ids[i]>=0 && *ids[i]<64);
    }
    CHECK(!rf_vpp_find(&tables,"clutter.tbl",&entry));
    text=malloc(entry.size);CHECK(text);
    CHECK(!rf_vpp_read(&tables,&entry,0,text,entry.size));
    CHECK(!rf_clutter_gameplay_read(text,entry.size,"riot_shield",&scene_npc_shields.definition));
    free(text);rf_vpp_close(&tables);
    CHECK(scene_npc_shields.definition.life==1250);
    campaign_inventory_ready=1;campaign_startup_inventory_replay=0;
    campaign_equipped_slot=11;campaign_explicit_unarmed=0;
    campaign_player_damage.state.effects.health=100;
    scene_player_shield_damage_reset();
    /* owned[63] also makes an erroneous reserve[-1] diagnostic nonzero. */
    campaign_player_inventory.owned[63]=1;
    campaign_player_inventory.reserve[0]=17;campaign_player_inventory.loaded[0]=7;
    expected=campaign_player_inventory;expected.owned[campaign_shield_id]=1;
    CHECK(!campaign_give_item((void *)path,"riot shield"));
    CHECK(!memcmp(&campaign_player_inventory,&expected,sizeof(expected)));
    CHECK(rf_scene_script_grants[1]==1 && rf_scene_script_grants[2]==0 && rf_scene_script_grants[6]==0);
    CHECK(scene_player_shield_damage_state.owned && scene_player_shield_damage_state.damage.health==1250);
    CHECK(!scene_player_shield_receive(&scene_player_shield_damage_state,&scene_npc_shields.definition,
        1,1,forward,travel,100,-1,&hit));CHECK(hit.intercepted && !hit.broken);
    damaged=scene_player_shield_damage_state.damage.health;CHECK(damaged==1150);
    CHECK(!campaign_give_item((void *)path,"riot shield"));
    CHECK(scene_player_shield_damage_state.damage.health==damaged);
    CHECK(rf_scene_script_grants[1]==1 && rf_scene_script_grants[2]==0);
    CHECK(!memcmp(&campaign_player_inventory,&expected,sizeof(expected)));
    CHECK(!campaign_strip_weapons(NULL));
    CHECK(!memcmp(&campaign_player_inventory,&empty,sizeof(empty)));
    CHECK(!scene_player_shield_damage_state.owned && scene_player_shield_damage_state.damage.health==0);
    CHECK(!campaign_give_item((void *)path,"riot shield"));
    empty.owned[campaign_shield_id]=1;
    CHECK(!memcmp(&campaign_player_inventory,&empty,sizeof(empty)));
    CHECK(scene_player_shield_damage_state.owned && !scene_player_shield_damage_state.broken);
    CHECK(scene_player_shield_damage_state.damage.health==1250);
    CHECK(rf_scene_script_grants[1]==2 && rf_scene_script_grants[2]==0 && rf_scene_script_grants[6]==0);
    puts("Scene shield Give_Item duplicate/Strip_Weapons/regrant durability passed");
    return 0;
}
