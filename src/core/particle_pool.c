#include "rf/particle_pool.h"
#include "rf/level.h"
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
static double emission_range(rf_random_state *random,float low,float high)
{
    uint32_t draw;rf_random_next(random,&draw);
    return ((double)high-low)*((double)draw/32768.0)+low;
}
int rf_particle_emitter_emit_parent(rf_particle_pool *pool,rf_particle_emitter *emitter,
    uint32_t handle,int32_t now_ms,const rf_particle_emitter_parent *parent,
    rf_random_state *random,uint32_t *index)
{
    rf_particle_emitter value;rf_random_state rng;float speed;double projection,delay;
    unsigned i;int status;uint32_t owner;float direction[3];double inverse;
    if(!pool_valid(pool) || !emitter || !random || !index || !handle ||
       handle>pool->list_count-5 || now_ms<0 || now_ms>RF_TIMER_PERIOD)return RF_RANGE;
    value=*emitter;rng=*random;owner=(uint32_t)value.owner;
    if(value.owner<0)parent=NULL;
    if(!isfinite(value.direction_random) || !isfinite(value.min_velocity) ||
       !isfinite(value.max_velocity) || !isfinite(value.spawn_radius) ||
       !isfinite(value.min_spawn_delay) || !isfinite(value.max_spawn_delay) ||
       value.min_spawn_delay<0 || value.max_spawn_delay<0 ||
       value.min_spawn_delay>RF_TIMER_PERIOD/1000.0 || value.max_spawn_delay>RF_TIMER_PERIOD/1000.0 ||
       !isfinite(value.min_life) || !isfinite(value.max_life) ||
       !isfinite(value.min_radius) || !isfinite(value.max_radius))return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(value.position[i]) || !isfinite(value.direction[i]))return RF_RANGE;
        value.spawn.position[i]=value.position[i];direction[i]=value.direction[i];
    }
    if(parent && !(value.flags&0x40u)) {
        for(i=0;i<9;i++)if(!isfinite(parent->basis[i]))return RF_RANGE;
        for(i=0;i<3;i++) {
            if(!isfinite(parent->position[i]))return RF_RANGE;
            value.spawn.position[i]=(float)(((double)value.position[2]*parent->basis[6+i]+
                (double)value.position[1]*parent->basis[3+i])+(double)value.position[0]*parent->basis[i]);
            value.spawn.position[i]+=parent->position[i];
            direction[i]=(float)(((double)value.direction[2]*parent->basis[6+i]+
                (double)value.direction[1]*parent->basis[3+i])+(double)value.direction[0]*parent->basis[i]);
        }
        inverse=1.0/sqrt(((double)direction[0]*direction[0]+(double)direction[1]*direction[1])+
                        (double)direction[2]*direction[2]);
        for(i=0;i<3;i++) {
            direction[i]=(float)((double)direction[i]*inverse);
            if(!isfinite(direction[i]))return RF_RANGE;
        }
    }
    for(i=0;i<3;i++)value.spawn.velocity[i]=direction[i];
    if(value.direction_random<1) {
        if(value.direction_random<-1)value.direction_random=-1;
        status=rf_particle_cone_oriented(direction,value.direction_random,&rng,value.spawn.velocity);
        if(status)return status;
    }
    for(i=0;i<3;i++)value.spawn.position[i]+=(float)((double)value.spawn.velocity[i]*value.spawn_radius);
    speed=(float)emission_range(&rng,value.min_velocity,value.max_velocity);
    if(value.flags&8u) {
        projection=((double)value.spawn.velocity[2]*direction[2]+(double)value.spawn.velocity[1]*direction[1])+
                    (double)value.spawn.velocity[0]*direction[0];
        speed=(float)(projection*speed);
    }
    for(i=0;i<3;i++) {
        value.spawn.velocity[i]=(float)((double)value.spawn.velocity[i]*speed);
        if(!isfinite(value.spawn.position[i]) || !isfinite(value.spawn.velocity[i]))return RF_RANGE;
    }
    value.spawn.radius=(float)emission_range(&rng,value.min_radius,value.max_radius);
    value.spawn.life=(float)emission_range(&rng,value.min_life,value.max_life);
    if(parent) {
        if(value.flags&0x80u) {
            owner=parent->handle;
            for(i=0;i<3;i++) {
                value.spawn.velocity[i]+=parent->velocity[i];
                if(!isfinite(value.spawn.velocity[i]))return RF_RANGE;
            }
        }
        if(parent->remaining_life>0 && (parent->class_flags&0x40u))owner=parent->handle;
    }
    status=rf_particle_pool_create(pool,1,&value.spawn,owner,value.room,handle,&rng,index);
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    delay=emission_range(&rng,value.min_spawn_delay,value.max_spawn_delay)*1000.0;
    rf_timer_set(&value.deadline,now_ms,(int32_t)delay);
    *emitter=value;*random=rng;return status;
}

