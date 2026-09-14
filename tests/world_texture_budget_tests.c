#include "rf/material.h"
#include "rf/resource_budget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"world texture budget line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_level level={0};rf_vpp archive={0},maps[5]={{0}};rf_geometry world={0};rf_geometry_movers movers={0};
    rf_geometry_materials materials={0},empty={0};const rf_geometry **sources;uint32_t i,j;char path[1024];
    static const char *names[]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    CHECK(argc==2);CHECK(!rf_level_campaign_open(&level,&archive,argv[1],"L2S2a.rfl"));
    CHECK(!rf_geometry_open(&world,&level,8*1024*1024));CHECK(!rf_geometry_movers_open(&level,1024*1024,&movers));
    sources=malloc((movers.count+1)*sizeof(*sources));CHECK(sources);sources[0]=&world;
    for(i=0;i<movers.count;i++)sources[i+1]=&movers.items[i].geometry;
    for(i=0;i<5;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(rf_geometry_materials_open(&materials,sources,movers.count+1,maps,5,RF_CAMPAIGN_MATERIAL_BUDGET)==RF_RANGE);
    CHECK(!memcmp(&materials,&empty,sizeof(empty)));
    CHECK(rf_geometry_materials_open_limit(&materials,sources,movers.count+1,maps,5,RF_CAMPAIGN_MATERIAL_BUDGET,129)==RF_RANGE);
    CHECK(!memcmp(&materials,&empty,sizeof(empty)));
    CHECK(!rf_geometry_materials_open_limit(&materials,sources,movers.count+1,maps,5,RF_CAMPAIGN_MATERIAL_BUDGET,128));
    CHECK(materials.peak_bytes<=RF_CAMPAIGN_MATERIAL_BUDGET && materials.peak_bytes>=materials.resident_bytes);
    CHECK(materials.textures.loaded>0 && materials.count==movers.count+1);
    for(i=0;i<materials.textures.count;i++)if(materials.textures.items[i].image.rgba) {
        CHECK(materials.textures.items[i].image.width<=128 && materials.textures.items[i].image.height<=128);
        CHECK(materials.textures.items[i].image.bytes>0);
    }
    for(i=0;i<materials.count;i++) {
        CHECK(materials.offsets[i+1]-materials.offsets[i]==sources[i]->textures);
        for(j=materials.offsets[i];j<materials.offsets[i+1];j++)CHECK(materials.slots[j]<materials.textures.count);
    }
    printf("PASS L2S2a bounded fallback: %u resident, %u peak, %u textures\n",materials.resident_bytes,materials.peak_bytes,materials.textures.count);
    rf_geometry_materials_close(&materials);CHECK(!memcmp(&materials,&empty,sizeof(empty)));
    for(i=0;i<5;i++)rf_vpp_close(maps+i);
    free(sources);rf_geometry_movers_close(&movers);rf_geometry_close(&world);rf_vpp_close(&archive);return 0;
}
