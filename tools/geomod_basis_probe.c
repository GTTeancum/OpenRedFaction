#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    rf_random_state state;float basis[9];uint32_t i;
    if(argc==6 && !strcmp(argv[1],"--debris")) {
        rf_geomod_debris_mesh mesh;uint32_t j,k;
        state.value=(uint32_t)strtoul(argv[2],NULL,0);
        if(rf_geomod_debris_build(strtof(argv[3],NULL),(uint32_t)strtoul(argv[4],NULL,10),
            (uint32_t)strtoul(argv[5],NULL,10),&state,&mesh))return 2;
        printf("%u %.9g",state.value,mesh.lifetime);
        for(i=0;i<8;i++)for(j=0;j<3;j++)printf(" %.9g",mesh.positions[i][j]);
        for(i=0;i<12;i++)for(j=0;j<3;j++)printf(" %u",mesh.indices[i][j]);
        for(i=0;i<12;i++)for(j=0;j<3;j++)for(k=0;k<2;k++)printf(" %.9g",mesh.uv[i][j][k]);
        puts("");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--hardness")) {
        rf_geo_region regions[16];rf_geomod_hardness_result result;uint32_t count,default_value,j;float point[3],scale;
        if(scanf("%u %u %f %f %f %f",&count,&default_value,&scale,point,point+1,point+2)!=6 || count>16)return 3;
        memset(regions,0,sizeof(regions));
        for(i=0;i<count;i++) {
            unsigned flags,hardness;if(scanf("%u %u",&flags,&hardness)!=2)return 3;
            regions[i].flags=(uint16_t)flags;regions[i].hardness=(uint16_t)hardness;
            for(j=0;j<3;j++)if(scanf("%f",regions[i].position+j)!=1)return 3;
            for(j=0;j<9;j++)if(scanf("%f",regions[i].file_basis+j)!=1)return 3;
            for(j=0;j<3;j++)if(scanf("%f",regions[i].dimensions+j)!=1)return 3;
            if(scanf("%f",&regions[i].radius)!=1)return 3;
        }
        if(rf_geomod_hardness(regions,count,default_value,point,scale,&result))return 4;
        printf("%u %u %u %u %.9g\n",result.hardness,result.allowed,result.matches,result.flags,result.scale);return 0;
    }
    if(argc==10) {
        float normal[3],position[3],uv[2];uint32_t width=(uint32_t)strtoul(argv[2],NULL,10),height=(uint32_t)strtoul(argv[3],NULL,10);
        for(i=0;i<3;i++){normal[i]=strtof(argv[4+i],NULL);position[i]=strtof(argv[7+i],NULL);}
        if(rf_geomod_planar_uv(normal,position,width,height,uv))return 2;
        printf("%.9g %.9g\n",uv[0],uv[1]);return 0;
    }
    if(argc!=2)return 1;
    state.value=(uint32_t)strtoul(argv[1],NULL,0);
    if(rf_geomod_random_basis(&state,basis))return 2;
    printf("%u",state.value);
    for(i=0;i<9;i++)printf(" %.9g",basis[i]);
    puts("");return 0;
}
