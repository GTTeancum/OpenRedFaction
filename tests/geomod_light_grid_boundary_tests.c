/* Captured tenth-overlap face; binary32 coordinates, no original assets. */
#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"boundary line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static const rf_geomod_vertex vertices[]={
    {{-33.4893684f,-11.0623569f,-2.69323492f},{0,0}},
    {{-33.4924088f,-11.0567064f,-2.69610405f},{0,0}},
    {{-33.5147133f,-11.015274f,-2.7171433f},{0,0}},
    {{-33.5077438f,-11.0012407f,-2.70276666f},{0,0}},
    {{-33.4772148f,-10.9397535f,-2.63977528f},{0,0}},
    {{-33.4449272f,-10.874733f,-2.57316446f},{0,0}},
    {{-33.4208794f,-10.8263063f,-2.52355266f},{0,0}},
    {{-33.3061104f,-10.8254328f,-2.35335898f},{0,0}},
    {{-33.2503624f,-11.0973091f,-2.34944892f},{0,0}},
    {{-33.2298775f,-11.544426f,-2.44844055f},{0,0}}
};
static int run(const rf_geomod_vertex *vertices,unsigned count,const float plane[4])
{
    rf_geomod_light_grid grid;unsigned x,y,i,k;
    CHECK(!rf_geomod_light_grid_open(vertices,count,plane,.5f,&grid));
    for(y=0;y<grid.height;y++)for(x=0;x<grid.width;x++) {
        float point[3];double distance=grid.plane[3];
        CHECK(!rf_geomod_light_grid_sample(&grid,vertices,count,x,y,point));
        for(k=0;k<3;k++)distance+=grid.plane[k]*(double)point[k];
        CHECK(fabs(distance)<1e-5);
        for(i=0;i<count;i++) {
            const float *a=vertices[i].position,*b=vertices[(i+1)%count].position;
            double edge[3],q[3],length=0,side=0;
            for(k=0;k<3;k++){edge[k]=(double)b[k]-a[k];q[k]=(double)point[k]-a[k];length+=edge[k]*edge[k];}
            for(k=0;k<3;k++)side+=grid.plane[k]*(edge[(k+1)%3]*q[(k+2)%3]-edge[(k+2)%3]*q[(k+1)%3]);
            if(side < -1e-5*sqrt(length))fprintf(stderr,"sample%u,%u edge%u distance%.12g\n",x,y,i,side/sqrt(length));
            CHECK(side>=-1e-5*sqrt(length));
        }
    }
    puts("PASS captured crater grid samples remain on the face and within all edge halfspaces");return 0;
}

static const rf_geomod_vertex eleven[]={
    {{-35.5308723f,-11.0065556f,-3.94150829f},{0,0}},
    {{-35.8131142f,-10.8438892f,-3.78575182f},{0,0}},
    {{-35.0296288f,-10.2010584f,-3.33147216f},{0,0}},
    {{-34.9840965f,-10.3731117f,-3.47473383f},{0,0}},
    {{-35.0535316f,-10.4535513f,-3.53400826f},{0,0}}
};
int main(void)
{
    const float a[4]={0.818061888f,0.159797937f,-0.552484691f,27.6761475f};
    const float b[4]={-0.0658372641f,0.628141999f,-0.775308371f,1.51853991f};
    CHECK(!run(vertices,10,a));CHECK(!run(eleven,5,b));return 0;
}
