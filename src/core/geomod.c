#include "rf/geomod.h"
#include <math.h>
#include <string.h>

static int append(rf_geomod_vertex *out,uint32_t *count,const rf_geomod_vertex *v)
{
    if(*count==RF_GEOMOD_POLYGON_LIMIT)return RF_RANGE;
    out[(*count)++]=*v;return RF_OK;
}
int rf_geomod_polygon_split(const rf_geomod_vertex *vertices,uint32_t count,
    const float plane[4],rf_geomod_vertex *front,uint32_t front_capacity,
    rf_geomod_vertex *back,uint32_t back_capacity,uint32_t *front_count,uint32_t *back_count)
{
    rf_geomod_vertex f[RF_GEOMOD_POLYGON_LIMIT],b[RF_GEOMOD_POLYGON_LIMIT];
    double distances[RF_GEOMOD_POLYGON_LIMIT],norm=0;
    int sides[RF_GEOMOD_POLYGON_LIMIT];uint32_t i,j,nf=0,nb=0,positive=0,negative=0;
    if(!vertices || !plane || !front_count || !back_count || front_count==back_count || count<3 || count>RF_GEOMOD_POLYGON_LIMIT)return RF_RANGE;
    for(j=0;j<4;j++)if(!isfinite(plane[j]))return RF_FORMAT;
    for(j=0;j<3;j++)norm+=(double)plane[j]*plane[j];
    if(fabs(norm-1)>1e-4)return RF_FORMAT;
    for(i=0;i<count;i++) {
        double d=plane[3];
        for(j=0;j<3;j++){if(!isfinite(vertices[i].position[j]))return RF_FORMAT;d+=(double)plane[j]*vertices[i].position[j];}
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_FORMAT;
        distances[i]=d;sides[i]=d>1e-5?1:d< -1e-5?-1:0;
        positive+=sides[i]>0;negative+=sides[i]<0;
    }
    if(!negative){memcpy(f,vertices,count*sizeof(*f));nf=count;}
    else if(!positive){memcpy(b,vertices,count*sizeof(*b));nb=count;}
    else for(i=0;i<count;i++) {
        uint32_t next=(i+1)%count;rf_geomod_vertex cut;double t;
        if(sides[i]>=0 && append(f,&nf,vertices+i))return RF_RANGE;
        if(sides[i]<=0 && append(b,&nb,vertices+i))return RF_RANGE;
        if(sides[i]*sides[next]>=0)continue;
        t=distances[i]/(distances[i]-distances[next]);
        for(j=0;j<3;j++) {
            cut.position[j]=(float)((1-t)*vertices[i].position[j]+t*vertices[next].position[j]);
            if(!isfinite(cut.position[j]))return RF_FORMAT;
        }
        for(j=0;j<2;j++) {
            cut.uv[j]=(float)((1-t)*vertices[i].uv[j]+t*vertices[next].uv[j]);
            if(!isfinite(cut.uv[j]))return RF_FORMAT;
        }
        if(append(f,&nf,&cut) || append(b,&nb,&cut))return RF_RANGE;
    }
    if((front && front_capacity<nf) || (back && back_capacity<nb))return RF_RANGE;
    if(front && nf)memcpy(front,f,nf*sizeof(*front));
    if(back && nb)memcpy(back,b,nb*sizeof(*back));
    *front_count=nf;*back_count=nb;return RF_OK;
}

int rf_geomod_polygon_subtract(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count)
{
    rf_geomod_vertex current[64],front[64],back[64];
    uint32_t pass,i,j,left,nf,nb,total=0,pieces=0,required=0,required_pieces=0;int status;
    if(!vertices || count<3 || count>64 || !planes || !plane_count || plane_count>32 ||
       !vertex_count || !fragment_count || vertex_count==fragment_count || (!!out != !!fragments))return RF_RANGE;
    /* Validate even planes beyond an early empty intersection. */
    for(i=0;i<plane_count;i++) {
        double norm=0;
        for(j=0;j<4;j++)if(!isfinite(planes[i][j]))return RF_FORMAT;
        for(j=0;j<3;j++)norm+=(double)planes[i][j]*planes[i][j];
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
    }
    for(pass=0;pass<(out?2u:1u);pass++) {
        memcpy(current,vertices,count*sizeof(*current));left=count;total=pieces=0;
        for(i=0;i<plane_count && left;i++) {
            status=rf_geomod_polygon_split(current,left,planes[i],front,64,back,64,&nf,&nb);
            if(status)return status;
            if(nf) {
                if(pass) {
                    memcpy(out+total,front,nf*sizeof(*out));
                    fragments[pieces].first=total;fragments[pieces].count=nf;
                }
                total+=nf;pieces++;
            }
            memcpy(current,back,nb*sizeof(*current));left=nb;
        }
        if(!pass) {
            required=total;required_pieces=pieces;
            if(out && (capacity<total || fragment_capacity<pieces))return RF_RANGE;
        }
    }
    *vertex_count=required;*fragment_count=required_pieces;return RF_OK;
}
