#include "rf/particle_pool.h"
#include <string.h>
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
