#include "rf/geomod_retained_material_digest.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int rejected(rf_geomod_retained_material_input *v)
{unsigned char out[32],old[32];memset(out,0xa5,32);memcpy(old,out,32);return rf_geomod_retained_material_digest(v,out)!=RF_OK&&!memcmp(out,old,32);}
int main(void)
{
    /* Independent408-byte canonical journal and original CRT/1555 oracle. */
    static const unsigned char expected[32]={0x32,0x79,0x3f,0xb7,0xac,0x7a,0xf0,0x2f,0x42,0xcb,0xaf,0x1f,0x13,0x7c,0x51,0xa2,0x7d,0x11,0xea,0x2a,0x00,0x0b,0x0d,0x61,0xaf,0xaa,0xba,0xa2,0x7f,0xfd,0x86,0x7a};
    rf_geomod_retained_material_map maps[2]={0};rf_geomod_retained_material_input in={0};
    rf_geomod_publication_origin origins[2]={{0,94,550,149},{1,94,UINT32_MAX,149}};uint16_t face_maps[2]={65535,0};
    unsigned char source[32],substrate[32],digest[32],again[32];uint32_t i;
    rf_random_state random={1};unsigned char rgb[16*3];
    for(i=0;i<32;i++){source[i]=(unsigned char)i;substrate[i]=(unsigned char)(32+i);}
    for(i=0;i<2;i++) {
        maps[i].plane[2]=1;maps[i].plane[3]=i?-1.f:0.f;maps[i].minimum[2]=maps[i].maximum[2]=(float)i;
        maps[i].maximum[0]=maps[i].maximum[1]=1;maps[i].width=maps[i].height=4;maps[i].x=i*4;
        maps[i].projection.axes[1]=1;maps[i].projection.scale[0]=maps[i].projection.scale[1]=2.f/512;
        maps[i].projection.offset[0]=(i*4+1.f)/512;maps[i].projection.offset[1]=1.f/512;
        maps[i].base_seed=random.value;CHECK(!rf_geomod_light_noise(rgb,sizeof(rgb),48,16,1,&random));
    }
    in.source_identity=source;in.substrate_identity=substrate;in.maps=maps;in.map_count=in.baked=2;
    in.origins=origins;in.face_maps=face_maps;in.face_count=2;in.owner=94;
    in.serial=in.cuts=in.owner_generation=in.owner_cuts=2;in.random=random.value;in.x=8;in.row=4;in.material_policy=1;
    CHECK(!rf_geomod_retained_material_digest(&in,digest));
    CHECK(maps[1].base_seed==2179684817u && in.random==1423890337u);
    CHECK(!memcmp(digest,expected,32));
    printf("CANONICAL ");for(i=0;i<32;i++)printf("%02x",digest[i]);printf(" second_seed%u continuation%u\n",maps[1].base_seed,in.random);
    CHECK(!rf_geomod_retained_material_digest(&in,again)&&!memcmp(digest,again,32));
    /* Map1 is retained but unused by either final face. Removing it while
     * repairing cursor/RNG is a different valid history and must hash apart. */
    in.map_count=in.baked=1;in.x=4;in.random=maps[1].base_seed;
    CHECK(!rf_geomod_retained_material_digest(&in,again)&&memcmp(digest,again,32));
    in.map_count=in.baked=2;in.x=8;in.random=random.value;
    /* Same journal with different final binding is distinct, too. Geometry
     * containment belongs to the separate candidate validation gate. */
    face_maps[1]=1;CHECK(!rf_geomod_retained_material_digest(&in,again)&&memcmp(digest,again,32));face_maps[1]=0;
    maps[1].plane[3]=-2;CHECK(!rf_geomod_retained_material_digest(&in,again)&&memcmp(digest,again,32));maps[1].plane[3]=-1;
    origins[0].reference=999;CHECK(!rf_geomod_retained_material_digest(&in,again)&&!memcmp(digest,again,32));origins[0].reference=149;
    in.random^=1;CHECK(rejected(&in));in.random^=1;
    maps[1].base_seed^=1;CHECK(rejected(&in));maps[1].base_seed^=1;
    maps[1].x=8;CHECK(rejected(&in));maps[1].x=4;
    maps[1].projection.offset[0]+=1.f/512;CHECK(rejected(&in));maps[1].projection.offset[0]-=1.f/512;
    maps[1].width=3;CHECK(rejected(&in));maps[1].width=4;
    maps[1].material_token=7;CHECK(rejected(&in));maps[1].material_token=0;
    maps[1].plane[2]=2;CHECK(rejected(&in));maps[1].plane[2]=1;
    maps[1].minimum[0]=NAN;CHECK(rejected(&in));maps[1].minimum[0]=0;
    in.sample=1;CHECK(rejected(&in));in.sample=0;
    in.baked=1;CHECK(rejected(&in));in.baked=2;
    in.owner_generation=1;CHECK(rejected(&in));in.owner_generation=2;
    face_maps[0]=0;CHECK(rejected(&in));face_maps[0]=65535;
    face_maps[1]=2;CHECK(rejected(&in));face_maps[1]=0;
    in.serial=UINT32_MAX;CHECK(rejected(&in));in.serial=2;
    CHECK(!rf_geomod_retained_material_digest(&in,again)&&!memcmp(digest,again,32));
    {rf_geomod_retained_material_input empty={0};empty.owner=94;empty.source_identity=source;empty.substrate_identity=substrate;empty.material_policy=1;
     CHECK(!rf_geomod_retained_material_digest(&empty,again));
     empty.serial=3;CHECK(!rf_geomod_retained_material_digest(&empty,again));
     empty.random=1;CHECK(!rf_geomod_retained_material_digest(&empty,again));
     empty.random=2;CHECK(rejected(&empty));empty.random=1;
     empty.owner_generation=2;CHECK(rejected(&empty));empty.owner_generation=3;
     CHECK(!rf_geomod_retained_material_digest(&empty,again));
     empty.random=0;CHECK(rejected(&empty));}
    puts("PASS retained material journal historical maps, exact seed continuation, packing, base noise, visibility, rollback");return 0;
}
