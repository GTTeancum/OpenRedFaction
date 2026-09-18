#include "rf/composed_checkpoint.h"
#include "rf/apc_checkpoint.h"
#include "rf/jeep_checkpoint.h"
#include "rf/submarine_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Vehicle profiles line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void put(unsigned char *p,uint32_t value){uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i));}
static void rehash(unsigned char *p,uint32_t bytes){uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}put(p+12,h);}
int main(void)
{
    rf_player_checkpoint_catalog catalog={0};rf_player_checkpoint player={0},decoded_player;
    rf_vehicle_checkpoint driller={0};rf_apc_checkpoint apc={0};rf_jeep_checkpoint jeep={0},restored;
    rf_submarine_checkpoint submarine={0},restored_sub;
    rf_composed_checkpoint_v3 out,sentinel;unsigned char vc[160],terrain[288]={0},data[1200],saved[1200];
    uint32_t kind,occupied,bytes,n,written,at,i;const void *record;
    catalog.hash=123;catalog.count=1;catalog.health_capacity=catalog.armor_capacity=100;
    player.player.catalog_hash=123;player.player.health=90;player.player.weapon=UINT32_MAX;
    driller.health=900;driller.alive=1;driller.orientation[0]=driller.orientation[4]=driller.orientation[8]=1;
    apc.vehicle=driller;apc.vehicle.health=5000;apc.primary_reserve=990;apc.secondary_reserve=12;apc.aim_pitch=.3f;
    jeep.vehicle=driller;jeep.vehicle.health=400;jeep.primary_reserve=981;
    jeep.aim_reference[2]=-1;jeep.aim_reference[4]=jeep.aim_reference[6]=1;
    submarine.vehicle=driller;submarine.vehicle.health=700;submarine.torpedo_reserve=19;submarine.torpedo_cooldown=1.25f;
    memcpy(terrain,"RFDS",4);put(terrain+4,1);put(terrain+8,sizeof(terrain));
    for(kind=1;kind<=4;kind++)for(occupied=0;occupied<=1;occupied++){
        driller.player_occupied=apc.vehicle.player_occupied=jeep.vehicle.player_occupied=submarine.vehicle.player_occupied=occupied;
        jeep.role=occupied;jeep.aim_pitch=occupied?.5f:0;jeep.aim_yaw=occupied?-1:0;
        bytes=kind==3?160:128;
        if(kind==1){CHECK(!rf_vehicle_checkpoint_encode(&driller,vc,bytes));record=&driller;}
        else if(kind==2){CHECK(!rf_apc_checkpoint_encode(&apc,vc,bytes));record=&apc;}
        else if(kind==3){CHECK(!rf_jeep_checkpoint_encode(&jeep,vc,bytes));record=&jeep;}
        else {CHECK(!rf_submarine_checkpoint_encode(&submarine,vc,bytes));record=&submarine;}
        memset(data,0xa5,sizeof(data));
        CHECK(!rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,sizeof(terrain),NULL,0,0,0,vc,bytes,data,sizeof(data),&n));
        CHECK(!rf_composed_checkpoint_preflight_v3(data,n,1,&catalog,0,0,&out));
        CHECK(out.vehicle_profile==kind && out.vehicle_bytes==bytes && data[36]==(occupied?3:1));
        CHECK(!memcmp(out.vehicle,vc,bytes));
        if(occupied){
            CHECK(!rf_player_checkpoint_seated_profile_decode(data+32,544,&catalog,kind,record,&decoded_player));
            CHECK(decoded_player.player.health==90);
        }else CHECK(rf_player_checkpoint_seated_profile_validate(&player,&catalog,kind,record)==RF_FORMAT);
        if(kind==3){CHECK(!rf_jeep_checkpoint_decode(out.vehicle,bytes,&restored));CHECK(restored.role==occupied && !memcmp(restored.aim_reference,jeep.aim_reference,36));}
        if(kind==4){CHECK(!rf_submarine_checkpoint_decode(out.vehicle,bytes,&restored_sub));CHECK(restored_sub.torpedo_reserve==19 && restored_sub.torpedo_cooldown==1.25f);}
        memcpy(saved,data,sizeof(saved));at=n-bytes;memset(&sentinel,0xa5,sizeof(sentinel));
        /* Valid checksum cannot disguise crossed class/version or bad values. */
        for(i=0;i<3;i++){
            memcpy(data,saved,sizeof(saved));
            if(i==0)put(data+at+16,kind==3?1:kind+1);
            if(i==1)put(data+at+4,kind==3?1:kind+1);
            if(i==2)put(data+at+20,4);
            rehash(data+at,bytes);out=sentinel;
            CHECK(rf_composed_checkpoint_preflight_v3(data,n,1,&catalog,0,0,&out)!=RF_OK && !memcmp(&out,&sentinel,sizeof(out)));
        }
        memcpy(data,saved,sizeof(saved));vc[12]^=1;written=17;
        CHECK(rf_composed_checkpoint_encode_v3(1,&player,&catalog,terrain,sizeof(terrain),NULL,0,0,0,vc,bytes,data,sizeof(data),&written)!=RF_OK);
        CHECK(written==17 && !memcmp(data,saved,sizeof(data)));vc[12]^=1;
        if(occupied){
            CHECK(!rf_player_checkpoint_encode(&player,&catalog,data+32,544));out=sentinel;
            CHECK(rf_composed_checkpoint_preflight_v3(data,n,1,&catalog,0,0,&out)==RF_FORMAT && !memcmp(&out,&sentinel,sizeof(out)));
        }
    }
    CHECK(!rf_composed_checkpoint_encode(1,&player,&catalog,terrain,sizeof(terrain),data,sizeof(data),&n));
    CHECK(!rf_composed_checkpoint_preflight_v3(data,n,1,&catalog,0,0,&out) && !out.vehicle_profile);
    CHECK(!rf_composed_checkpoint_encode_v2(1,&player,&catalog,terrain,sizeof(terrain),NULL,0,0,0,data,sizeof(data),&n));
    CHECK(!rf_composed_checkpoint_preflight_v3(data,n,1,&catalog,0,0,&out) && !out.vehicle_profile);
    puts("RFCP3 real Driller/APC/Jeep/submarine profiles: parked/seated,5000HP,role/reference retention, crossed rejection and legacy compatibility passed");return 0;
}
