#include "rf/jeep_checkpoint.h"
#include "rf/apc_checkpoint.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Jeep checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_jeep_checkpoint v={0},out,bad,sentinel;rf_apc_checkpoint apc={0};rf_vehicle_checkpoint driller;
    unsigned char data[160],saved[160],legacy[128];uint32_t i;
    v.vehicle.orientation[0]=v.vehicle.orientation[4]=v.vehicle.orientation[8]=1;
    v.aim_reference[2]=-1;v.aim_reference[4]=v.aim_reference[6]=1;
    v.vehicle.health=400;v.vehicle.alive=v.vehicle.player_occupied=1;
    v.vehicle.position[2]=-167;v.primary_reserve=987;v.primary_random=0xdeadbeef;
    v.role=1;v.aim_pitch=.5f;v.aim_yaw=-1;
    CHECK(!rf_jeep_checkpoint_encode(&v,data,sizeof(data)));
    CHECK(data[4]==3 && data[8]==160 && data[16]==3 && data[108]==1);
    CHECK(!rf_jeep_checkpoint_decode(data,sizeof(data),&out) && !memcmp(&v,&out,sizeof(v)));
    CHECK(memcmp(out.vehicle.orientation,out.aim_reference,36)); /* Independent view retained. */
    CHECK(rf_vehicle_checkpoint_decode(data,sizeof(data),&driller)==RF_FORMAT);
    CHECK(rf_apc_checkpoint_decode(data,sizeof(data),&apc)==RF_FORMAT);
    memcpy(saved,data,sizeof(data));memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<9;i++){
        bad=v;
        if(i==0)bad.vehicle.health=401;
        if(i==1)bad.primary_reserve=1000;
        if(i==2)bad.role=2;
        if(i==3)bad.vehicle.player_occupied=0;
        if(i==4)bad.aim_pitch=1.048f;
        if(i==5)bad.aim_yaw=3.142f;
        if(i==6)bad.aim_reference[0]=NAN;
        if(i==7)bad.aim_reference[4]=2;
        if(i==8)bad.role=0; /* Driver may not inherit gunner aim. */
        CHECK(rf_jeep_checkpoint_encode(&bad,data,sizeof(data))==RF_FORMAT && !memcmp(data,saved,sizeof(data)));
    }
    data[124]^=1;out=sentinel;
    CHECK(rf_jeep_checkpoint_decode(data,sizeof(data),&out)==RF_FORMAT && !memcmp(&out,&sentinel,sizeof(out)));
    bad=v;bad.role=0;bad.aim_pitch=bad.aim_yaw=0;
    CHECK(!rf_jeep_checkpoint_encode(&bad,data,sizeof(data)) && !rf_jeep_checkpoint_decode(data,sizeof(data),&out));
    CHECK(out.role==0 && out.vehicle.player_occupied);
    bad.vehicle.player_occupied=0;CHECK(!rf_jeep_checkpoint_validate(&bad));
    bad=v;bad.vehicle.alive=0;bad.vehicle.health=-10;
    CHECK(!rf_jeep_checkpoint_encode(&bad,data,sizeof(data)) && !rf_jeep_checkpoint_decode(data,sizeof(data),&out));
    CHECK(out.role==1 && out.vehicle.player_occupied && !out.vehicle.alive);
    driller=v.vehicle;CHECK(!rf_vehicle_checkpoint_encode(&driller,legacy,sizeof(legacy)));
    CHECK(rf_jeep_checkpoint_decode(legacy,sizeof(legacy),&out)==RF_FORMAT);
    apc.vehicle=v.vehicle;apc.primary_reserve=999;apc.secondary_reserve=15;
    CHECK(!rf_apc_checkpoint_encode(&apc,legacy,sizeof(legacy)));
    CHECK(rf_jeep_checkpoint_decode(legacy,sizeof(legacy),&out)==RF_FORMAT);
    puts("Jeep RFVC3: authored400HP/ammo, driver/gunner, independent aim reference, policy limits and profile isolation passed");return 0;
}
