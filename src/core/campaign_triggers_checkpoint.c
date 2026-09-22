#include "rf/campaign_triggers_checkpoint.h"
#include <string.h>
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t signed_word(const unsigned char *p)
{uint32_t v=word(p);return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);}
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
static int valid_state(const rf_campaign_trigger_state *s)
{return !(s->flags&64u)&&s->cooldown_remaining>=-1&&s->cooldown_remaining<=RF_TIMER_PERIOD&&
    s->contact_remaining>=-1&&s->contact_remaining<=RF_TIMER_PERIOD;}
static void read_state(const unsigned char *p,rf_campaign_trigger_state *s)
{
    s->flags=word(p);s->count=word(p+4);s->object_flags=word(p+8);s->activation_time_bits=word(p+12);
    s->limit=signed_word(p+16);s->cooldown_remaining=signed_word(p+20);s->contact_remaining=signed_word(p+24);
}
int rf_campaign_triggers_checkpoint_encode(const unsigned char identity[32],const rf_campaign_triggers *s,
    void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output,*r;uint32_t bytes,i,j;
    if(!identity||!s||!output||!written||s->level_count>RF_CAMPAIGN_PICKUP_LEVELS||s->count>RF_CAMPAIGN_TRIGGER_SLOTS)return RF_RANGE;
    bytes=64+s->level_count*64+s->count*40;if(capacity<bytes)return RF_RANGE;
    for(i=0;i<s->level_count;i++){
        if(!valid_name(s->levels[i]))return RF_FORMAT;
        for(j=0;j<i;j++)if(same(s->levels[i],s->levels[j]))return RF_FORMAT;
    }
    for(i=0;i<s->count;i++){
        if(s->items[i].level>=s->level_count||s->items[i].uid==UINT32_MAX||s->items[i].retired>1||!valid_state(s->states+i))return RF_FORMAT;
        for(j=0;j<i;j++)if(s->items[i].level==s->items[j].level&&s->items[i].uid==s->items[j].uid)return RF_FORMAT;
    }
    memset(p,0,bytes);memcpy(p,"RFTC",4);put(p+4,1);put(p+8,bytes);put(p+16,s->level_count);put(p+20,s->count);memcpy(p+24,identity,32);
    memcpy(p+64,s->levels,s->level_count*64);r=p+64+s->level_count*64;
    for(i=0;i<s->count;i++,r+=40){const rf_campaign_trigger_state *v=s->states+i;
        put(r,s->items[i].level);put(r+4,s->items[i].uid);put(r+8,s->items[i].retired);
        put(r+12,v->flags);put(r+16,v->count);put(r+20,v->object_flags);put(r+24,v->activation_time_bits);
        put(r+28,(uint32_t)v->limit);put(r+32,(uint32_t)v->cooldown_remaining);put(r+36,(uint32_t)v->contact_remaining);}
    put(p+12,hash(p,bytes));*written=bytes;return RF_OK;
}
int rf_campaign_triggers_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32])
{
    const unsigned char *p=data,*rows,*r,*prior;uint32_t levels,count,i,j;rf_campaign_trigger_state state;
    if(!data||!identity)return RF_RANGE;
    if(bytes<64||bytes>RF_CAMPAIGN_TRIGGERS_CHECKPOINT_MAX_BYTES||memcmp(p,"RFTC",4)||word(p+4)!=1||word(p+8)!=bytes||
       memcmp(p+24,identity,32)||word(p+56)||word(p+60)||word(p+12)!=hash(p,bytes))return RF_FORMAT;
    levels=word(p+16);count=word(p+20);
    if(levels>RF_CAMPAIGN_PICKUP_LEVELS||count>RF_CAMPAIGN_TRIGGER_SLOTS||bytes!=64+levels*64+count*40)return RF_FORMAT;
    for(i=0;i<levels;i++){
        if(!valid_name(p+64+i*64))return RF_FORMAT;
        for(j=0;j<i;j++)if(same(p+64+i*64,p+64+j*64))return RF_FORMAT;
    }
    rows=p+64+levels*64;
    for(i=0;i<count;i++){
        r=rows+i*40;read_state(r+12,&state);
        if(word(r)>=levels||word(r+4)==UINT32_MAX||word(r+8)>1||!valid_state(&state))return RF_FORMAT;
        for(j=0;j<i;j++){prior=rows+j*40;if(word(r)==word(prior)&&word(r+4)==word(prior+4))return RF_FORMAT;}
    }
    return RF_OK;
}
int rf_campaign_triggers_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],rf_campaign_triggers *s)
{
    const unsigned char *p=data,*r;uint32_t i;int status;
    if(!s)return RF_RANGE;
    status=rf_campaign_triggers_checkpoint_preflight(data,bytes,identity);if(status)return status;
    memset(s,0,sizeof(*s));s->level_count=word(p+16);s->count=word(p+20);memcpy(s->levels,p+64,s->level_count*64);
    r=p+64+s->level_count*64;
    for(i=0;i<s->count;i++,r+=40){s->items[i].level=word(r);s->items[i].uid=word(r+4);s->items[i].retired=word(r+8);read_state(r+12,s->states+i);}
    return RF_OK;
}
