#include "rf/fighter_checkpoint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Fighter checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void rehash(unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<160;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}put(p+12,h);}
int main(void)
{
    rf_fighter_checkpoint v={0},out,bad,sentinel;rf_vehicle_checkpoint legacy;
    unsigned char data[160],saved[160];uint32_t i;
    v.vehicle.orientation[0]=v.vehicle.orientation[4]=v.vehicle.orientation[8]=1;
    v.vehicle.health=900;v.vehicle.alive=v.vehicle.player_occupied=1;
    v.vehicle.position[0]=-25;v.vehicle.position[1]=-15;
    v.vehicle.velocity[0]=2;v.vehicle.velocity[1]=-1;v.vehicle.velocity[2]=3;
    v.vehicle.angular_velocity[0]=.125f;v.vehicle.angular_velocity[1]=-.25f;
    v.primary_reserve=870;v.rocket_reserve=17;v.primary_cooldown=-1.f/60;v.rocket_cooldown=2.25f;
    v.primary_shots=30;v.rocket_shots=3;
    CHECK(!rf_fighter_checkpoint_encode(&v,data,sizeof(data)));
    CHECK(data[4]==5 && data[16]==5 && data[108]==17);
    CHECK(!rf_fighter_checkpoint_decode(data,sizeof(data),&out));
    CHECK(!memcmp(&v,&out,sizeof(v)));
    CHECK(rf_vehicle_checkpoint_decode(data,sizeof(data),&legacy)==RF_FORMAT);
    memcpy(saved,data,sizeof(data));memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<16;i++){
        bad=v;
        if(i==0)bad.vehicle.health=901;
        if(i==1)bad.rocket_reserve=21;
        if(i==2)bad.rocket_cooldown=-.051f;
        if(i==3)bad.rocket_cooldown=3.01f;
        if(i==4)bad.rocket_cooldown=NAN;
        if(i==5)bad.vehicle.velocity[0]=1001;
        if(i==6)bad.vehicle.angular_velocity[0]=101;
        if(i==7)bad.vehicle.position[0]=NAN;
        if(i==8)bad.vehicle.orientation[0]=-1;
        if(i==9)bad.vehicle.armor=1;
        if(i==10)bad.vehicle.accepted_drill_cuts=1;
        if(i==11)bad.vehicle.drill_spin=.1f;
        if(i==12)bad.vehicle.alive=0;
        if(i==13)bad.primary_reserve=901;
        if(i==14)bad.primary_cooldown=.051f;
        if(i==15)bad.primary_cooldown=-.051f;
        CHECK(rf_fighter_checkpoint_encode(&bad,data,sizeof(data))==RF_FORMAT);
        CHECK(!memcmp(data,saved,sizeof(data)));
    }
    /* Recompute hash so semantic validation, not just checksum, rejects these. */
    for(i=0;i<12;i++){
        memcpy(data,saved,sizeof(data));
        if(i==0)put(data+4,1);
        if(i==1)put(data+16,2);
        if(i==2)put(data+20,7);
        if(i==3)put(data+108,21);
        if(i>=4)put(data+128+(i-4)*4,1);
        rehash(data);out=sentinel;
        CHECK(rf_fighter_checkpoint_decode(data,sizeof(data),&out)==RF_FORMAT);
        CHECK(!memcmp(&out,&sentinel,sizeof(out)));
    }
    memcpy(data,saved,sizeof(data));data[32]^=1;out=sentinel;
    CHECK(rf_fighter_checkpoint_decode(data,sizeof(data),&out)==RF_FORMAT && !memcmp(&out,&sentinel,sizeof(out)));
    CHECK(rf_fighter_checkpoint_decode(saved,159,&out)==RF_FORMAT && !memcmp(&out,&sentinel,sizeof(out)));
    bad=v;bad.vehicle.health=-5;bad.vehicle.alive=0;bad.rocket_reserve=0;bad.rocket_cooldown=3;
    CHECK(!rf_fighter_checkpoint_encode(&bad,data,sizeof(data)));
    CHECK(!rf_fighter_checkpoint_decode(data,sizeof(data),&out));
    CHECK(out.vehicle.player_occupied && !out.vehicle.alive && out.rocket_cooldown==3 && !out.rocket_reserve);
    bad=v;bad.vehicle.player_occupied=0;bad.rocket_reserve=20;bad.rocket_cooldown=0;
    CHECK(!rf_fighter_checkpoint_encode(&bad,data,sizeof(data)) && !rf_fighter_checkpoint_decode(data,sizeof(data),&out));
    legacy=v.vehicle;CHECK(!rf_vehicle_checkpoint_encode(&legacy,data,RF_VEHICLE_CHECKPOINT_BYTES));
    CHECK(rf_fighter_checkpoint_decode(data,sizeof(data),&out)==RF_FORMAT);
    puts("Fighter RFVC5: pose, motion, 900HP, finite ammo/cooldowns, occupancy and atomic rejection passed");return 0;
}
