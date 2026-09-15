#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"split line %d\n",__LINE__);return 1;}} while(0)
static double area(const rf_geomod_vertex *v,uint32_t n)
{
    uint32_t i;double a=0;
    for(i=0;i<n;i++)a+=(double)v[i].position[0]*v[(i+1)%n].position[1]-(double)v[i].position[1]*v[(i+1)%n].position[0];
    return a*.5;
}
int main(void)
{
    rf_geomod_vertex square[]={{{-1,-1,0},{0,0}},{{1,-1,0},{1,0}},{{1,1,0},{1,1}},{{-1,1,0},{0,1}}};
    rf_geomod_vertex f[64],b[64],oldf[64],oldb[64];uint32_t nf,nb,i,cut=0;
    float plane[]={1,0,0,-.25f};
    CHECK(!rf_geomod_polygon_split(square,4,plane,f,64,b,64,&nf,&nb));
    CHECK(nf==4 && nb==4 && fabs(area(f,nf)-1.5)<1e-6 && fabs(area(b,nb)-2.5)<1e-6);
    for(i=0;i<nf;i++)if(f[i].position[0]==.25f){CHECK(fabs(f[i].uv[0]-.625)<1e-6);cut++;}
    CHECK(cut==2);
    {
        rf_geomod_vertex forward[3]={{{-16,-1,0},{-16,-1}},{{32,2,0},{32,2}},{{0,-1,1},{0,1}}};
        rf_geomod_vertex reverse[3]={forward[1],forward[0],{{0,1,-1},{0,-1}}};
        rf_geomod_vertex first={0},second={0};uint32_t found=0,j;
        const float split[4]={0,1,0,0};
        CHECK(!rf_geomod_polygon_split(forward,3,split,f,64,b,64,&nf,&nb));
        for(j=0;j<nf;j++)if(f[j].position[2]==0 && fabsf(f[j].position[0])<1e-4f){first=f[j];found++;}
        CHECK(!rf_geomod_polygon_split(reverse,3,split,f,64,b,64,&nf,&nb));
        for(j=0;j<nf;j++)if(f[j].position[2]==0 && fabsf(f[j].position[0])<1e-4f){second=f[j];found++;}
        CHECK(found==2 && !memcmp(&first,&second,sizeof(first)));
        reverse[0].uv[0]+=1;reverse[1].uv[0]+=1;found=0;
        CHECK(!rf_geomod_polygon_split(reverse,3,split,f,64,b,64,&nf,&nb));
        for(j=0;j<nf;j++)if(f[j].position[2]==0 && fabsf(f[j].position[0])<1e-4f) {
            CHECK(!memcmp(first.position,f[j].position,12));
            CHECK(fabsf(f[j].uv[0]-first.uv[0]-1)<1e-6f);found++;
        }
        CHECK(found==1);
    }
    CHECK(!rf_geomod_polygon_split(square,4,plane,NULL,0,NULL,0,&nf,&nb) && nf==4 && nb==4);
    memset(f,0xa5,sizeof(f));memset(b,0x5a,sizeof(b));memcpy(oldf,f,sizeof(f));memcpy(oldb,b,sizeof(b));nf=77;nb=88;
    CHECK(rf_geomod_polygon_split(square,4,plane,f,3,b,64,&nf,&nb)==RF_RANGE);
    CHECK(nf==77 && nb==88 && !memcmp(f,oldf,sizeof(f)) && !memcmp(b,oldb,sizeof(b)));
    plane[0]=2;CHECK(rf_geomod_polygon_split(square,4,plane,f,64,b,64,&nf,&nb)==RF_FORMAT);
    plane[0]=1;plane[3]=-1;
    CHECK(!rf_geomod_polygon_split(square,4,plane,f,64,b,64,&nf,&nb) && nf==0 && nb==4);
    plane[0]=0;plane[2]=1;plane[3]=0;
    CHECK(!rf_geomod_polygon_split(square,4,plane,f,64,b,64,&nf,&nb) && nf==4 && nb==0);
    /* Independent area conservation and half-space checks for rotated cuts. */
    for(i=0;i<360;i++) {
        uint32_t j;double angle=i*.017453292519943295;
        plane[0]=(float)cos(angle);plane[1]=(float)sin(angle);plane[2]=0;plane[3]=.3f;
        CHECK(!rf_geomod_polygon_split(square,4,plane,f,64,b,64,&nf,&nb));
        CHECK(nf>=3 && nb>=3 && fabs(area(f,nf)+area(b,nb)-4)<1e-5);
        for(j=0;j<nf;j++)CHECK(f[j].position[0]*plane[0]+f[j].position[1]*plane[1]+plane[3]>=-1e-5);
        for(j=0;j<nb;j++)CHECK(b[j].position[0]*plane[0]+b[j].position[1]*plane[1]+plane[3]<=1e-5);
        {
            rf_geomod_vertex reversed[4]={square[3],square[2],square[1],square[0]};uint32_t nr,mr,k;
            CHECK(!rf_geomod_polygon_split(reversed,4,plane,oldf,64,oldb,64,&nr,&mr) && nr==nf && mr==nb);
            for(j=0;j<nf;j++){for(k=0;k<nr;k++)if(!memcmp(f+j,oldf+k,sizeof(*f)))break;CHECK(k<nr);}
            for(j=0;j<nb;j++){for(k=0;k<mr;k++)if(!memcmp(b+j,oldb+k,sizeof(*b)))break;CHECK(k<mr);}
        }
    }
    {
        const float cutter[4][4]={{1,0,0,-.5f},{-1,0,0,-.5f},{0,1,0,-.5f},{0,-1,0,-.5f}};
        rf_geomod_fragment fragments[4];uint32_t vertices,pieces,j;double remaining=0;
        CHECK(!rf_geomod_polygon_subtract(square,4,cutter,4,f,64,fragments,4,&vertices,&pieces));
        CHECK(pieces==4);
        for(j=0;j<pieces;j++)remaining+=area(f+fragments[j].first,fragments[j].count);
        CHECK(fabs(remaining-3)<1e-6); /* Four-unit square minus one-unit hole. */
        memcpy(oldf,f,sizeof(f));vertices=77;pieces=88;
        CHECK(rf_geomod_polygon_subtract(square,4,cutter,4,f,1,fragments,4,&vertices,&pieces)==RF_RANGE);
        CHECK(vertices==77 && pieces==88 && !memcmp(f,oldf,sizeof(f)));
    }
    puts("PASS: split area/UV/winding, rollback,360 reversed-edge exact comparisons, preserved UV seams and fragment area");return 0;
}
