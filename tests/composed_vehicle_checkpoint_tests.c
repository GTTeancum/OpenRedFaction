#include "rf/composed_checkpoint.h"
#include "rf/vehicle_checkpoint.h"
#include "rf/remote_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"RFCP3 line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned char terrain[RF_COMPOSED_CHECKPOINT_RFDS_MAX],packed[RF_CHECKPOINT_FILE_MAX],saved[RF_CHECKPOINT_FILE_MAX];
static void put(unsigned char *p,uint32_t n){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
int main(void)
{
    rf_player_checkpoint_catalog catalog={0};rf_player_checkpoint player={0};
    rf_vehicle_checkpoint vehicle={0},decoded;rf_remote_checkpoint remote={0};
    rf_composed_checkpoint_v3 out,sentinel;rf_composed_checkpoint_v2 old;
    unsigned char vc[128],rm[RF_REMOTE_CHECKPOINT_HEADER];uint32_t rn,n,kept,i,terrain_bytes;
    catalog.hash=123;catalog.count=1;catalog.health_capacity=catalog.armor_capacity=100;
    player.player.catalog_hash=123;player.player.weapon=UINT32_MAX;player.player.health=100;
    vehicle.orientation[0]=vehicle.orientation[4]=vehicle.orientation[8]=1;vehicle.health=900;vehicle.alive=1;
    CHECK(!rf_vehicle_checkpoint_encode(&vehicle,vc,sizeof(vc)));
    CHECK(!rf_remote_checkpoint_encode(&remote,77,123,rm,sizeof(rm),&rn));
    memcpy(terrain,"RFDS",4);put(terrain+4,1);put(terrain+8,288);
    CHECK(!rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,288,rm,rn,77,123,vc,128,packed,sizeof(packed),&n));
    CHECK(n==32+RF_PLAYER_CHECKPOINT_BYTES+288+rn+128 && packed[4]==3 && packed[12]==128);
    CHECK(!rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out));
    CHECK(out.vehicle_bytes==128 && out.base.remote_bytes==rn && out.base.base.rfds_bytes==288);
    CHECK(out.vehicle==packed+n-128 && out.base.remote==out.vehicle-rn);
    CHECK(!rf_vehicle_checkpoint_decode(out.vehicle,out.vehicle_bytes,&decoded) && decoded.health==900);
    CHECK(rf_composed_checkpoint_preflight_v2(packed,n,1,&catalog,77,123,&old)==RF_FORMAT);
    /* In-place construction of exact final slices is part of the API. */
    memcpy(saved,packed,n);kept=n;
    CHECK(!rf_composed_checkpoint_encode_v3(1,&player,&catalog,out.base.base.rfds,288,out.base.remote,rn,77,123,out.vehicle,128,packed,sizeof(packed),&n));
    CHECK(n==kept && !memcmp(saved,packed,n));
    memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<4;++i){
        memcpy(packed,saved,n);
        if(i==0)packed[n-1]^=1; /* nested RFVC checksum */
        if(i==1)put(packed+12,127);
        if(i==2)put(packed+28,UINT32_MAX);
        if(i==3)put(packed+24,UINT32_MAX);
        out=sentinel;CHECK(rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out)!=RF_OK);
        CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    }
    memcpy(packed,saved,n);out=sentinel;
    CHECK(rf_composed_checkpoint_preflight_v3(packed,n-1,1,&catalog,77,123,&out)!=RF_OK && !memcmp(&out,&sentinel,sizeof(out)));
    CHECK(rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,78,123,&out)!=RF_OK);
    /* Encoder rejection is atomic even with malformed vehicle payload. */
    vc[127]^=1;kept=0xabcdefu;
    CHECK(rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,288,rm,rn,77,123,vc,128,packed,sizeof(packed),&kept)!=RF_OK);
    CHECK(kept==0xabcdefu && !memcmp(packed,saved,n));vc[127]^=1;
    /* Legacy saves remain accepted and never fabricate a vehicle. */
    for(i=1;i<=3;++i){
        if(i==1)CHECK(!rf_composed_checkpoint_encode(1,&player,&catalog,terrain,288,packed,sizeof(packed),&n));
        if(i==2)CHECK(!rf_composed_checkpoint_encode_v2(1,&player,&catalog,terrain,288,rm,rn,77,123,packed,sizeof(packed),&n));
        if(i==3)CHECK(!rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,288,NULL,0,77,123,NULL,0,packed,sizeof(packed),&n));
        CHECK(!rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out));
        CHECK(!out.vehicle && !out.vehicle_bytes);
    }
    /* Seated RFPL is admitted only with the matching occupied RFVC. */
    vehicle.player_occupied=1;
    CHECK(!rf_vehicle_checkpoint_encode(&vehicle,vc,sizeof(vc)));
    CHECK(!rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,288,NULL,0,77,123,vc,128,packed,sizeof(packed),&n));
    CHECK(packed[32+4]==3 && packed[32+12]==1);
    CHECK(!rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out));
    memcpy(saved,packed,n);
    /* Standing player paired with occupied host is inconsistent. */
    CHECK(!rf_player_checkpoint_encode(&player,&catalog,packed+32,RF_PLAYER_CHECKPOINT_BYTES));
    out=sentinel;CHECK(rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out)==RF_FORMAT);
    CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    memcpy(packed,saved,n);vehicle.player_occupied=0;
    CHECK(!rf_vehicle_checkpoint_encode(&vehicle,packed+n-128,128));
    CHECK(rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out)==RF_FORMAT);
    /* A seated player cannot be smuggled in after removing the RFVC section. */
    memcpy(packed,saved,n);put(packed+12,0);put(packed+8,n-128);
    CHECK(rf_composed_checkpoint_preflight_v3(packed,n-128,1,&catalog,77,123,&out)==RF_FORMAT);
    CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    vehicle.player_occupied=1;vehicle.alive=0;vehicle.health=-1;
    CHECK(!rf_vehicle_checkpoint_encode(&vehicle,vc,sizeof(vc)));
    CHECK(!rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,288,NULL,0,77,123,vc,128,packed,sizeof(packed),&n));
    CHECK(!rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out));
    vehicle.player_occupied=0;vehicle.alive=1;vehicle.health=900;
    CHECK(!rf_vehicle_checkpoint_encode(&vehicle,vc,sizeof(vc)));
    /* Vehicle consumes the existing fixed transport budget, not extra RAM. */
    terrain_bytes=RF_COMPOSED_CHECKPOINT_RFDS_MAX-128;put(terrain+8,terrain_bytes);
    CHECK(!rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,terrain_bytes,NULL,0,77,123,vc,128,packed,sizeof(packed),&n));
    CHECK(n==RF_CHECKPOINT_FILE_MAX && !rf_composed_checkpoint_preflight_v3(packed,n,1,&catalog,77,123,&out));
    memcpy(saved,packed,n);put(terrain+8,terrain_bytes+1);kept=42;
    CHECK(rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,terrain_bytes+1,NULL,0,77,123,vc,128,packed,sizeof(packed),&kept)==RF_RANGE);
    CHECK(kept==42 && !memcmp(packed,saved,n));
    puts("RFCP3: RFVC/RFRM framing, legacy reads, nested rejection, atomic output and fixed transport cap passed");return 0;
}
