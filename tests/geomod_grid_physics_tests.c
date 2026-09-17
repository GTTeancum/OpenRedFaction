#include "rf/physics.h"
#include "rf/collision.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"grid physics line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int solid_mass_cases(void)
{
    static const struct {uint32_t count,boxes[12],density;uint8_t cells[64];uint32_t output[17];} rows[]={
#include "fixtures/geomod_solid_mass.inc"
    };
    uint32_t r,b,axis,side,j,k,n;
    for(r=0;r<sizeof(rows)/sizeof(rows[0]);r++) {
        rf_collision_face faces[12]={0};float vertices[12][4][3],boxes[12],density,lo[3],hi[3];
        rf_physics_solid_mass out;float expected[17],actual[17];
        memcpy(boxes,rows[r].boxes,sizeof(boxes));memcpy(&density,&rows[r].density,4);
        memcpy(expected,rows[r].output,sizeof(expected));
        for(k=0;k<3;k++){lo[k]=boxes[k];hi[k]=boxes[k+3];}
        n=0;
        for(b=0;b<rows[r].count;b++) {
            const float *low=boxes+6*b,*high=low+3;
            for(k=0;k<3;k++){if(low[k]<lo[k])lo[k]=low[k];if(high[k]>hi[k])hi[k]=high[k];}
            for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
                uint32_t other[2],o=0;float plane=side?high[axis]:low[axis];
                rf_collision_face *f=faces+n;
                for(k=0;k<3;k++)if(k!=axis)other[o++]=k;
                f->plane[axis]=side?1.f:-1.f;f->plane[3]=-f->plane[axis]*plane;
                memcpy(f->minimum,low,12);memcpy(f->maximum,high,12);
                f->minimum[axis]=(float)((double)plane-.00001);f->maximum[axis]=(float)((double)plane+.00001);
                f->vertices=vertices[n];f->count=4;
                for(j=0;j<4;j++) {
                    vertices[n][j][axis]=plane;
                    vertices[n][j][other[0]]=(j==1 || j==2)?high[other[0]]:low[other[0]];
                    vertices[n][j][other[1]]=j>=2?high[other[1]]:low[other[1]];
                }
                n++;
            }
        }
        for(k=0;k<3;k++){lo[k]=(float)((double)lo[k]-.0001);hi[k]=(float)((double)hi[k]+.0001);}
        CHECK(!rf_physics_solid_mass_prepare(faces,n,lo,hi,density,&out));
        CHECK(!memcmp(out.cells,rows[r].cells,64));
        memcpy(actual,&out.spacing,sizeof(actual));
        if(memcmp(actual,expected,sizeof(expected))) {
            for(k=0;k<17;k++)if(memcmp(actual+k,expected+k,4))
                fprintf(stderr,"case%u word%u got %.9g expected %.9g\n",r,k,actual[k],expected[k]);
            return 1;
        }
        {rf_physics_solid_mass before=out;lo[0]=NAN;
         CHECK(rf_physics_solid_mass_prepare(faces,n,lo,hi,density,&out)==RF_RANGE);
         CHECK(!memcmp(&out,&before,sizeof(out)));}
    }
    printf("PASS %u original solid-grid mass/inertia cases\n",r);return 0;
}
int main(void)
{
    static const struct {uint8_t cells[64];uint32_t inputs[4],count,radius,spheres[64][5];} rows[]={
#include "fixtures/geomod_grid_spheres.inc"
    };
    CHECK(!solid_mass_cases());
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
