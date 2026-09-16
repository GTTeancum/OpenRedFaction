#include "rf/liquid_damage.h"
#include "rf/entity_assets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    const uint32_t wet[]={0,0x1000,0x2000,0x3000},kinds[]={0,3};const float dt[]={0,.0625f,1};
    rf_liquid_damage_input in={0};rf_liquid_damage_result out,old;unsigned type,w,k,t,cases=0;
    in.target=42;in.room_present=1;in.lava_per_second=35;in.acid_per_second=25;
    /* Exact96 cases captured from whole original421240. */
    for(type=0;type<4;type++)for(w=0;w<4;w++)for(k=0;k<2;k++)for(t=0;t<3;t++) {
        uint32_t emit=wet[w] && (type==3 || (type==2 && kinds[k]==3));
        in.liquid_type=(int32_t)type;in.actor_flags_810=wet[w];in.actor_kind_1fc=kinds[k];in.frame_seconds=dt[t];
        CHECK(rf_liquid_damage_prepare(&in,&out)==RF_OK && out.emit==emit);
        if(emit) {
            CHECK(out.target==42 && out.hit_region==-1 && out.request.source==UINT32_MAX);
            CHECK(out.request.kind==(type==3?7:4) && out.request.amount==dt[t]*(type==3?25:35));
            CHECK(out.request.argument6==0 && out.request.auxiliary_uid==UINT32_MAX && out.request.force==0);
        }
        cases++;
    }
    in.rejection_4290d0=257;CHECK(rf_liquid_damage_prepare(&in,&out)==RF_OK && !out.emit);
    in.rejection_4290d0=2;CHECK(rf_liquid_damage_prepare(&in,&out)==RF_OK && out.emit);
    in.room_present=0;CHECK(rf_liquid_damage_prepare(&in,&out)==RF_OK && !out.emit);
    in.room_present=1;in.liquid_type=1;in.frame_seconds=NAN;CHECK(rf_liquid_damage_prepare(&in,&out)==RF_OK && !out.emit);
    in.liquid_type=3;memset(&out,0xa5,sizeof(out));old=out;
    CHECK(rf_liquid_damage_prepare(&in,&out)==RF_RANGE && !memcmp(&out,&old,sizeof(out)));
    in.frame_seconds=FLT_MAX;in.acid_per_second=FLT_MAX;
    CHECK(rf_liquid_damage_prepare(&in,&out)==RF_RANGE && !memcmp(&out,&old,sizeof(out)));
    in.frame_seconds=-1;CHECK(rf_liquid_damage_prepare(&in,&out)==RF_RANGE && !memcmp(&out,&old,sizeof(out)));
    CHECK(rf_liquid_damage_prepare(NULL,&out)==RF_RANGE && !memcmp(&out,&old,sizeof(out)));
    {
        unsigned char data[256]={0};uint32_t offsets[3]={0,64,160};rf_geometry g={0};rf_liquid_rooms rooms={0},saved;
        float bottom=-1.25f,depth=2.75f;int32_t liquid=2;float rates[2]={-1,-1};
        const char *table="; comment\n$Lava Damage Per Second: 35\n$Acid Damage Per Second: 25\n";
        CHECK(sizeof(rf_liquid_room)==12);
        g.data=data;g.bytes=sizeof(data);g.rooms=3;g.room_offsets=offsets;
        data[64+32]=1;memcpy(data+64+8,&bottom,4);memcpy(data+64+42,&depth,4);memcpy(data+64+56,&liquid,4);
        data[160+32]=1;memcpy(data+160+8,&bottom,4);memcpy(data+160+42,&depth,4);liquid=0;memcpy(data+160+56,&liquid,4);
        CHECK(rf_liquid_rooms_open(&g,35,&rooms)==RF_RANGE && !rooms.items);
        CHECK(rf_liquid_rooms_open(&g,36,&rooms)==RF_OK && rooms.bytes==36 && rooms.count==3);
        CHECK(isnan(rooms.items[0].depth) && isnan(rooms.items[0].minimum_y));
        CHECK(rooms.items[1].minimum_y==bottom && rooms.items[1].depth==depth && rooms.items[1].type==2);
        CHECK(isfinite(rooms.items[2].depth) && rooms.items[2].type==0); /* presence independent of type */
        memset(data,0,sizeof(data));CHECK(rooms.items[1].depth==depth && rooms.items[1].type==2); /* source lifetime */
        saved=rooms;g.bytes=200;CHECK(rf_liquid_rooms_open(&g,36,&rooms)==RF_FORMAT && !memcmp(&saved,&rooms,sizeof(rooms)));
        rf_liquid_rooms_close(&rooms);CHECK(!rooms.items && !rooms.bytes);
        CHECK(rf_game_liquid_damage_read(table,(uint32_t)strlen(table),rates)==RF_OK && rates[0]==35 && rates[1]==25);
        table="$Lava Damage Per Second: 35 $Lava Damage Per Second: 2 $Acid Damage Per Second: 25";
        CHECK(rf_game_liquid_damage_read(table,(uint32_t)strlen(table),rates)==RF_FORMAT && rates[0]==35 && rates[1]==25);
        table="$Lava Damage Per Second: -1 $Acid Damage Per Second: 25";
        CHECK(rf_game_liquid_damage_read(table,(uint32_t)strlen(table),rates)==RF_FORMAT && rates[0]==35);
        table="$Lava Damage Per Second: 35";CHECK(rf_game_liquid_damage_read(table,(uint32_t)strlen(table),rates)==RF_NOT_FOUND);
    }
    if(argc==3) {
        rf_vpp archive,tables;rf_level level;rf_geometry geometry;rf_liquid_rooms rooms={0};float rates[2];uint32_t i,lava=0,water=0;
        CHECK(rf_vpp_open(&archive,argv[1])==RF_OK);
        CHECK(rf_level_open(&level,&archive,"L5S2.rfl")==RF_OK);
        CHECK(rf_geometry_open(&geometry,&level,16u*1024u*1024u)==RF_OK);
        CHECK(rf_liquid_rooms_open(&geometry,196608,&rooms)==RF_OK);
        rf_geometry_close(&geometry); /* All following accesses survive source release. */
        for(i=0;i<rooms.count;i++)if(isfinite(rooms.items[i].depth)){lava+=rooms.items[i].type==2;water+=rooms.items[i].type==1;}
        CHECK(lava==1 && water==2 && rooms.items[13].type==2 && rooms.items[13].depth==1.75f);
        CHECK(rooms.items[13].minimum_y==-10.500100135803223f);
        rf_liquid_rooms_close(&rooms);rf_vpp_close(&archive);
        CHECK(rf_vpp_open(&tables,argv[2])==RF_OK);CHECK(rf_game_liquid_damage_load(&tables,65536,rates)==RF_OK);
        CHECK(rates[0]==35 && rates[1]==25);
        {rf_entity_physics_config config;CHECK(rf_entity_physics_config_load(&tables,"miner1",512*1024,&config)==RF_OK);CHECK(config.material.index==3);}
        rf_vpp_close(&tables);
        printf("PASS: authored L5S2 raw lava/water ownership and game.tbl35/25 rates\n");
    }
    printf("PASS:%u captured liquid-damage cases plus eligibility and rollback\n",cases);return 0;
}
