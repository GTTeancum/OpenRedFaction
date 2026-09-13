#include "rf/geometry.h"
#include "rf/visibility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_level level;
    rf_geometry geometry;
    uint32_t budget = 8u * 1024u * 1024u;
    int result,flags_mode=argc==4 && !strcmp(argv[3],"--flags"),links_mode=argc==4 && !strcmp(argv[3],"--links"),primary_mode=argc==4 && !strcmp(argv[3],"--primary");
    if(argc==2 && !strcmp(argv[1],"--adjacency-fixture")) {
        unsigned char data[276]={0};uint32_t offsets[3]={0,92,184},ids[2]={0,2},corners[9]={0,1,0,1,2,1,0,2,3};
        uint32_t expected_offsets[5]={0,2,3,4,5},expected_faces[5]={0,2,0,2,2},i,j,value;
        rf_geometry g={0};rf_geometry_vertex_faces graph={0},empty={0};
        g.data=data;g.bytes=sizeof(data);g.vertices=4;g.faces=3;g.face_offsets=offsets;
        for(i=0;i<3;i++) {
            value=UINT32_MAX;memcpy(data+offsets[i]+20,&value,4);value=3;memcpy(data+offsets[i]+52,&value,4);
            for(j=0;j<3;j++)memcpy(data+offsets[i]+56+j*12,corners+i*3+j,4);
        }
        if(rf_geometry_vertex_faces_open(&g,ids,2,4096,&graph) || graph.links!=5 ||
           memcmp(graph.offsets,expected_offsets,sizeof(expected_offsets)) || memcmp(graph.faces,expected_faces,sizeof(expected_faces)))return 10;
        value=graph.resident_bytes;rf_geometry_vertex_faces_close(&graph);
        if(!rf_geometry_vertex_faces_open(&g,ids,2,value-1,&graph) || memcmp(&graph,&empty,sizeof(graph)))return 11;
        ids[1]=0;if(!rf_geometry_vertex_faces_open(&g,ids,2,4096,&graph))return 12;
        ids[1]=3;if(!rf_geometry_vertex_faces_open(&g,ids,2,4096,&graph))return 13;
        if(rf_geometry_vertex_faces_open(&g,NULL,0,4096,&graph) || graph.links)return 14;
        for(i=0;i<=4;i++)if(graph.offsets[i])return 15;
        rf_geometry_vertex_faces_close(&graph);rf_geometry_vertex_faces_close(&graph);puts("PASS adjacency subset, duplicate corners, empty selection, budget/index guards and close");return 0;
    }
    if (argc != 3 && argc != 4 && !(argc==5 && (!strcmp(argv[3],"--portal-graph") || !strcmp(argv[3],"--visibility") || !strcmp(argv[3],"--adjacency") || !strcmp(argv[3],"--lightmap-vertices")))) return 2;
    if (argc == 4 && !flags_mode && !links_mode && !primary_mode && strcmp(argv[3],"--portals")) budget = (uint32_t)strtoul(argv[3], NULL, 10);
    result = rf_vpp_open(&archive, argv[1]);
    if (result != RF_OK) return 3;
    result = rf_level_open(&level, &archive, argv[2]);
    if (result == RF_OK) result = rf_geometry_open(&geometry, &level, budget);
    if (result == RF_OK) {
        if(argc==5 && !strcmp(argv[3],"--lightmap-vertices")) {
            rf_geometry_vertex_faces graph={0};rf_lightmap_normal_face *work;uint32_t i,j,max=0,*ids=(uint32_t *)malloc((geometry.faces+1)*4);
            if(!ids)return 6;for(i=0;i<geometry.faces;i++)ids[i]=i;
            result=rf_geometry_vertex_faces_open(&geometry,ids,geometry.faces,(uint32_t)strtoul(argv[4],NULL,10),&graph);free(ids);if(result)return 7;
            for(i=0;i<graph.vertices;i++)if(graph.offsets[i+1]-graph.offsets[i]>max)max=graph.offsets[i+1]-graph.offsets[i];
            work=(rf_lightmap_normal_face *)malloc((max+1)*sizeof(*work));if(!work)return 8;
            _setmode(_fileno(stdout),_O_BINARY);
            for(i=0;i<geometry.faces;i++) {
                rf_geometry_face face;if(rf_geometry_get_face(&geometry,i,&face))return 9;
                if(face.lightmap_mapping==UINT32_MAX)continue;
                for(j=0;j<face.corners;j++) {
                    rf_lightmap_sample_vertex vertex;uint32_t header[3]={i,j,0};memset(&vertex,0xa5,sizeof(vertex));
                    header[2]=rf_geometry_lightmap_vertex(&geometry,&graph,i,j,work,max,&vertex);
                    fwrite(header,sizeof(header),1,stdout);fwrite(&vertex,sizeof(vertex),1,stdout);
                }
            }
            free(work);rf_geometry_vertex_faces_close(&graph);rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
        if(argc==5 && !strcmp(argv[3],"--adjacency")) {
            rf_geometry_vertex_faces graph={0},empty={0};uint32_t i,j,*ids=(uint32_t *)malloc((geometry.faces+1)*4);
            int status;if(!ids)return 6;for(i=0;i<geometry.faces;i++)ids[i]=i;
            status=rf_geometry_vertex_faces_open(&geometry,ids,geometry.faces,(uint32_t)strtoul(argv[4],NULL,10),&graph);free(ids);
            rf_geometry_close(&geometry);rf_vpp_close(&archive);printf("%d\n",status);
            if(status)return memcmp(&graph,&empty,sizeof(graph))?7:0;
            printf("%u %u %u\n",graph.vertices,graph.links,graph.resident_bytes);
            for(i=0;i<graph.vertices;i++){printf("%u",graph.offsets[i+1]-graph.offsets[i]);for(j=graph.offsets[i];j<graph.offsets[i+1];j++)printf(" %u",graph.faces[j]);printf("\n");}
            rf_geometry_vertex_faces_close(&graph);rf_geometry_vertex_faces_close(&graph);return memcmp(&graph,&empty,sizeof(graph))?8:0;
        }
        if(argc==5 && !strcmp(argv[3],"--visibility")) {
            rf_level_visibility state={0},empty={0};rf_visibility_camera camera={0};uint32_t i,j,stage;
            rf_visibility_camera_parameters parameters={{640,480,0,0,1,90,100,1},{0,0,0},{1,0,0,0,1,0,0,0,1},.1f,1,1,1,0};
            int status=rf_level_visibility_open(&geometry,(uint32_t)strtoul(argv[4],NULL,10),&state);
            rf_geometry_close(&geometry);rf_vpp_close(&archive);printf("%d\n",status);
            if(status)return memcmp(&state,&empty,sizeof(state))?6:0;
            printf("%u %u %u %u\n",state.state.count,state.graph.count,state.primary_count,state.resident_bytes);
            for(i=0;i<state.state.count;i++)printf("%u %u %u %u\n",state.rooms[i].first,state.rooms[i].count,state.rooms[i].blocked,state.rooms[i].detail);
            for(i=0;i<state.primary_count;i++)printf("%u ",state.primary[i]);printf("\n");
            for(stage=0;stage<3;stage++) {
                uint32_t start=state.primary_count?state.primary[stage?state.primary_count-1:0]:UINT32_MAX;
                parameters.origin[0]=stage?8:0;
                if(rf_visibility_camera_setup(&parameters,&camera))return 7;
                if(stage!=1 && rf_level_visibility_begin_render(&state))return 8;
                if(stage==2)start=UINT32_MAX;
                status=rf_level_visibility_view(&state,&camera,640,480,start,UINT32_MAX,0,1);
                printf("%d %u\n",status,state.state.visible_count);
                for(i=0;i<state.state.visible_count;i++)printf("%u ",state.state.order[i]);printf("\n");
                for(i=0;i<state.state.count;i++) {
                    uint32_t words[7];memcpy(words,state.state.rooms+i,sizeof(words));
                    for(j=0;j<7;j++)printf("%08x%s",words[j],j==6?"\n":" ");
                }
                for(i=0;i<state.graph.count;i++) {
                    uint32_t words[4];memcpy(words,state.portals[i].rectangle,sizeof(words));
                    printf("%u %u",state.cache[i].valid,state.portals[i].rejected);
                    for(j=0;j<4;j++)printf(" %08x",words[j]);printf("\n");
                }
            }
            rf_level_visibility_close(&state);rf_level_visibility_close(&state);
            return memcmp(&state,&empty,sizeof(state))?9:0;
        }
        if(argc==5 && !strcmp(argv[3],"--portal-graph")) {
            rf_geometry_portal_graph graph={0},empty={0};uint32_t i,j;
            int status=rf_geometry_portal_graph_open(&geometry,(uint32_t)strtoul(argv[4],NULL,10),&graph);
            rf_geometry_close(&geometry);rf_vpp_close(&archive);
            printf("%d\n",status);
            if(status)return memcmp(&graph,&empty,sizeof(graph))?6:0;
            printf("%u %u %u\n",graph.rooms,graph.count,graph.resident_bytes);
            for(i=0;i<graph.rooms;i++) {
                printf("%u",graph.offsets[i+1]-graph.offsets[i]);
                for(j=graph.offsets[i];j<graph.offsets[i+1];j++)printf(" %u",graph.links[j]);
                printf("\n");
            }
            for(i=0;i<graph.count;i++) {
                uint32_t words[8];memcpy(words,graph.portals+i,32);
                for(j=0;j<8;j++)printf("%08x%s",words[j],j==7?"\n":" ");
            }
            rf_geometry_portal_graph_close(&graph);rf_geometry_portal_graph_close(&graph);
            return memcmp(&graph,&empty,sizeof(graph))?7:0;
        }
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
