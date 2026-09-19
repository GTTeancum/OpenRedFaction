#include "rf/weapon_modes_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"weapon modes checkpoint line%d\n",__LINE__);return 1;}}while(0)
static void checksum(unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<32;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}for(i=0;i<4;i++)p[12+i]=(unsigned char)(h>>(8*i));}
int main(void)
{
    const uint32_t catalog=0x12345678u,rngs[3]={0,UINT32_MAX,0x89abcdefu};
    unsigned char wire[33],saved[33];uint32_t bytes=999,i,j;
    rf_weapon_modes_checkpoint state={0},out={0xaaaaaaaa,0xbbbbbbbb},before=out;
    for(i=0;i<4;i++)for(j=0;j<3;j++){
        memset(wire,0xa5,sizeof(wire));state.flags=i;state.conventional_rng=rngs[j];
        CHECK(!rf_weapon_modes_checkpoint_encode(catalog,&state,wire,sizeof(wire),&bytes)&&bytes==32&&wire[32]==0xa5);
        CHECK(!memcmp(wire,"RFWM\1\0\0\0\40\0\0\0",12));
        CHECK(wire[16]==0x78&&wire[17]==0x56&&wire[18]==0x34&&wire[19]==0x12);
        CHECK(!rf_weapon_modes_checkpoint_decode(wire,bytes,catalog,&out)&&out.flags==state.flags&&out.conventional_rng==state.conventional_rng);
    }
    memcpy(saved,wire,sizeof(wire));out=before;bytes=999;
    CHECK(rf_weapon_modes_checkpoint_decode(wire,32,catalog+1,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    for(i=0;i<32;i++){
        memcpy(wire,saved,sizeof(wire));wire[i]^=0x80;
        CHECK(rf_weapon_modes_checkpoint_decode(wire,32,catalog,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    }
    memcpy(wire,saved,sizeof(wire));wire[20]=4;checksum(wire);
    CHECK(rf_weapon_modes_checkpoint_decode(wire,32,catalog,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    memcpy(wire,saved,sizeof(wire));wire[28]=1;checksum(wire);
    CHECK(rf_weapon_modes_checkpoint_decode(wire,32,catalog,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    memcpy(wire,saved,sizeof(wire));
    CHECK(rf_weapon_modes_checkpoint_decode(wire,31,catalog,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    CHECK(rf_weapon_modes_checkpoint_decode(wire,33,catalog,&out)==RF_FORMAT&&!memcmp(&out,&before,sizeof(out)));
    state.flags=4;
    CHECK(rf_weapon_modes_checkpoint_encode(catalog,&state,wire,sizeof(wire),&bytes)==RF_FORMAT&&bytes==999&&!memcmp(wire,saved,sizeof(wire)));
    state.flags=0;
    CHECK(rf_weapon_modes_checkpoint_encode(catalog,&state,wire,31,&bytes)==RF_RANGE&&bytes==999&&!memcmp(wire,saved,sizeof(wire)));
    puts("PASS RFWM modes/RNG roundtrip, fixed LE framing, corruption/catalog/flags rejection and atomic errors");return 0;
}
