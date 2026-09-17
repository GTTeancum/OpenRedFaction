#include "rf/geomod_solid_clip.h"
#include <math.h>
#include <string.h>

static double dot(const double a[3],const double b[3])
{return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void cross(const double a[3],const double b[3],double c[3])
{c[0]=a[1]*b[2]-a[2]*b[1];c[1]=a[2]*b[0]-a[0]*b[2];c[2]=a[0]*b[1]-a[1]*b[0];}
/* Plane partitioning makes each non-boundary cell homogeneous. Winding at
 * its interior centroid therefore classifies the whole cell, not an arbitrary
 * unsplit polygon that might cross a removed component. */
static int classify(const double point[3],const rf_collision_face *faces,uint32_t count,uint32_t *where)
{
    double angle=0;uint32_t f,j,k;
    for(f=0;f<count;f++) {
        const rf_collision_face *face=faces+f;double d=face->plane[3];
        for(k=0;k<3;k++)d+=(double)face->plane[k]*point[k];
        if(fabs(d)<=1e-5) {
            uint32_t contained=1;
            for(j=0;j<face->count;j++) {
                double edge[3],offset[3],c[3],side=0;
                for(k=0;k<3;k++){edge[k]=(double)face->vertices[(j+1)%face->count][k]-face->vertices[j][k];offset[k]=point[k]-face->vertices[j][k];}
                cross(edge,offset,c);for(k=0;k<3;k++)side+=c[k]*face->plane[k];
                if(side< -1e-5*sqrt(dot(edge,edge))){contained=0;break;}
            }
            if(contained){*where=2;return RF_OK;}
        }
        for(j=1;j+1<face->count;j++) {
            double a[3],b[3],c[3],bc[3],la,lb,lc,denominator;
            for(k=0;k<3;k++){a[k]=face->vertices[0][k]-point[k];b[k]=face->vertices[j][k]-point[k];c[k]=face->vertices[j+1][k]-point[k];}
            la=sqrt(dot(a,a));lb=sqrt(dot(b,b));lc=sqrt(dot(c,c));
            cross(b,c,bc);denominator=la*lb*lc+dot(a,b)*lc+dot(b,c)*la+dot(c,a)*lb;
            angle+=2*atan2(dot(a,bc),denominator);
        }
    }
    angle=fabs(angle)/(4*3.14159265358979323846);
    if(!isfinite(angle) || fabs(angle-floor(angle+.5))>1e-4)return RF_FORMAT;
    *where=angle>.5;return RF_OK;
}
int rf_geomod_polygon_clip_solid(const rf_geomod_vertex *polygon,uint32_t n,
    const rf_collision_face *solid,uint32_t faces,uint32_t inside,
    rf_geomod_solid_clip_work *work,rf_geomod_solid_clip_result *result)
{
    uint32_t bank=0,nf=1,f,i,j,k;int status;
    rf_geomod_vertex front[64],back[64];
    if(!polygon || n<3 || n>64 || !solid || !faces || inside>1 || !work || !result ||
        !work->vertices[0] || !work->vertices[1] || !work->fragments[0] || !work->fragments[1] ||
        work->vertex_capacity<n || !work->fragment_capacity)return RF_RANGE;
    for(i=0;i<n;i++) {
        for(k=0;k<3;k++)if(!isfinite(polygon[i].position[k]))return RF_FORMAT;
        for(k=0;k<2;k++)if(!isfinite(polygon[i].uv[k]))return RF_FORMAT;
    }
    for(f=0;f<faces;f++) {
        double norm=0;
        if(!solid[f].vertices || solid[f].count<3)return RF_FORMAT;
        for(k=0;k<4;k++)if(!isfinite(solid[f].plane[k]))return RF_FORMAT;
        for(k=0;k<3;k++)norm+=(double)solid[f].plane[k]*solid[f].plane[k];
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
        for(j=0;j<solid[f].count;j++)for(k=0;k<3;k++)if(!isfinite(solid[f].vertices[j][k]))return RF_FORMAT;
    }
    memcpy(work->vertices[0],polygon,n*sizeof(*polygon));work->fragments[0][0]=(rf_geomod_fragment){0,n};
    for(f=0;f<faces;f++) {
        uint32_t next=bank^1,used=0,parts=0;
        for(i=0;i<nf;i++) {
            const rf_geomod_fragment *fragment=work->fragments[bank]+i;uint32_t counts[2];
            status=rf_geomod_polygon_split(work->vertices[bank]+fragment->first,fragment->count,
                solid[f].plane,front,64,back,64,counts,counts+1);if(status)return status;
            for(j=0;j<2;j++)if(counts[j]) {
                if(parts==work->fragment_capacity || counts[j]>work->vertex_capacity-used)return RF_RANGE;
                memcpy(work->vertices[next]+used,j?back:front,counts[j]*sizeof(*front));
                work->fragments[next][parts++]=(rf_geomod_fragment){used,counts[j]};used+=counts[j];
            }
        }
        bank=next;nf=parts;
    }
    {
        uint32_t next=bank^1,used=0,parts=0;
        for(i=0;i<nf;i++) {
            const rf_geomod_fragment *fragment=work->fragments[bank]+i;double point[3]={0};uint32_t where;
            const rf_geomod_vertex *v=work->vertices[bank]+fragment->first;
            for(j=0;j<fragment->count;j++)for(k=0;k<3;k++)point[k]+=v[j].position[k];
            for(k=0;k<3;k++)point[k]/=fragment->count;
            status=classify(point,solid,faces,&where);if(status)return status;
            if(where!=inside)continue;
            memcpy(work->vertices[next]+used,v,fragment->count*sizeof(*v));
            work->fragments[next][parts++]=(rf_geomod_fragment){used,fragment->count};used+=fragment->count;
        }
        *result=(rf_geomod_solid_clip_result){work->vertices[next],work->fragments[next],used,parts};
    }
    return RF_OK;
}
