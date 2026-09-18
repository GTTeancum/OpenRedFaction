#include "rf/vehicle_checkpoint.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vehicle checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vehicle_checkpoint v={0},decoded,sentinel,bad;unsigned char bytes[RF_VEHICLE_CHECKPOINT_BYTES],saved[RF_VEHICLE_CHECKPOINT_BYTES],roundtrip[RF_VEHICLE_CHECKPOINT_BYTES];uint32_t i;
    v.position[0]=32.5f;v.position[1]=5.4f;v.position[2]=-167;
    v.orientation[2]=-1;v.orientation[4]=1;v.orientation[6]=1;
    v.velocity[0]=3.25f;v.velocity[1]=-.25f;v.angular_velocity[1]=.375f;
    v.health=875;v.alive=v.player_occupied=1;v.accepted_drill_cuts=2;v.drill_spin=8;
    CHECK(!rf_vehicle_checkpoint_encode(&v,bytes,sizeof(bytes)));
    CHECK(!memcmp(bytes,"RFVC\1\0\0\0\200\0\0\0",12));
    CHECK(bytes[20]==3 && bytes[104]==2 && bytes[108]==0 && bytes[109]==0 && bytes[110]==0 && bytes[111]==0x41);
    CHECK(!rf_vehicle_checkpoint_decode(bytes,sizeof(bytes),&decoded));CHECK(!memcmp(&v,&decoded,sizeof(v)));
    CHECK(!rf_vehicle_checkpoint_encode(&decoded,roundtrip,sizeof(roundtrip)));CHECK(!memcmp(bytes,roundtrip,sizeof(bytes)));
    memcpy(saved,bytes,sizeof(bytes));memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<sizeof(bytes);i++){
        bytes[i]^=1;decoded=sentinel;
        CHECK(rf_vehicle_checkpoint_decode(bytes,sizeof(bytes),&decoded)==RF_FORMAT && !memcmp(&decoded,&sentinel,sizeof(decoded)));
        bytes[i]^=1;
    }
    decoded=sentinel;CHECK(rf_vehicle_checkpoint_decode(bytes,sizeof(bytes)-1,&decoded)==RF_FORMAT && !memcmp(&decoded,&sentinel,sizeof(decoded)));
    bad=v;bad.orientation[0]=1;CHECK(rf_vehicle_checkpoint_encode(&bad,bytes,sizeof(bytes))==RF_FORMAT && !memcmp(bytes,saved,sizeof(bytes)));
    bad=v;bad.orientation[2]=1;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT); /* Reflection, not rotation. */
    bad=v;bad.velocity[0]=INFINITY;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT);
    bad=v;bad.angular_velocity[0]=NAN;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT);
    bad=v;bad.health=901;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT);
    bad=v;bad.armor=1;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT);
    bad=v;bad.player_occupied=2;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT);
    bad=v;bad.accepted_drill_cuts=26;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT);
    bad=v;bad.drill_spin=12;CHECK(rf_vehicle_checkpoint_validate(&bad)==RF_FORMAT);
    /* A wreck may still have a driver awaiting collision-validated ejection. */
    v.health=-40;v.alive=0;
    CHECK(!rf_vehicle_checkpoint_encode(&v,bytes,sizeof(bytes)));
    CHECK(!rf_vehicle_checkpoint_decode(bytes,sizeof(bytes),&decoded));CHECK(decoded.player_occupied && !decoded.alive && decoded.health==-40);
    puts("PASS RFVC128-byte living/wreck roundtrip, corruption rejection, state validation, atomic outputs");return 0;
}
