#include "rf/editor_brush.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct brush_record {
    uint32_t start,vertices,vertex_count,faces,face_count,corners,operation,end;
    float origin[3],basis[9];
} brush_record;
static uint32_t u32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t u16(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
static float f32(const unsigned char *p)
{uint32_t word=u32(p);float value;memcpy(&value,&word,4);return value;}
static int advance(uint32_t *at,uint32_t bytes,uint32_t amount)
{if(amount>bytes-*at)return RF_FORMAT;*at+=amount;return RF_OK;}
static int record(const unsigned char *p,uint32_t bytes,uint32_t start,
    uint32_t uid,brush_record *out)
{
    brush_record b={0};uint32_t at=start,n,i,j,textures;
    if(bytes-start<4+12+36+6+4 || u32(p+start)!=uid)return RF_FORMAT;
    b.start=start;at+=4;
    for(i=0;i<3;i++){b.origin[i]=f32(p+at);at+=4;if(!isfinite(b.origin[i]))return RF_FORMAT;}
    /* The file stores rows 2,0,1; runtime uses rows 0,1,2. */
    for(i=0;i<9;i++){
        float v=f32(p+at+i*4);
        if(!isfinite(v))return RF_FORMAT;
        b.basis[(i+6)%9]=v;
    }
    for(i=0;i<3;i++){
        double norm=0;
        for(j=0;j<3;j++)norm+=(double)b.basis[j*3+i]*b.basis[j*3+i];
        if(fabs(norm-1)>0.01)return RF_FORMAT;
    }
    at+=36+6;textures=u32(p+at);at+=4;
    if(textures>64)return RF_FORMAT;
    for(i=0;i<textures;i++){
        if(bytes-at<2)return RF_FORMAT;
        n=u16(p+at);at+=2;
        if(!n || n>255 || advance(&at,bytes,n))return RF_FORMAT;
    }
    if(advance(&at,bytes,16+4))return RF_FORMAT;
    b.vertex_count=u32(p+at-4);b.vertices=at;
    if(b.vertex_count<4 || b.vertex_count>8192 ||
       advance(&at,bytes,b.vertex_count*12+4))return RF_FORMAT;
    b.face_count=u32(p+at-4);b.faces=at;
    if(!b.face_count || b.face_count>8192)return RF_FORMAT;
    for(i=0;i<b.face_count;i++){
        uint32_t mapping,corners,stride;
        if(advance(&at,bytes,56))return RF_FORMAT;
        if(u32(p+at-56+16)>=textures)return RF_FORMAT;
        mapping=u32(p+at-56+20);corners=u32(p+at-56+52);
        if(corners<3 || corners>64 || b.corners>UINT32_MAX-corners)return RF_FORMAT;
        b.corners+=corners;stride=mapping==UINT32_MAX?12:20;
        if(corners>(bytes-at)/stride)return RF_FORMAT;
        for(j=0;j<corners;j++)if(u32(p+at+j*stride)>=b.vertex_count)return RF_FORMAT;
        at+=corners*stride;
    }
    if(advance(&at,bytes,20))return RF_FORMAT;
    b.operation=u32(p+at-20+8);b.end=at;
    if(b.operation>255)return RF_FORMAT;
    *out=b;return RF_OK;
}
static void world(const unsigned char *p,const brush_record *b,uint32_t index,float out[3])
{
    float local[3];uint32_t i,j;
    for(j=0;j<3;j++)local[j]=f32(p+b->vertices+index*12+j*4);
    for(i=0;i<3;i++){
        double v=b->origin[i];
        for(j=0;j<3;j++)v+=(double)local[j]*b->basis[j*3+i];
        out[i]=(float)v;
    }
}
int rf_editor_brush_decode(const void *section,uint32_t bytes,uint32_t uid,
    uint32_t budget,rf_editor_brush *out)
{
    const unsigned char *p=section;brush_record b={0},candidate;
    rf_editor_brush result={0};uint32_t at,found=0,i,j,corner=0;
    float (*points)[3];
    if(!p || !out || !uid || bytes<4 || bytes>4*1024*1024 ||
       budget<sizeof(rf_collision_face))return RF_RANGE;
    for(at=4;at+4<=bytes;at++)if(u32(p+at)==uid &&
        !record(p,bytes,at,uid,&candidate)){
        if(found++)return RF_FORMAT;b=candidate;
    }
    if(!found)return RF_NOT_FOUND;
    if(b.face_count>512 || b.corners>32768 ||
       b.face_count>(UINT32_MAX-b.corners*12)/(sizeof(rf_collision_face)+4))return RF_RANGE;
    result.resident_bytes=b.face_count*(sizeof(rf_collision_face)+4)+b.corners*12;
    if(result.resident_bytes>budget)return RF_RANGE;
    result.storage=calloc(1,result.resident_bytes);if(!result.storage)return RF_IO;
    result.faces=result.storage;
    result.source_words=(uint32_t *)(result.faces+b.face_count);
    points=(float (*)[3])(result.source_words+b.face_count);
    result.uid=uid;result.operation=b.operation;
    result.face_count=b.face_count;result.corner_count=b.corners;
    at=b.faces;
    for(i=0;i<b.face_count;i++){
        rf_collision_face *face=result.faces+i;
        uint32_t count=u32(p+at+52),stride=u32(p+at+20)==UINT32_MAX?12:20;
        double normal[3]={0},length,offset;uint32_t k;
        result.source_words[i]=u32(p+at+24);
        face->vertices=points+corner;face->count=count;at+=56;
        for(j=0;j<count;j++){
            const float *v;
            world(p,&b,u32(p+at+j*stride),points[corner+j]);
            v=points[corner+j];
            for(k=0;k<3;k++){
                if(!isfinite(v[k])){rf_editor_brush_close(&result);return RF_FORMAT;}
                if(!j)face->minimum[k]=face->maximum[k]=v[k];
                if(v[k]<face->minimum[k])face->minimum[k]=v[k];
                if(v[k]>face->maximum[k])face->maximum[k]=v[k];
            }
        }
        at+=count*stride;
        for(j=0;j<count;j++){
            const float *a=points[corner+j],*c=points[corner+(j+1)%count];
            for(k=0;k<3;k++)normal[k]+=(double)(a[(k+1)%3]-c[(k+1)%3])*(a[(k+2)%3]+c[(k+2)%3]);
        }
        length=sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
        if(!isfinite(length) || length<1e-8){rf_editor_brush_close(&result);return RF_FORMAT;}
        offset=0;
        for(k=0;k<3;k++){
            face->plane[k]=(float)(normal[k]/length);
            offset+=(double)face->plane[k]*points[corner][k];
        }
        face->plane[3]=(float)-offset;
        for(j=0;j<count;j++){
            double d=face->plane[3];
            for(k=0;k<3;k++)d+=(double)face->plane[k]*points[corner+j][k];
            if(fabs(d)>1e-4){rf_editor_brush_close(&result);return RF_FORMAT;}
        }
        corner+=count;
    }
    *out=result;return RF_OK;
}
int rf_editor_brush_open(const rf_level *level,uint32_t uid,uint32_t budget,
    rf_editor_brush *out)
{
    const rf_level_section *section;void *data;int status;
    if(!level || !out)return RF_RANGE;
    section=rf_level_find(level,0x2000000);
    if(!section)return RF_NOT_FOUND;
    if(section->size<4 || section->size>4*1024*1024)return RF_RANGE;
    data=malloc(section->size);if(!data)return RF_IO;
    status=rf_level_read(level,section,0,data,section->size);
    if(!status)status=rf_editor_brush_decode(data,section->size,uid,budget,out);
    free(data);return status;
}
void rf_editor_brush_close(rf_editor_brush *brush)
{if(brush){free(brush->storage);memset(brush,0,sizeof(*brush));}}
