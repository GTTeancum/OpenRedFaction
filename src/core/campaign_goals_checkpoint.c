#include "rf/campaign_goals_checkpoint.h"
#include <string.h>
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t signed_word(const unsigned char *p)
{uint32_t v=word(p);return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);}
static uint32_t hash(const unsigned char *p,uint32_t n)
{uint32_t i,h=2166136261u;for(i=0;i<n;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static int name_valid(const void *p,uint32_t n)
{return ((const unsigned char *)p)[0]&&memchr(p,0,n)!=NULL;}
/* Called only after both names have passed bounded termination checks. */
static int same(const void *left,const void *right)
{
    const unsigned char *a=left,*b=right;
    for(;;a++,b++){
        unsigned char x=*a,y=*b;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return 1;
    }
}
int rf_campaign_goals_checkpoint_encode(const unsigned char identity[32],
    const rf_campaign_goals *goals,const rf_campaign_local_goals *locals,
    void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output,*r;uint32_t i,j,bytes;
    if(!identity||!goals||!locals||!output||!written||goals->count>RF_CAMPAIGN_GOALS_MAX||
       locals->count>RF_CAMPAIGN_LOCAL_GOALS_MAX)return RF_RANGE;
    bytes=64+goals->count*264+locals->count*324;if(capacity<bytes)return RF_RANGE;
    for(i=0;i<goals->count;i++){
        if(!name_valid(goals->items[i].name,256)||goals->items[i].persistent>1)return RF_FORMAT;
        for(j=0;j<i;j++)if(same(goals->items[i].name,goals->items[j].name))return RF_FORMAT;
    }
    for(i=0;i<locals->count;i++){
        if(!name_valid(locals->items[i].level,64)||!name_valid(locals->items[i].name,256))return RF_FORMAT;
        for(j=0;j<i;j++)if(same(locals->items[i].level,locals->items[j].level)&&
            same(locals->items[i].name,locals->items[j].name))return RF_FORMAT;
    }
    memset(p,0,bytes);memcpy(p,"RFGC",4);put(p+4,1);put(p+8,bytes);
    put(p+16,goals->count);put(p+20,locals->count);memcpy(p+24,identity,32);
    r=p+64;
    for(i=0;i<goals->count;i++,r+=264){memcpy(r,goals->items[i].name,256);
        put(r+256,(uint32_t)goals->items[i].value);put(r+260,goals->items[i].persistent);}
    for(i=0;i<locals->count;i++,r+=324){memcpy(r,locals->items[i].level,64);
        memcpy(r+64,locals->items[i].name,256);put(r+320,(uint32_t)locals->items[i].value);}
    put(p+12,hash(p,bytes));*written=bytes;return RF_OK;
}
int rf_campaign_goals_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32])
{
    const unsigned char *p=data,*r,*prior,*local;uint32_t count,n,i,j;
    if(!data||!identity)return RF_RANGE;
    if(bytes<64||bytes>RF_CAMPAIGN_GOALS_CHECKPOINT_MAX_BYTES||memcmp(p,"RFGC",4)||
       word(p+4)!=1||word(p+8)!=bytes||memcmp(p+24,identity,32)||word(p+56)||word(p+60)||
       word(p+12)!=hash(p,bytes))return RF_FORMAT;
    count=word(p+16);n=word(p+20);
    if(count>RF_CAMPAIGN_GOALS_MAX||n>RF_CAMPAIGN_LOCAL_GOALS_MAX||bytes!=64+count*264+n*324)return RF_FORMAT;
    for(i=0;i<count;i++){
        r=p+64+i*264;if(!name_valid(r,256)||word(r+260)>1)return RF_FORMAT;
        for(j=0;j<i;j++)if(same(r,p+64+j*264))return RF_FORMAT;
    }
    local=p+64+count*264;
    for(i=0;i<n;i++){
        r=local+i*324;if(!name_valid(r,64)||!name_valid(r+64,256))return RF_FORMAT;
        for(j=0;j<i;j++){prior=local+j*324;if(same(r,prior)&&same(r+64,prior+64))return RF_FORMAT;}
    }
    return RF_OK;
}
int rf_campaign_goals_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],
    rf_campaign_goals *goals,rf_campaign_local_goals *locals)
{
    const unsigned char *p=data,*r;uint32_t i;int status;
    if(!goals||!locals)return RF_RANGE;
    status=rf_campaign_goals_checkpoint_preflight(data,bytes,identity);if(status)return status;
    memset(goals,0,sizeof(*goals));memset(locals,0,sizeof(*locals));
    goals->count=word(p+16);locals->count=word(p+20);r=p+64;
    for(i=0;i<goals->count;i++,r+=264){memcpy(goals->items[i].name,r,256);
        goals->items[i].value=signed_word(r+256);goals->items[i].persistent=word(r+260);}
    for(i=0;i<locals->count;i++,r+=324){memcpy(locals->items[i].level,r,64);
        memcpy(locals->items[i].name,r+64,256);locals->items[i].value=signed_word(r+320);}
    return RF_OK;
}
