#include "rf/player_weapon.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"weapon line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp meshes={0},motions={0},maps[5]={{0}};rf_player_weapon *w=NULL,*other=NULL;
    const char *map_names[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    char path[1024];uint32_t i,j;rf_motion_sample sample;
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/meshes.vpp",argv[1]);CHECK(rf_vpp_open(&meshes,path)==RF_OK);
    snprintf(path,sizeof(path),"%s/motions.vpp",argv[1]);CHECK(rf_vpp_open(&motions,path)==RF_OK);
    for(i=0;i<5;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],map_names[i]);CHECK(rf_vpp_open(maps+i,path)==RF_OK);}
    CHECK(rf_player_weapon_open(&meshes,&motions,maps,5,1024*1024,&w)==RF_OK);
    CHECK(w->bone_count && w->geometry.vertex_count && w->materials.count && w->peak_bytes<=1024*1024);
    CHECK(rf_player_weapon_open(&meshes,&motions,maps,5,w->resident_bytes-1,&other)==RF_RANGE && !other);
    rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<5;i++)rf_vpp_close(maps+i);
    for(i=0;i<3;i++)for(j=0;j<w->bone_count;j++) {
        rf_motion_track t;CHECK(rf_motion_file_track(w->clips+i,j,&t)==RF_OK);
        CHECK(rf_motion_file_sample(w->clips+i,j,t.envelope.start_tick,1,&sample)==RF_OK);
        CHECK(rf_motion_file_sample(w->clips+i,j,t.envelope.end_tick,1,&sample)==RF_OK);
    }
    printf("PASS bones=%u vertices=%u materials=%u resident=%u peak=%u\n",w->bone_count,w->geometry.vertex_count,w->materials.count,w->resident_bytes,w->peak_bytes);
    rf_player_weapon_close(&w);rf_player_weapon_close(&w);CHECK(!w);return 0;
}
