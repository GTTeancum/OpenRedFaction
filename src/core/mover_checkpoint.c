#include "rf/mover_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFMC binary32");
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t checksum(const unsigned char *p,uint32_t n)
{uint32_t h=2166136261u,i;for(i=0;i<n;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static int cursor_valid(int32_t value,uint32_t n){return value>=-1&&(value<0||(uint32_t)value<n);}
static int valid(const rf_mover_checkpoint_record *r)
{
    uint32_t i;
    if(r->uid==UINT32_MAX || !r->key_count || r->key_count>65535 ||
       (r->kind!=RF_GROUP_RUNTIME_TRANSLATION && r->kind!=RF_GROUP_RUNTIME_ROTATION_PENDING) ||
       !!(r->flags&4)!=(r->kind==RF_GROUP_RUNTIME_ROTATION_PENDING) || r->mode>5 ||
       !cursor_valid(r->current_key,r->key_count) || !cursor_valid(r->next_key,r->key_count) ||
       !cursor_valid(r->terminal_key,r->key_count) || (r->next_key>=0&&r->current_key<0) ||
       r->remaining_ms< -1 || r->remaining_ms>RF_TIMER_PERIOD/2 ||
       !isfinite(r->phase)||r->phase<0||!isfinite(r->speed)||!isfinite(r->distance))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(r->position[i])||!isfinite(r->pending[i])||!isfinite(r->velocity[i]))return RF_FORMAT;
    return RF_OK;
}
static void float_put(unsigned char *p,float f){uint32_t v;memcpy(&v,&f,4);put(p,v);}
static float float_read(const unsigned char *p){uint32_t v=word(p);float f;memcpy(&f,&v,4);return f;}
static void write_row(unsigned char *p,const rf_mover_checkpoint_record *r)
{
    uint32_t i;put(p,r->uid);put(p+4,r->kind);put(p+8,r->key_count);put(p+12,r->flags);put(p+16,r->mode);
    put(p+20,(uint32_t)r->current_key);put(p+24,(uint32_t)r->next_key);put(p+28,(uint32_t)r->terminal_key);
    put(p+32,(uint32_t)r->remaining_ms);put(p+36,r->object_flags);
    float_put(p+40,r->phase);float_put(p+44,r->speed);float_put(p+48,r->distance);
    for(i=0;i<3;i++){float_put(p+52+i*4,r->position[i]);float_put(p+64+i*4,r->pending[i]);float_put(p+76+i*4,r->velocity[i]);}
}
static int32_t signed_read(const unsigned char *p){uint32_t v=word(p);int32_t s;memcpy(&s,&v,4);return s;}
static void read_row(const unsigned char *p,rf_mover_checkpoint_record *r)
{
    uint32_t i;memset(r,0,sizeof(*r));r->uid=word(p);r->kind=word(p+4);r->key_count=word(p+8);r->flags=word(p+12);r->mode=word(p+16);
    r->current_key=signed_read(p+20);r->next_key=signed_read(p+24);r->terminal_key=signed_read(p+28);
    r->remaining_ms=signed_read(p+32);r->object_flags=word(p+36);
    r->phase=float_read(p+40);r->speed=float_read(p+44);r->distance=float_read(p+48);
    for(i=0;i<3;i++){r->position[i]=float_read(p+52+i*4);r->pending[i]=float_read(p+64+i*4);r->velocity[i]=float_read(p+76+i*4);}
}
int rf_mover_checkpoint_capture(const rf_group_runtime_entry *entry,int32_t now,rf_mover_checkpoint_record *out)
{
    rf_mover_checkpoint_record r={0};const rf_group_translation_runtime *s;int status;
    if(!entry||!out||!entry->source||!entry->source->keys||!entry->source->record.key_count||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    s=&entry->translation;r.uid=entry->source->keys[0].uid;r.kind=entry->kind;r.key_count=entry->source->record.key_count;
    r.flags=s->motion.flags;r.mode=s->motion.mode;r.current_key=s->motion.current_key;r.next_key=s->motion.next_key;r.terminal_key=s->motion.terminal_key;
    r.remaining_ms=-1;if(s->deadline>=0){status=rf_timer_remaining(s->deadline,now,&r.remaining_ms);if(status)return status;if(r.remaining_ms<0)r.remaining_ms=0;}
    r.object_flags=s->object_flags;r.phase=s->motion.phase;r.speed=s->speed;r.distance=s->distance;
    memcpy(r.position,s->position,12);memcpy(r.pending,s->pending,12);memcpy(r.velocity,s->velocity,12);
    status=valid(&r);if(status)return status;*out=r;return RF_OK;
}
int rf_mover_checkpoint_prepare(const rf_mover_checkpoint_record *r,const rf_group_runtime_entry *entry,int32_t now,rf_group_translation_runtime *out)
{
    rf_group_translation_runtime s;int status;
    if(!r||!entry||!out||!entry->source||!entry->source->keys||!entry->source->record.key_count||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=valid(r);if(status)return status;
    if(r->uid!=entry->source->keys[0].uid||r->kind!=entry->kind||r->key_count!=entry->source->record.key_count)return RF_FORMAT;
    s=entry->translation;s.motion.flags=r->flags;s.motion.mode=r->mode;s.motion.current_key=r->current_key;s.motion.next_key=r->next_key;s.motion.terminal_key=r->terminal_key;
    s.deadline=-1;if(r->remaining_ms>=0){status=rf_timer_set(&s.deadline,now,r->remaining_ms);if(status)return status;}
    s.object_flags=r->object_flags;s.motion.phase=r->phase;s.speed=r->speed;s.distance=r->distance;
    memcpy(s.position,r->position,12);memcpy(s.pending,r->pending,12);memcpy(s.velocity,r->velocity,12);*out=s;return RF_OK;
}
int rf_mover_checkpoint_encode(const unsigned char identity[32],const rf_mover_checkpoint_record *rows,uint32_t count,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t bytes,i;int status;
    if(!identity||!output||!written||(count&&!rows)||count>RF_MOVER_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    bytes=64+count*88;if(capacity<bytes)return RF_RANGE;
    for(i=0;i<count;i++){status=valid(rows+i);if(status)return status;if(i&&rows[i-1].uid>=rows[i].uid)return RF_FORMAT;}
    memset(p,0,bytes);memcpy(p,"RFMC",4);put(p+4,1);put(p+8,bytes);put(p+16,count);memcpy(p+24,identity,32);
    for(i=0;i<count;i++)write_row(p+64+i*88,rows+i);
    put(p+12,checksum(p,bytes));*written=bytes;return RF_OK;
}
static int decode(const void *data,uint32_t bytes,const unsigned char identity[32],rf_mover_checkpoint_record *rows,uint32_t capacity,uint32_t *count,uint32_t publish)
{
    const unsigned char *p=data;uint32_t n,i,previous=0;rf_mover_checkpoint_record r;int status;
    if(!data||!identity||!count)return RF_RANGE;
    if(bytes<64||bytes>64+RF_MOVER_CHECKPOINT_MAX_COUNT*88||memcmp(p,"RFMC",4)||word(p+4)!=1||word(p+8)!=bytes||word(p+20)||word(p+56)||word(p+60)||memcmp(p+24,identity,32)||word(p+12)!=checksum(p,bytes))return RF_FORMAT;
    n=word(p+16);if(n>RF_MOVER_CHECKPOINT_MAX_COUNT||bytes!=64+n*88)return RF_FORMAT;
    if(publish&&(capacity<n||(n&&!rows)))return RF_RANGE;
    for(i=0;i<n;i++){read_row(p+64+i*88,&r);status=valid(&r);if(status)return status;if(i&&previous>=r.uid)return RF_FORMAT;previous=r.uid;}
    if(publish)for(i=0;i<n;i++)read_row(p+64+i*88,rows+i);
    *count=n;return RF_OK;
}
int rf_mover_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],rf_mover_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{return decode(data,bytes,identity,rows,capacity,count,1);}
int rf_mover_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32],uint32_t *count)
{return decode(data,bytes,identity,NULL,0,count,0);}
