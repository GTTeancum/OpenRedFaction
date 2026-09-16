#include "rf/swim.h"
#include "rf/physics.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    const float eye[9]={1,0,0,0,.8f,-.6f,0,.6f,.8f};
    rf_movement_descriptor d[16]={{0}};rf_swim_state s={0};rf_swim_transition t,saved;
    rf_swim_controls c={0};rf_swim_motion m,old;rf_swim_jump j;
    rf_physics_body_state body={0};float support[3]={0};unsigned i;
    for(i=0;i<16;i++){d[i].enabled=1;d[i].index=i;}
    s.mode=1;
    CHECK(rf_swim_transition_prepare(&s,d,0x100,1,1,0,0,&t)==RF_OK && !t.entered && t.state.mode==1);
    CHECK(t.state.object_flags==0x80000 && t.state.entity_flags==0x1000);
    CHECK(rf_swim_transition_prepare(&s,d,0x100,1,1,1,0,&t)==RF_OK && t.entered && t.state.mode==4 && t.posture_request==0);
    CHECK(rf_swim_transition_prepare(&s,d,0x100,1,1,0,1,&t)==RF_OK && t.entered && t.state.mode==4);
    CHECK(rf_swim_transition_prepare(&s,d,0,1,1,1,1,&t)==RF_OK && !t.entered);
    CHECK(rf_swim_transition_prepare(&s,d,0x400,1,1,1,0,&t)==RF_OK && t.state.mode==7 && t.posture_request==1);
    d[4].enabled=0;CHECK(rf_swim_transition_prepare(&s,d,0x100,1,1,1,0,&t)==RF_OK && t.state.mode==0);d[4].enabled=1;
    s.mode=4;s.entity_flags=0x3000;s.object_flags=0x80000;
    CHECK(rf_swim_transition_prepare(&s,d,0x100,0,0,0,0,&t)==RF_OK && !t.exited && t.state.mode==4 && !t.state.entity_flags);
    CHECK(rf_swim_transition_prepare(&s,d,0x100,1,0,0,0,&t)==RF_OK && t.exited && t.state.mode==3 && t.state.body_flags==1 && t.state.query_flags==0x1000);
    CHECK(rf_swim_transition_prepare(&s,d,0x100,1,1,1,1,&t)==RF_OK && !t.entered && t.posture_request==-1);
    saved=t;CHECK(rf_swim_transition_prepare(NULL,d,0,0,0,0,0,&t)==RF_RANGE && !memcmp(&t,&saved,sizeof(t)));
    /* Captured original49f6cd..49f89f pitched-forward vector. */
    c.forward=1;CHECK(rf_swim_motion_prepare(&c,eye,20,2,2,.125f,1,&m)==RF_OK);
    CHECK(m.resistance==2 && m.acceleration[0]==0 && m.acceleration[1]==12 && m.acceleration[2]==16);
    body.mass=2;CHECK(rf_physics_ground_propose(&body,.125f,m.resistance,m.acceleration,support)==RF_OK);
    CHECK(body.velocity[1]==1.5f && body.velocity[2]==2 && body.next_position[1]==.28125f && body.next_position[2]==.375f);
    c.forward=0;CHECK(rf_swim_motion_prepare(&c,eye,20,2,2,.125f,1,&m)==RF_OK);
    memset(&body,0,sizeof(body));body.mass=2;body.velocity[1]=-4;
    CHECK(rf_physics_ground_propose(&body,.125f,m.resistance,m.acceleration,support)==RF_OK);
    CHECK(body.velocity[1]==-3 && body.next_position[1]==-.3125f);
    memset(&body,0,sizeof(body));body.mass=2;body.vector_e0[1]=-20;
    CHECK(rf_physics_ground_propose(&body,.125f,m.resistance,m.acceleration,support)==RF_OK);
    CHECK(body.velocity[1]==-1.25f && body.next_position[1]==-.234375f);
    c.forward=0;c.up=.75f;c.down=.25f;c.jump=.5f;c.crouch=.25f;c.jump_press_mode=1;c.crouch_press_mode=1;
    CHECK(rf_swim_motion_prepare(&c,eye,20,2,2,.125f,1,&m)==RF_OK && m.local[1]==.75f);
    c.jump_press_mode=2;CHECK(rf_swim_motion_prepare(&c,eye,20,2,2,.125f,1,&m)==RF_OK && m.local[1]==.25f);
    c.up=1;c.down=0;c.jump=1;c.crouch=0;c.jump_press_mode=1;
    CHECK(rf_swim_motion_prepare(&c,eye,20,20,2,.125f,1,&m)==RF_OK && m.local[1]==2 && m.resistance==8);
    CHECK(fabsf(m.acceleration[1]-16)<.00001f && fabsf(m.acceleration[2]+12)<.00001f);
    CHECK(rf_swim_motion_prepare(&c,eye,20,20,2,.125f,0,&m)==RF_OK && m.resistance==20);
    old=m;CHECK(rf_swim_motion_prepare(&c,eye,20,2,0,.125f,1,&m)==RF_RANGE && !memcmp(&old,&m,sizeof(m)));
    CHECK(rf_swim_motion_prepare(&c,eye,20,2,2,0,1,&m)==RF_RANGE && !memcmp(&old,&m,sizeof(m)));
    s.mode=4;s.entity_flags=0;
    CHECK(rf_swim_surface_jump(&s,d,0x100,1,0,6,.0625f,&j)==RF_OK && j.applied && j.state.mode==3 && j.state.body_flags==1);
    CHECK(j.velocity_y==8.444999694824219f);
    CHECK(rf_swim_surface_jump(&s,d,0x100,1,-2,6,.0625f,&j)==RF_OK && j.velocity_y==6.445000171661377f);
    s.entity_flags=0x2000;CHECK(rf_swim_surface_jump(&s,d,0x100,1,3,6,.0625f,&j)==RF_OK && !j.applied && j.velocity_y==3 && j.state.mode==4);
    s.entity_flags=0;CHECK(rf_swim_surface_jump(&s,d,0x100,0,3,6,.0625f,&j)==RF_OK && !j.applied);
    puts("PASS: swimming transition, original captured motion/control/jump vectors and rollback");return 0;
}
