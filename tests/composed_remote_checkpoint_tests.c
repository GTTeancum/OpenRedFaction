#include "rf/composed_checkpoint.h"
#include "rf/remote_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"composed remote line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned char terrain[RF_CHECKPOINT_FILE_MAX],remote[RF_REMOTE_CHECKPOINT_MAX],bytes[RF_CHECKPOINT_FILE_MAX],saved[RF_CHECKPOINT_FILE_MAX];
static rf_remote_checkpoint charges;
static void put(unsigned char *p,uint32_t n){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
int main(void)
{
    rf_player_checkpoint_catalog c={0};rf_player_checkpoint player={0};rf_composed_checkpoint_v2 out,before;rf_composed_checkpoint legacy;
    uint32_t count,remote_bytes,n;float p[3]={0},d[3]={0,0,1};
    c.hash=20;c.count=2;c.supported[1]=1;c.weapons[1]=(rf_weapon_acquire_definition){0,100,10};c.reserve_capacity[0]=100;c.health_capacity=c.armor_capacity=100;
    player.player.catalog_hash=20;player.player.health=100;player.player.weapon=1;player.player.inventory.owned[1]=1;player.player.inventory.loaded[1]=3;
    memcpy(terrain,"RFDS",4);put(terrain+4,1);put(terrain+8,288);
    CHECK(!rf_remote_charge_launch(charges.charges+3,7,0,p,d,10,.051f,20));charges.owner_keys[3]=42;
    CHECK(!rf_remote_checkpoint_encode(&charges,10,20,remote,sizeof(remote),&remote_bytes));
    CHECK(!rf_composed_checkpoint_encode_v2(1,&player,&c,terrain,288,remote,remote_bytes,10,20,bytes,sizeof(bytes),&count));
    CHECK(count==864+remote_bytes && bytes[4]==2);
    CHECK(!rf_composed_checkpoint_preflight_v2(bytes,count,1,&c,10,20,&out));
    CHECK(out.base.rfds_bytes==288 && out.base.rfds==bytes+576 && out.remote==bytes+864 && out.remote_bytes==remote_bytes);
    CHECK(rf_composed_checkpoint_preflight(bytes,count,1,&c,&legacy)==RF_FORMAT);
    before=out;CHECK(rf_composed_checkpoint_preflight_v2(bytes,count,1,&c,11,20,&out)!=RF_OK && !memcmp(&before,&out,sizeof(out)));
    memcpy(saved,bytes,count);put(bytes+28,remote_bytes-1);
    CHECK(rf_composed_checkpoint_preflight_v2(bytes,count,1,&c,10,20,&out)!=RF_OK && !memcmp(&before,&out,sizeof(out)));
    memcpy(bytes,saved,count);bytes[864+4]=2;
    CHECK(rf_composed_checkpoint_preflight_v2(bytes,count,1,&c,10,20,&out)!=RF_OK && !memcmp(&before,&out,sizeof(out)));
    memset(bytes,0xa5,sizeof(bytes));memcpy(saved,bytes,sizeof(bytes));n=77;remote[4]=2;
    CHECK(rf_composed_checkpoint_encode_v2(1,&player,&c,terrain,288,remote,remote_bytes,10,20,bytes,sizeof(bytes),&n)!=RF_OK && n==77 && !memcmp(bytes,saved,sizeof(bytes)));remote[4]=1;
    CHECK(!rf_composed_checkpoint_encode_v2(1,&player,&c,terrain,288,NULL,0,10,20,bytes,sizeof(bytes),&count));
    CHECK(!rf_composed_checkpoint_preflight_v2(bytes,count,1,&c,10,20,&out) && !out.remote && !out.remote_bytes);
    CHECK(!rf_composed_checkpoint_encode(1,&player,&c,terrain,288,bytes,sizeof(bytes),&count));
    CHECK(!rf_composed_checkpoint_preflight_v2(bytes,count,1,&c,10,20,&out) && !out.remote && !out.remote_bytes);
    CHECK(!rf_composed_checkpoint_preflight(bytes,count,1,&c,&legacy));
    /* Optional payload consumes, rather than expands, the fixed RFSG budget. */
    n=RF_CHECKPOINT_FILE_MAX-576-remote_bytes;put(terrain+8,n);
    CHECK(!rf_composed_checkpoint_encode_v2(1,&player,&c,terrain,n,remote,remote_bytes,10,20,bytes,sizeof(bytes),&count) && count==RF_CHECKPOINT_FILE_MAX);
    put(terrain+8,n+1);CHECK(rf_composed_checkpoint_encode_v2(1,&player,&c,terrain,n+1,remote,remote_bytes,10,20,bytes,sizeof(bytes),&count)==RF_RANGE);
    puts("RFCP2 optional remote, RFCP1 compatibility, fixed cap and nested rejection passed");return 0;
}
