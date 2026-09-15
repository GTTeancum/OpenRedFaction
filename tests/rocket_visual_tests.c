#include "rf/effect.h"
#include <stdio.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"rocket visual line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_vfx_geometry_asset *a=NULL,*other=NULL;char path[1024];unsigned i,tick,live=0;
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/meshes.vpp",argv[1]);CHECK(!rf_vpp_open(&archive,path));
    CHECK(!rf_vfx_geometry_asset_open(&archive,"DrillMissile01.vfx",128*1024,&a));
    CHECK(a->count==7 && a->peak_bytes<=128*1024);
    CHECK(rf_vfx_geometry_asset_open(&archive,"DrillMissile01.vfx",a->resident_bytes-1,&other)==RF_RANGE && !other);
    CHECK(rf_vfx_geometry_asset_open(&archive,"DrillMissile01.vfx",a->peak_bytes-1,&other)==RF_RANGE && !other);
    CHECK(!rf_vfx_geometry_asset_open(&archive,"DrillMissile01.vfx",a->peak_bytes,&other));
    rf_vfx_geometry_asset_close(&other);
    CHECK(rf_vfx_geometry_asset_open(&archive,"NanoAttackMissile.vfx",128*1024,&other)==RF_FORMAT && !other);
    rf_vpp_close(&archive);
    printf("Rocket VFX: meshes=%u resident=%u peak=%u\n",a->count,a->resident_bytes,a->peak_bytes);
    for(i=0;i<a->count;i++) {
        printf("mesh %s parent %s vertices=%u faces=%u\n",a->meshes[i]->prefix.name,a->meshes[i]->prefix.parent,a->meshes[i]->prefix.vertices,a->meshes[i]->prefix.faces);
        if(!a->instances[i])continue;
        for(tick=0;tick<=64;tick++) {
            CHECK(!rf_vfx_instance_update(a->instances[i],tick*.25f));
            if(a->instances[i]->active) {
                ++live;for(unsigned k=0;k<a->meshes[i]->prefix.vertices*3;k++)CHECK(isfinite(a->instances[i]->vertices[k]));
                for(unsigned k=0;k<a->meshes[i]->prefix.faces*6;k++)CHECK(isfinite(a->instances[i]->uv[k]));
            }
        }
    }
    CHECK(live);rf_vfx_geometry_asset_close(&a);rf_vfx_geometry_asset_close(&a);CHECK(!a);
    puts("PASS: complete authored rocket mesh ownership and numeric playback after archive close");return 0;
}
