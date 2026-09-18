#include <stdio.h>
#include "rf/apc_checkpoint.h"
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"APC RFVC2 line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t word(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t hash(const unsigned char *p){uint32_t i,h=2166136261u;for(i=0;i<128;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
int main(void)
{
    rf_apc_checkpoint v={0},out,bad,sentinel;rf_vehicle_checkpoint legacy;
    unsigned char data[128],saved[128];uint32_t i;
    v.vehicle.orientation[0]=v.vehicle.orientation[4]=v.vehicle.orientation[8]=1;
    v.vehicle.health=5000;v.vehicle.alive=v.vehicle.player_occupied=1;
    v.vehicle.position[2]=-167;v.vehicle.velocity[0]=3;v.vehicle.angular_velocity[1]=.25f;
    v.primary_reserve=998;v.secondary_reserve=14;v.primary_random=0xdeadbeef;v.aim_pitch=.25f;
    CHECK(!rf_apc_checkpoint_encode(&v,data,sizeof(data)));
    CHECK(data[4]==2 && data[16]==2 && word(data+104)==998);
    CHECK(!rf_apc_checkpoint_decode(data,sizeof(data),&out) && !memcmp(&v,&out,sizeof(v)));
    CHECK(rf_vehicle_checkpoint_decode(data,sizeof(data),&legacy)==RF_FORMAT);
    memcpy(saved,data,sizeof(data));memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<4;i++){
        bad=v;if(i==0)bad.primary_reserve=1000;if(i==1)bad.secondary_reserve=16;
        if(i==2)bad.aim_pitch=1;if(i==3)bad.vehicle.health=5001;
        CHECK(rf_apc_checkpoint_encode(&bad,data,sizeof(data))==RF_FORMAT && !memcmp(data,saved,sizeof(data)));
    }
    data[120]^=1;out=sentinel;
    CHECK(rf_apc_checkpoint_decode(data,sizeof(data),&out)==RF_FORMAT && !memcmp(&out,&sentinel,sizeof(out)));
    memcpy(data,saved,sizeof(data));put(data+16,1);
    put(data+12,hash(data));
    CHECK(rf_apc_checkpoint_decode(data,sizeof(data),&out)==RF_FORMAT);
    for(i=0;i<6;i++){
        bad=v;
        if(i==0)bad.vehicle.orientation[0]=2;
        if(i==1)bad.vehicle.orientation[0]=-1;
        if(i==2)bad.vehicle.velocity[0]=1001;
        if(i==3)bad.vehicle.angular_velocity[0]=101;
        if(i==4)bad.vehicle.position[0]=NAN;
        if(i==5)bad.vehicle.armor=1;
        CHECK(rf_apc_checkpoint_validate(&bad)==RF_FORMAT);
    }
    bad=v;bad.vehicle.health=-50;bad.vehicle.alive=0;
    CHECK(!rf_apc_checkpoint_encode(&bad,data,sizeof(data)) && !rf_apc_checkpoint_decode(data,sizeof(data),&out));
    CHECK(out.vehicle.player_occupied && !out.vehicle.alive && out.vehicle.health==-50);
    legacy=v.vehicle;legacy.health=900;CHECK(!rf_vehicle_checkpoint_encode(&legacy,data,sizeof(data)));
    CHECK(rf_apc_checkpoint_decode(data,sizeof(data),&out)==RF_FORMAT);
    puts("Core APC RFVC2:5000HP, finite ammo, relative aim, RNG, occupancy and strict profile isolation passed");return 0;
}