int rf_particle_emitter_emit(rf_particle_pool *pool,rf_particle_emitter *emitter,
    uint32_t handle,int32_t now_ms,rf_random_state *random,uint32_t *index)
{
    if(emitter && emitter->owner>=0)return RF_NOT_FOUND;
    return rf_particle_emitter_emit_parent(pool,emitter,handle,now_ms,NULL,random,index);
}

int rf_particle_emitter_update(rf_particle_pool *pool,rf_particle_emitter_runtime *runtime,
    uint32_t handle,uint32_t global_enabled,float dt,int32_t now_ms,
    const rf_particle_emitter_parent *parent,uint32_t parent_room,
    rf_random_state *random,rf_particle_emitter_update_result *result)
{
    rf_particle_emitter_runtime next;rf_particle_emitter_clock clock;rf_random_state rng;
    rf_particle_emitter_update_result out={{0,0,0},0,UINT32_MAX};int due=0,status;
    if(!pool_valid(pool) || !runtime || !random || !result || !handle ||
       handle>pool->list_count-5)return RF_RANGE;
    next=*runtime;rng=*random;
    clock.flags=next.emitter.flags;clock.enabled=next.enabled;
    clock.elapsed=next.elapsed;clock.duration=next.duration;
    if(global_enabled&255u) {
        status=rf_timer_expired(next.emitter.deadline,now_ms,&due);if(status)return status;
    }
    status=rf_particle_emitter_tick(&next.cycle,global_enabled,dt,(uint32_t)due,&rng,&clock,&out.actions);
    if(status)return status;
    next.enabled=clock.enabled;next.elapsed=clock.elapsed;next.duration=clock.duration;
    if(out.actions.emit) {
        status=rf_particle_emitter_emit_parent(pool,&next.emitter,handle,now_ms,parent,&rng,&out.index);
        if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
        out.created=status==RF_OK;
    }
    if((global_enabled&255u) && next.emitter.owner>=0 && parent && !(next.emitter.flags&0x40u))
        next.emitter.room=parent_room;
    *runtime=next;*random=rng;*result=out;return RF_OK;
}

int rf_particle_emitter_fresh(rf_particle_emitter_runtime *runtime)
{
    if(!runtime)return RF_RANGE;
    memset(runtime,0,sizeof(*runtime));runtime->emitter.deadline=-1;return RF_OK;
}

