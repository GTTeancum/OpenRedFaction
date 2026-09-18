/* Installed Fusion central emitter resources and actual projectile admission. */
#include "rf/material.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene_fusion_effects_resources.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fusion effects line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp maps[4]={{0}},meshes={0};rf_vfx_geometry_asset *geometry=NULL;
    rf_vfx_asset_materials *materials=NULL;uint32_t i;char path[128];
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_fusion_effects_open("Installed_Game/tables.vpp",maps,4));
    CHECK(scene_fusion_effects->definition.recipe.central_count==3);
    CHECK(scene_fusion_effects->definition.recipe.play_time==2);
    CHECK(scene_fusion_effects->definition.resolved==7);
    for(i=0;i<3;i++)CHECK(scene_fusion_effects->materials.slot_texture[i]<scene_fusion_effects->materials.count);
    printf("FUSION_EFFECT resident=%u textures=%u owner=%u\n",scene_fusion_effects->materials.resident_bytes,scene_fusion_effects->materials.count,(unsigned)sizeof(*scene_fusion_effects));
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_vfx_geometry_asset_open(&meshes,"ShellTest.vfx",128*1024,&geometry));
    CHECK(geometry->count==2);
    CHECK(!rf_vfx_asset_materials_open(geometry,maps,4,128*1024,&materials));
    for(i=0;i<geometry->count;i++) {
        CHECK(!strcmp(geometry->meshes[i]->prefix.parent,"Dummy23"));
        CHECK(geometry->meshes[i]->prefix.faces==2 && geometry->meshes[i]->prefix.vertices==4);
    }
    CHECK(materials->textures.texture_count==2);
    for(i=0;i<2;i++)CHECK(materials->textures.textures[i].animation.count==1);
    printf("FUSION_PROJECTILE geometry=%u peak=%u materials=%u\n",geometry->resident_bytes,geometry->peak_bytes,materials->resident_bytes);
    rf_vfx_asset_materials_close(&materials);rf_vfx_geometry_asset_close(&geometry);
    scene_fusion_effects_close();CHECK(!scene_fusion_effects);
    rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);return 0;
}
