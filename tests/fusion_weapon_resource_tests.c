/* Installed Fusion/shoulder_cannon resource ownership and three authored FP
 * actions. Prints actual accounted residency for parent stock64MiB planning. */
#include "rf/player_weapon.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fusion resource line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp tables={0},meshes={0},motions={0},maps[5]={{0}};
    const char *map_names[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    rf_weapon_view_definition view;rf_weapon_primary_definition primary;rf_weapon_explosive_definition explosive;
    rf_weapon_model_names names;rf_weapon_names weapon_names;rf_weapon_model_owner world={0};
    rf_model_file file;rf_model_materials materials={0};rf_player_weapon *player=NULL;
    uint32_t selected[2]={0},i,ticks,geometry_bytes;int32_t id;char path[128];
    CHECK(rf_vpp_open(&tables,"Installed_Game/tables.vpp")==RF_OK);
    CHECK(rf_vpp_open(&meshes,"Installed_Game/meshes.vpp")==RF_OK);
    CHECK(rf_vpp_open(&motions,"Installed_Game/motions.vpp")==RF_OK);
    for(i=0;i<5;++i){snprintf(path,sizeof(path),"Installed_Game/%s",map_names[i]);CHECK(rf_vpp_open(maps+i,path)==RF_OK);}
    CHECK(rf_weapon_view_load(&tables,"shoulder_cannon",128*1024,&view)==RF_OK);
    CHECK(!strcmp(view.mesh,"fp_shol.v3c") && !strcmp(view.clips[0],"fp_shol_idle.rfa"));
    CHECK(!strcmp(view.clips[1],"fp_shol_fire.rfa") && !strcmp(view.clips[2],"fp_shol_reload.rfa") && !view.clips[3][0]);
    CHECK(rf_weapon_primary_load(&tables,"shoulder_cannon",128*1024,&primary)==RF_OK);
    CHECK(primary.damage==1000 && primary.magazine==1 && primary.fire_seconds==.8f && primary.reload_seconds==1);
    CHECK(rf_weapon_explosive_load(&tables,"shoulder_cannon",128*1024,&explosive)==RF_OK);
    CHECK(explosive.speed==20 && explosive.lifetime==15 && explosive.damage_radius==25 && explosive.crater_radius==7);
    CHECK(explosive.impact_count==1 && !strcmp(explosive.impact_vclips[0],"shoulder_cannon_explode") && explosive.impact_radius[0]==9);
    CHECK(rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,1536*1024,&player)==RF_OK);
    CHECK(player->bone_count==24 && player->clip_count==3 && player->materials.count);
    CHECK(rf_player_weapon_step(player,0,1.f/60)==RF_OK);
    CHECK(rf_player_weapon_step(player,1,1.f/60)==RF_OK && player->current==1);
    for(ticks=0;ticks<240 && player->current!=0;++ticks)CHECK(rf_player_weapon_step(player,-1,1.f/60)==RF_OK);
    CHECK(player->current==0);
    CHECK(rf_player_weapon_step(player,2,1.f/60)==RF_OK && player->current==2);
    for(ticks=0;ticks<240 && player->current!=0;++ticks)CHECK(rf_player_weapon_step(player,-1,1.f/60)==RF_OK);
    CHECK(player->current==0);
    printf("FUSION_FP resident=%u peak=%u bones=%u vertices=%u materials=%u\n",player->resident_bytes,player->peak_bytes,player->bone_count,player->geometry.vertex_count,player->materials.count);
    CHECK(rf_weapon_names_load(&tables,128*1024,&weapon_names)==RF_OK);
    id=rf_weapon_name_find(&weapon_names,"shoulder_cannon");CHECK(id>=0 && id<64);selected[id/32]|=1u<<(id%32);
    CHECK(rf_weapon_model_names_load(&tables,128*1024,&names)==RF_OK);
    CHECK(rf_weapon_models_open(&meshes,&names,selected,256*1024,&world)==RF_OK && world.count==1);
    CHECK(world.weapons[id].model==1);geometry_bytes=world.items[0].render.allocated_bytes;
    CHECK(rf_model_file_open(&file,&meshes,"Weapon_shoulder.v3m")==RF_OK);
    CHECK(rf_model_materials_open(&materials,&file,maps,5,1024*1024)==RF_OK);
    printf("FUSION_WORLD owner=%u peak=%u geometry=%u materials=%u material_peak=%u\n",world.allocated_bytes,world.peak_bytes,geometry_bytes,materials.resident_bytes,materials.peak_bytes);
    rf_model_materials_close(&materials);rf_weapon_models_close(&world);rf_player_weapon_close(&player);
    CHECK(!player && !world.items && !materials.items);
    rf_vpp_close(&tables);rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<5;++i)rf_vpp_close(maps+i);
    return 0;
}
