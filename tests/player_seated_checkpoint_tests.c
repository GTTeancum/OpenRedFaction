#include "rf/player_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Seated RFPL line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 rf_player_checkpoint_catalog catalog={0};rf_player_checkpoint player={0},out,sentinel;
 rf_vehicle_checkpoint vehicle={0},bad;unsigned char packed[544],saved[544],standing[544];
 uint32_t i;
 catalog.hash=123;catalog.count=2;catalog.health_capacity=catalog.armor_capacity=100;
 catalog.supported[1]=1;catalog.weapons[1]=(rf_weapon_acquire_definition){0,100,10};catalog.reserve_capacity[0]=100;
 player.player.catalog_hash=123;player.player.health=81;player.player.armor=12;player.player.weapon=1;
 player.player.inventory.owned[1]=1;player.player.inventory.loaded[1]=3;player.player.inventory.reserve[0]=17;
 player.position[0]=30;player.position[1]=6;player.position[2]=-167;
 player.body_angles[1]=.75f;player.eye_angles[0]=-.25f;
 vehicle.orientation[0]=vehicle.orientation[4]=vehicle.orientation[8]=1;
 vehicle.health=850;vehicle.alive=vehicle.player_occupied=1;vehicle.velocity[0]=2;
 CHECK(!rf_player_checkpoint_seated_encode(&player,&catalog,&vehicle,packed,sizeof(packed)));
 CHECK(packed[4]==3 && packed[12]==1);
 CHECK(!rf_player_checkpoint_seated_decode(packed,sizeof(packed),&catalog,&vehicle,&out));
 CHECK(!memcmp(&player,&out,sizeof(player)));
 CHECK(!rf_player_checkpoint_encode(&player,&catalog,standing,sizeof(standing)) && standing[4]==1 && standing[12]==0);
 for(i=0;i<544;i++)if(i!=4 && i!=12)CHECK(packed[i]==standing[i]);
 memset(&sentinel,0xa5,sizeof(sentinel));out=sentinel;
 CHECK(rf_player_checkpoint_decode(packed,sizeof(packed),&catalog,&out)==RF_FORMAT && !memcmp(&out,&sentinel,sizeof(out)));
 CHECK(rf_player_checkpoint_seated_decode(standing,sizeof(standing),&catalog,&vehicle,&out)==RF_FORMAT);
 memcpy(saved,packed,sizeof(saved));bad=vehicle;bad.player_occupied=0;
 CHECK(rf_player_checkpoint_seated_decode(packed,sizeof(packed),&catalog,&bad,&out)==RF_FORMAT && !memcmp(&out,&sentinel,sizeof(out)));
 CHECK(rf_player_checkpoint_seated_encode(&player,&catalog,&bad,packed,sizeof(packed))==RF_FORMAT && !memcmp(packed,saved,sizeof(saved)));
 bad=vehicle;bad.orientation[0]=2;CHECK(rf_player_checkpoint_seated_validate(&player,&catalog,&bad)==RF_FORMAT);
 /* Dead occupied host is structural state, not permission for unsafe ejection. */
 bad=vehicle;bad.health=-10;bad.alive=0;
 CHECK(!rf_player_checkpoint_seated_decode(packed,sizeof(packed),&catalog,&bad,&out));
 player.velocity[0]=.0001f;CHECK(rf_player_checkpoint_seated_validate(&player,&catalog,&vehicle)==RF_FORMAT);player.velocity[0]=0;
 player.body_angles[2]=.1f;CHECK(rf_player_checkpoint_seated_validate(&player,&catalog,&vehicle)==RF_FORMAT);player.body_angles[2]=0;
 player.player.inventory.loaded[1]=11;CHECK(rf_player_checkpoint_seated_validate(&player,&catalog,&vehicle)==RF_FORMAT);player.player.inventory.loaded[1]=3;
 for(i=0;i<3;i++){
  memcpy(packed,saved,sizeof(saved));packed[i==0?12:i==1?68:528]=2;out=sentinel;
  CHECK(rf_player_checkpoint_seated_decode(packed,sizeof(packed),&catalog,&vehicle,&out)==RF_FORMAT);
  CHECK(!memcmp(&out,&sentinel,sizeof(out)));
 }
 puts("RFPL3 seated: paired occupancy, legacy isolation, exit-look/inventory rules, dead host and atomic rejection passed");return 0;
}
