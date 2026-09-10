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
    int result,flags_mode=argc==4 && !strcmp(argv[3],"--flags"),links_mode=argc==4 && !strcmp(argv[3],"--links"),primary_mode=argc==4 && !strcmp(argv[3],"--primary");
    if (argc != 3 && argc != 4) return 2;
    if (argc == 4 && !flags_mode && !links_mode && !primary_mode && strcmp(argv[3],"--portals")) budget = (uint32_t)strtoul(argv[3], NULL, 10);
    result = rf_vpp_open(&archive, argv[1]);
    if (result != RF_OK) return 3;
    result = rf_level_open(&level, &archive, argv[2]);
    if (result == RF_OK) result = rf_geometry_open(&geometry, &level, budget);
    if (result == RF_OK) {
        if(argc==4 && !strcmp(argv[3],"--portals")) {
            rf_geometry_portal *portals;uint32_t count=0,i,j,guard=0xa5a5a5a5;
            if(rf_geometry_portals(&geometry,NULL,0,&count))return 5;
            portals=malloc((size_t)(count+1)*sizeof(*portals));if(!portals)return 5;
            if(count) {
                memset(portals,0xa5,(size_t)(count+1)*sizeof(*portals));
                if(rf_geometry_portals(&geometry,portals,count-1,&guard)!=RF_RANGE || guard!=0xa5a5a5a5)return 6;
                for(i=0;i<(count+1)*sizeof(*portals);i++)if(((unsigned char*)portals)[i]!=0xa5)return 7;
            }
            if(rf_geometry_portals(&geometry,portals,count,&count))return 8;
            printf("%u %u\n",geometry.rooms,count);
            for(i=0;i<count;i++) {
                uint32_t words[8];memcpy(words,portals+i,32);
                for(j=0;j<8;j++)printf("%08x%s",words[j],j==7?"\n":" ");
            }
            free(portals);rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
        if(primary_mode) {
            uint32_t i,n=0,guard=0xa5a5a5a5,*indices=(uint32_t*)malloc((size_t)(geometry.rooms+1)*4);
            if(!indices)return 5;
            printf("%u",geometry.rooms);for(i=0;i<geometry.rooms;i++)printf(" %u",geometry.data[geometry.room_offsets[i]+34]);printf("\n");
            if(rf_geometry_primary_rooms(&geometry,indices,geometry.rooms,&n))return 6;
            printf("%u",n);for(i=0;i<n;i++)printf(" %u",indices[i]);printf("\n");
            if(n) {
                memset(indices,0xa5,(size_t)(geometry.rooms+1)*4);
                if(rf_geometry_primary_rooms(&geometry,indices,n-1,&guard)!=RF_RANGE || guard!=0xa5a5a5a5)return 7;
                for(i=0;i<=geometry.rooms;i++)if(indices[i]!=0xa5a5a5a5)return 8;
            }
            free(indices);rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
        if(links_mode) {
            uint32_t i,j,at=geometry.room_links_offset,total=0,*indices;
            printf("%u %u\n",geometry.rooms,geometry.room_link_records);
            for(i=0;i<geometry.room_link_records;i++) {
                uint32_t parent,n;memcpy(&parent,geometry.data+at,4);memcpy(&n,geometry.data+at+4,4);at+=8;total+=n;
                printf("%u %u",parent,n);for(j=0;j<n;j++) {uint32_t child;memcpy(&child,geometry.data+at,4);at+=4;printf(" %u",child);}printf("\n");
            }
            indices=(uint32_t*)malloc((size_t)(total+1)*4);if(!indices)return 5;
            for(i=0;i<geometry.rooms;i++) {
                uint32_t n=0,guard=0xa5a5a5a5;
                if(rf_geometry_room_children(&geometry,i,indices,total,&n))return 6;
                printf("%u",n);for(j=0;j<n;j++)printf(" %u",indices[j]);printf("\n");
                if(n) {
                    memset(indices,0xa5,(size_t)(total+1)*4);
                    if(rf_geometry_room_children(&geometry,i,indices,n-1,&guard)!=RF_RANGE || guard!=0xa5a5a5a5)return 7;
                    for(j=0;j<=total;j++)if(indices[j]!=0xa5a5a5a5)return 8;
                }
            }
            free(indices);rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
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
