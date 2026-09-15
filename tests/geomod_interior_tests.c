#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"interior line %d\n",__LINE__);return 1;}} while(0)
static void box(const float lo[3],const float hi[3],float planes[6][4],rf_geomod_vertex faces[6][4])
{
    unsigned axis,side,j;const int u[4]={-1,1,1,-1},v[4]={-1,-1,1,1};
    memset(planes,0,6*4*sizeof(float));memset(faces,0,6*4*sizeof(rf_geomod_vertex));
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
        unsigned f=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        planes[f][axis]=side?1:-1;planes[f][3]=side?-hi[axis]:lo[axis];
        for(j=0;j<4;j++) {
            unsigned k=side?j:3-j;
            faces[f][j].position[axis]=side?hi[axis]:lo[axis];
            faces[f][j].position[a]=u[k]>0?hi[a]:lo[a];
            faces[f][j].position[b]=v[k]>0?hi[b]:lo[b];
            faces[f][j].uv[0]=(float)u[k];faces[f][j].uv[1]=(float)v[k];
        }
    }
}
static double volume(const rf_geomod_vertex *p,unsigned n)
{
    unsigned i;double result=0;const float *a=p[0].position;
    for(i=1;i+1<n;i++) {
        const float *b=p[i].position,*c=p[i+1].position;
        result+=((double)a[0]*(b[1]*c[2]-b[2]*c[1])+(double)a[1]*(b[2]*c[0]-b[0]*c[2])+(double)a[2]*(b[0]*c[1]-b[1]*c[0]))/6;
    }
    return result;
}
int main(void)
{
    float source[6][4],cutter[6][4],lo[3]={-2,-2,-2},hi[3]={2,2,2};
    rf_geomod_vertex faces[6][4],cut_faces[6][4],out[2048],sentinel[64];rf_geomod_fragment fragments[32];
    unsigned i,j,n,pieces,caps=0;double result=0;
    const float cut_lo[3]={-1,-1,-3},cut_hi[3]={1,1,3};
    box(lo,hi,source,faces);box(cut_lo,cut_hi,cutter,cut_faces);
    for(i=0;i<6;i++) {
        CHECK(!rf_geomod_polygon_subtract(faces[i],4,cutter,6,out,2048,fragments,32,&n,&pieces));
        for(j=0;j<pieces;j++)result+=volume(out+fragments[j].first,fragments[j].count);
        CHECK(!rf_geomod_interior_face(cut_faces[i],4,source,6,out,2048,&n));
        if(n){CHECK(n==4);caps++;result+=volume(out,n);}
    }
    CHECK(caps==4 && fabs(result-48)<1e-5); /* Cube64 minus through-tunnel16. */
    memset(out,0xa5,sizeof(out));memcpy(sentinel,out,sizeof(sentinel));n=77;
    CHECK(rf_geomod_interior_face(cut_faces[0],4,source,6,out,3,&n)==RF_RANGE);
    CHECK(n==77 && !memcmp(out,sentinel,sizeof(sentinel)));
    CHECK(!rf_geomod_interior_face(faces[0],4,source,6,out,64,&n) && n==0);
    CHECK(!rf_geomod_interior_face(cut_faces[0],4,source,6,NULL,0,&n) && n==4);
    puts("PASS: tunnel signed volume48, four reversed interior faces, boundary exclusion and rollback");return 0;
}
