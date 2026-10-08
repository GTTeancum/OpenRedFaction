#include "rf/campaign_pickups_checkpoint.h"
#include <string.h>
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t hash(const unsigned char *p,uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static int valid_name(const void *p)
{return ((const unsigned char *)p)[0]&&memchr(p,0,64)!=NULL;}
static int same(const void *a,const void *b)
{
    const unsigned char *x=a,*y=b;
    for(;;x++,y++){unsigned char u=*x,v=*y;
        if(u>='A'&&u<='Z')u+=32;if(v>='A'&&v<='Z')v+=32;
        if(u!=v)return 0;if(!u)return 1;}
}
int rf_campaign_pickups_checkpoint_encode_states(const unsigned char identity[32],const rf_campaign_pickups *s,
    const uint8_t *states,uint32_t state_capacity,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output,*r;uint32_t bytes,i,j,version=1,stride=12;
    if(!identity||!s||!output||!written||s->level_count>RF_CAMPAIGN_PICKUP_LEVELS||s->count>RF_CAMPAIGN_PICKUP_SLOTS)return RF_RANGE;
    if((!states&&state_capacity)||(states&&(state_capacity<s->count||state_capacity>RF_CAMPAIGN_PICKUP_SLOTS)))return RF_RANGE;
    if(states)for(i=0;i<state_capacity;i++){
        if(states[i]>2||(i>=s->count&&states[i]))return RF_FORMAT;
        if(states[i])version=2;
    }
    stride=version==2?16:12;
    bytes=64+s->level_count*64+s->count*stride;if(capacity<bytes)return RF_RANGE;
    for(i=0;i<s->level_count;i++){
        if(!valid_name(s->levels[i]))return RF_FORMAT;
        for(j=0;j<i;j++)if(same(s->levels[i],s->levels[j]))return RF_FORMAT;
    }
    for(i=0;i<s->count;i++){
        if(s->items[i].level>=s->level_count||s->items[i].uid==UINT32_MAX||s->items[i].retired>1)return RF_FORMAT;
        for(j=0;j<i;j++)if(s->items[i].level==s->items[j].level&&s->items[i].uid==s->items[j].uid)return RF_FORMAT;
    }
    memset(p,0,bytes);memcpy(p,"RFIP",4);put(p+4,version);put(p+8,bytes);put(p+16,s->level_count);put(p+20,s->count);memcpy(p+24,identity,32);
    memcpy(p+64,s->levels,s->level_count*64);r=p+64+s->level_count*64;
    for(i=0;i<s->count;i++,r+=stride){
        put(r,s->items[i].level);put(r+4,s->items[i].uid);put(r+8,s->items[i].retired);
        if(version==2)put(r+12,states[i]);
    }
    put(p+12,hash(p,bytes));*written=bytes;return RF_OK;
}
int rf_campaign_pickups_checkpoint_encode(const unsigned char identity[32],const rf_campaign_pickups *s,
    void *output,uint32_t capacity,uint32_t *written)
{return rf_campaign_pickups_checkpoint_encode_states(identity,s,NULL,0,output,capacity,written);}
int rf_campaign_pickups_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32])
{
    const unsigned char *p=data,*rows,*r,*prior;uint32_t levels,count,i,j,version,stride;
    if(!data||!identity)return RF_RANGE;
    if(bytes<64||bytes>RF_CAMPAIGN_PICKUPS_CHECKPOINT_MAX_BYTES||memcmp(p,"RFIP",4)||(word(p+4)!=1&&word(p+4)!=2)||word(p+8)!=bytes||
       memcmp(p+24,identity,32)||word(p+56)||word(p+60)||word(p+12)!=hash(p,bytes))return RF_FORMAT;
    version=word(p+4);stride=version==2?16:12;levels=word(p+16);count=word(p+20);
    if(levels>RF_CAMPAIGN_PICKUP_LEVELS||count>RF_CAMPAIGN_PICKUP_SLOTS||bytes!=64+levels*64+count*stride)return RF_FORMAT;
    for(i=0;i<levels;i++){
        if(!valid_name(p+64+i*64))return RF_FORMAT;
        for(j=0;j<i;j++)if(same(p+64+i*64,p+64+j*64))return RF_FORMAT;
    }
    rows=p+64+levels*64;
    for(i=0;i<count;i++){
        r=rows+i*stride;
        if(word(r)>=levels||word(r+4)==UINT32_MAX||word(r+8)>1||(version==2&&word(r+12)>2))return RF_FORMAT;
        for(j=0;j<i;j++){prior=rows+j*stride;if(word(r)==word(prior)&&word(r+4)==word(prior+4))return RF_FORMAT;}
    }
    return RF_OK;
}
int rf_campaign_pickups_checkpoint_decode_states(const void *data,uint32_t bytes,const unsigned char identity[32],
    rf_campaign_pickups *s,uint8_t *states,uint32_t state_capacity)
{
    const unsigned char *p=data,*r;uint32_t i,count,version,stride;int status;
    if(!s||(!states&&state_capacity)||state_capacity>RF_CAMPAIGN_PICKUP_SLOTS)return RF_RANGE;
    status=rf_campaign_pickups_checkpoint_preflight(data,bytes,identity);if(status)return status;
    count=word(p+20);version=word(p+4);stride=version==2?16:12;
    if(states&&state_capacity<count)return RF_RANGE;
    /* A legacy caller has nowhere to retain RFIP2 pickup policy. Reject it
     * before touching either output rather than silently dropping that state. */
    if(version==2&&!states)return RF_NOT_FOUND;
    memset(s,0,sizeof(*s));s->level_count=word(p+16);s->count=count;memcpy(s->levels,p+64,s->level_count*64);
    if(states)memset(states,0,state_capacity);
    r=p+64+s->level_count*64;
    for(i=0;i<count;i++,r+=stride){
        s->items[i].level=word(r);s->items[i].uid=word(r+4);s->items[i].retired=word(r+8);
        if(states&&version==2)states[i]=(uint8_t)word(r+12);
    }
    return RF_OK;
}
int rf_campaign_pickups_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],rf_campaign_pickups *s)
{return rf_campaign_pickups_checkpoint_decode_states(data,bytes,identity,s,NULL,0);}
