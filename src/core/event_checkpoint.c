#include "rf/event_checkpoint.h"
#include <string.h>
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t signed_word(const unsigned char *p)
{uint32_t v=word(p);return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);}
static uint32_t hash(const unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<128;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static int supported(uint32_t type)
{return type==16||type==20||type==87||type==88;}
static int owner_valid(const rf_runtime_event *e,int32_t now)
{
    if(!e||!e->authored||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!supported(e->state.type)||e->retired||e->state.deadline>=0)return RF_NOT_FOUND;
    if(e->object_kind!=6||e->authored->record.uid==UINT32_MAX)return RF_FORMAT;
    return RF_OK;
}
int rf_event_checkpoint_encode(const unsigned char identity[32],const rf_runtime_event *e,int32_t now,
    void *output,uint32_t capacity)
{
    unsigned char *p=output;int32_t remaining=-1;int status;
    if(!identity||!output||capacity<128)return RF_RANGE;
    status=owner_valid(e,now);if(status)return status;
    if(e->state.type==20){
        if(e->state.source!=UINT32_MAX||e->state.actor!=UINT32_MAX)return RF_NOT_FOUND;
        if(e->cycle.period_ms<0||e->cycle.period_ms>RF_TIMER_PERIOD||e->cycle.enabled>1||e->cycle.unlimited>1)return RF_FORMAT;
        if(e->cycle.deadline>=0){status=rf_timer_remaining(e->cycle.deadline,now,&remaining);if(status)return status;if(remaining<0)remaining=0;}
    } else if(e->state.type==16){if(e->death_fired>1||e->death_time>(uint32_t)RF_TIMER_PERIOD)return RF_FORMAT;}
    else if(e->threshold.fired>1)return RF_FORMAT;
    memset(p,0,128);memcpy(p,"RFEC",4);put(p+4,1);put(p+8,128);put(p+16,e->authored->record.uid);put(p+20,e->state.type);memcpy(p+24,identity,32);
    put(p+64,e->state.flags);put(p+68,e->state.mode);
    if(e->state.type==16){put(p+72,e->death_fired);put(p+76,e->death_time);}
    if(e->state.type==20){put(p+80,(uint32_t)remaining);put(p+84,(uint32_t)e->cycle.period_ms);put(p+88,(uint32_t)e->cycle.limit);
        put(p+92,e->cycle.count);put(p+96,e->cycle.enabled);put(p+100,e->cycle.unlimited);}
    if(e->state.type==87||e->state.type==88){put(p+104,(uint32_t)e->threshold.threshold);put(p+108,e->threshold.fired);}
    put(p+12,hash(p));return RF_OK;
}
int rf_event_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32],
    const rf_runtime_event *e,int32_t now)
{
    const unsigned char *p=data;uint32_t type,i;int32_t remaining,deadline;int status;
    if(!data||!identity)return RF_RANGE;
    status=owner_valid(e,now);if(status)return status;
    if(bytes!=128||memcmp(p,"RFEC",4)||word(p+4)!=1||word(p+8)!=128||word(p+12)!=hash(p)||
       memcmp(p+24,identity,32)||word(p+16)!=e->authored->record.uid||word(p+20)!=e->state.type||word(p+56)||word(p+60))return RF_FORMAT;
    for(i=112;i<128;i+=4)if(word(p+i))return RF_FORMAT;
    type=word(p+20);
    if(type==16){if(word(p+72)>1||word(p+76)>(uint32_t)RF_TIMER_PERIOD)return RF_FORMAT;}
    else if(word(p+72)||word(p+76))return RF_FORMAT;
    if(type==20){
        if(e->state.source!=UINT32_MAX||e->state.actor!=UINT32_MAX)return RF_NOT_FOUND;
        remaining=signed_word(p+80);
        if(remaining<-1||remaining>RF_TIMER_PERIOD||signed_word(p+84)!=e->cycle.period_ms||signed_word(p+84)<0||
           signed_word(p+84)>RF_TIMER_PERIOD||signed_word(p+88)!=e->cycle.limit||word(p+96)>1||word(p+100)>1||word(p+100)!=e->cycle.unlimited)return RF_FORMAT;
        if(remaining>=0){status=rf_timer_set(&deadline,now,remaining);if(status)return status;}
    } else for(i=80;i<104;i+=4)if(word(p+i))return RF_FORMAT;
    if(type==87||type==88){if(signed_word(p+104)!=e->threshold.threshold||word(p+108)>1)return RF_FORMAT;}
    else if(word(p+104)||word(p+108))return RF_FORMAT;
    return RF_OK;
}
int rf_event_checkpoint_restore(const void *data,uint32_t bytes,const unsigned char identity[32],rf_runtime_event *e,int32_t now)
{
    const unsigned char *p=data;int32_t deadline=-1;int status;
    status=rf_event_checkpoint_preflight(data,bytes,identity,e,now);if(status)return status;
    if(e->state.type==20&&signed_word(p+80)>=0){status=rf_timer_set(&deadline,now,signed_word(p+80));if(status)return status;}
    e->state.flags=word(p+64);e->state.mode=word(p+68);
    if(e->state.type==16){e->death_fired=word(p+72);e->death_time=word(p+76);}
    if(e->state.type==20){e->cycle.deadline=deadline;e->cycle.count=word(p+92);e->cycle.enabled=word(p+96);}
    if(e->state.type==87||e->state.type==88)e->threshold.fired=word(p+108);
    return RF_OK;
}
