#include "rf/vpp.h"
#include "rf/checksum.h"
#include "rf/level.h"
#include "rf/geometry.h"
#include "rf/material.h"
#include "rf/lightmap.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include "rf/scene_preview.h"
#include "renderer.h"
#include <string.h>
#include <stdio.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

/* Read-only monitor evidence. Resolve its VA from the matching linker map. */
volatile uint32_t rf_diagnostic[58] = {0x52464447u, 9u, 0};
static rf_geometry resident_geometry;
static rf_materials resident_materials;
static rf_lightmaps resident_lightmaps;
static int scene_frame(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{
    (void)context;
    if(rf_diagnostic[37]!=frame)return RF_FORMAT;
    rf_diagnostic[57]=world;
    return rf_xbox_scene_stream_frame(mesh,materials,&resident_lightmaps,world,&rf_diagnostic[32],&rf_diagnostic[44]);
}
static int scene_preview(rf_level *level,rf_preview_mesh *mesh)
{
    static const char *paths[]={"D:\\maps1.vpp","D:\\maps2.vpp","D:\\maps3.vpp","D:\\maps4.vpp","D:\\maps_en.vpp"};
    rf_vpp maps[5];uint32_t opened=0,world;int status;FILE *stream_flag;
    status=rf_scene_preview_camera(level,9858);if(status)return status;
    rf_preview_close(mesh);status=rf_preview_build(mesh,&resident_geometry,level,8*1024*1024);if(status)return status;
    world=mesh->count;
    while(!status && opened<5) {status=rf_vpp_open(maps+opened,paths[opened]);if(!status)++opened;}
    stream_flag=fopen("D:\\scene-stream.flag","rb");
    if(stream_flag) {
        fclose(stream_flag);rf_diagnostic[31]=4;rf_diagnostic[56]=9858;
        stream_flag=fopen("D:\\scene-states.flag","rb");
        if(stream_flag) {
            fclose(stream_flag);rf_diagnostic[31]=5;
            if(!status)status=rf_scene_stream_miner_states(level,9858,"D:\\meshes.vpp","D:\\motions.vpp","D:\\tables.vpp",
                maps,opened,mesh,&resident_materials,8*1024*1024,4*1024*1024,scene_frame,NULL);
        } else if(!status)status=rf_scene_stream_miner(level,9858,"D:\\meshes.vpp","D:\\motions.vpp","D:\\tables.vpp",
            maps,opened,mesh,&resident_materials,8*1024*1024,4*1024*1024,scene_frame,NULL);
        while(opened)rf_vpp_close(maps+--opened);
        return status;
    }
    if(!status)status=rf_scene_preview_miner(level,9858,"D:\\meshes.vpp","D:\\motions.vpp","D:\\tables.vpp",
        maps,opened,mesh,&resident_materials,8*1024*1024,4*1024*1024);
    while(opened)rf_vpp_close(maps+--opened);
    if(!status) {
        rf_diagnostic[31]=3;rf_diagnostic[56]=9858;rf_diagnostic[57]=world;
        status=rf_xbox_scene_preview(mesh,&resident_materials,&resident_lightmaps,world,&rf_diagnostic[32],&rf_diagnostic[44]);
    }
    return status;
}
static int model_frame(void *context,uint32_t frame,rf_preview_mesh *mesh)
{
    rf_model_materials *bundle=context;uint32_t i;
    if(rf_diagnostic[37]!=frame)return RF_FORMAT;
    for(i=0;i<mesh->count;++i) {
        uint32_t material=mesh->vertices[i].material;
        if(material>=bundle->count)return RF_FORMAT;
        memcpy(&mesh->vertices[i].material,bundle->items[material].record.bytes+0x10,4);
    }
    return rf_xbox_model_stream_frame(mesh,&bundle->textures,&rf_diagnostic[32],&rf_diagnostic[44]);
}
static int model_preview(void)
{
    static const char *paths[]={"D:\\maps1.vpp","D:\\maps2.vpp","D:\\maps3.vpp","D:\\maps4.vpp","D:\\maps_en.vpp"};
    rf_vpp meshes,archives[5];rf_model_file model;rf_model_materials bundle={0};rf_preview_mesh mesh={0};
    uint32_t opened=0,i;int status;FILE *stream_flag;
    static rf_entity_assets skin_assets;const char *skin_names[64];
    memset(&skin_assets,0,sizeof(skin_assets));
    FILE *skin_file=fopen("D:\\model-skin.txt","rb");
    if(skin_file) {
        char skin[64],compiled[64];size_t size=fread(skin,1,sizeof(skin),skin_file);
        int failed=ferror(skin_file);fclose(skin_file);
        if(failed || size==sizeof(skin))return RF_RANGE;
        while(size && (skin[size-1]=='\r' || skin[size-1]=='\n'))--size;
        if(!size || memchr(skin,0,size))return RF_FORMAT;skin[size]=0;
        status=rf_entity_assets_load("D:\\tables.vpp","miner1",skin,&skin_assets,512*1024);
        if(status)return status;
        status=rf_entity_skeletal_filename(skin_assets.model,compiled);
        if(status || strcmp(compiled,"miner.v3c"))return RF_FORMAT;
        for(i=0;i<skin_assets.texture_count;++i)skin_names[i]=skin_assets.textures[i];
        rf_diagnostic[56]=rf_filename_checksum(skin);rf_diagnostic[57]=skin_assets.texture_count;
    }
    status=rf_vpp_open(&meshes,"D:\\meshes.vpp");if(status)return status;
    status=rf_model_file_open(&model,&meshes,"miner.v3c");
    while(!status && opened<5) {status=rf_vpp_open(archives+opened,paths[opened]);if(!status)++opened;}
    if(!status)status=rf_model_materials_open_skin(&bundle,&model,skin_names,skin_assets.texture_count,archives,opened,4*1024*1024);
    stream_flag=fopen("D:\\model-stream.flag","rb");
    if(stream_flag) {
        rf_animation_placement placement={0};
        fclose(stream_flag);rf_diagnostic[31]=2;
        /* Equivalent inspection framing through a translated quarter-turn
         * entity, exercising model-local view conversion on the Xbox. */
        placement.world_view.camera[0]=2.2f;placement.world_view.camera[1]=8;placement.world_view.camera[2]=16;
        placement.world_view.rotation[2]=1;placement.world_view.rotation[4]=1;placement.world_view.rotation[6]=-1;
        placement.world_view.perspective=placement.world_view.compute_clip=placement.world_view.clipping=1;
        placement.world_view.screen[0]=320;placement.world_view.screen[1]=-240;placement.world_view.screen[2]=320;placement.world_view.screen[3]=240;
        placement.position[1]=8;placement.position[2]=16;
        placement.orientation[2]=-1;placement.orientation[4]=placement.orientation[6]=1;
        placement.clip_projection.scale[0]=320;placement.clip_projection.scale[1]=240;placement.clip_projection.clamp=1;
        if(!status)status=rf_animation_stream_placed("D:\\meshes.vpp","D:\\motions.vpp",1024*1024,&placement,model_frame,&bundle);
    } else {
    if(!status)status=rf_animation_preview("D:\\meshes.vpp","D:\\motions.vpp",0,&mesh,1024*1024);
    if(!status)for(i=0;i<mesh.count;++i) {
        uint32_t material=mesh.vertices[i].material;
        if(material>=bundle.count) {status=RF_FORMAT;break;}
        memcpy(&mesh.vertices[i].material,bundle.items[material].record.bytes+0x10,4);
    }
    if(!status) {rf_diagnostic[31]=1;status=rf_xbox_model_preview(&mesh,&bundle.textures,&rf_diagnostic[32],&rf_diagnostic[44]);}
    }
    rf_preview_close(&mesh);rf_model_materials_close(&bundle);
    while(opened)rf_vpp_close(archives+--opened);
    rf_vpp_close(&meshes);return status;
}
static int load_materials(void)
{
    static const char *paths[] = {"D:\\maps1.vpp", "D:\\maps2.vpp", "D:\\maps3.vpp", "D:\\maps4.vpp", "D:\\maps_en.vpp"};
    rf_vpp archives[5];
    uint32_t i, opened = 0, checksum = 0;
    int result = RF_OK;
    for (i = 0; i < 5; ++i) {
        result = rf_vpp_open(archives + i, paths[i]);
        if (result) break;
        ++opened;
    }
    if (!result) result = rf_materials_open(&resident_materials, &resident_geometry, archives, 5, 4u*1024u*1024u);
    while (opened) rf_vpp_close(archives + --opened);
    if (result) return result;
    for (i = 0; i < resident_materials.count; ++i) {
        const rf_image *image = &resident_materials.items[i].image;
        uint32_t j, hash = 2166136261u;
        if (!image->rgba) continue;
        for (j = 0; j < image->bytes; ++j) hash = (hash ^ image->rgba[j]) * 16777619u;
        checksum ^= hash;
    }
    rf_diagnostic[38] = resident_materials.loaded;
    rf_diagnostic[39] = resident_materials.allocated_bytes;
    rf_diagnostic[40] = checksum; rf_diagnostic[41] = resident_materials.missing;
    return RF_OK;
}

void WinMainCRTStartup(void);
void __attribute__((no_stack_protector)) rf_diagnostic_start(void)
{
    rf_diagnostic[2] = 10;
    WinMainCRTStartup();
}

int main(void)
{
    rf_vpp archive;
    MM_STATISTICS memory = {0};
    int result;
    rf_diagnostic[2] = 1;
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    memory.Length = sizeof(memory);
    debugPrint("Red Faction reconstruction - archive diagnostic\n");
    debugPrint("Not a playable game. Stock 64 MiB target.\n");
    if (NT_SUCCESS(MmQueryStatistics(&memory))) {
        rf_diagnostic[3] = memory.TotalPhysicalPages;
        rf_diagnostic[4] = memory.AvailablePages;
        debugPrint("Physical pages: %lu; available: %lu\n", memory.TotalPhysicalPages, memory.AvailablePages);
    }
    OutputDebugStringA("RF_DIAGNOSTIC_BOOT\n");
    result = rf_vpp_open(&archive, "D:\\tables.vpp");
    if (result == RF_OK) {
        rf_diagnostic[5] = archive.count;
        rf_diagnostic[6] = archive.length;
        rf_diagnostic[7] = rf_filename_checksum("tables.vpp");
        debugPrint("tables.vpp: %u files, %u bytes\n", archive.count, archive.length);
        OutputDebugStringA("RF_VPP_VALIDATED\n");
        rf_vpp_close(&archive);
        rf_diagnostic[2] = 2;
    } else {
        debugPrint("tables.vpp unavailable or invalid: %d\n", result);
        OutputDebugStringA("RF_VPP_FAILED\n");
        rf_diagnostic[2] = 0x80000000u | (uint32_t)(-result);
    }
    if (result == RF_OK) {
        uint32_t animation[8], i;
        result=rf_animation_check("D:\\meshes.vpp","D:\\motions.vpp",animation);
        for (i=0;i<8;++i) rf_diagnostic[48+i]=animation[i];
        if (result!=RF_OK) rf_diagnostic[2]=0x80000200u | (uint32_t)(-result);
    }
    if (result == RF_OK) {
        result = rf_vpp_open(&archive, "D:\\levels1.vpp");
        if (result == RF_OK) {
            rf_level level;
            result = rf_level_open(&level, &archive, "L1S1.rfl");
            if (result == RF_OK) {
                const rf_level_section *geometry = rf_level_find(&level, 0x100);
                const rf_level_section *lightmaps = rf_level_find(&level, 0x1200);
                uint32_t i;
                rf_diagnostic[8] = level.version;
                rf_diagnostic[9] = level.section_count;
                rf_diagnostic[10] = level.entry.size;
                rf_diagnostic[11] = geometry ? geometry->size : 0;
                rf_diagnostic[12] = lightmaps ? lightmaps->size : 0;
                for (i = 0; i < 3; ++i) {
                    uint32_t bits;
                    memcpy(&bits, &level.player_position[i], sizeof(bits));
                    rf_diagnostic[13 + i] = bits;
                }
                debugPrint("%s: %u sections, %u bytes\n", level.name, level.section_count, level.entry.size);
                OutputDebugStringA("RF_LEVEL_DIRECTORY_VALIDATED\n");
                result = rf_geometry_open(&resident_geometry, &level, 8u * 1024u * 1024u);
                if (result == RF_OK) {
                    float vertex[3];
                    rf_diagnostic[16] = resident_geometry.textures;
                    rf_diagnostic[17] = resident_geometry.rooms;
                    rf_diagnostic[18] = resident_geometry.vertices;
                    rf_diagnostic[19] = resident_geometry.faces;
                    rf_diagnostic[20] = resident_geometry.corners;
                    rf_diagnostic[21] = resident_geometry.mappings;
                    rf_diagnostic[22] = resident_geometry.allocated_bytes;
                    if (NT_SUCCESS(MmQueryStatistics(&memory))) rf_diagnostic[23] = memory.AvailablePages;
                    rf_geometry_vertex(&resident_geometry, 0, vertex);
                    for (i = 0; i < 3; ++i) { uint32_t bits; memcpy(&bits, &vertex[i], 4); rf_diagnostic[24 + i] = bits; }
                    rf_geometry_vertex(&resident_geometry, resident_geometry.vertices - 1, vertex);
                    for (i = 0; i < 3; ++i) { uint32_t bits; memcpy(&bits, &vertex[i], 4); rf_diagnostic[27 + i] = bits; }
                    rf_diagnostic[30] = resident_geometry.bytes - resident_geometry.tail_offset - 4;
                    debugPrint("Geometry: %u vertices, %u faces, %u bytes\n", resident_geometry.vertices, resident_geometry.faces, resident_geometry.allocated_bytes);
                    {
                        rf_preview_mesh mesh;
                        result = rf_lightmaps_open(&resident_lightmaps, &level, 4u*1024u*1024u);
                        if (result == RF_OK) {
                            uint32_t mapping, image;
                            for (mapping = 0; mapping < resident_geometry.mappings && result == RF_OK; ++mapping)
                                result = rf_geometry_lightmap(&resident_geometry, mapping, resident_lightmaps.count, &image);
                            rf_diagnostic[43] = resident_lightmaps.count;
                        }
                        if (result == RF_OK) result = load_materials();
                        if (NT_SUCCESS(MmQueryStatistics(&memory))) rf_diagnostic[42] = memory.AvailablePages;
                        if (result == RF_OK) result = rf_preview_build(&mesh, &resident_geometry, &level, 8u*1024u*1024u);
                        if (result == RF_OK) {
                            FILE *scene_flag=fopen("D:\\scene-preview.flag","rb");
                            if(scene_flag) {fclose(scene_flag);result=scene_preview(&level,&mesh);}
                            else {
                                FILE *model_flag=fopen("D:\\model-preview.flag","rb");
                                if(model_flag) {fclose(model_flag);result=model_preview();}
                                else result = rf_xbox_preview(&mesh, &resident_materials, &resident_lightmaps, &rf_diagnostic[32], &rf_diagnostic[44]);
                            }
                            rf_preview_close(&mesh);
                            if (NT_SUCCESS(MmQueryStatistics(&memory))) rf_diagnostic[47] = memory.AvailablePages;
                        }
                    }
                }
            }
            rf_vpp_close(&archive);
        }
        rf_diagnostic[2] = result == RF_OK ? 5u : 0x80000100u | (uint32_t)(-result);
    }
    for (;;) Sleep(1000);
    return 0;
}
