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
    if(argc==2 && !strcmp(argv[1],"--shadow-traversal")) {
        struct {rf_lightmap_shadow_cull cull;unsigned char pass[172],raw[1472];uint32_t formats[8];
            float receiver[4][2],threshold[2];unsigned char mask[1024];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_geometry g={0};uint32_t ids[8],offsets[8],i,status;rf_image images[8]={{0}};const rf_image *table[8];
            float face[4][3],vertices[2][64][3],uv[64][2]={0},intersection[64][2]={0},polygons[2][64][2],distances[64];
            rf_lightmap_shadow_clip_work clip={{polygons[0],polygons[1]},distances,64};
            rf_lightmap_uv_polygon receiver={in.receiver,4};
            rf_lightmap_shadow_filter filter={&receiver,1,{in.threshold[0],in.threshold[1]},&clip,intersection,64};
            rf_lightmap_shadow_pass pass;rf_lightmap_shadow_pass_work pass_work={{vertices[0],vertices[1]},uv,64};
            rf_geometry_shadow_work work={face,4,&pass_work};rf_geometry_shadow_result result;
            g.data=in.raw;g.bytes=1472;g.vertices=32;g.faces=8;g.rooms=1;g.textures=8;g.mappings=2;g.face_offsets=offsets;
            for(i=0;i<8;i++){ids[i]=i;offsets[i]=384+i*136;images[i].source_format=in.formats[i];table[i]=in.formats[i]==UINT32_MAX?NULL:images+i;}
            memcpy(&pass,in.pass,172);pass.filter=&filter;memset(&result,0xa5,sizeof(result));
            status=rf_geometry_shadow_traverse(&g,ids,8,table,8,&in.cull,&pass,&work,in.mask,1024,127,&result);
            fwrite(&status,4,1,stdout);fwrite(&result,sizeof(result),1,stdout);fwrite(in.mask,1024,1,stdout);
        }
        return 0;
    }
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
    if (argc != 3 && argc != 4 && !(argc==5 && (!strcmp(argv[3],"--portal-graph") || !strcmp(argv[3],"--visibility") || !strcmp(argv[3],"--adjacency") || !strcmp(argv[3],"--lightmap-vertices") || !strcmp(argv[3],"--lightmap-polygons") || !strcmp(argv[3],"--shadow-receiver-groups")))) return 2;
    int receiver_mode=argc==4 && !strcmp(argv[3],"--shadow-receivers");
    int shadow_mode=argc==4 && !strcmp(argv[3],"--shadow-faces");
    if (argc == 4 && !receiver_mode && !shadow_mode && !flags_mode && !links_mode && !primary_mode && strcmp(argv[3],"--portals")) budget = (uint32_t)strtoul(argv[3], NULL, 10);
    result = rf_vpp_open(&archive, argv[1]);
    if (result != RF_OK) return 3;
    result = rf_level_open(&level, &archive, argv[2]);
    if (result == RF_OK) result = rf_geometry_open(&geometry, &level, budget);
    if (result == RF_OK) {
        if(receiver_mode) {
            float output[256][2],guard[256][2];uint32_t i,header[2],count;
            _setmode(_fileno(stdout),_O_BINARY);
            for(i=0;i<geometry.faces;i++) {
                rf_geometry_face face;rf_lightmap_mapping mapping;rf_lightmap_sample_plane view={0};
                if(rf_geometry_get_face(&geometry,i,&face))return 6;
                if(face.lightmap_mapping==UINT32_MAX)continue;
                if(rf_geometry_get_lightmap_mapping(&geometry,face.lightmap_mapping,1,&mapping))return 7;
                view.image_width=128;view.image_height=128;view.x=mapping.x;view.y=mapping.y;
                memset(guard,0xa5,sizeof(guard));memcpy(output,guard,sizeof(output));count=UINT32_MAX;
                if(!rf_geometry_shadow_receiver(&geometry,i,&view,output,face.corners-1,&count) || count!=UINT32_MAX || memcmp(output,guard,sizeof(output)))return 8;
                if(rf_geometry_shadow_receiver(&geometry,i,&view,output,256,&count))return 9;
                header[0]=i;header[1]=count;fwrite(header,sizeof(header),1,stdout);fwrite(output,8,count,stdout);
            }
            rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
        if(shadow_mode) {
            float scratch[256][3];rf_lightmap_shadow_face face,guard;uint32_t i,header[2];
            _setmode(_fileno(stdout),_O_BINARY);
            for(i=0;i<geometry.faces;i++) {
                rf_geometry_face source;if(rf_geometry_get_face(&geometry,i,&source))return 6;
                memset(&guard,0xa5,sizeof(guard));face=guard;
                if(!rf_geometry_shadow_face(&geometry,i,scratch,source.corners-1,&face) || memcmp(&face,&guard,sizeof(face)))return 7;
                header[0]=i;header[1]=rf_geometry_shadow_face(&geometry,i,scratch,256,&face);if(header[1])return 8;
                fwrite(header,sizeof(header),1,stdout);fwrite(&face,sizeof(face),1,stdout);
            }
            rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
        if(argc==5 && !strcmp(argv[3],"--shadow-receiver-groups")) {
            rf_lightmaps maps={0};rf_geometry_shadow_receiver_work work={0};uint32_t i,j,*ids=(uint32_t *)malloc((geometry.faces+1)*4);
            if(!ids)return 6;for(i=0;i<geometry.faces;i++)ids[i]=i;
            if(rf_lightmaps_open(&maps,&level,16u*1024u*1024u))return 7;
            _setmode(_fileno(stdout),_O_BINARY);
            for(i=0;i<geometry.mappings;i++) {
                rf_lightmap_mapping mapping;rf_lightmap_sample_plane view={0};uint32_t counts[2],header[3],guard[2]={UINT32_MAX,UINT32_MAX};
                if(rf_geometry_lightmap_sample_binding(&geometry,&maps,i,&mapping,&view))return 9;
                if(rf_geometry_shadow_receivers(&geometry,ids,geometry.faces,i,mapping.room,NULL,NULL,counts,counts+1))return 10;
                if((uint64_t)counts[0]*sizeof(*work.polygons)+(uint64_t)counts[1]*sizeof(*work.vertices)>strtoul(argv[4],NULL,10))return 11;
                work.polygon_capacity=counts[0];work.vertex_capacity=counts[1];
                work.polygons=malloc((counts[0]+1)*sizeof(*work.polygons));work.vertices=malloc((counts[1]+1)*sizeof(*work.vertices));if(!work.polygons || !work.vertices)return 12;
                if(counts[1]) {
                    --work.vertex_capacity;
                    if(!rf_geometry_shadow_receivers(&geometry,ids,geometry.faces,i,mapping.room,&view,&work,guard,guard+1) || guard[0]!=UINT32_MAX || guard[1]!=UINT32_MAX)return 13;
                    ++work.vertex_capacity;
                }
                header[0]=i;header[1]=counts[0];header[2]=counts[1];
                if(rf_geometry_shadow_receivers(&geometry,ids,geometry.faces,i,mapping.room,&view,&work,counts,counts+1))return 14;
                fwrite(header,sizeof(header),1,stdout);
                for(j=0;j<counts[0];j++){fwrite(&work.polygons[j].count,4,1,stdout);fwrite(work.polygons[j].uv,sizeof(*work.vertices),work.polygons[j].count,stdout);}
                free(work.polygons);free(work.vertices);
            }
            rf_lightmaps_close(&maps);free(ids);rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
        if(argc==5 && !strcmp(argv[3],"--lightmap-polygons")) {
            rf_geometry_vertex_faces graph={0};rf_geometry_lightmap_work work={0};uint32_t i,j,max=0,*ids=(uint32_t *)malloc((geometry.faces+1)*4);
            if(!ids)return 6;for(i=0;i<geometry.faces;i++)ids[i]=i;
            if(rf_geometry_vertex_faces_open(&geometry,ids,geometry.faces,262144,&graph))return 7;
            for(i=0;i<graph.vertices;i++)if(graph.offsets[i+1]-graph.offsets[i]>max)max=graph.offsets[i+1]-graph.offsets[i];
            work.normal_capacity=max;work.normals=malloc((max+1)*sizeof(*work.normals));if(!work.normals)return 8;
            _setmode(_fileno(stdout),_O_BINARY);
            for(i=0;i<geometry.mappings;i++) {
                rf_lightmap_mapping mapping;uint32_t counts[2],header[3],guard[2]={UINT32_MAX,UINT32_MAX};
                if(rf_geometry_get_lightmap_mapping(&geometry,i,1,&mapping))return 9;
                if(rf_geometry_lightmap_polygons(&geometry,NULL,ids,geometry.faces,i,mapping.room,NULL,counts,counts+1))return 10;
                if((uint64_t)counts[0]*sizeof(*work.polygons)+(uint64_t)counts[1]*sizeof(*work.vertices)>strtoul(argv[4],NULL,10))return 11;
                work.polygon_capacity=counts[0];work.vertex_capacity=counts[1];
                work.polygons=malloc((counts[0]+1)*sizeof(*work.polygons));work.vertices=malloc((counts[1]+1)*sizeof(*work.vertices));if(!work.polygons || !work.vertices)return 12;
                if(counts[1]) {
                    --work.vertex_capacity;
                    if(!rf_geometry_lightmap_polygons(&geometry,&graph,ids,geometry.faces,i,mapping.room,&work,guard,guard+1) || guard[0]!=UINT32_MAX || guard[1]!=UINT32_MAX)return 13;
                    ++work.vertex_capacity;
                }
                header[0]=i;header[1]=counts[0];header[2]=counts[1];
                if(rf_geometry_lightmap_polygons(&geometry,&graph,ids,geometry.faces,i,mapping.room,&work,counts,counts+1))return 14;
                fwrite(header,sizeof(header),1,stdout);
                for(j=0;j<counts[0];j++){fwrite(&work.polygons[j].count,4,1,stdout);fwrite(work.polygons[j].vertices,sizeof(*work.vertices),work.polygons[j].count,stdout);}
                free(work.polygons);free(work.vertices);
            }
            free(work.normals);free(ids);rf_geometry_vertex_faces_close(&graph);rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
        }
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
