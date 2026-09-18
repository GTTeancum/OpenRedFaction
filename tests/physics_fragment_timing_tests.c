#include <math.h>
#include "rf/physics.h"
#include "rf/collision.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fragment timing line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {unsigned calls,mode;float times[10];} fixture;
static int query(const rf_physics_body_state *body,float remaining,
    rf_physics_solid_hit *hit,uint32_t *found,void *opaque)
{
    fixture *f=opaque;unsigned n=f->calls++;
    if(n>=10)return RF_RANGE;
    f->times[n]=remaining;
    if((f->mode==1 || f->mode==7 || f->mode==10) && n==1)return RF_IO;
    if(f->mode==3) {
        rf_collision_mover_motion m={0};rf_collision_mover_relative relative;
        float vertices[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
        rf_collision_face face={0};rf_collision_ray_hit contact;unsigned i;int status;
        memcpy(m.start,body->position,12);memcpy(m.end,body->next_position,12);
        memcpy(m.body_matrix,body->orientation,36);memcpy(m.next_body_matrix,body->next_orientation,36);
        for(i=0;i<3;i++)m.mover_matrix[i][i]=1;
        m.mover_end[1]=2;m.body_remaining=remaining;m.mover_remaining=.125f;
        status=rf_collision_mover_relative_sphere(&m,&relative);if(status)return status;
        face.vertices=vertices;face.count=4;face.plane[1]=1;
        face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
        status=rf_collision_thin_face(&face,relative.start,relative.delta,1,&contact,found);if(status)return status;
        if(*found){hit->fraction=contact.fraction;memcpy(hit->point,body->position,12);memcpy(hit->normal,contact.normal,12);}
        return RF_OK;
    }
    if(f->mode>=4) {
        *found=f->mode==5 || n==0;hit->fraction=0;
        hit->normal[f->mode==6?0:1]=1;memcpy(hit->point,body->position,12);
        hit->moving_surface=f->mode!=6;
        if(f->mode>=8)hit->recovery_distance=f->mode==9?NAN:.5f;
        return RF_OK;
    }
    *found=f->mode==2 || n<2;
    hit->fraction=f->mode==2?0:(n==0?.25f:.5f);
    hit->normal[1]=1;memcpy(hit->point,body->position,12);return RF_OK;
}
static rf_physics_body_state initial(void)
{
    rf_physics_body_state b={0};unsigned i;b.mass=1;b.position[1]=1;b.bounds.radius=.25f;
    b.flags=0x80000200u; /* Disable response to isolate repeat-query time. */
    for(i=0;i<3;i++)b.orientation[i*4]=b.local_tensor[i*4]=b.world_tensor[i*4]=1;
    return b;
}
static int legacy_query(const rf_physics_body_state *body,rf_physics_solid_hit *hit,uint32_t *found,void *opaque)
{return query(body,0,hit,found,opaque);}
int main(void)
{
    rf_physics_body_state body=initial(),before;fixture f={0};uint32_t flags=17;
    float position[3]={0,1,0},basis[9]={1,0,0,0,1,0,0,0,1},saved_position[3],saved_basis[9];
    rf_physics_solid_step_report report,saved_report;
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,query,&f,&report));
    CHECK(f.calls==3 && report.contacts==2 && report.steps==3 && !report.limited);
    CHECK(f.times[0]==.125f && f.times[1]==.09375f && f.times[2]==.046875f);
    CHECK(!memcmp(position,body.position,12));
    /* A late provider error rolls back all public state, including the report. */
    body=initial();before=body;f=(fixture){0,1,{0}};flags=17;
    memcpy(saved_position,position,12);memcpy(saved_basis,basis,36);memset(&report,0xa5,sizeof(report));saved_report=report;
    CHECK(rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,query,&f,&report)==RF_IO);
    CHECK(f.calls==2 && flags==17 && !memcmp(&body,&before,sizeof(body)));
    CHECK(!memcmp(position,saved_position,12) && !memcmp(basis,saved_basis,36) && !memcmp(&report,&saved_report,sizeof(report)));
    body=initial();f=(fixture){0,2,{0}};
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,query,&f,&report));
    CHECK(f.calls==10 && report.limited && report.remaining==.125f);
    /* Motion of the counterpart still produces a query for a stationary body. */
    body=initial();body.flags=0x80000000u;f=(fixture){0,3,{0}};
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,query,&f,&report));
    CHECK(f.calls==1 && report.contacts==1 && report.stopped && !(body.flags&0x80000000u));
    before=body;f.calls=0;
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,query,&f,&report));
    CHECK(!f.calls && !report.steps && !memcmp(&body,&before,sizeof(body)));
    body=initial();before=body;
    CHECK(!rf_physics_fragment_step_timed(&body,0,0,&flags,position,basis,query,&f,&report));
    CHECK(!f.calls && !report.steps && !memcmp(&body,&before,sizeof(body)));
    CHECK(rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,NULL,&f,&report)==RF_RANGE);
    {
        rf_physics_body_state legacy=initial(),timed;fixture a={0},b={0};
        rf_physics_solid_step_report ra,rb;uint32_t fa=17,fb=17;
        float pa[3]={0,1,0},pb[3]={0,1,0},ba[9]={1,0,0,0,1,0,0,0,1},bb[9];
        legacy.velocity[0]=2;timed=legacy;memcpy(bb,ba,36);
        CHECK(!rf_physics_fragment_step(&legacy,.125f,0,&fa,pa,ba,legacy_query,&a,&ra));
        CHECK(!rf_physics_fragment_step_timed(&timed,.125f,0,&fb,pb,bb,query,&b,&rb));
        CHECK(a.calls==3 && b.calls==3 && !memcmp(&legacy,&timed,sizeof(legacy)));
        CHECK(fa==fb && !memcmp(pa,pb,12) && !memcmp(ba,bb,36) && !memcmp(&ra,&rb,sizeof(ra)));
    }
    /* Moving-contact port policy bypasses both low-speed and exhausted-bounce sleep. */
    body=initial();body.flags=0x9800003fu;body.coefficients[0]=0;f=(fixture){0,4,{0}};
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,9.8f,&flags,position,basis,query,&f,&report));
    CHECK(f.calls==2 && report.contacts==1 && !report.stopped && !report.limited && (body.flags&0x80000000u));
    before=body;f=(fixture){2,4,{0}};
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,9.8f,&flags,position,basis,query,&f,&report));
    CHECK(body.position[1]<before.position[1] && body.velocity[1]<0);
    body=initial();body.flags=0x9800003fu;f=(fixture){0,5,{0}};
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,9.8f,&flags,position,basis,query,&f,&report));
    CHECK(f.calls==10 && report.limited && !report.stopped && (body.flags&0x80000000u));
    body=initial();body.flags=0x9800003fu;f=(fixture){0,6,{0}};
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,9.8f,&flags,position,basis,query,&f,&report));
    CHECK(report.contacts==1 && !report.stopped && (body.flags&0x80000000u));
    body=initial();body.flags=0x9800003fu;before=body;f=(fixture){0,7,{0}};flags=17;
    memcpy(saved_position,position,12);memcpy(saved_basis,basis,36);memset(&report,0xa5,sizeof(report));saved_report=report;
    CHECK(rf_physics_fragment_step_timed(&body,.125f,9.8f,&flags,position,basis,query,&f,&report)==RF_IO);
    CHECK(f.calls==2 && flags==17 && !memcmp(&body,&before,sizeof(body)));
    CHECK(!memcmp(position,saved_position,12) && !memcmp(basis,saved_basis,36) && !memcmp(&report,&saved_report,sizeof(report)));
    /* Recovery consumes a query, not time; a subsequent miss advances normally. */
    body=initial();body.flags=0x9800003fu;body.velocity[0]=2;f=(fixture){0,8,{0}};
    CHECK(!rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,query,&f,&report));
    CHECK(body.position[1]==1.5f && body.position[0]==.25f && report.steps==2 && report.contacts==1);
    CHECK(f.times[0]==.125f && f.times[1]==.125f && !report.limited && !report.stopped);
    for(unsigned mode=9;mode<=10;mode++) {
        body=initial();body.flags=0x9800003fu;before=body;f=(fixture){0,mode,{0}};flags=17;
        memcpy(saved_position,position,12);memcpy(saved_basis,basis,36);memset(&report,0xa5,sizeof(report));saved_report=report;
        CHECK(rf_physics_fragment_step_timed(&body,.125f,0,&flags,position,basis,query,&f,&report)==(mode==9?RF_RANGE:RF_IO));
        CHECK(flags==17 && !memcmp(&body,&before,sizeof(body)));
        CHECK(!memcmp(position,saved_position,12) && !memcmp(basis,saved_basis,36) && !memcmp(&report,&saved_report,sizeof(report)));
    }
    puts("PASS timed fragment repeats, stationary moving-surface query, bounded retries and rollback");return 0;
}
