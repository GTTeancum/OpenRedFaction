#include "rf/collision.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    struct {float lo[3],hi[3],start[3],end[3],point[3];} input;
    struct {int32_t status;uint32_t hit;float point[3];} output;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
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
