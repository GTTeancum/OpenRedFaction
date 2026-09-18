#include "rf/physics.h"
#include "rf/collision.h"
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"solid propose line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct floor_query_context {unsigned calls,mode;} floor_query_context;
static int floor_query(const rf_physics_body_state *body,rf_physics_solid_hit *hit,uint32_t *found,void *context)
{
    floor_query_context *c=context;float delta[3],plane[4]={0,1,0,0};unsigned i;int status;
    c->calls++;
    if(c->mode==1 && c->calls==2)return RF_IO;
    if(c->mode==2) {
        hit->fraction=0;memcpy(hit->point,body->position,12);hit->normal[1]=-1;
        hit->elasticity=.5f;*found=1;return RF_OK;
    }
    for(i=0;i<3;i++)delta[i]=body->next_position[i]-body->position[i];
    status=rf_collision_sphere_plane(body->position,delta,body->bounds.radius,plane,
        &hit->fraction,hit->point,found);if(status)return status;
    if(*found && hit->fraction>=1)*found=0;
    hit->normal[1]=1;hit->elasticity=.5f;hit->friction=.2f;return RF_OK;
}
static int angular_query(const rf_physics_body_state *body,rf_physics_solid_hit *hit,uint32_t *found,void *context)
{
    unsigned *calls=context;(*calls)++;
    if(*calls==1){hit->fraction=.25f;hit->normal[1]=1;memcpy(hit->point,body->position,12);*found=1;}
    else *found=0;
    return RF_OK;
}
static rf_physics_body_state falling_body(void)
{
    rf_physics_body_state body={0};body.mass=1;body.bounds.radius=.25f;
    body.position[1]=2;body.flags=0x8000003f;body.coefficients[0]=.8f;body.coefficients[2]=.2f;
    body.orientation[0]=body.orientation[4]=body.orientation[8]=1;
    body.local_tensor[0]=body.local_tensor[4]=body.local_tensor[8]=1;
    body.world_tensor[0]=body.world_tensor[4]=body.world_tensor[8]=1;return body;
}
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
    {
        float published[9]={1,0,0,0,1,0,0,0,1},old_basis[9],left;
        memset(&body,0,sizeof(body));body.mass=1;body.bounds.radius=.25f;
        body.local_tensor[0]=2;body.local_tensor[4]=3;body.local_tensor[8]=4;
        body.next_position[0]=2;body.next_position[1]=4;
        body.next_orientation[1]=1;body.next_orientation[3]=-1;body.next_orientation[8]=1;
        body.bounds.minimum[0]=-99;body.bounds.maximum[0]=99;
        before=body;memcpy(old_basis,published,36);
        CHECK(!rf_physics_solid_advance(&body,.1f,.5f,published,&left));
        CHECK(left==.05f && body.position[0]>0 && body.position[0]<1);
        CHECK(body.bounds.minimum[0]==-99 && body.bounds.maximum[0]==99);
        CHECK(!memcmp(published,old_basis,36) && !memcmp(body.orientation,body.next_orientation,36));
        CHECK(body.world_tensor[0]==3 && body.world_tensor[4]==2 && body.world_tensor[8]==4);
        body=before;CHECK(!rf_physics_solid_advance(&body,.1f,1,published,&left));
        CHECK(left==0 && body.position[0]==2 && body.position[1]==4 && body.scalar_144==1);
        CHECK(body.bounds.minimum[0]==1.75f && body.bounds.maximum[1]==4.25f);
        CHECK(!memcmp(published,body.orientation,36));
        for(test=0;test<3;test++) {
            body=before;left=123;memcpy(published,old_basis,36);
            if(test==0)body.next_orientation[0]=NAN;
            if(test==1){body.next_position[0]=FLT_MAX;body.bounds.radius=FLT_MAX;}
            if(test==2)body.flags=0x4000;
            {rf_physics_body_state unchanged=body;
             CHECK(rf_physics_solid_advance(&body,.1f,1,published,&left)!=RF_OK);
             CHECK(!memcmp(&body,&unchanged,sizeof(body)) && left==123 && !memcmp(published,old_basis,36));}
        }
    }
    {
        floor_query_context context={0};rf_physics_solid_step_report report;
        float position[3]={0,2,0},basis[9]={1,0,0,0,1,0,0,0,1};uint32_t flags=0,contacts=0,bounced=0;
        body=falling_body();
        for(test=0;test<300;test++) {
            CHECK(!rf_physics_solid_step(&body,1.f/60,9.8f,&flags,position,basis,floor_query,&context,&report));
            CHECK(body.position[1]>=.25f && !report.limited);
            contacts+=report.contacts;if(body.velocity[1]>0)bounced=1;
            CHECK(!memcmp(position,body.position,12));
            if(report.stopped)break;
        }
        CHECK(test<300 && contacts>=2 && bounced && !(body.flags&0x80000000u));
        before=body;context.calls=0;
        CHECK(!rf_physics_solid_step(&body,1.f/60,9.8f,&flags,position,basis,floor_query,&context,&report));
        CHECK(!context.calls && !report.steps && !memcmp(&before,&body,sizeof(body)));
        /* Failure on a later substep must roll back earlier accepted contact. */
        body=falling_body();body.position[1]=.3f;body.velocity[1]=-4;before=body;
        context=(floor_query_context){0,1};flags=17;
        {float saved_position[3],saved_basis[9];rf_physics_solid_step_report saved_report;
         memcpy(saved_position,position,12);memcpy(saved_basis,basis,36);
         memset(&report,0xa5,sizeof(report));saved_report=report;
         CHECK(rf_physics_solid_step(&body,.1f,9.8f,&flags,position,basis,floor_query,&context,&report)==RF_IO);
         CHECK(context.calls==2 && flags==17 && !memcmp(&before,&body,sizeof(body)));
         CHECK(!memcmp(position,saved_position,12) && !memcmp(basis,saved_basis,36) && !memcmp(&report,&saved_report,sizeof(report)));}
        body=falling_body();context=(floor_query_context){0,2};
        CHECK(!rf_physics_solid_step(&body,.1f,9.8f,&flags,position,basis,floor_query,&context,&report));
        CHECK(context.calls==10 && report.steps==10 && report.limited && report.remaining==.1f);
    }
    {
        /* A rotation-only collision is queried and commits a partial rotation;
         * the recovered legacy scheduler intentionally keeps its old contract. */
        rf_physics_solid_step_report report;unsigned calls=0;uint32_t flags=0;
        float position[3],basis[9],expected[9],unused[3];rf_physics_body_state proposed;
        body=falling_body();body.flags=0x80000000u;body.coefficients[0]=0;
        body.mass_vector_d4[2]=2;body.vector_c8[2]=2;proposed=body;
        CHECK(!rf_physics_solid_angular_propose(&proposed,.1f));
        memcpy(proposed.next_position,proposed.position,12);
        CHECK(!rf_physics_fragment_pose(&proposed,.25f,unused,expected));
        memcpy(position,body.position,12);memcpy(basis,body.orientation,36);
        CHECK(!rf_physics_fragment_step(&body,.1f,0,&flags,position,basis,angular_query,&calls,&report));
        CHECK(calls==1 && report.contacts==1 && report.stopped);
        CHECK(!memcmp(body.orientation,expected,36) && !memcmp(basis,expected,36));
        CHECK(memcmp(body.orientation,proposed.next_orientation,36));
        CHECK(!memcmp(position,proposed.position,12));
        for(test=0;test<3;test++) {
            double norm=0;unsigned k;for(k=0;k<3;k++)norm+=(double)basis[test*3+k]*basis[test*3+k];
            CHECK(fabs(norm-1)<1e-6);
        }
        before=body;memcpy(saved,position,12);
        CHECK(rf_physics_fragment_pose(&body,NAN,position,basis)==RF_RANGE);
        CHECK(!memcmp(&body,&before,sizeof(body)) && !memcmp(saved,position,12));
    }
    puts("PASS solid prediction/contact, settling, unsupported routes and atomic invalid/overflow rejection");return 0;
}
