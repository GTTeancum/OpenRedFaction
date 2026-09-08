#include "rf/lightmap.h"
#include "rf/geometry.h"
#include <stdlib.h>
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_level level;
    rf_lightmaps maps;
    rf_geometry geometry;
    int result;
    uint32_t i;
    if (argc != 4) return 2;
    if (rf_vpp_open(&archive, argv[1])) return 1;
    result = rf_level_open(&level, &archive, argv[2]);
    if (result) { rf_vpp_close(&archive); return 1; }
    result = rf_lightmaps_open(&maps, &level, (uint32_t)strtoul(argv[3], NULL, 10));
    if (!result) {
        uint32_t image;
        result = rf_geometry_open(&geometry, &level, 8u*1024u*1024u);
        if (!result) {
            for (i = 0; i < geometry.mappings && !result; ++i)
                result = rf_geometry_lightmap(&geometry, i, maps.count, &image);
            if (!result && rf_geometry_lightmap(&geometry, geometry.mappings, maps.count, &image) != RF_RANGE) result = RF_FORMAT;
            if (!result && geometry.mappings && rf_geometry_lightmap(&geometry, 0, 0, &image) != RF_FORMAT) result = RF_FORMAT;
            rf_geometry_close(&geometry);
        }
        if (result) rf_lightmaps_close(&maps);
    }
    rf_vpp_close(&archive);
    if (result) {
        if (maps.images || maps.count || maps.allocated_bytes) return 3;
        printf("%d\n", result); return 1;
    }
    printf("%u %u\n", maps.count, maps.allocated_bytes);
    for (i = 0; i < maps.count; ++i) {
        uint32_t j, hash = 2166136261u;
        for (j = 0; j < maps.images[i].bytes; ++j) hash = (hash ^ maps.images[i].rgba[j])*16777619u;
        printf("%u %u %u\n", maps.images[i].width, maps.images[i].height, hash);
    }
    rf_lightmaps_close(&maps);
    return 0;
}
