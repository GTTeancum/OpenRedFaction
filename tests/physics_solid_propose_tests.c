#include "rf/physics.h"
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"solid propose line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_physics_body_state body={0},before;float acceleration[3]={1,2,3},saved[3];unsigned test;
    body.mass=2;body.flags=1;body.position[1]=10;
    CHECK(!rf_physics_solid_propose(&body,1,4,0,acceleration));
    CHECK(body.position[1]==10 && body.next_position[1]==8 && body.velocity[1]==-4);
    CHECK(acceleration[0]==0 && acceleration[1]==-4 && acceleration[2]==0);
    body.flags|=0x1000000;body.position[1]=8;
    CHECK(!rf_physics_solid_propose(&body,.5f,99,0,acceleration));
    CHECK(body.velocity[1]==-4 && body.next_position[1]==6.5f && acceleration[1]==-4);
    for(test=0;test<7;test++) {
        float dt=.1f,gravity=9.8f;
        memset(&body,0,sizeof(body));body.mass=2;body.flags=3;
        body.position[0]=7;body.velocity[2]=3;body.next_position[1]=123;
        acceleration[0]=1;acceleration[1]=2;acceleration[2]=3;
        switch(test) {
        case 0:body.mass=0;break;
        case 1:dt=-1;break;
        case 2:gravity=NAN;break;
        case 3:body.vector_e0[2]=INFINITY;break;
        case 4:body.mass=FLT_MIN;body.vector_e0[2]=FLT_MAX;break;
        case 5:acceleration[2]=INFINITY;break;
        case 6:body.coefficients[1]=NAN;break;
        }
        before=body;memcpy(saved,acceleration,12);
        CHECK(rf_physics_solid_propose(&body,dt,gravity,0x80000,acceleration)==RF_RANGE);
        CHECK(!memcmp(&before,&body,sizeof(body)) && !memcmp(saved,acceleration,12));
    }
    CHECK(rf_physics_solid_propose(NULL,0,0,0,acceleration)==RF_RANGE);
    CHECK(rf_physics_solid_propose(&body,0,0,0,NULL)==RF_RANGE);
    puts("PASS solid free-flight, retained retry acceleration and atomic invalid/overflow rejection");return 0;
}