int rf_particle_emitter_initialize(rf_particle_pool *pool,rf_particle_emitter_runtime *runtime,
    const rf_particle_emitter_template *source,int32_t owner,uint32_t room,uint32_t handle,
    uint32_t enabled,int32_t now_ms,const rf_particle_emitter_parent *parent,
    rf_random_state *random,rf_particle_emitter_init_result *result)
{
    rf_particle_emitter_runtime next;rf_particle_emitter *e;rf_random_state rng;
    rf_particle_emitter_init_result out={0,UINT32_MAX,0,0};double inverse,delay;unsigned i;int status;
    if(!pool_valid(pool) || !runtime || !source || !random || !result || !handle ||
       handle>pool->list_count-5 || now_ms<0 || now_ms>RF_TIMER_PERIOD)return RF_RANGE;
    if(pool->lists[handle+4].next!=RF_PARTICLE_CAPACITY+handle+4 ||
       pool->lists[handle+4].previous!=RF_PARTICLE_CAPACITY+handle+4)return RF_RANGE;
    if(!isfinite(source->min_spawn_delay) || !isfinite(source->max_spawn_delay) ||
       source->min_spawn_delay<0 || source->max_spawn_delay<0 ||
       source->min_spawn_delay>RF_TIMER_PERIOD/1000.0 || source->max_spawn_delay>RF_TIMER_PERIOD/1000.0)return RF_RANGE;
    if((source->flags&0x20u) &&
       (!isfinite((float)(fabs((double)source->cycle.on_time)+fabs((double)source->cycle.on_variance))) ||
        !isfinite((float)(fabs((double)source->cycle.off_time)+fabs((double)source->cycle.off_variance)))))return RF_RANGE;
    next=*runtime;e=&next.emitter;rng=*random;
    e->owner=owner;e->room=room;
    for(i=0;i<3;i++) {
        if(!isfinite(source->position[i]) || !isfinite(source->direction[i]))return RF_RANGE;
        e->position[i]=e->spawn.position[i]=source->position[i];
    }
    inverse=1.0/sqrt(((double)source->direction[0]*source->direction[0]+(double)source->direction[1]*source->direction[1])+
                    (double)source->direction[2]*source->direction[2]);
    for(i=0;i<3;i++) {
        e->direction[i]=(float)((double)source->direction[i]*inverse);
        if(!isfinite(e->direction[i]))return RF_RANGE;
    }
    e->direction_random=source->direction_random;e->min_velocity=source->min_velocity;e->max_velocity=source->max_velocity;
    e->spawn_radius=source->spawn_radius;e->min_spawn_delay=source->min_spawn_delay;e->max_spawn_delay=source->max_spawn_delay;
    e->flags=(e->flags&0xffff0000u)|(source->flags&0xffffu);
    e->min_life=source->min_life;e->max_life=source->max_life;e->min_radius=source->min_radius;e->max_radius=source->max_radius;
    e->spawn.growth=source->growth;e->spawn.acceleration=source->acceleration;e->spawn.gravity_scale=source->gravity_scale;
    e->spawn.bitmap=source->bitmap;e->spawn.frame_count=source->frame_count;e->spawn.color=source->color;
    e->spawn.color_destination=source->color_destination;e->spawn.flags=source->particle_flags;e->spawn.secondary=source->secondary;
    e->spawn.age_to_finish_vbm=source->age_to_finish_vbm;next.cycle=source->cycle;
    if(e->flags&0x10u) {
        if(e->flags&2u) {
            status=rf_particle_emitter_emit_parent(pool,e,handle,now_ms,parent,&rng,&out.index);
            if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
            out.created=status==RF_OK;
        } else {
            delay=emission_range(&rng,e->min_spawn_delay,e->max_spawn_delay)*1000.0;
            rf_timer_set(&e->deadline,now_ms,(int32_t)delay);
        }
    }
    next.enabled=(next.enabled&~255u)|(enabled&255u);
    if(e->flags&0x20u) {
        next.elapsed=0;
        status=rf_particle_cycle_duration(&next.cycle,next.enabled,&rng,&next.duration);
        if(status)return status;
    }
    out.source_id=source->source_id;out.copied_80=source->copied_80;
    *runtime=next;*random=rng;*result=out;return RF_OK;
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

int rf_particle_pool_step_unowned(rf_particle_pool *pool,uint32_t index,float dt,
    rf_particle_emitter_bounds *bounds)
{
    rf_particle value;unsigned i;double radius,length,inverse;float speed,bound=0;volatile float ratio;
    int track;
    if(!pool_valid(pool) || index>=RF_PARTICLE_CAPACITY)return RF_RANGE;
    value=pool->particles[index];
    if(!(value.flags&1u) || value.pool>1 || !pool->live[value.pool])return RF_RANGE;
    if((int32_t)value.owner>=0 || (value.emitter && !bounds) || (value.flags&0xff000010u) || (value.secondary&1u))return RF_NOT_FOUND;
    track=value.emitter && bounds && bounds->owner>=0;
    if(track) {
        bound=bounds->maximum_distance_squared;
        if(!isfinite(bound))return RF_RANGE;
        for(i=0;i<3;i++)if(!isfinite(bounds->center[i]))return RF_RANGE;
    }
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
    if(track) {
        float delta[3];double distance;
        for(i=0;i<3;i++)delta[i]=bounds->center[i]-value.position[i];
        distance=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2];
        if(distance>bound)bound=(float)distance;
        if(!isfinite(bound))return RF_RANGE;
    }
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
    pool->particles[index]=value;if(track)bounds->maximum_distance_squared=bound;return RF_OK;
}

int rf_particle_pool_step_free(rf_particle_pool *pool,uint32_t index,float dt)
{
    return rf_particle_pool_step_unowned(pool,index,dt,NULL);
}

