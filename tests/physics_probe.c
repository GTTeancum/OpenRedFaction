#include "rf/physics.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    float in[3];struct {rf_physics_fallback value;int32_t status;} out;
    _Static_assert(sizeof(out)==28,"Physics probe wire format");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--spheres")) {
        rf_physics_sphere source[32];uint32_t count;
        {
            rf_physics_spheres owned={0},empty={0};uint32_t bad=0x7fc00000;
            memset(source,0,sizeof(source));source[0].radius=1;
            memcpy(&source[0].center[1],&bad,4);
            if(rf_physics_spheres_open(source,1,1024,&owned)!=RF_RANGE || memcmp(&owned,&empty,sizeof(owned)))return 3;
            source[0].center[1]=0;source[0].radius=-1;
            if(rf_physics_spheres_open(source,1,1024,&owned)!=RF_RANGE || memcmp(&owned,&empty,sizeof(owned)))return 3;
            if(rf_physics_spheres_open(NULL,1,1024,&owned)!=RF_RANGE || memcmp(&owned,&empty,sizeof(owned)))return 3;
        }
        while(fread(&count,4,1,stdin)==1) {
            rf_physics_spheres owned={0},small={0},empty={0};uint32_t budget;
            if(count>32 || fread(source,sizeof(*source),count,stdin)!=count)return 2;
            budget=(uint32_t)(sizeof(owned)+count*sizeof(*source));
            if(rf_physics_spheres_open(source,count,budget-1,&small)!=RF_RANGE || memcmp(&small,&empty,sizeof(small)))return 3;
            if(rf_physics_spheres_open(source,count,budget,&owned) || owned.allocated_bytes!=budget)return 3;
            if(rf_physics_spheres_open(source,count,budget,&owned)!=RF_RANGE)return 3;
            memset(source,0xa5,sizeof(source));
            if(fwrite(owned.items,sizeof(*source),count,stdout)!=count)return 1;
            rf_physics_spheres_close(&owned);rf_physics_spheres_close(&owned);
            if(memcmp(&owned,&empty,sizeof(owned)))return 3;
        }
        return ferror(stdin)?1:0;
    }
    while(fread(in,sizeof(in),1,stdin)==1) {
        memset(&out,0xa5,sizeof(out));
        out.status=rf_physics_fallback_prepare(in[0],in[1],in[2],&out.value);
        if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
    }
    return ferror(stdin)?1:0;
}
