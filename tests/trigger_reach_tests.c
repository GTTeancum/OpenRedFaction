#include "rf/event.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_trigger_volume v={0};float start[3]={1.4f,0,0},point[3]={7,8,9},saved[3];uint32_t found=99;
    v.shape=1;v.size[0]=v.size[1]=v.size[2]=2;
    v.matrix[0][0]=v.matrix[1][1]=v.matrix[2][2]=1;
    CHECK(rf_trigger_reach_point(&v,start,.5f,point,&found)==0 && found==1);
    CHECK(point[0]<1 && point[0]>.99f && point[1]==0 && point[2]==0);
    memcpy(saved,point,12);start[0]=2;
    CHECK(rf_trigger_reach_point(&v,start,.5f,point,&found)==0 && found==0 && !memcmp(saved,point,12));
    /* Rotated narrow box: local X follows world Z. */
    memset(v.matrix,0,sizeof(v.matrix));v.matrix[0][2]=1;v.matrix[1][1]=1;v.matrix[2][0]=-1;
    v.size[0]=.5f;start[0]=0;start[2]=.6f;
    CHECK(rf_trigger_reach_point(&v,start,.4f,point,&found)==0 && found && fabsf(point[2]-.25f)<.001f);
    v.shape=0;v.radius=1;start[0]=1.4f;start[2]=0;
    CHECK(rf_trigger_reach_point(&v,start,.5f,point,&found)==0 && found && point[0]<1);
    memcpy(saved,point,12);found=99;
    CHECK(rf_trigger_reach_point(&v,start,NAN,point,&found)!=0 && found==99 && !memcmp(saved,point,12));
    puts("Use reach geometry/range/invalid-input checks PASS");return 0;
}
