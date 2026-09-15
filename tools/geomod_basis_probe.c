#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
    rf_random_state state;float basis[9];uint32_t i;
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
