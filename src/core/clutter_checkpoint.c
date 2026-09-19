#include "rf/clutter_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFPC needs binary32");
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static uint32_t hash(const unsigned char *p,uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static int valid(const rf_clutter_checkpoint_record *r)
{
    if(r->uid==UINT32_MAX||r->class_id==UINT32_MAX||!isfinite(r->health)||
       (r->flags&~0x204002u)||r->killing_type< -1||r->killing_type>10||r->cooldown_ms< -1||r->cooldown_ms>50||
       (r->health<=0&&!(r->flags&2u)))return RF_FORMAT;
    return RF_OK;
}
static void read_row(const unsigned char *p,rf_clutter_checkpoint_record *r)
{
    uint32_t bits=word(p+8),kind=word(p+16),cooldown=word(p+20);
    r->uid=word(p);r->class_id=word(p+4);memcpy(&r->health,&bits,4);r->flags=word(p+12);
    memcpy(&r->killing_type,&kind,4);memcpy(&r->cooldown_ms,&cooldown,4);
}
int rf_clutter_checkpoint_encode(const unsigned char identity[32],const rf_clutter_checkpoint_record *rows,
    uint32_t count,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,j,bytes,bits;int status;
    if(!identity||!output||!written||(count&&!rows)||count>RF_CLUTTER_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    bytes=RF_CLUTTER_CHECKPOINT_HEADER+count*RF_CLUTTER_CHECKPOINT_ROW;if(capacity<bytes)return RF_RANGE;
    for(i=0;i<count;i++){
        status=valid(rows+i);if(status)return status;
        if(i&&rows[i-1].uid>=rows[i].uid)return RF_FORMAT;
        for(j=0;j<i;j++)if(rows[j].class_id==rows[i].class_id&&rows[j].cooldown_ms!=rows[i].cooldown_ms)return RF_FORMAT;
    }
    memset(p,0,bytes);memcpy(p,"RFPC",4);put(p+4,1);put(p+8,bytes);put(p+16,count);memcpy(p+24,identity,32);
    for(i=0;i<count;i++){
        unsigned char *r=p+64+i*24;put(r,rows[i].uid);put(r+4,rows[i].class_id);
        memcpy(&bits,&rows[i].health,4);put(r+8,bits);put(r+12,rows[i].flags);
        put(r+16,(uint32_t)rows[i].killing_type);put(r+20,(uint32_t)rows[i].cooldown_ms);
    }
    put(p+12,hash(p,bytes));*written=bytes;return RF_OK;
}
static int decode(const void *data,uint32_t bytes,const unsigned char identity[32],
    rf_clutter_checkpoint_record *rows,uint32_t capacity,uint32_t *out_count,uint32_t publish)
{
    const unsigned char *p=data;uint32_t count,i,j,previous=0;rf_clutter_checkpoint_record r,prior;int status;
    if(!data||!identity||!out_count)return RF_RANGE;
    if(bytes<64||bytes>RF_CLUTTER_CHECKPOINT_MAX_BYTES||memcmp(p,"RFPC",4)||word(p+4)!=1||word(p+8)!=bytes||
       word(p+20)||word(p+56)||word(p+60)||memcmp(p+24,identity,32)||word(p+12)!=hash(p,bytes))return RF_FORMAT;
    count=word(p+16);if(count>1024||bytes!=64+count*24)return RF_FORMAT;
    if(publish&&(capacity<count||(count&&!rows)))return RF_RANGE;
    for(i=0;i<count;i++){
        read_row(p+64+i*24,&r);status=valid(&r);if(status)return status;
        if(i&&previous>=r.uid)return RF_FORMAT;previous=r.uid;
        for(j=0;j<i;j++){read_row(p+64+j*24,&prior);if(prior.class_id==r.class_id&&prior.cooldown_ms!=r.cooldown_ms)return RF_FORMAT;}
    }
    if(publish)for(i=0;i<count;i++)read_row(p+64+i*24,rows+i);
    *out_count=count;return RF_OK;
}

int rf_clutter_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],
    rf_clutter_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{return decode(data,bytes,identity,rows,capacity,count,1);}
int rf_clutter_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32],uint32_t *count)
{return decode(data,bytes,identity,NULL,0,count,0);}
