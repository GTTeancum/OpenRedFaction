#include "rf/particle_pool.h"
#include "rf/level.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"gameplay effects line %d\n",__LINE__);return 1;}}while(0)
static rf_particle particles[RF_PARTICLE_CAPACITY];
static rf_particle_list lists[6];
static int query_mode;
static int wall(void *context,const float start[3],const float end[3],rf_particle_collision_hit *hit)
{
    float fraction;unsigned i;(void)context;
    if(query_mode==1)return RF_IO;
    hit->hit=start[0]<1 && end[0]>=1;
    if(hit->hit){fraction=(1-start[0])/(end[0]-start[0]);
        for(i=0;i<3;i++)hit->point[i]=start[i]+(end[i]-start[i])*fraction;
        hit->normal[0]=query_mode==2?0:-1;}
    return RF_OK;
}
static int particle_case(uint32_t flags)
{
    rf_particle_pool pool;rf_particle_spawn spawn={0};rf_random_state random={1};
    rf_particle before;rf_particle_emitter_bounds bounds={0};rf_particle_owner_gate gate={0};
    uint32_t index;
    CHECK(!rf_particle_pool_init(&pool,particles,lists,6));
    spawn.radius=1;spawn.life=10;spawn.velocity[0]=2;spawn.velocity[1]=1;spawn.flags=flags;
    CHECK(!rf_particle_pool_create(&pool,1,&spawn,UINT32_MAX,1,1,&random,&index));
    before=particles[index];
    CHECK(rf_particle_pool_step_resolved(&pool,index,1,&bounds,&gate)==RF_NOT_FOUND);
    CHECK(!memcmp(&before,particles+index,sizeof(before)));
    query_mode=1;
    CHECK(rf_particle_pool_step_collision(&pool,index,1,&bounds,&gate,wall,NULL)==RF_IO);
    CHECK(!memcmp(&before,particles+index,sizeof(before)) && bounds.maximum_distance_squared==0);
    query_mode=2;
    CHECK(rf_particle_pool_step_collision(&pool,index,1,&bounds,&gate,wall,NULL)==RF_FORMAT);
    CHECK(!memcmp(&before,particles+index,sizeof(before)));
    query_mode=0;
    CHECK(!rf_particle_pool_step_collision(&pool,index,.25f,&bounds,&gate,wall,NULL));
    CHECK(particles[index].position[0]==.5f && !(particles[index].flags&0x8000));
    CHECK(!rf_particle_pool_step_collision(&pool,index,.5f,&bounds,&gate,wall,NULL));
    CHECK(particles[index].position[0]>.998f && particles[index].position[0]<1);
    CHECK(particles[index].flags&0x8000);
    CHECK(fabsf(particles[index].velocity[0]+2)<.00001f);
    CHECK(fabsf(particles[index].velocity[1]-((flags&0xf00000)?0:1))<.00001f);
    CHECK(bounds.maximum_distance_squared>1);
    if(flags&0x800){CHECK(particles[index].age==particles[index].life);
        CHECK(!rf_particle_pool_step_collision(&pool,index,.1f,&bounds,&gate,wall,NULL));
        CHECK(!pool.live[1] && !(particles[index].flags&1));}
    else CHECK(particles[index].age==.75f && pool.live[1]==1);
    return 0;
}
int main(void)
{
    rf_group_translation_step step={0};rf_group_translation_progress progress;
    float pending[3],position[3]={56,.875f,75.5f};uint32_t arrival;
    CHECK(!particle_case(16u|0xf0000u));
    CHECK(!particle_case(16u|0xff0000u));
    CHECK(!particle_case(16u|0xf0800u));
    memcpy(step.from,position,12);memcpy(step.to,position,12);step.to[0]=58.25f;
    step.dt=1.0f/60;step.flags=0x2000;
    CHECK(!rf_group_translation_integrate(&step,&progress));
    CHECK(progress.length==2.25f && progress.distance==2.25f && progress.speed==0);
    CHECK(!rf_group_translation_position(&step,&progress,position,pending,&arrival));
    CHECK(arrival && !memcmp(pending,step.to,12));
    step.timing=1;
    CHECK(!rf_group_translation_integrate(&step,&progress));
    CHECK(progress.speed==2.25f && progress.distance>0 && progress.distance<.04f);
    puts("PASS particle collision, failure preservation, bounce/stick/death and instantaneous mover");return 0;
}
