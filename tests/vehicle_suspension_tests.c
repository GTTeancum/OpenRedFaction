#include "rf/vehicle_suspension.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"suspension line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint32_t calls,fail;float starts[6][3];} fixture;
static int query(void *context,const float start[3],const float end[3],float radius,rf_vehicle_spring_hit *hit)
{
    fixture *f=context;uint32_t i=f->calls++;
    if(i>=6||radius!=1||fabsf(end[1]-(start[1]-.25f))>.00001f)return RF_FORMAT;
    memcpy(f->starts[i],start,12);
    if(f->fail)return RF_IO;
    hit->matched=1;hit->fraction=.5f;hit->material_id=i;hit->material=.8f;
    hit->support_handle=i?UINT32_MAX:42;hit->velocity[2]=i?0:2;
    return RF_OK;
}
int main(void)
{
    rf_vehicle_rigid_state state={0};rf_vehicle_spring springs[6]={0};rf_vehicle_suspension_result result,saved;
    fixture f={0};uint32_t i;
    state.position[0]=10;state.position[1]=20;state.position[2]=30;
    state.orientation[0]=state.orientation[4]=state.orientation[8]=1;
    /* Authored Driller constant pattern3/1/1.8 mirrored, length.25;
     * synthetic centers/radii isolate recovered force/torque arithmetic. */
    for(i=0;i<6;i++){springs[i].center[0]=i<3?1:-1;springs[i].radius=2;springs[i].length=.25f;springs[i].constant=i%3==0?3:i%3==1?1:1.8f;}
    CHECK(!rf_vehicle_suspension_sample(&state,2,springs,6,query,&f,&result));
    CHECK(f.calls==6&&result.hits==6&&fabsf(result.support.force[1]-11.6f)<.00001f);
    CHECK(fabsf(result.support.torque[2])<.00001f&&result.support_handle==42&&result.last_material==5&&result.support.velocity[2]==2);
    CHECK(f.starts[0][0]==11&&f.starts[0][1]==19&&f.starts[0][2]==30);
    springs[0].constant=0;memset(&f,0,sizeof(f));
    CHECK(!rf_vehicle_suspension_sample(&state,2,springs,6,query,&f,&result));
    CHECK(f.calls==5&&fabsf(result.support.force[1]-8.6f)<.00001f&&fabsf(result.support.torque[2]+3)<.00001f);
    saved=result;f.fail=1;f.calls=0;
    CHECK(rf_vehicle_suspension_sample(&state,2,springs,6,query,&f,&result)==RF_IO&&!memcmp(&saved,&result,sizeof(result)));
    springs[5].length=NAN;f.calls=0;
    CHECK(rf_vehicle_suspension_sample(&state,2,springs,6,query,&f,&result)==RF_RANGE&&!f.calls);
    puts("six-spring recovered force/torque, world-down sampling, dynamic support retention and failure atomicity passed");return 0;
}
