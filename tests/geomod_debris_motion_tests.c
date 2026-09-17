#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d case%u: %s\n",__LINE__,i,#x);return 1;}}while(0)
static const struct {uint32_t flag,inputs[9],output[3];} cases[]={
#include "fixtures/debris_motion.inc"
};
int main(void)
{
    uint32_t i;float input[9],output[3],before[3]={11,12,13};
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        memcpy(input,cases[i].inputs,sizeof(input));
        CHECK(!rf_geomod_debris_motion(input,input+3,input[6],cases[i].flag,input[7],input[8],output));
        CHECK(!memcmp(output,cases[i].output,12));
        CHECK(!rf_geomod_debris_motion(input,input+3,input[6],cases[i].flag,input[7],input[8],input));
        CHECK(!memcmp(input,cases[i].output,12));
    }
    memcpy(output,before,12);input[0]=NAN;
    CHECK(rf_geomod_debris_motion(input,input+3,.1f,0,0,0,output)==RF_RANGE && !memcmp(output,before,12));
    input[0]=0;CHECK(rf_geomod_debris_motion(input,input+3,.1f,1,NAN,0,output)==RF_RANGE && !memcmp(output,before,12));
    CHECK(rf_geomod_debris_motion(input,input+3,-1,0,0,0,output)==RF_RANGE && !memcmp(output,before,12));
    puts("PASS72 bit-exact original debris motion vectors, aliasing and atomic guards");return 0;
}
