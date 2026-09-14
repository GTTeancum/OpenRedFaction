#include "rf/entity_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"script catalog line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_entity_motion_catalog catalog={0},before;rf_entity_playback_resources playback={0};
    rf_entity_model_motion saved;int32_t a=-99,b=-99;char path[1024];uint32_t references;
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/motions.vpp",argv[1]);CHECK(!rf_vpp_open(&archive,path));
    catalog.models=calloc(1,sizeof(*catalog.models));CHECK(catalog.models);catalog.model_count=1;
    catalog.resident_bytes=catalog.peak_bytes=sizeof(catalog)+sizeof(*catalog.models);
    CHECK(!rf_entity_motion_catalog_append(&catalog,0,&archive,"Ult2_cower.mvf",1,512*1024,&a));CHECK(a==0);
    CHECK(!rf_entity_motion_catalog_find(&catalog,0,"ULT2_COWER.MVF",1,&b) && b==a);
    before=catalog;saved=catalog.models[0].items[0];
    CHECK(!rf_entity_motion_catalog_append(&catalog,0,&archive,"ult2_cower.mvf",1,512*1024,&b));CHECK(b==0 && !memcmp(&before,&catalog,sizeof(catalog)));
    b=-99;CHECK(rf_entity_motion_catalog_append(&catalog,0,&archive,"Ult2_cower.mvf",0,catalog.resident_bytes,&b)==RF_RANGE);
    CHECK(b==-99 && !memcmp(&before,&catalog,sizeof(catalog)) && !memcmp(&saved,catalog.models[0].items,sizeof(saved)));
    CHECK(rf_entity_motion_catalog_append(&catalog,0,&archive,"no_such_script_clip.mvf",0,512*1024,&b)==RF_NOT_FOUND);
    CHECK(b==-99 && !memcmp(&before,&catalog,sizeof(catalog)));
    strcpy(catalog.models[0].marker_names[0].names[0],"test");catalog.models[0].items[0].marker_mask=1;
    CHECK(!rf_entity_motion_catalog_append(&catalog,0,&archive,"Ult2_cower.mvf",0,512*1024,&b));CHECK(b==1);
    CHECK(catalog.models[0].count==2 && !strcmp(catalog.models[0].marker_names[0].names[0],"test"));
    CHECK(!catalog.models[0].marker_names[1].names[0][0] && !catalog.models[0].items[1].marker_mask);
    CHECK(catalog.peak_bytes>=catalog.resident_bytes && catalog.peak_bytes<=512*1024);
    CHECK(!rf_entity_playback_resources_open(&catalog,256*1024,&playback));
    CHECK(playback.resource_count==2 && playback.cache_count==1 && playback.cache_ids[0]==playback.cache_ids[1]);
    CHECK(!rf_entity_playback_cache_references(&playback,0,&references) && references==0);
    rf_entity_playback_resources_close(&playback);rf_entity_motion_catalog_close(&catalog);rf_vpp_close(&archive);
    puts("PASS script motion registration: alias/loop identity, markers, shared cache and failure preservation");return 0;
}