static void emitter_next(rf_emitter_pool *pool,uint32_t index,uint32_t next)
{
    if(index<128)pool->slots[index].next=next;else pool->lists[index-128].next=next;
}
static void emitter_previous(rf_emitter_pool *pool,uint32_t index,uint32_t previous)
{
    if(index<128)pool->slots[index].previous=previous;else pool->lists[index-128].previous=previous;
}
static void emitter_unlink(rf_emitter_pool *pool,uint32_t index)
{
    rf_emitter_slot *slot=&pool->slots[index];
    emitter_next(pool,slot->previous,slot->next);emitter_previous(pool,slot->next,slot->previous);
}
static void emitter_append(rf_emitter_pool *pool,uint32_t list,uint32_t index)
{
    rf_emitter_slot *slot=&pool->slots[index];slot->next=128+list;slot->previous=pool->lists[list].previous;
    emitter_next(pool,slot->previous,index);pool->lists[list].previous=index;
}
static int emitter_pool_valid(const rf_emitter_pool *pool)
{
    return pool && pool->slots && pool_valid(pool->particles) && pool->particles->list_count>=133;
}
int rf_emitter_pool_init(rf_emitter_pool *pool,rf_emitter_slot *slots,rf_particle_pool *particles)
{
    uint32_t i;
    if(!pool || !slots || !pool_valid(particles) || particles->list_count<133)return RF_RANGE;
    for(i=5;i<133;i++)if(particles->lists[i].next!=1600+i || particles->lists[i].previous!=1600+i)return RF_RANGE;
    pool->slots=slots;pool->particles=particles;pool->live=0;
    pool->lists[0].next=pool->lists[0].previous=128;pool->lists[1].next=pool->lists[1].previous=129;
    memset(slots,0,sizeof(*slots)*128);
    for(i=0;i<128;i++){rf_particle_emitter_fresh(&slots[i].runtime);emitter_append(pool,0,i);}
    return RF_OK;
}
int rf_emitter_pool_create(rf_emitter_pool *pool,const rf_particle_emitter_template *source,
    int32_t owner,uint32_t room,uint32_t enabled,int32_t now_ms,
    const rf_particle_emitter_parent *parent,rf_random_state *random,uint32_t *index)
{
    uint32_t first;double radius;rf_emitter_slot *slot;rf_particle_emitter_init_result result;int status;
    if(!emitter_pool_valid(pool) || !source || !random || !index)return RF_RANGE;
    first=pool->lists[0].next;if(first==128)return RF_NOT_FOUND;
    radius=fabs((double)source->gravity_scale*9.80000019073486328125)+fabs((double)source->acceleration);
    radius=((radius*source->max_life)*source->max_life)*0.5;
    radius+=fabs((double)source->max_velocity)*source->max_life;
    radius+=source->max_radius;if(!isfinite((float)radius))return RF_RANGE;
    slot=&pool->slots[first];
    status=rf_particle_emitter_initialize(pool->particles,&slot->runtime,source,owner,room,first+1,
        enabled,now_ms,parent,random,&result);if(status)return status;
    emitter_unlink(pool,first);emitter_append(pool,1,first);slot->active=1;pool->live++;
    slot->source_id=result.source_id;slot->copied_80=result.copied_80;
    slot->bounds.owner=owner;slot->bounds.maximum_distance_squared=0;slot->estimated_radius=(float)radius;
    *index=first;return RF_OK;
}
int rf_emitter_pool_release(rf_emitter_pool *pool,uint32_t index)
{
    int status;
    if(!emitter_pool_valid(pool) || index>=128 || !pool->slots[index].active)return RF_RANGE;
    status=rf_particle_pool_detach(pool->particles,index+1);if(status)return status;
    emitter_unlink(pool,index);emitter_append(pool,0,index);pool->slots[index].active=0;pool->live--;return RF_OK;
}

static void level_emitter_range(const float pair[2],float *low,float *high)
{
    *low=(float)((double)pair[0]-pair[1]);if(*low<0)*low=0;
    *high=(float)((double)pair[0]+pair[1]);
}
int rf_level_emitter_template(const rf_level_emitter *level,uint32_t bitmap,
    uint32_t frame_count,rf_particle_emitter_template *result)
{
    rf_particle_emitter_template value;unsigned i;
    if(!level || !result)return RF_RANGE;
    value=*result;value.source_id=level->uid;
    for(i=0;i<3;i++){value.position[i]=level->position[i];value.direction[i]=level->orientation_disk[6+i];}
    value.direction_random=(float)cos((double)level->cone_angle*0.01745329238474369049072265625);
    level_emitter_range(level->delay,&value.min_spawn_delay,&value.max_spawn_delay);
    level_emitter_range(level->speed,&value.min_velocity,&value.max_velocity);
    level_emitter_range(level->life,&value.min_life,&value.max_life);
    level_emitter_range(level->radius,&value.min_radius,&value.max_radius);
    value.spawn_radius=level->spawn_radius;value.acceleration=level->acceleration;
    value.growth=level->growth;value.gravity_scale=level->gravity_scale;
    value.flags=(value.flags&0xffff0000u)|(level->emitter_flags&0xffffu);
    memcpy(&value.cycle,level->cycle,sizeof(value.cycle));
    for(i=0;i<4;i++)if(level->cycle[i]!=0)value.flags|=0x20u;
    value.bitmap=bitmap;value.frame_count=frame_count;
    memcpy(&value.color,level->color,4);memcpy(&value.color_destination,level->color_destination,4);
    value.particle_flags=level->particle_flags;value.secondary=0;
    memcpy(&value.copied_80,&level->finish_age,4);
    *result=value;return RF_OK;
}
