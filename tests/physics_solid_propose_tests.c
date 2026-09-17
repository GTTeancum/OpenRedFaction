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
    memset(&body,0,sizeof(body));body.mass=1;
    body.orientation[0]=body.orientation[4]=body.orientation[8]=1;
    body.world_tensor[0]=body.world_tensor[4]=body.world_tensor[8]=1;
    body.mass_vector_d4[0]=100;before=body;
    CHECK(!rf_physics_solid_angular_propose(&body,.1f));
    CHECK(fabsf(body.vector_c8[0]-15)<2e-6f && body.mass_vector_d4[0]==body.vector_c8[0]);
    CHECK(!memcmp(body.orientation,before.orientation,36) && !memcmp(body.world_tensor,before.world_tensor,36));
    CHECK(fabsf(body.next_orientation[4]-cosf(1.5f))<1e-6f && fabsf(body.next_orientation[5]-sinf(1.5f))<1e-6f);
    body.flags=0x1000000;body.vector_ec[0]=100;body.mass_vector_d4[0]=10;before=body;
    CHECK(!rf_physics_solid_angular_propose(&body,0));
    CHECK(!memcmp(body.mass_vector_d4,before.mass_vector_d4,12));
    for(test=0;test<9;test++)CHECK(body.orientation[test]==body.next_orientation[test]);
    for(test=0;test<5;test++) {
        float dt=.1f;
        memset(&body,0,sizeof(body));body.orientation[0]=body.orientation[4]=body.orientation[8]=1;
        body.world_tensor[0]=body.world_tensor[4]=body.world_tensor[8]=1;
        switch(test) {
        case 0:dt=NAN;break;
        case 1:body.world_tensor[8]=INFINITY;break;
        case 2:body.orientation[8]=0;break;
        case 3:body.orientation[4]=0;body.orientation[5]=1;break;
        case 4:dt=2;body.vector_ec[2]=FLT_MAX;break;
        }
        before=body;CHECK(rf_physics_solid_angular_propose(&body,dt)==RF_RANGE);
        CHECK(!memcmp(&body,&before,sizeof(body)));
    }
    {
        float point[3]={0,0,0},normal[3]={0,1,0},gravity[3]={0,-9.8f,0};
        rf_physics_solid_response response;
        memset(&body,0,sizeof(body));body.mass=2;body.coefficients[0]=1;
        body.position[1]=.2f;body.velocity[0]=3;body.velocity[1]=-4;
        body.world_tensor[0]=body.world_tensor[4]=body.world_tensor[8]=1;before=body;
        CHECK(!rf_physics_solid_contact(&body,point,normal,gravity,.5f,0,&response));
        CHECK(response==RF_SOLID_CONTACT_IMPULSE && body.velocity[1]==2 && body.velocity[0]==3 && body.coefficients[0]==.8f);
        CHECK(!memcmp(body.position,before.position,12) && !memcmp(body.world_tensor,before.world_tensor,36));
        body.coefficients[0]=.04f;body.velocity[1]=-4;body.flags=0x8000003f;
        CHECK(!rf_physics_solid_contact(&body,point,normal,gravity,.5f,0,&response));
        CHECK(response==RF_SOLID_CONTACT_STOPPED && body.flags==0x1800003f && body.velocity[0]==0 && body.velocity[1]==0);
        for(test=0;test<4;test++) {
            body=before;response=(rf_physics_solid_response)777;point[0]=0;normal[1]=1;
            if(test==0)body.flags=0x100;
            if(test==1)body.flags=0x4000;
            if(test==2)normal[1]=NAN;
            if(test==3){point[0]=1;body.world_tensor[8]=-100;}
            {rf_physics_body_state saved_body=body;
             CHECK(rf_physics_solid_contact(&body,point,normal,gravity,.5f,0,&response)==(test<2?RF_NOT_FOUND:RF_RANGE));
             CHECK(!memcmp(&body,&saved_body,sizeof(body)) && response==(rf_physics_solid_response)777);}
        }
    }
    puts("PASS solid prediction/contact, settling, unsupported routes and atomic invalid/overflow rejection");return 0;
}
