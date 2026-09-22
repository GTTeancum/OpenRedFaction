#include "rf/world_checkpoint.h"
#include <string.h>
#include <stdint.h>
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t checksum(const unsigned char *p,uint32_t n)
{uint32_t i,h=2166136261u;for(i=0;i<n;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static int overlap(const void *a,uint32_t an,const void *b,uint32_t bn)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(!an||!bn)return 0;
    if(x>UINTPTR_MAX-an||y>UINTPTR_MAX-bn)return 1;
    return x<y+bn&&y<x+an;
}
int rf_world_checkpoint_encode(const rf_world_checkpoint *input,uint32_t required,
    void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,at=RF_WORLD_CHECKPOINT_PREFIX,bytes,n;uint64_t total=RF_WORLD_CHECKPOINT_PREFIX;
    const char *end;
    if(!input||!output||!written||(required&~RF_WORLD_CHECKPOINT_ALL_MASK))return RF_RANGE;
    end=memchr(input->level,0,64);if(!end||end==input->level)return RF_FORMAT;n=(uint32_t)(end-input->level);
    for(i=0;i<RF_WORLD_CHECKPOINT_SECTIONS;i++){
        uint32_t size=input->sections[i].bytes;
        if(size&&!input->sections[i].data)return RF_RANGE;
        if(!size&&(required&(1u<<i)))return RF_FORMAT;
        total+=size;
    }
    if(total>RF_CHECKPOINT_FILE_MAX||total>capacity)return RF_RANGE;bytes=(uint32_t)total;
    if(overlap(input,sizeof(*input),output,bytes)||overlap(written,sizeof(*written),output,bytes)||
       overlap(input,sizeof(*input),written,sizeof(*written)))return RF_RANGE;
    for(i=0;i<RF_WORLD_CHECKPOINT_SECTIONS;i++){
        const rf_world_checkpoint_slice *s=input->sections+i;
        if(s->bytes&&((overlap(s->data,s->bytes,output,bytes)&&s->data!=p+at)||
           overlap(s->data,s->bytes,written,sizeof(*written))))return RF_RANGE;
        at+=s->bytes;
    }
    /* All failure paths precede writes; only clear metadata, never staged slices. */
    memset(p,0,RF_WORLD_CHECKPOINT_PREFIX);memcpy(p,"RFWC",4);put(p+4,1);put(p+8,bytes);
    put(p+16,RF_WORLD_CHECKPOINT_SECTIONS);memcpy(p+24,input->identity,32);memcpy(p+56,input->level,n);
    at=RF_WORLD_CHECKPOINT_PREFIX;
    for(i=0;i<RF_WORLD_CHECKPOINT_SECTIONS;i++){
        const rf_world_checkpoint_slice *s=input->sections+i;unsigned char *d=p+128+i*12;
        put(d,i+1);put(d+4,at);put(d+8,s->bytes);
        if(s->bytes&&s->data!=p+at)memcpy(p+at,s->data,s->bytes);at+=s->bytes;
    }
    put(p+12,checksum(p,bytes));*written=bytes;return RF_OK;
}
static int decode(const void *input,uint32_t bytes,uint32_t required,rf_world_checkpoint *output)
{
    const unsigned char *p=input;rf_world_checkpoint result={0};uint32_t i,at=RF_WORLD_CHECKPOINT_PREFIX,n;
    if(!input||(required&~RF_WORLD_CHECKPOINT_ALL_MASK))return RF_RANGE;
    if(output&&overlap(input,bytes,output,sizeof(*output)))return RF_RANGE;
    if(bytes<RF_WORLD_CHECKPOINT_PREFIX||bytes>RF_CHECKPOINT_FILE_MAX||memcmp(p,"RFWC",4)||word(p+4)!=1||
       word(p+8)!=bytes||word(p+16)!=RF_WORLD_CHECKPOINT_SECTIONS||word(p+20)||word(p+120)||word(p+124)||word(p+12)!=checksum(p,bytes))return RF_FORMAT;
    for(n=0;n<64&&p[56+n];n++){}if(!n||n==64)return RF_FORMAT;
    for(i=n;i<64;i++)if(p[56+i])return RF_FORMAT;
    memcpy(result.identity,p+24,32);memcpy(result.level,p+56,64);
    for(i=0;i<RF_WORLD_CHECKPOINT_SECTIONS;i++){
        const unsigned char *d=p+128+i*12;uint32_t size=word(d+8);
        if(word(d)!=i+1||word(d+4)!=at||size>bytes-at||(!size&&(required&(1u<<i))))return RF_FORMAT;
        result.sections[i].bytes=size;result.sections[i].data=size?p+at:NULL;at+=size;
    }
    if(at!=bytes)return RF_FORMAT;
    if(output)*output=result;return RF_OK;
}
int rf_world_checkpoint_decode(const void *data,uint32_t bytes,uint32_t required,rf_world_checkpoint *out)
{if(!out)return RF_RANGE;return decode(data,bytes,required,out);}
int rf_world_checkpoint_preflight(const void *data,uint32_t bytes,uint32_t required)
{return decode(data,bytes,required,NULL);}
