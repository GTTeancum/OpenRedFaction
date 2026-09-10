#include "rf/particle_pool.h"
#include <string.h>
#include <math.h>
static int pool_valid(const rf_particle_pool *pool)
{
    return pool && pool->particles && pool->lists && pool->list_count>=5;
}
static void set_next(rf_particle_pool *pool,uint32_t index,uint32_t next)
{
    if(index<RF_PARTICLE_CAPACITY)pool->particles[index].next=next;
    else pool->lists[index-RF_PARTICLE_CAPACITY].next=next;
}
static void set_previous(rf_particle_pool *pool,uint32_t index,uint32_t previous)
{
    if(index<RF_PARTICLE_CAPACITY)pool->particles[index].previous=previous;
    else pool->lists[index-RF_PARTICLE_CAPACITY].previous=previous;
}
static void unlink_particle(rf_particle_pool *pool,uint32_t index)
{
    rf_particle *p=&pool->particles[index];
    set_next(pool,p->previous,p->next);set_previous(pool,p->next,p->previous);
}
static void append_particle(rf_particle_pool *pool,uint32_t list,uint32_t index)
{
    rf_particle *p=&pool->particles[index];
    p->next=RF_PARTICLE_CAPACITY+list;p->previous=pool->lists[list].previous;
    set_next(pool,p->previous,index);pool->lists[list].previous=index;
}
int rf_particle_pool_init(rf_particle_pool *pool,rf_particle *storage,
    rf_particle_list *lists,uint32_t list_count)
{
    uint32_t i;
    if(!pool || !storage || !lists || list_count<5 || list_count>UINT32_MAX-RF_PARTICLE_CAPACITY)return RF_RANGE;
    pool->particles=storage;pool->lists=lists;pool->list_count=list_count;
    pool->live[0]=pool->live[1]=0;
    memset(storage,0,RF_PARTICLE_CAPACITY*sizeof(*storage));
    for(i=0;i<list_count;i++)lists[i].next=lists[i].previous=RF_PARTICLE_CAPACITY+i;
    for(i=0;i<RF_PARTICLE_CAPACITY;i++)append_particle(pool,i<RF_PARTICLE_POOL0_CAPACITY?0:1,i);
    return RF_OK;
}
int rf_particle_pool_create(rf_particle_pool *pool,uint32_t kind,
    const rf_particle_spawn *spawn,uint32_t owner,uint32_t room,uint32_t emitter,
    rf_random_state *random,uint32_t *index)
{
    uint32_t first,list;rf_particle value;rf_random_state rng;int status;
    if(!pool_valid(pool) || kind>1 || !spawn || !random || !index || emitter>pool->list_count-5)return RF_RANGE;
    first=pool->lists[kind].next;
    if(first==RF_PARTICLE_CAPACITY+kind)return RF_NOT_FOUND;
    value=pool->particles[first];rng=*random;
    status=rf_particle_initialize(spawn,kind,owner,room,emitter,&rng,&value);
    if(status)return status;
    unlink_particle(pool,first);pool->particles[first]=value;
    list=emitter?4+emitter:2+kind;append_particle(pool,list,first);
    pool->live[kind]++;*random=rng;*index=first;return RF_OK;
}
int rf_particle_pool_detach(rf_particle_pool *pool,uint32_t emitter)
{
    uint32_t list,index,next;
    if(!pool_valid(pool) || !emitter || emitter>pool->list_count-5)return RF_RANGE;
    list=4+emitter;index=pool->lists[list].next;
    while(index!=RF_PARTICLE_CAPACITY+list) {
        next=pool->particles[index].next;unlink_particle(pool,index);
        pool->particles[index].emitter=0;append_particle(pool,4,index);index=next;
    }
    return RF_OK;
}
int rf_particle_pool_recycle(rf_particle_pool *pool,uint32_t index)
{
    rf_particle *p;
    if(!pool_valid(pool) || index>=RF_PARTICLE_CAPACITY)return RF_RANGE;
    p=&pool->particles[index];
    if(!(p->flags&1u) || p->pool>1 || !pool->live[p->pool])return RF_RANGE;
    unlink_particle(pool,index);p->flags=0;append_particle(pool,p->pool,index);
    pool->live[p->pool]--;return RF_OK;
}

int rf_particle_pool_step_free(rf_particle_pool *pool,uint32_t index,float dt)
{
    rf_particle value;unsigned i;double radius,length,inverse;float speed;volatile float ratio;
    if(!pool_valid(pool) || index>=RF_PARTICLE_CAPACITY)return RF_RANGE;
    value=pool->particles[index];
    if(!(value.flags&1u) || value.pool>1 || !pool->live[value.pool])return RF_RANGE;
    if((int32_t)value.owner>=0 || value.emitter || (value.flags&0xff000010u) || (value.secondary&1u))return RF_NOT_FOUND;
    if(!isfinite(dt) || dt<0 || !isfinite(value.age) || value.age<0 ||
       !isfinite(value.life) || value.life<=0 || !isfinite(value.radius) ||
       !isfinite(value.growth) || !isfinite(value.acceleration) || !isfinite(value.gravity))return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(value.position[i]) || !isfinite(value.velocity[i]))return RF_RANGE;
        value.previous_position[i]=value.position[i];
    }
    value.age+=dt;radius=(double)dt*value.growth+value.radius;value.radius=(float)radius;
    if(!isfinite(value.age) || !isfinite(value.radius))return RF_RANGE;
    if(value.age>=value.life || radius<=0) {
        pool->particles[index]=value;return rf_particle_pool_recycle(pool,index);
    }
    value.flags&=~0x8000u;
    for(i=0;i<3;i++)value.position[i]+=(float)((double)value.velocity[i]*dt);
    if(value.flags&0x40u) {
        double x=value.velocity[0],y=value.velocity[1],z=value.velocity[2];
        length=sqrt((x*x+y*y)+z*z);
        if(length<=0) {length=1;value.velocity[0]=1;value.velocity[1]=value.velocity[2]=0;}
        else {
            inverse=1.0/length;
            for(i=0;i<3;i++)value.velocity[i]=(float)(value.velocity[i]*inverse);
        }
        speed=(float)length;speed=(float)((double)dt*value.acceleration+speed);
        for(i=0;i<3;i++)value.velocity[i]*=speed;
    }
    if(value.flags&8u)value.velocity[1]=(float)((double)value.velocity[1]-(double)dt*value.gravity);
    if(value.flags&4u) {
        uint32_t color=0;double fraction;
        ratio=value.age/value.life;fraction=(double)ratio*ratio;
        for(i=0;i<4;i++) {
            int start=(value.color>>(i*8))&255,end=(value.color_destination>>(i*8))&255;
            color|=(uint32_t)(int)(start+(end-start)*fraction)<<(i*8);
        }
        value.color_current=color;
    }
    for(i=0;i<3;i++)if(!isfinite(value.position[i]) || !isfinite(value.velocity[i]))return RF_RANGE;
    pool->particles[index]=value;return RF_OK;
}
