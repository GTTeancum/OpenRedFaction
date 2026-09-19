#include "rf/composed_checkpoint.h"
#include "rf/clutter_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"RFCP4 line%d\n",__LINE__);return 1;}}while(0)
static void put(unsigned char *p,uint32_t v){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
int main(void)
{
    rf_player_checkpoint_catalog c={0};rf_player_checkpoint p={0};rf_composed_checkpoint_v4 out,sentinel;
    rf_composed_checkpoint_v3 legacy;rf_clutter_checkpoint_record row={45,3,-11,2,3,0},decoded;
    unsigned char identity[32]={1},bad_identity[32]={2},props[88],terrain[288]={0},wire[2048],saved[2048];
    uint32_t pn,n,count,kept;
    c.hash=123;c.count=2;c.supported[1]=1;c.weapons[1]=(rf_weapon_acquire_definition){0,100,10};c.reserve_capacity[0]=100;c.health_capacity=c.armor_capacity=100;
    p.player.catalog_hash=c.hash;p.player.health=100;p.player.weapon=1;p.player.inventory.owned[1]=1;p.player.inventory.loaded[1]=3;
    memcpy(terrain,"RFDS",4);put(terrain+4,1);put(terrain+8,sizeof(terrain));
    CHECK(!rf_clutter_checkpoint_encode(identity,&row,1,props,sizeof(props),&pn));
    CHECK(!rf_composed_checkpoint_encode_v4(1,&p,&c,terrain,288,NULL,0,0,0,NULL,0,props,pn,identity,wire,sizeof(wire),&n));
    CHECK(n==952&&wire[4]==4);
    CHECK(!rf_composed_checkpoint_preflight_v4(wire,n,1,&c,0,0,identity,&out));
    CHECK(out.clutter_count==1&&out.clutter_bytes==88&&out.clutter==wire+n-88&&out.base.base.base.rfds==wire+576);
    CHECK(!rf_clutter_checkpoint_decode(out.clutter,out.clutter_bytes,identity,&decoded,1,&count)&&decoded.health==-11);
    CHECK(rf_composed_checkpoint_preflight_v3(wire,n,1,&c,0,0,&legacy)==RF_FORMAT);
    memset(&sentinel,0xa5,sizeof(sentinel));out=sentinel;
    CHECK(rf_composed_checkpoint_preflight_v4(wire,n,1,&c,0,0,bad_identity,&out)==RF_FORMAT&&!memcmp(&out,&sentinel,sizeof(out)));
    wire[n-1]^=1;CHECK(rf_composed_checkpoint_preflight_v4(wire,n,1,&c,0,0,identity,&out)==RF_FORMAT&&!memcmp(&out,&sentinel,sizeof(out)));wire[n-1]^=1;
    memcpy(saved,wire,sizeof(wire));kept=999;
    CHECK(rf_composed_checkpoint_encode_v4(1,&p,&c,terrain,288,NULL,0,0,0,NULL,0,props,pn,identity,wire,n-1,&kept)==RF_RANGE);
    CHECK(kept==999&&!memcmp(wire,saved,sizeof(wire)));
    CHECK(!rf_composed_checkpoint_encode_v3(1,&p,&c,terrain,288,NULL,0,0,0,NULL,0,wire,sizeof(wire),&n));
    CHECK(!rf_composed_checkpoint_preflight_v4(wire,n,1,&c,0,0,identity,&out)&&!out.clutter&&!out.clutter_count&&!out.clutter_bytes);
    puts("PASS composed prop framing, nested identity/checksum, atomic capacity failure and legacy absence");return 0;
}
