#include "rf/npc_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFNC requires binary32");
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t signed_word(const unsigned char *p)
{uint32_t u=word(p);int32_t v;memcpy(&v,&u,4);return v;}
static float real(const unsigned char *p)
{uint32_t u=word(p);float v;memcpy(&v,&u,4);return v;}
static void put_real(unsigned char *p,float v)
{uint32_t u;memcpy(&u,&v,4);put(p,u);}
static uint32_t hash(const unsigned char *p,uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=(i>=12&&i<16)?0:p[i];h*=16777619u;}return h;}
static int catalog_valid(const rf_npc_checkpoint_catalog *c)
{
    uint32_t i;if(!c||!c->count||c->count>64)return RF_RANGE;
    for(i=0;i<64;i++){
        const rf_weapon_acquire_definition *d=c->weapons+i;
        if(c->supported[i]>1||(i>=c->count&&c->supported[i]))return RF_RANGE;
        if(c->supported[i]&&(d->ammo_type< -1||d->ammo_type>=32||d->capacity<0||d->magazine<0||
           (d->ammo_type<0&&d->magazine)))return RF_RANGE;
    }
    return RF_OK;
}
static int selected(int32_t id,const rf_npc_checkpoint_record *r,const rf_npc_checkpoint_catalog *c)
{return id==-1||(id>=0&&(uint32_t)id<c->count&&c->supported[id]&&r->inventory.owned[id]);}
static int valid(const rf_npc_checkpoint_record *r,const rf_npc_checkpoint_catalog *c)
{
    uint32_t i;uint8_t used[32]={0};
    if(r->uid==UINT32_MAX||r->class_id==UINT32_MAX||r->retired>1||(r->flags&~0x4004u)||
       !isfinite(r->health)||(!r->retired&&r->health<=0)||!isfinite(r->armor)||r->armor<0||
       !isfinite(r->yaw)||
       (r->ai_mode!=-1&&r->ai_mode!=0&&r->ai_mode!=1&&r->ai_mode!=2&&r->ai_mode!=11))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(r->eye_angles[i])||!isfinite(r->position[i])||!isfinite(r->drop.position[i]))return RF_FORMAT;
    for(i=0;i<64;i++){
        const rf_weapon_acquire_definition *d=c->weapons+i;
        if(r->inventory.owned[i]>1||r->inventory.loaded[i]<0||
           (!c->supported[i]&&(r->inventory.owned[i]||r->inventory.loaded[i]))||
           (c->supported[i]&&r->inventory.loaded[i]>d->magazine))return RF_FORMAT;
        if(c->supported[i]&&d->ammo_type>=0)used[d->ammo_type]=1;
    }
    for(i=0;i<32;i++)if(r->inventory.reserve[i]<0||(!used[i]&&r->inventory.reserve[i]))return RF_FORMAT;
    if(!selected(r->primary,r,c)||!selected(r->secondary,r,c))return RF_FORMAT;
    if(r->drop.state>2||r->drop.quantity<0)return RF_FORMAT;
    if(r->drop.state){
        if(!r->retired||r->drop.weapon<0||(uint32_t)r->drop.weapon>=c->count||!c->supported[r->drop.weapon])return RF_FORMAT;
    }else if(r->drop.weapon||r->drop.quantity||r->drop.position[0]||r->drop.position[1]||r->drop.position[2])return RF_FORMAT;
    return RF_OK;
}
static void read_row(const unsigned char *p,uint32_t version,rf_npc_checkpoint_record *r)
{
    uint32_t i;memset(r,0,sizeof(*r));r->uid=word(p);r->class_id=word(p+4);r->retired=word(p+8);
    r->flags=word(p+12);r->affiliation=word(p+16);r->health=real(p+20);r->armor=real(p+24);
    for(i=0;i<3;i++)r->position[i]=real(p+28+i*4);r->yaw=real(p+40);
    r->primary=signed_word(p+44);r->secondary=signed_word(p+48);
    r->drop.state=word(p+52);r->drop.weapon=signed_word(p+56);r->drop.quantity=signed_word(p+60);
    for(i=0;i<3;i++)r->drop.position[i]=real(p+64+i*4);
    memcpy(r->inventory.owned,p+76,64);
    for(i=0;i<32;i++)r->inventory.reserve[i]=signed_word(p+140+i*4);
    for(i=0;i<64;i++)r->inventory.loaded[i]=signed_word(p+268+i*4);
    r->ai_mode=signed_word(p+524);
    if(version>=2)for(i=0;i<3;i++)r->eye_angles[i]=real(p+528+i*4);
}
static void write_row(unsigned char *p,const rf_npc_checkpoint_record *r)
{
    uint32_t i;put(p,r->uid);put(p+4,r->class_id);put(p+8,r->retired);put(p+12,r->flags);put(p+16,r->affiliation);
    put_real(p+20,r->health);put_real(p+24,r->armor);for(i=0;i<3;i++)put_real(p+28+i*4,r->position[i]);
    put_real(p+40,r->yaw);put(p+44,(uint32_t)r->primary);put(p+48,(uint32_t)r->secondary);
    put(p+52,r->drop.state);put(p+56,(uint32_t)r->drop.weapon);put(p+60,(uint32_t)r->drop.quantity);
    for(i=0;i<3;i++)put_real(p+64+i*4,r->drop.position[i]);memcpy(p+76,r->inventory.owned,64);
    for(i=0;i<32;i++)put(p+140+i*4,(uint32_t)r->inventory.reserve[i]);
    for(i=0;i<64;i++)put(p+268+i*4,(uint32_t)r->inventory.loaded[i]);put(p+524,(uint32_t)r->ai_mode);
    for(i=0;i<3;i++)put_real(p+528+i*4,r->eye_angles[i]);
}
int rf_npc_checkpoint_encode(const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    const rf_npc_checkpoint_record *rows,uint32_t count,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,bytes;int status;
    if(!identity||!output||!written||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    status=catalog_valid(c);if(status)return status;
    bytes=64+count*RF_NPC_CHECKPOINT_ROW;if(bytes>capacity)return RF_RANGE;
    for(i=0;i<count;i++){status=valid(rows+i,c);if(status)return status;if(i&&rows[i-1].uid>=rows[i].uid)return RF_FORMAT;}
    memset(p,0,bytes);memcpy(p,"RFNC",4);put(p+4,2);put(p+8,bytes);put(p+16,count);memcpy(p+24,identity,32);put(p+56,c->hash);
    for(i=0;i<count;i++)write_row(p+64+i*RF_NPC_CHECKPOINT_ROW,rows+i);
    put(p+12,hash(p,bytes));*written=bytes;return RF_OK;
}
static int decode(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *out_count,uint32_t publish)
{
    const unsigned char *p=data;rf_npc_checkpoint_record r;uint32_t i,count,version,stride,previous=0;int status;
    if(!data||!identity||!out_count)return RF_RANGE;
    status=catalog_valid(c);if(status)return status;
    if(bytes<64||bytes>64+RF_NPC_CHECKPOINT_MAX_COUNT*RF_NPC_CHECKPOINT_ROW||memcmp(p,"RFNC",4)||(word(p+4)!=1&&word(p+4)!=2)||
       word(p+8)!=bytes||word(p+20)||word(p+60)||word(p+56)!=c->hash||memcmp(p+24,identity,32)||word(p+12)!=hash(p,bytes))return RF_FORMAT;
    version=word(p+4);stride=version==1?RF_NPC_CHECKPOINT_ROW_V1:RF_NPC_CHECKPOINT_ROW;
    count=word(p+16);if(count>RF_NPC_CHECKPOINT_MAX_COUNT||bytes!=64+count*stride)return RF_FORMAT;
    if(publish&&(count>capacity||(count&&!rows)))return RF_RANGE;
    for(i=0;i<count;i++){read_row(p+64+i*stride,version,&r);status=valid(&r,c);if(status)return status;
        if(i&&previous>=r.uid)return RF_FORMAT;previous=r.uid;}
    if(publish)for(i=0;i<count;i++)read_row(p+64+i*stride,version,rows+i);
    *out_count=count;return RF_OK;
}
int rf_npc_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{return decode(data,bytes,identity,c,rows,capacity,count,1);}
int rf_npc_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,uint32_t *count)
{return decode(data,bytes,identity,c,NULL,0,count,0);}
