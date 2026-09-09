#include "rf/collision.h"
#include "rf/geometry.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    struct {float lo[3],hi[3],start[3],end[3],point[3];} input;
    struct {int32_t status;uint32_t hit;float point[3];} output;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==4 && (!strcmp(argv[1],"--rooms") || !strcmp(argv[1],"--room-layout") || !strcmp(argv[1],"--room-layout-tight"))) {
        int tight=!strcmp(argv[1],"--room-layout-tight"),layout=strcmp(argv[1],"--rooms")!=0;
        rf_vpp archive;rf_level level;rf_geometry geometry;uint32_t room,total=0,nodes=0,peak=0,queries=0,hits=0;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        for(room=0;room<geometry.rooms;room++) {
            rf_geometry_collision_room owned={0},guard,sentinel;uint32_t i,j,k;
            if(tight)memset(geometry.data+geometry.room_offsets[room]+4,0,24);
            if(rf_geometry_collision_room_open(&geometry,room,8u*1024u*1024u,&owned))return 4;
            memset(&guard,0xa5,sizeof(guard));sentinel=guard;
            if(rf_geometry_collision_room_open(&geometry,room,owned.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
            if(rf_geometry_collision_room_open(&geometry,room,owned.peak_bytes,&guard))return 6;
            rf_geometry_collision_room_close(&guard);
            total+=owned.tree.face_count;nodes+=owned.tree.node_count;if(owned.peak_bytes>peak)peak=owned.peak_bytes;
            if(layout) {
                if(fwrite(geometry.data+geometry.room_offsets[room]+4,24,1,stdout)!=1 || fwrite(owned.minimum,24,1,stdout)!=1 || fwrite(&owned.tree.face_count,4,1,stdout)!=1)return 14;
                for(i=0;i<owned.tree.face_count;i++)if(fwrite(owned.tree.source_indices+i,4,1,stdout)!=1 || fwrite(owned.tree.faces[i].minimum,24,1,stdout)!=1)return 14;
                rf_geometry_collision_room_close(&owned);continue;
            }
            for(i=0;i<owned.tree.face_count;i++) {
                rf_geometry_face original;rf_collision_face *face=owned.tree.faces+i;
                float start[3]={0},delta[3],limit=1;uint32_t matched,brute=0;
                rf_collision_tree_hit result;rf_collision_ray_hit hit;
                if(rf_geometry_get_face(&geometry,owned.tree.source_indices[i],&original) || original.room!=room || original.corners!=face->count)return 7;
                for(j=0;j<i;j++)if(owned.tree.source_indices[j]==owned.tree.source_indices[i])return 8;
                for(j=0;j<face->count;j++) {
                    rf_geometry_corner corner;float position[3];
                    if(rf_geometry_get_corner(&geometry,owned.tree.source_indices[i],j,&corner) || rf_geometry_vertex(&geometry,corner.vertex,position) || memcmp(position,face->vertices[j],12))return 9;
                    for(k=0;k<3;k++)start[k]+=position[k];
                }
                /* Skew synthetic rays to avoid coplanar parallel rays against
                 * adjacent faces (the primitive intentionally preserves NaN). */
                for(j=0;j<3;j++) {start[j]=start[j]/face->count+face->plane[j]+0.0037f*(j+1);delta[j]=-2*face->plane[j]+0.0013f*(j+1);}
                /* Nearest mode: equal-depth identities can differ with traversal order. */
                for(j=0;j<owned.tree.face_count;j++) {
                    rf_collision_face candidate=owned.tree.faces[j];uint32_t accepted;
                    candidate.filter.query_flags=0x460;
                    {int status=rf_collision_thin_face(&candidate,start,delta,limit,&hit,&accepted);if(status) {fprintf(stderr,"room %u ray %u candidate %u status %d\n",room,i,j,status);return 10;}}
                    if(accepted) {brute=1;limit=hit.fraction;}
                }
                if(rf_collision_thin_tree(owned.tree.nodes,owned.tree.node_count,owned.tree.faces,owned.tree.face_count,0x460,start,delta,1,owned.tree.stack,owned.tree.node_capacity,&result,&matched))return 11;
                if(matched!=brute || (matched && result.hit.fraction!=limit))return 12;
                queries++;hits+=matched;
            }
            rf_geometry_collision_room_close(&owned);
        }
        if(total!=geometry.faces)return 13;
        if(!layout)printf("%u %u %u %u %u %u\n",geometry.rooms,total,nodes,peak,queries,hits);
        rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
    }
    if(argc==4 && (!strcmp(argv[1],"--level") || !strcmp(argv[1],"--level-initial"))) {
        int initial=!strcmp(argv[1],"--level-initial");
        rf_vpp archive;rf_level level;rf_geometry geometry;float scratch[256][3];
        rf_collision_face_filter filter={0x461,0,0,0,0,0};uint32_t index,j,k;
        if(rf_vpp_open(&archive,argv[2]))return 3;
        if(rf_level_open(&level,&archive,argv[3])) {rf_vpp_close(&archive);return 3;}
        if(rf_geometry_open(&geometry,&level,8u*1024u*1024u)) {rf_vpp_close(&archive);return 3;}
        for(index=0;index<geometry.faces;index++) {
            rf_collision_face face,guard,sentinel;float start[3]={0},delta[3];
            struct {int32_t status;uint32_t matched;rf_collision_ray_hit hit;} out;
            if(initial && rf_geometry_initial_collision_filter(&geometry,index,0x461,&filter))return 6;
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,256,&face))return 4;
            memset(&guard,0xa5,sizeof(guard));memcpy(&sentinel,&guard,sizeof(guard));
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,face.count-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
            for(j=0;j<3;j++) {
                for(k=0;k<face.count;k++)start[j]+=scratch[k][j];
                start[j]=start[j]/face.count+face.plane[j];delta[j]=-2*face.plane[j];
            }
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=rf_collision_thin_face(&face,start,delta,1,&out.hit,&out.matched);
            if(initial && fwrite(&filter,sizeof(filter),1,stdout)!=1)return 2;
            if(fwrite(&index,4,1,stdout)!=1 || fwrite(&face.count,4,1,stdout)!=1 || fwrite(face.plane,4,4,stdout)!=4 ||
               fwrite(face.minimum,4,3,stdout)!=3 || fwrite(face.maximum,4,3,stdout)!=3 || fwrite(scratch,12,face.count,stdout)!=face.count ||
               fwrite(start,4,3,stdout)!=3 || fwrite(delta,4,3,stdout)!=3 || fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--build")) {
        struct {float faces[16][6];uint32_t count,budget;} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[16];rf_collision_tree tree={0};uint32_t i;int32_t status;
            memset(faces,0,sizeof(faces));for(i=0;i<16;i++)memcpy(faces[i].minimum,in.faces[i],24);
            status=in.count>16?RF_RANGE:rf_collision_tree_open(faces,in.count,in.budget,&tree);
            if(status) {
                rf_collision_tree guard,sentinel;
                memset(&guard,0xa5,sizeof(guard));memcpy(&sentinel,&guard,sizeof(guard));
                if(in.count<=16 && (rf_collision_tree_open(faces,in.count,in.budget,&guard)!=status || memcmp(&guard,&sentinel,sizeof(guard))))return 7;
            }
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&tree.node_count,4,1,stdout)!=1 || fwrite(&tree.peak_bytes,4,1,stdout)!=1)return 2;
            if(!status) {
                if(fwrite(tree.nodes,sizeof(*tree.nodes),tree.node_count,stdout)!=tree.node_count || fwrite(tree.source_indices,4,in.count,stdout)!=in.count)return 2;
            }
            rf_collision_tree_close(&tree);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--partition")) {
        struct {float bounds[6],faces[8][6];} in;
        struct {int32_t status;uint32_t axis,counts[3];uint8_t labels[8];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_node node;rf_collision_face faces[8];unsigned i;
            memcpy(node.minimum,in.bounds,24);
            for(i=0;i<8;i++)memcpy(faces[i].minimum,in.faces[i],24);
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_partition(&node,faces,8,out.labels,&out.axis,out.counts);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--tree")) {
        struct {rf_collision_node nodes[3];float z[3],start[3],delta[3],limit;uint32_t flags;} in;
        struct {int32_t status;uint32_t matched;rf_collision_tree_hit result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[3];float vertices[3][4][3];uint32_t stack[3],i,j;
            for(i=0;i<3;i++) {
                static const float xy[4][2]={{-2,-2},{2,-2},{2,2},{-2,2}};
                memset(faces+i,0,sizeof(faces[i]));faces[i].plane[2]=1;faces[i].plane[3]=-in.z[i];
                faces[i].minimum[0]=faces[i].minimum[1]=-2.0001f;faces[i].maximum[0]=faces[i].maximum[1]=2.0001f;
                faces[i].minimum[2]=in.z[i]-.0001f;faces[i].maximum[2]=in.z[i]+.0001f;
                for(j=0;j<4;j++) {vertices[i][j][0]=xy[j][0];vertices[i][j][1]=xy[j][1];vertices[i][j][2]=in.z[i];}
                faces[i].vertices=vertices[i];faces[i].count=4;
            }
            memset(&out.result,0xa5,sizeof(out.result));out.matched=0xa5a5a5a5;
            out.status=rf_collision_thin_tree(in.nodes,3,faces,3,in.flags,in.start,in.delta,in.limit,stack,3,&out.result,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--thin")) {
        struct {float plane[4],lo[3],hi[3],vertices[8][3],start[3],delta[3],limit;rf_collision_face_filter filter;uint32_t count;} in;
        struct {int32_t status;uint32_t matched;rf_collision_ray_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face face;
            memcpy(face.plane,in.plane,16);memcpy(face.minimum,in.lo,12);memcpy(face.maximum,in.hi,12);
            face.vertices=in.vertices;face.count=in.count;face.filter=in.filter;
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=in.count>8?RF_RANGE:rf_collision_thin_face(&face,in.start,in.delta,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--filter")) {
        rf_collision_face_filter in;struct {int32_t status;uint32_t accepted;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.accepted=0xa5a5a5a5;out.status=rf_collision_face_accept(&in,&out.accepted);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--polygon")) {
        struct {float normal[3],point[3],vertices[16][3];uint32_t count;} in;
        struct {int32_t status;uint32_t inside;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.inside=0xa5a5a5a5;
            out.status=in.count>16?RF_RANGE:rf_collision_polygon_contains(in.normal,in.point,in.vertices,in.count,&out.inside);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--plane")) {
        struct {float start[3],delta[3],plane[4],fraction;} in;
        struct {int32_t status;uint32_t hit;float fraction;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.hit=0xa5a5a5a5;out.fraction=in.fraction;
            out.status=rf_collision_segment_plane(in.start,in.delta,in.plane,&out.fraction,&out.hit);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    while(fread(&input,sizeof(input),1,stdin)==1) {
        unsigned i;output.hit=0xa5a5a5a5;for(i=0;i<3;i++)output.point[i]=input.point[i];
        output.status=rf_collision_segment_box(input.lo,input.hi,input.start,input.end,output.point,&output.hit);
        if(fwrite(&output,sizeof(output),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?2:0;
}
