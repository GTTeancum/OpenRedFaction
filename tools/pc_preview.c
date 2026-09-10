#include "rf/preview.h"
#include "pc_raster.h"
#include "rf/material.h"
#include "rf/lightmap.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include "rf/scene_preview.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
static int scene_last(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{(void)frame;(void)mesh;(void)materials;if(context)*(uint32_t*)context=world;return RF_OK;}
static int miner_skin(const char *path,const char *skin,rf_entity_assets *assets)
{
    char compiled[64];int result=rf_entity_assets_load(path,"miner1",skin,assets,512*1024);
    if(!result)result=rf_entity_skeletal_filename(assets->model,compiled);
    /* Pose diagnostic currently owns miner geometry; do not pair another mesh
     * with that pose stream if the supplied table has a different declaration. */
    if(!result && strcmp(compiled,"miner.v3c"))result=RF_FORMAT;
    return result;
}
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_level level;
    rf_geometry geometry={0};
    rf_preview_mesh mesh={0};
    rf_materials materials = {0};
    rf_lightmaps lightmaps = {0};
    rf_pc_raster raster={0};
    uint32_t i,scale=1;
    rf_entity_assets skin_assets={0};const char *skin_names[64];
    uint32_t world_vertices=0;
    int showcase=argc>1 && !strcmp(argv[1],"--scene-showcase");
    int door_motion=argc>1 && !strcmp(argv[1],"--scene-door-motion-last");
    int door_view=door_motion || (argc>1 && !strcmp(argv[1],"--scene-door-states-last"));
    rf_scene_world_geometry follow_owned={0};
    int actor_turn=argc>1 && !strcmp(argv[1],"--scene-turn-last");
    int actor_look=actor_turn || (argc>1 && !strcmp(argv[1],"--scene-look-last"));
    int actor_eye=actor_look || (argc>1 && !strcmp(argv[1],"--scene-eye-last"));
    int actor_follow=actor_eye || (argc>1 && !strcmp(argv[1],"--scene-follow-last"));
    int actor_live=actor_follow || (argc>1 && !strcmp(argv[1],"--scene-live-last"));
    int actor_drive=argc>1 && !strcmp(argv[1],"--scene-contact-last")?2:argc>1 && !strcmp(argv[1],"--scene-drive-last");
    int actor_body=actor_live || actor_drive || (argc>1 && !strcmp(argv[1],"--scene-body-last"));
    int scene_states=actor_body || door_view || (argc>1 && !strcmp(argv[1],"--scene-states-last"));
    int scene_stream=scene_states || (argc>1 && !strcmp(argv[1],"--scene-close-last"));
    int scene_close=showcase || scene_stream || (argc>1 && !strcmp(argv[1],"--scene-close"));
    int scene_mode=scene_close || (argc>1 && !strcmp(argv[1],"--scene"));
    int skin_mode=argc>1 && (!strcmp(argv[1],"--model-skin") || !strcmp(argv[1],"--model-skin-last"));
    int model_mode=skin_mode || (argc>1 && (!strcmp(argv[1],"--model") || !strcmp(argv[1],"--model-last")));
    const char *output_path=(model_mode || scene_mode)?(argc>4?argv[4]:NULL):(argc>3?argv[3]:NULL);
    if (argc < 4 || argc > 20) return 2;
    {extern uint32_t rf_scene_actor_live_enabled;rf_scene_actor_live_enabled=actor_live;}
    rf_scene_actor_eye_enabled=actor_eye;rf_scene_actor_look_enabled=actor_look;rf_scene_actor_turn_enabled=actor_turn;
    rf_scene_actor_drive(actor_live?1:actor_drive);
    if(scene_mode && argc<10)return 2;
    if(skin_mode) {
        if(argc<8 || miner_skin(argv[5],argv[6],&skin_assets))return 1;
        for(i=0;i<skin_assets.texture_count;++i)skin_names[i]=skin_assets.textures[i];
    }
    if(model_mode) {
        if(argc<6 || rf_vpp_open(&archive,argv[2]) || rf_animation_preview(argv[2],argv[3],(!strcmp(argv[1],"--model-last") || !strcmp(argv[1],"--model-skin-last"))?63:0,&mesh,1024*1024))return 1;
    } else {
        if(rf_vpp_open(&archive, argv[scene_mode?2:1]) || rf_level_open(&level, &archive, argv[scene_mode?3:2]))return 1;
        rf_scene_showcase_enabled=showcase;
        if(scene_close && !door_motion && rf_scene_preview_camera(&level,(int32_t)strtol(argv[8],NULL,10)))return 1;
        if(actor_live && rf_scene_preview_route_camera(&level,(int32_t)strtol(argv[8],NULL,10)))return 1;
        if(door_view && rf_scene_preview_mover_camera(&level,8544,6.0f))return 1;
        if(showcase && rf_scene_showcase_camera(&level))return 1;
        if(rf_geometry_open(&geometry,&level,8*1024*1024) || rf_preview_build(&mesh,&geometry,&level,8*1024*1024))return 1;
    }
    if (argc > 4) {
        rf_vpp archives[16];
        uint32_t opened = 0;
        int result = RF_OK;
        for (i = scene_mode?9:skin_mode?7:model_mode?5:4; i < (uint32_t)argc; ++i) {
            result = rf_vpp_open(archives+opened, argv[i]);
            if (result) break;
            ++opened;
        }
        if(!result && model_mode) {
            rf_model_file model;rf_model_materials bundle={0};
            result=rf_model_file_open(&model,&archive,"miner.v3c");
            if(!result)result=rf_model_materials_open_skin(&bundle,&model,skin_names,skin_assets.texture_count,archives,opened,4*1024*1024);
            if(!result)for(i=0;i<mesh.count;++i) {
                uint32_t slot=mesh.vertices[i].material;
                if(slot>=bundle.count) {result=RF_FORMAT;break;}
                memcpy(&mesh.vertices[i].material,bundle.items[slot].record.bytes+0x10,4);
            }
            if(!result) {materials=bundle.textures;memset(&bundle.textures,0,sizeof(bundle.textures));}
            rf_model_materials_close(&bundle);
        } else if(!result && scene_mode) {
            rf_preview_close(&mesh);
            if(door_motion) {
                rf_scene_world_geometry owned={0};rf_group_attached_pose *poses=NULL;FILE *input=NULL;
                result=rf_scene_world_open_retained(&level,&geometry,archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,&owned);
                if(!result) {
                    uint32_t capacity=mesh.bytes+1024*1024;void *buffer=realloc(mesh.vertices,capacity);
                    size_t size=owned.movers.count*sizeof(*poses);
                    if(buffer)mesh.vertices=buffer;else result=RF_RANGE;
                    poses=malloc(size?size:1);input=fopen(argv[8],"rb");
                    if(!poses || !input)result=RF_IO;
                    if(!result && (fread(poses,1,size,input)!=size || fgetc(input)!=EOF || ferror(input)))result=RF_FORMAT;
                    if(!result)result=rf_scene_world_update(&owned,poses,owned.movers.count,&mesh,capacity);
                }
                if(input)fclose(input);free(poses);rf_scene_world_geometry_close(&owned);
            } else if(actor_follow) {result=rf_scene_world_open_retained(&level,&geometry,archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,&follow_owned);if(!result)rf_scene_actor_follow(&follow_owned);}
            else result=rf_scene_world_open(&level,&geometry,archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024);
        } else if (!result) result = rf_materials_open(&materials, &geometry, archives, opened, 4*1024*1024);
        if(!result && door_motion)world_vertices=mesh.count;
        if(!result && scene_mode && !door_motion) {
            world_vertices=mesh.count;
            if(actor_body) {
                rf_geometry_collision_world collision={0};result=rf_geometry_collision_world_open(&geometry,8*1024*1024,&collision);
                if(!result)result=rf_scene_stream_miner_body(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                    archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,scene_last,&world_vertices,&collision,&geometry);
                rf_geometry_collision_world_close(&collision);
            }
            else if(scene_states)result=rf_scene_stream_miner_states(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,scene_last,&world_vertices);
            else if(scene_stream)result=rf_scene_stream_miner(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,scene_last,&world_vertices);
            else result=rf_scene_preview_miner(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024);
            if(!result)printf("Combined %u world and %u actor triangles\n",world_vertices/3,(mesh.count-world_vertices)/3);
        }
        rf_scene_actor_follow(NULL);rf_scene_world_geometry_close(&follow_owned);
        while (opened) rf_vpp_close(archives + --opened);
        if (result) { rf_preview_close(&mesh); rf_geometry_close(&geometry); rf_vpp_close(&archive); return 1; }
        if (!model_mode && rf_lightmaps_open(&lightmaps, &level, 4*1024*1024)) return 1;
    }
    /* Offline capture resolution; default retains the Xbox comparison grid.
     * Re-rasterize projected triangles at the requested size, never upscale RGB. */
    {const char *setting=getenv("RF_PREVIEW_SCALE");
     if(setting){if(setting[0]<'1' || setting[0]>'4' || setting[1])return 2;scale=(uint32_t)(setting[0]-'0');}}
    if(rf_pc_raster_open(&raster,scale) || rf_pc_raster_frame(&raster,&mesh,&materials,&lightmaps,
       model_mode?0:scene_mode?world_vertices:mesh.count) || rf_pc_raster_save(&raster,output_path))return 1;
    printf("Prepared %u triangles (%u bytes), %s\n",mesh.count/3,mesh.bytes,scene_close?"close level/actor inspection":model_mode?"posed miner inspection":"original spawn");
    rf_pc_raster_close(&raster); rf_lightmaps_close(&lightmaps); rf_materials_close(&materials); rf_preview_close(&mesh); rf_geometry_close(&geometry); rf_vpp_close(&archive);
    return 0;
}
