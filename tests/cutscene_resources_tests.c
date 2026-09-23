#include "rf/cutscene.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"cutscene resource line %u: %s\n",(unsigned)__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    static const struct {const char *level;uint32_t cameras,points,paths,selector;} cases[]={
        {"L6S3.rfl",15,14,2,3696},{"L11S3.rfl",11,23,0,10626},
        {"L20S2.rfl",1,1,1,18355},{"L7S1.rfl",3,4,0,5495},
        {"L7S2.rfl",6,3,0,4948},{"L7S3.rfl",3,3,0,7886},
        {"L7S4.rfl",17,11,6,10562},{"L8S4.rfl",8,12,0,6816},
        {"L13S3.rfl",5,6,0,8771},{"L14S3.rfl",5,5,4,9618},
        {"L15S4.rfl",1,1,0,9613},{"L17S4.rfl",1,1,2,18248}};
    uint32_t i,total=0;CHECK(argc==2);
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        rf_vpp archive={0};rf_level level={0};rf_cutscene_resources r={0},unchanged={0};
        const rf_cutscene_descriptor *d;int status;
        CHECK(rf_level_campaign_open(&level,&archive,argv[1],cases[i].level)==RF_OK);
        status=rf_cutscene_resources_open(&level,65536,&r);CHECK(status==RF_OK);
        CHECK(r.camera_count==cases[i].cameras && r.point_count==cases[i].points && r.path_count==cases[i].paths);
        CHECK(r.descriptor_count==1 && r.allocated_bytes<65536);
        d=rf_cutscene_find(&r,cases[i].selector);CHECK(d && d->point_count==cases[i].points);
        CHECK(!rf_cutscene_find(&r,UINT32_MAX));
        if(i==0) {
            float position[3];const rf_cutscene_path *path=rf_cutscene_path_find(&r,"PLEASEWORK");
            rf_cutscene_runtime runtime={0};uint32_t action=0,finished=0,j;
            CHECK(d->hide==1 && d->fov==45 && r.points[0].camera_uid==6851 && r.points[0].words[1]==6852);
            CHECK(fabsf(r.points[0].durations[2]-3.2f)<.00001f && !strcmp(r.points[0].path,"none"));
            CHECK(path && rf_cutscene_camera_find(&r,6851));
            rf_cutscene_path_sample(path,.5f,position);
            CHECK(fabsf(position[0]+2.15673518f)<.0005f && fabsf(position[2]+22.517065f)<.0005f);
            CHECK(rf_cutscene_begin(&runtime,&r,3696,0,&action)==RF_OK && action==6852);
            CHECK(runtime.active && runtime.point_index==0 && runtime.fov==45);
            CHECK(rf_cutscene_step(&runtime,3144,1.f/60,&action,&finished)==RF_OK && runtime.point_index==0);
            CHECK(rf_cutscene_step(&runtime,3145,1.f/60,&action,&finished)==RF_OK && runtime.point_index==1 && action==6884);
            rf_cutscene_cancel(&runtime);CHECK(!runtime.active);
            for(j=0;j<d->point_count && rf_cutscene_path_find(&r,r.points[d->first_point+j].path)!=path;++j);
            CHECK(j<d->point_count);
            runtime.resources=&r;runtime.active=1;runtime.point_index=j;runtime.total_deadline=10000;
            runtime.pre_deadline=-1;runtime.elapsed=2;
            CHECK(rf_cutscene_step(&runtime,1000,1.f/60,&action,&finished)==RF_OK);
            CHECK(!runtime.moving && runtime.elapsed==2 && action==UINT32_MAX);
            CHECK(rf_cutscene_resources_open(&level,1,&unchanged)==RF_RANGE && !unchanged.storage);
        }
        total+=r.point_count;rf_cutscene_resources_close(&r);rf_vpp_close(&archive);
    }
    CHECK(total==84);printf("PASS 12 authored cutscenes, %u points, bounded camera paths\n",total);return 0;
}
