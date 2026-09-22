#include "rf/player_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"RFPL paired magazine line%d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    rf_player_checkpoint_catalog c={0},bad;rf_player_checkpoint p={0},out,before;
    unsigned char wire[RF_PLAYER_CHECKPOINT_BYTES],saved[RF_PLAYER_CHECKPOINT_BYTES];
    c.hash=123;c.count=3;c.supported[1]=c.supported[2]=1;
    c.weapons[1]=(rf_weapon_acquire_definition){0,100,20};c.weapons[2]=(rf_weapon_acquire_definition){0,100,10};
    c.reserve_capacity[0]=100;c.health_capacity=c.armor_capacity=100;c.mode_owner[2]=2;
    p.player.catalog_hash=c.hash;p.player.health=100;p.player.weapon=1;
    p.player.inventory.owned[1]=1;p.player.inventory.loaded[1]=17;p.player.inventory.loaded[2]=10;
    CHECK(!rf_player_checkpoint_encode(&p,&c,wire,sizeof(wire)));
    CHECK(!rf_player_checkpoint_decode(wire,sizeof(wire),&c,&out));
    CHECK(out.player.inventory.owned[1]==1&&!out.player.inventory.owned[2]&&out.player.inventory.loaded[1]==17&&out.player.inventory.loaded[2]==10);
    memcpy(saved,wire,sizeof(wire));memset(&before,0xa5,sizeof(before));out=before;
    /* A valid wire requires the explicitly opted-in mode catalog. */
    bad=c;bad.mode_owner[2]=0;
    CHECK(rf_player_checkpoint_decode(wire,sizeof(wire),&bad,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    p.player.inventory.loaded[1]=0;p.player.inventory.owned[1]=0;p.player.weapon=UINT32_MAX;
    CHECK(rf_player_checkpoint_encode(&p,&c,wire,sizeof(wire))==RF_FORMAT&&!memcmp(wire,saved,sizeof(wire)));
    p.player.inventory.owned[1]=1;p.player.weapon=1;p.player.inventory.owned[2]=1;
    CHECK(rf_player_checkpoint_encode(&p,&c,wire,sizeof(wire))==RF_FORMAT);
    p.player.inventory.owned[2]=0;p.player.weapon=2;
    CHECK(rf_player_checkpoint_encode(&p,&c,wire,sizeof(wire))==RF_FORMAT);
    p.player.weapon=1;p.player.inventory.loaded[2]=11;
    CHECK(rf_player_checkpoint_encode(&p,&c,wire,sizeof(wire))==RF_FORMAT);
    p.player.inventory.loaded[2]=10;
    bad=c;bad.mode_owner[2]=4;CHECK(rf_player_checkpoint_encode(&p,&bad,wire,sizeof(wire))==RF_RANGE);
    bad=c;bad.mode_owner[2]=3;CHECK(rf_player_checkpoint_encode(&p,&bad,wire,sizeof(wire))==RF_RANGE);
    bad=c;bad.supported[1]=0;CHECK(rf_player_checkpoint_encode(&p,&bad,wire,sizeof(wire))==RF_RANGE);
    bad=c;bad.mode_owner[1]=3;CHECK(rf_player_checkpoint_encode(&p,&bad,wire,sizeof(wire))==RF_RANGE);
    bad=c;bad.supported[2]=0;CHECK(rf_player_checkpoint_encode(&p,&bad,wire,sizeof(wire))==RF_RANGE);
    CHECK(!memcmp(wire,saved,sizeof(wire)));
    puts("PASS RFPL paired magazine roundtrip, ownership/selection/capacity admission and invalid catalog rejection");return 0;
}
