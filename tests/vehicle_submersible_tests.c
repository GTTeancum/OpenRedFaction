#include "rf/vehicle_submersible.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"submersible line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint32_t dry,wall,wet_calls,collision_calls;int error;float water_top;} fixture;
static int water(void *context,const rf_vehicle_rigid_state *s,const rf_vehicle_rigid_proposal *p,uint32_t *allowed)
{
    fixture *f=context;(void)s;++f->wet_calls;
    if(f->error)return f->error;
    *allowed=!f->dry&&p->position[1]<=f->water_top;return RF_OK;
}
static int collision(void *context,const rf_vehicle_rigid_state *s,const rf_vehicle_rigid_proposal *p,rf_vehicle_rigid_proposal *out)
{
    fixture *f=context;(void)s;++f->collision_calls;*out=*p;
    if(f->wall&&out->position[2]>1){out->position[2]=1;out->velocity[2]=0;}
    return RF_OK;
}
static void initialize(rf_vehicle_rigid_state *s)
{
    memset(s,0,sizeof(*s));s->orientation[0]=s->orientation[4]=s->orientation[8]=1;
    s->inverse_inertia[0]=.5f;s->inverse_inertia[4]=.25f;s->inverse_inertia[8]=.125f;
}
int main(void)
{
    rf_vehicle_rigid_state s,before;rf_vehicle_submersible_parameters p={2500,6,8,4,2,2};
    rf_vehicle_submersible_command c={0};rf_vehicle_submersible_result result,untouched;
    fixture f={0};rf_vehicle_submersible_backend backend={&f,water,collision};uint32_t i;float speed;
    f.water_top=100;initialize(&s);c.throttle=1;c.controlled=1;
    for(i=0;i<120;i++)CHECK(!rf_vehicle_submersible_step(&s,&p,&c,1.f/60,&backend,&result));
    CHECK(result.wet&&result.moved&&!result.water_blocked&&fabsf(s.velocity[2]-6)<.001f&&s.position[2]>9);
    CHECK(s.position[1]==0); /* neutral buoyancy; no fake floor */
    c.throttle=0;speed=s.velocity[2];CHECK(!rf_vehicle_submersible_step(&s,&p,&c,.1f,&backend,&result));
    CHECK(s.velocity[2]<speed&&s.velocity[2]>0);
    initialize(&s);c.throttle=c.strafe=c.rise=1;
    for(i=0;i<120;i++)CHECK(!rf_vehicle_submersible_step(&s,&p,&c,1.f/60,&backend,&result));
    speed=sqrtf(s.velocity[0]*s.velocity[0]+s.velocity[1]*s.velocity[1]+s.velocity[2]*s.velocity[2]);
    CHECK(fabsf(speed-6)<.001f&&s.position[0]>0&&s.position[1]>0&&s.position[2]>0);
    initialize(&s);memset(&c,0,sizeof(c));c.controlled=1;c.yaw=1;
    for(i=0;i<60;i++)CHECK(!rf_vehicle_submersible_step(&s,&p,&c,1.f/60,&backend,&result));
    CHECK(fabsf(s.momentum[1]*.25f-2)<.002f&&fabsf(s.orientation[2])>.1f);
    /* Real pitch input and provider rejection preserve a last accepted pose. */
    initialize(&s);c.yaw=0;c.pitch=1;CHECK(!rf_vehicle_submersible_step(&s,&p,&c,.1f,&backend,&result));
    CHECK(s.momentum[0]>0&&s.orientation[5]!=0);
    initialize(&s);memset(&c,0,sizeof(c));c.controlled=1;c.rise=1;f.water_top=.005f;
    before=s;CHECK(!rf_vehicle_submersible_step(&s,&p,&c,.1f,&backend,&result));
    CHECK(result.wet&&result.water_blocked&&!result.moved&&!memcmp(s.position,before.position,12)&&s.velocity[1]==0);
    f.dry=1;s.velocity[2]=3;before=s;
    CHECK(!rf_vehicle_submersible_step(&s,&p,&c,.1f,&backend,&result));
    CHECK(!result.wet&&result.water_blocked&&s.velocity[2]==0&&!memcmp(s.position,before.position,12));
    f.dry=0;f.water_top=100;f.wall=1;initialize(&s);c.rise=0;c.throttle=1;
    for(i=0;i<120;i++)CHECK(!rf_vehicle_submersible_step(&s,&p,&c,1.f/60,&backend,&result));
    CHECK(s.position[2]==1&&s.velocity[2]==0&&f.collision_calls>0);
    before=s;memset(&untouched,0xa5,sizeof(untouched));result=untouched;f.error=RF_IO;
    CHECK(rf_vehicle_submersible_step(&s,&p,&c,.1f,&backend,&result)==RF_IO);
    CHECK(!memcmp(&s,&before,sizeof(s))&&!memcmp(&result,&untouched,sizeof(result)));
    f.error=0;c.rise=NAN;
    CHECK(rf_vehicle_submersible_step(&s,&p,&c,.1f,&backend,&result)==RF_RANGE);
    CHECK(!memcmp(&s,&before,sizeof(s)));
    puts("PASS submarine neutral wet movement, finite controls, speed/turn rates, water admission and collision boundary");return 0;
}
