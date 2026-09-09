#include "rf/geometry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_level level;
    rf_geometry geometry;
    uint32_t budget = 8u * 1024u * 1024u;
    int result,flags_mode=argc==4 && !strcmp(argv[3],"--flags");
    if (argc != 3 && argc != 4) return 2;
    if (argc == 4 && !flags_mode) budget = (uint32_t)strtoul(argv[3], NULL, 10);
    result = rf_vpp_open(&archive, argv[1]);
    if (result != RF_OK) return 3;
    result = rf_level_open(&level, &archive, argv[2]);
    if (result == RF_OK) result = rf_geometry_open(&geometry, &level, budget);
    if (result == RF_OK) {
        if(flags_mode) {
            uint32_t i;
            for(i=0;i<geometry.faces;i++) {
                rf_geometry_face face;uint32_t expected;
                memcpy(&expected,geometry.data+geometry.face_offsets[i]+40,4);
                if(rf_geometry_get_face(&geometry,i,&face) || face.flags!=expected)return 4;
                printf("%08x\n",face.flags);
            }
            rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
        printf("%u %u %u %u %u %u %u %u %u\n", geometry.textures, geometry.rooms, geometry.vertices,
               geometry.faces, geometry.corners, geometry.mappings, geometry.bytes,
               geometry.allocated_bytes, geometry.bytes - geometry.tail_offset - 4);
        if (geometry.vertices && geometry.faces) {
            float position[3];
            rf_geometry_face face;
            rf_geometry_corner corner;
            if (rf_geometry_vertex(&geometry, geometry.vertices, position) != RF_RANGE ||
                rf_geometry_get_face(&geometry, geometry.faces, &face) != RF_RANGE ||
                rf_geometry_get_face(&geometry, 0, &face) != RF_OK ||
                rf_geometry_get_corner(&geometry, 0, face.corners, &corner) != RF_RANGE) result = RF_FORMAT;
        }
        rf_geometry_close(&geometry);
    }
    rf_vpp_close(&archive);
    if (result != RF_OK) fprintf(stderr, "Geometry error %d\n", result);
    return result == RF_OK ? 0 : 1;
}
