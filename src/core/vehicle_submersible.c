#include "rf/vehicle_submersible.h"
#include "rf/physics.h"
#include <math.h>
#include <string.h>
static int finite_values(const float *p,uint32_t n)
{uint32_t i;for(i=0;i<n;i++)if(!isfinite(p[i]))return 0;return 1;}
static float dot(const float *a,const float *b)
{return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static int basis_valid(const float *m)
{
    uint32_t i,j;float cross[3];
    if(!finite_values(m,9))return 0;
    for(i=0;i<3;i++)for(j=i;j<3;j++)if(fabsf(dot(m+3*i,m+3*j)-(i==j?1.f:0.f))>.002f)return 0;
    for(i=0;i<3;i++)cross[i]=m[(i+1)%3]*m[3+(i+2)%3]-m[(i+2)%3]*m[3+(i+1)%3];
    return fabsf(dot(cross,m+6)-1)<.002f;
}
static int inertia(const float *inverse,float *tensor)
{
    double minor,det;uint32_t i,j;
    if(!finite_values(inverse,9))return RF_RANGE;
    for(i=0;i<3;i++)for(j=i+1;j<3;j++)if(fabs((double)inverse[3*i+j]-inverse[3*j+i])>1e-5*(fabs(inverse[3*i+j])+fabs(inverse[3*j+i])+1e-8))return RF_RANGE;
    minor=(double)inverse[0]*inverse[4]-(double)inverse[1]*inverse[3];
    det=(double)inverse[0]*((double)inverse[4]*inverse[8]-(double)inverse[5]*inverse[7])-
        (double)inverse[1]*((double)inverse[3]*inverse[8]-(double)inverse[5]*inverse[6])+
        (double)inverse[2]*((double)inverse[3]*inverse[7]-(double)inverse[4]*inverse[6]);
    if(inverse[0]<=0||minor<=0||!isfinite(det)||det<=0)return RF_RANGE;
    return rf_physics_tensor_inverse(inverse,tensor);
}
static void transform_tensor(const float *basis,const float *tensor,const float *input,float *out)
{
    float local[3],value[3];uint32_t i,j;
    for(i=0;i<3;i++)local[i]=dot(basis+3*i,input);
    for(i=0;i<3;i++)value[i]=dot(tensor+3*i,local);
    for(i=0;i<3;i++){out[i]=0;for(j=0;j<3;j++)out[i]+=basis[j*3+i]*value[j];}
}
static void approach(float *value,const float *target,float amount)
{
    float delta[3],length;uint32_t i;
    for(i=0;i<3;i++)delta[i]=target[i]-value[i];
    length=sqrtf(dot(delta,delta));
    if(length>amount&&length>0)for(i=0;i<3;i++)delta[i]*=amount/length;
    for(i=0;i<3;i++)value[i]+=delta[i];
}
static int proposal_valid(const rf_vehicle_rigid_proposal *p)
{return finite_values(p->position,3)&&basis_valid(p->orientation)&&finite_values(p->velocity,3)&&finite_values(p->momentum,3)&&finite_values(p->angular_velocity,3);}
int rf_vehicle_submersible_step(rf_vehicle_rigid_state *state,const rf_vehicle_submersible_parameters *p,
    const rf_vehicle_submersible_command *c,float dt,const rf_vehicle_submersible_backend *backend,rf_vehicle_submersible_result *out)
{
    rf_vehicle_rigid_state next;rf_vehicle_rigid_proposal initial={0},proposal,resolved;
    rf_vehicle_submersible_result result={0};float tensor[9],target[3]={0},omega[3],length;uint32_t i,j,allowed=0;int status;
    if(!state||!p||!c||!backend||!backend->water_path||!backend->resolve||!out||!isfinite(dt)||dt<0||dt>.1f)return RF_RANGE;
    if(!finite_values(state->position,3)||!basis_valid(state->orientation)||!finite_values(state->velocity,3)||
       !finite_values(state->momentum,3)||!finite_values(state->force,3)||!finite_values(state->torque,3)||state->skip_forces>1||
       !isfinite(p->mass)||p->mass<=0||!isfinite(p->maximum_speed)||p->maximum_speed<=0||
       !isfinite(p->acceleration)||p->acceleration<=0||!isfinite(p->maximum_rotation)||p->maximum_rotation<=0||
       !isfinite(p->rotation_acceleration)||p->rotation_acceleration<=0||!isfinite(p->drag)||p->drag<0||
       !isfinite(c->throttle)||fabsf(c->throttle)>1||!isfinite(c->strafe)||fabsf(c->strafe)>1||
       !isfinite(c->rise)||fabsf(c->rise)>1||!isfinite(c->yaw)||fabsf(c->yaw)>1||!isfinite(c->pitch)||fabsf(c->pitch)>1||c->controlled>1)return RF_RANGE;
    status=inertia(state->inverse_inertia,tensor);if(status)return status;
    memcpy(initial.position,state->position,12);memcpy(initial.orientation,state->orientation,36);
    memcpy(initial.velocity,state->velocity,12);memcpy(initial.momentum,state->momentum,12);
    transform_tensor(state->orientation,state->inverse_inertia,state->momentum,initial.angular_velocity);
    if(!proposal_valid(&initial))return RF_RANGE;
    status=backend->water_path(backend->context,state,&initial,&allowed);if(status)return status;
    if(allowed>1)return RF_FORMAT;
    next=*state;result.wet=allowed;
    if(!allowed){result.water_blocked=1;goto park;}
    proposal=initial;
    if(!state->skip_forces){
        if(c->controlled)for(i=0;i<3;i++)target[i]=c->throttle*state->orientation[6+i]+c->strafe*state->orientation[i]+(i==1?c->rise:0);
        length=sqrtf(dot(target,target));
        if(length>0){
            float scale=p->maximum_speed/fmaxf(1,length);
            for(i=0;i<3;i++)target[i]*=scale;
            approach(proposal.velocity,target,p->acceleration*dt);
        }else for(i=0;i<3;i++)proposal.velocity[i]*=expf(-p->drag*dt);
        for(i=0;i<3;i++)proposal.velocity[i]+=state->force[i]/p->mass*dt;
        length=sqrtf(dot(proposal.velocity,proposal.velocity));
        if(length>p->maximum_speed)for(i=0;i<3;i++)proposal.velocity[i]*=p->maximum_speed/length;
        for(i=0;i<3;i++)target[i]=c->controlled?p->maximum_rotation*(c->pitch*state->orientation[i]+(i==1?c->yaw:0)):0;
        length=sqrtf(dot(target,target));
        if(length>p->maximum_rotation)for(i=0;i<3;i++)target[i]*=p->maximum_rotation/length;
        approach(proposal.angular_velocity,target,p->rotation_acceleration*dt);
        transform_tensor(state->orientation,tensor,proposal.angular_velocity,proposal.momentum);
        for(i=0;i<3;i++)proposal.momentum[i]+=state->torque[i]*dt;
        transform_tensor(state->orientation,state->inverse_inertia,proposal.momentum,proposal.angular_velocity);
    }
    for(i=0;i<3;i++)proposal.position[i]=state->position[i]+proposal.velocity[i]*dt;
    memcpy(omega,proposal.angular_velocity,12);length=sqrtf(dot(omega,omega));
    if(length>0&&dt>0){float axis[3],co=cosf(length*dt),si=sinf(length*dt);
        for(i=0;i<3;i++)axis[i]=omega[i]/length;
        for(j=0;j<3;j++){const float *v=state->orientation+3*j;float projection=dot(axis,v);
            for(i=0;i<3;i++)proposal.orientation[j*3+i]=v[i]*co+
                (axis[(i+1)%3]*v[(i+2)%3]-axis[(i+2)%3]*v[(i+1)%3])*si+axis[i]*projection*(1-co);
        }
    }
    if(!proposal_valid(&proposal))return RF_RANGE;
    status=backend->water_path(backend->context,state,&proposal,&allowed);if(status)return status;
    if(allowed>1)return RF_FORMAT;
    if(!allowed){result.water_blocked=1;goto park;}
    resolved=proposal;status=backend->resolve(backend->context,state,&proposal,&resolved);if(status)return status;
    if(!proposal_valid(&resolved))return RF_FORMAT;
    status=backend->water_path(backend->context,state,&resolved,&allowed);if(status)return status;
    if(allowed>1)return RF_FORMAT;
    if(!allowed){result.water_blocked=1;goto park;}
    memcpy(next.position,resolved.position,12);memcpy(next.orientation,resolved.orientation,36);
    memcpy(next.velocity,resolved.velocity,12);memcpy(next.momentum,resolved.momentum,12);
    result.moved=memcmp(next.position,state->position,12)!=0||memcmp(next.orientation,state->orientation,36)!=0;
    goto publish;
park:
    memset(next.velocity,0,12);memset(next.momentum,0,12);
publish:
    if(!next.skip_forces){memset(next.force,0,12);memset(next.torque,0,12);}
    *state=next;*out=result;return RF_OK;
}
