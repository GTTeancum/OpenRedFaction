#include "rf/physics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void)
{
    rf_physics_body b={0};rf_physics_sphere spheres[3]={0};float f=99;
    float start[3]={0},delta[3]={10,0,0};
    b.spheres.items=spheres;b.spheres.count=2;
    b.state.orientation[0]=b.state.orientation[4]=b.state.orientation[8]=1;
    spheres[0].center[0]=8;spheres[0].radius=1;
    spheres[1].center[0]=4;spheres[1].radius=1;
    CHECK(rf_physics_body_segment(&b,start,delta,1,&f) && fabsf(f-.3f)<1e-6f);
    f=99;CHECK(!rf_physics_body_segment(&b,start,delta,.29f,&f) && f==99);
    CHECK(rf_physics_body_segment(&b,start,delta,.3f,&f));
    start[1]=1;CHECK(rf_physics_body_segment(&b,start,delta,1,&f) && fabsf(f-.4f)<1e-6f);
    start[1]=1.01f;CHECK(!rf_physics_body_segment(&b,start,delta,1,&f));
    start[1]=0;start[0]=4;delta[0]=0;
    CHECK(rf_physics_body_segment(&b,start,delta,1,&f) && f==0);
    start[0]=0;CHECK(!rf_physics_body_segment(&b,start,delta,1,&f));
    /* Rotate local +X onto world +Y, then translate. */
    memset(b.state.orientation,0,sizeof(b.state.orientation));
    b.state.orientation[1]=1;b.state.orientation[3]=-1;b.state.orientation[8]=1;
    b.state.position[0]=2;start[0]=2;delta[1]=10;
    CHECK(rf_physics_body_segment(&b,start,delta,1,&f) && fabsf(f-.3f)<1e-6f);
    /* Captured L3S1 miner910: broad box accepted, all body spheres miss. */
    memset(&b.state,0,sizeof(b.state));b.state.orientation[0]=b.state.orientation[4]=b.state.orientation[8]=1;
    b.spheres.count=3;
    {const float shapes[3][4]={{-20.3831539f,-3.26782179f,-13.5880966f,.600000024f},
        {-20.4285946f,-2.62155271f,-13.6560221f,.400000006f},
        {-20.4607391f,-2.16436672f,-13.7040739f,.150000006f}};
     for(int i=0;i<3;i++){memcpy(spheres[i].center,shapes[i],12);spheres[i].radius=shapes[i][3];}}
    start[0]=-23.4887104f;start[1]=-2.333076f;start[2]=-16.5543461f;
    delta[0]=47.2933998f;delta[1]=-1.40071094f;delta[2]=88.0986557f;
    CHECK(!rf_physics_body_segment(&b,start,delta,1,&f));
    /* Captured guard781 frame2362 must retain its middle-sphere hit. */
    spheres[0].center[0]=-21.0114021f;spheres[0].center[1]=-2.74213719f;spheres[0].center[2]=-10.4852524f;spheres[0].radius=.6f;
    b.spheres.count=1;
    start[0]=-23.4886665f;start[1]=-2.3331306f;start[2]=-15.3981609f;
    delta[0]=37.4444618f;delta[1]=-3.6590004f;delta[2]=92.65271f;
    CHECK(rf_physics_body_segment(&b,start,delta,1,&f) && f>0 && f<1);
    puts("body segment PASS: captured false hit, retained hit, nearest, limit, tangent, overlap, rotation");return 0;
}
