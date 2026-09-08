#include "rf/vpp.h"
#include "rf/checksum.h"
#include "rf/level.h"
#include "rf/geometry.h"
#include "rf/material.h"
#include "rf/lightmap.h"
#include "renderer.h"
#include <string.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

/* Read-only monitor evidence. Resolve its VA from the matching linker map. */
volatile uint32_t rf_diagnostic[48] = {0x52464447u, 7u, 0};
static rf_geometry resident_geometry;
static rf_materials resident_materials;
static rf_lightmaps resident_lightmaps;
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
                            result = rf_xbox_preview(&mesh, &resident_materials, &resident_lightmaps, &rf_diagnostic[32], &rf_diagnostic[44]);
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
