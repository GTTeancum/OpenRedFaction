#include "rf/vehicle_rigid.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vehicle rigid line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int support_cb(void *context,const rf_vehicle_rigid_state *s,rf_vehicle_rigid_support *out)
{(void)context;(void)s;memset(out,0,sizeof(*out));out->grounded=1;out->material=1;out->force[1]=19.6f;return RF_OK;}
static int resolve_cb(void *context,const rf_vehicle_rigid_state *s,const rf_vehicle_rigid_proposal *p,rf_vehicle_rigid_proposal *out)
{(void)s;if(*(int *)context)return RF_NOT_FOUND;*out=*p;if(out->position[2]>1){out->position[2]=1;out->velocity[2]=0;}return RF_OK;}
int main(void)
{
    rf_vehicle_rigid_state s={0},saved;rf_vehicle_rigid_parameters p={2,6,3,1.2f,2,9.8f};
    rf_vehicle_rigid_command c={1,0,1};rf_vehicle_rigid_support support={0};rf_vehicle_rigid_proposal proposal;
    int blocked=0;rf_vehicle_rigid_backend backend={&blocked,support_cb,resolve_cb};uint32_t i;
    s.orientation[0]=s.orientation[4]=s.orientation[8]=1;s.inverse_inertia[0]=s.inverse_inertia[4]=s.inverse_inertia[8]=1;
    support.grounded=1;support.material=1;support.force[1]=19.6f;
    CHECK(!rf_vehicle_rigid_propose(&s,&p,&c,&support,.1f,&proposal));
    CHECK(fabsf(proposal.velocity[2]-.1f)<.00001f && s.position[2]==0);
    for(i=0;i<100;i++)CHECK(!rf_vehicle_rigid_step(&s,&p,&c,.05f,&backend));
    CHECK(s.position[2]==1 && s.velocity[2]==0 && fabsf(s.position[1])<.0001f);
    s.velocity[2]=3;c.throttle=-1;
    CHECK(!rf_vehicle_rigid_propose(&s,&p,&c,&support,.1f,&proposal));CHECK(proposal.velocity[2]<3 && proposal.velocity[2]>0);
    c.throttle=1;c.turn=1;saved=s;
    CHECK(!rf_vehicle_rigid_propose(&s,&p,&c,&support,.1f,&proposal));
    CHECK(proposal.angular_velocity[1]>0 && proposal.orientation[6]>0 && !memcmp(&s,&saved,sizeof(s)));
    blocked=1;s.force[0]=5;s.torque[1]=2;saved=s;
    CHECK(rf_vehicle_rigid_step(&s,&p,&c,.05f,&backend)==RF_NOT_FOUND && !memcmp(&s,&saved,sizeof(s)));
    blocked=0;CHECK(!rf_vehicle_rigid_step(&s,&p,&c,.05f,&backend));CHECK(!s.force[0] && !s.torque[1]);
    s.skip_forces=1;s.force[0]=5;s.torque[1]=2;
    CHECK(!rf_vehicle_rigid_step(&s,&p,&c,.05f,&backend));CHECK(s.force[0]==5 && s.torque[1]==2);
    saved=s;c.throttle=NAN;CHECK(rf_vehicle_rigid_step(&s,&p,&c,.05f,&backend)==RF_RANGE && !memcmp(&s,&saved,sizeof(s)));
    puts("rigid vehicle proposal/support/contact commit tests passed; synthetic providers, no scene driving proof");return 0;
}
