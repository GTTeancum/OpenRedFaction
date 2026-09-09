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
    if(argc==4 && !strcmp(argv[1],"--level")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;float scratch[256][3];
        rf_collision_face_filter filter={0x461,0,0,0,0,0};uint32_t index,j,k;
        if(rf_vpp_open(&archive,argv[2]))return 3;
        if(rf_level_open(&level,&archive,argv[3])) {rf_vpp_close(&archive);return 3;}
        if(rf_geometry_open(&geometry,&level,8u*1024u*1024u)) {rf_vpp_close(&archive);return 3;}
        for(index=0;index<geometry.faces;index++) {
            rf_collision_face face,guard,sentinel;float start[3]={0},delta[3];
            struct {int32_t status;uint32_t matched;rf_collision_ray_hit hit;} out;
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,256,&face))return 4;
            memset(&guard,0xa5,sizeof(guard));memcpy(&sentinel,&guard,sizeof(guard));
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,face.count-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
            for(j=0;j<3;j++) {
                for(k=0;k<face.count;k++)start[j]+=scratch[k][j];
                start[j]=start[j]/face.count+face.plane[j];delta[j]=-2*face.plane[j];
            }
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=rf_collision_thin_face(&face,start,delta,1,&out.hit,&out.matched);
            if(fwrite(&index,4,1,stdout)!=1 || fwrite(&face.count,4,1,stdout)!=1 || fwrite(face.plane,4,4,stdout)!=4 ||
               fwrite(face.minimum,4,3,stdout)!=3 || fwrite(face.maximum,4,3,stdout)!=3 || fwrite(scratch,12,face.count,stdout)!=face.count ||
               fwrite(start,4,3,stdout)!=3 || fwrite(delta,4,3,stdout)!=3 || fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
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
