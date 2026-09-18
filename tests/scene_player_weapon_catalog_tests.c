#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Player catalog line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp tables={0};rf_player_checkpoint_catalog catalog;
    rf_player_checkpoint player={0},decoded={0},bad;unsigned char data[RF_PLAYER_CHECKPOINT_BYTES];
    int32_t ids[10];uint32_t i,supported=0;
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply));rf_vpp_close(&tables);
    campaign_player_damage.state.effects.class_health=100;
    campaign_player_damage.state.effects.class_armor=100;
    rf_scene_weapon_supply[3]=0x52464350u;
    CHECK(!scene_checkpoint_player_catalog(&catalog));
    for(i=0;i<10;i++){ids[i]=rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[i]);CHECK(ids[i]>=0 && ids[i]<64);}
    for(i=0;i<64;i++)supported+=catalog.supported[i]!=0;
    CHECK(supported==9 && !catalog.supported[ids[9]]);
    for(i=0;i<9;i++)CHECK(catalog.supported[ids[i]]);
    CHECK(catalog.weapons[ids[5]].magazine==0 && catalog.weapons[ids[8]].magazine==0);
    CHECK(catalog.weapons[ids[6]].magazine==6 && catalog.weapons[ids[7]].magazine==1);
    player.player.catalog_hash=catalog.hash;player.player.health=100;player.player.armor=50;
    player.player.weapon=(uint32_t)ids[8];
    for(i=5;i<=8;i++)player.player.inventory.owned[ids[i]]=1;
    player.player.inventory.loaded[ids[6]]=3;player.player.inventory.loaded[ids[7]]=1;
    player.player.inventory.reserve[catalog.weapons[ids[5]].ammo_type]=2;
    player.player.inventory.reserve[catalog.weapons[ids[8]].ammo_type]=2;
    CHECK(!rf_player_checkpoint_encode(&player,&catalog,data,sizeof(data)));
    CHECK(!rf_player_checkpoint_decode(data,sizeof(data),&catalog,&decoded));
    CHECK(decoded.player.weapon==(uint32_t)ids[8] && !decoded.player.inventory.owned[ids[9]]);
    CHECK(decoded.player.inventory.loaded[ids[5]]==0 && decoded.player.inventory.loaded[ids[8]]==0);
    CHECK(decoded.player.inventory.loaded[ids[6]]==3 && decoded.player.inventory.loaded[ids[7]]==1);
    CHECK(decoded.player.inventory.reserve[catalog.weapons[ids[5]].ammo_type]==2);
    CHECK(decoded.player.inventory.reserve[catalog.weapons[ids[8]].ammo_type]==2);
    CHECK(!memcmp(&player.player.inventory,&decoded.player.inventory,sizeof(player.player.inventory)));
    bad=player;bad.player.inventory.owned[ids[9]]=1;
    CHECK(rf_player_checkpoint_encode(&bad,&catalog,data,sizeof(data))==RF_FORMAT);
    bad=player;bad.player.weapon=(uint32_t)ids[9];
    CHECK(rf_player_checkpoint_encode(&bad,&catalog,data,sizeof(data))==RF_FORMAT);
    puts("Player checkpoint catalog: nine base weapons, clipless reserves and remote selection round trip");return 0;
}
