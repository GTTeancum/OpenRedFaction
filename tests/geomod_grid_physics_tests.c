#include "rf/physics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"grid physics line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static const struct {uint8_t cells[64];uint32_t inputs[4],count,radius,spheres[64][5];} rows[]={
#include "fixtures/geomod_grid_spheres.inc"
    };
    rf_physics_sphere spheres[64],before[64];float values[4],radius;uint32_t i,j,count;
    for(i=0;i<sizeof(rows)/sizeof(rows[0]);i++) {
        memcpy(values,rows[i].inputs,sizeof(values));
        CHECK(!rf_physics_grid_spheres(rows[i].cells,values[0],values+1,spheres,64,&count,&radius));
        CHECK(count==rows[i].count && !memcmp(&radius,&rows[i].radius,4));
        for(j=0;j<count;j++)CHECK(!memcmp(spheres+j,rows[i].spheres[j],20) && !spheres[j].opaque_14);
    }
    memset(spheres,0xa5,sizeof(spheres));memcpy(before,spheres,sizeof(before));count=123;radius=456;
    CHECK(rf_physics_grid_spheres(rows[15].cells,1,values+1,spheres,63,&count,&radius)==RF_RANGE);
    CHECK(count==123 && radius==456 && !memcmp(spheres,before,sizeof(before)));
    values[1]=NAN;
    CHECK(rf_physics_grid_spheres(rows[15].cells,1,values+1,spheres,64,&count,&radius)==RF_RANGE);
    CHECK(count==123 && radius==456 && !memcmp(spheres,before,sizeof(before)));
    printf("PASS %u original grid-to-sphere cases, capacity and invalid rollback\n",i);return 0;
}
