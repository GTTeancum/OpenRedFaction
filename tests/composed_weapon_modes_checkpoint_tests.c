#include "rf/composed_checkpoint.h"
#include "rf/clutter_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"RFCP5 line%d\n",__LINE__);return 1;}}while(0)
static void put(unsigned char *p,uint32_t v){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
int main(void)
{
    rf_player_checkpoint_catalog c={0};rf_player_checkpoint p={0};rf_composed_checkpoint_v5 out,sentinel;
    rf_composed_checkpoint_v4 legacy;rf_weapon_modes_checkpoint modes={3,0};
    unsigned char identity[32]={1},props[64],packed[32],terrain[288]={0},wire[1024]={0},saved[1024];
    static unsigned char large[RF_CHECKPOINT_FILE_MAX+1],large_terrain[RF_CHECKPOINT_FILE_MAX];
    uint32_t pn,mn,n,kept,version,terrain_bytes;
    c.hash=123;c.count=2;c.supported[1]=1;c.weapons[1]=(rf_weapon_acquire_definition){0,100,10};
    c.reserve_capacity[0]=100;c.health_capacity=c.armor_capacity=100;
    p.player.catalog_hash=c.hash;p.player.health=100;p.player.weapon=1;p.player.inventory.owned[1]=1;p.player.inventory.loaded[1]=3;
    memcpy(terrain,"RFDS",4);put(terrain+4,1);put(terrain+8,sizeof(terrain));
    CHECK(!rf_clutter_checkpoint_encode(identity,NULL,0,props,sizeof(props),&pn));
    CHECK(!rf_weapon_modes_checkpoint_encode(c.hash,&modes,packed,sizeof(packed),&mn));
    CHECK(!rf_composed_checkpoint_encode_v5(1,&p,&c,terrain,288,NULL,0,0,c.hash,NULL,0,props,pn,identity,packed,mn,wire,sizeof(wire),&n));
    CHECK(n==960&&wire[4]==5);
    CHECK(!rf_composed_checkpoint_preflight_v5(wire,n,1,&c,0,c.hash,identity,&out));
    CHECK(out.modes_present==1&&out.modes.flags==3&&out.modes.conventional_rng==0);
    CHECK(out.base.clutter_bytes==64&&out.base.clutter==wire+864&&out.base.base.base.base.rfds==wire+576);
    CHECK(rf_composed_checkpoint_preflight_v4(wire,n,1,&c,0,c.hash,identity,&legacy)==RF_FORMAT);
    memset(&sentinel,0xa5,sizeof(sentinel));out=sentinel;
    CHECK(rf_composed_checkpoint_preflight_v5(wire,n,1,&c,0,c.hash+1,identity,&out)==RF_FORMAT&&!memcmp(&out,&sentinel,sizeof(out)));
    wire[n-8]^=1;
    CHECK(rf_composed_checkpoint_preflight_v5(wire,n,1,&c,0,c.hash,identity,&out)==RF_FORMAT&&!memcmp(&out,&sentinel,sizeof(out)));
    wire[n-8]^=1;wire[864+12]^=1;
    CHECK(rf_composed_checkpoint_preflight_v5(wire,n,1,&c,0,c.hash,identity,&out)==RF_FORMAT&&!memcmp(&out,&sentinel,sizeof(out)));
    wire[864+12]^=1;memcpy(saved,wire,sizeof(wire));kept=999;
    CHECK(rf_composed_checkpoint_encode_v5(1,&p,&c,terrain,288,NULL,0,0,c.hash,NULL,0,props,pn,identity,packed,mn,wire,n-1,&kept)==RF_RANGE);
    CHECK(kept==999&&!memcmp(wire,saved,sizeof(wire)));
    packed[24]^=1;
    CHECK(rf_composed_checkpoint_encode_v5(1,&p,&c,terrain,288,NULL,0,0,c.hash,NULL,0,props,pn,identity,packed,mn,wire,sizeof(wire),&kept)==RF_FORMAT);
    CHECK(kept==999&&!memcmp(wire,saved,sizeof(wire)));packed[24]^=1;
    /* Every variable source is already at its exact final destination. */
    CHECK(!rf_composed_checkpoint_encode_v5(1,&p,&c,wire+576,288,NULL,0,0,c.hash,NULL,0,wire+864,64,identity,wire+928,32,wire,sizeof(wire),&n));
    CHECK(!memcmp(wire,saved,n));
    for(version=1;version<=4;version++){
        int status;
        if(version==1)status=rf_composed_checkpoint_encode(1,&p,&c,terrain,288,wire,sizeof(wire),&n);
        else if(version==2)status=rf_composed_checkpoint_encode_v2(1,&p,&c,terrain,288,NULL,0,0,c.hash,wire,sizeof(wire),&n);
        else if(version==3)status=rf_composed_checkpoint_encode_v3(1,&p,&c,terrain,288,NULL,0,0,c.hash,NULL,0,wire,sizeof(wire),&n);
        else status=rf_composed_checkpoint_encode_v4(1,&p,&c,terrain,288,NULL,0,0,c.hash,NULL,0,props,pn,identity,wire,sizeof(wire),&n);
        CHECK(!status&&!rf_composed_checkpoint_preflight_v5(wire,n,1,&c,0,c.hash,identity,&out));
        CHECK(!out.modes_present&&!out.modes.flags&&!out.modes.conventional_rng);
    }
    terrain_bytes=RF_CHECKPOINT_FILE_MAX-576-64-32;
    memcpy(large_terrain,"RFDS",4);put(large_terrain+4,1);put(large_terrain+8,terrain_bytes);
    CHECK(!rf_composed_checkpoint_encode_v5(1,&p,&c,large_terrain,terrain_bytes,NULL,0,0,c.hash,NULL,0,props,pn,identity,packed,mn,large,sizeof(large),&n));
    CHECK(n==RF_CHECKPOINT_FILE_MAX&&!rf_composed_checkpoint_preflight_v5(large,n,1,&c,0,c.hash,identity,&out));
    put(large_terrain+8,terrain_bytes+1);kept=999;
    CHECK(rf_composed_checkpoint_encode_v5(1,&p,&c,large_terrain,terrain_bytes+1,NULL,0,0,c.hash,NULL,0,props,pn,identity,packed,mn,large,sizeof(large),&kept)==RF_RANGE&&kept==999);
    puts("PASS RFCP5 mode framing, corruption/catalog errors, exact aliases, legacy absence and unchanged total cap");return 0;
}
