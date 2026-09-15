#include "rf/effect.h"
#include "rf/material.h"
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
    {
        const char *names[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
        rf_vpp maps[5]={{0}};rf_vfx_asset_materials *materials=NULL,*failed=NULL;
        for(i=0;i<5;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);CHECK(!rf_vpp_open(maps+i,path));}
        CHECK(!rf_vfx_asset_materials_open(a,maps,5,1024*1024,&materials));
        CHECK(materials->count==9 && materials->textures.texture_count==5 && materials->first[a->count]==materials->count);
        CHECK(rf_vfx_asset_materials_open(a,maps,5,materials->resident_bytes-1,&failed)==RF_RANGE && !failed);
        printf("Rocket materials=%u unique textures=%u resident=%u\n",materials->count,materials->textures.texture_count,materials->resident_bytes);
        for(i=0;i<materials->textures.texture_count;i++) {
            printf("texture %s frames=%u\n",materials->textures.textures[i].name,materials->textures.textures[i].animation.count);
            CHECK(materials->textures.textures[i].animation.count);
        }
        for(i=0;i<5;i++)rf_vpp_close(maps+i);
        rf_vfx_geometry_asset_close(&a);
        for(i=0;i<materials->count;i++)for(unsigned slot=0;slot<2;slot++) {
            if(materials->textures.bindings[i][slot]==UINT32_MAX)continue;
            for(tick=0;tick<=60;tick++) {
                const rf_image *image=NULL;
                CHECK(!rf_vfx_material_texture_sample(&materials->textures,materials->views+i,i,slot,tick/60.f,0,&image));
                CHECK(image && image->width && image->height);
            }
        }
        rf_vfx_asset_materials_close(&materials);CHECK(!materials);
    }
    CHECK(live);rf_vfx_geometry_asset_close(&a);rf_vfx_geometry_asset_close(&a);CHECK(!a);
    puts("PASS: complete authored rocket mesh ownership and numeric playback after archive close");return 0;
}
