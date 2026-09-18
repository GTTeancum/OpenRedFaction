#include "rf/vehicle_rigid.h"
#include <math.h>
#include <string.h>
static float dot(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static int finite_array(const float *v,uint32_t n)
{
    uint32_t i;
    for(i=0;i<n;i++)if(!isfinite(v[i]))return 0;
    return 1;
}
static int basis_valid(const float *m)
{uint32_t i,j;for(i=0;i<3;i++){if(fabsf(dot(m+3*i,m+3*i)-1)>.02f)return 0;for(j=0;j<i;j++)if(fabsf(dot(m+3*i,m+3*j))>.02f)return 0;}return 1;}
static void inertia_apply(const rf_vehicle_rigid_state *s,const float *momentum,float *omega)
{
    float local[3],rotated[3];uint32_t i,j;
    for(i=0;i<3;i++)local[i]=dot(s->orientation+3*i,momentum);
    for(i=0;i<3;i++){rotated[i]=0;for(j=0;j<3;j++)rotated[i]+=s->inverse_inertia[3*i+j]*local[j];}
    for(i=0;i<3;i++){omega[i]=0;for(j=0;j<3;j++)omega[i]+=s->orientation[3*j+i]*rotated[j];}
}
static int proposal_valid(const rf_vehicle_rigid_proposal *p)
{return finite_array(p->position,3)&&finite_array(p->orientation,9)&&finite_array(p->velocity,3)&&finite_array(p->momentum,3)&&finite_array(p->angular_velocity,3)&&basis_valid(p->orientation);}
int rf_vehicle_rigid_propose(const rf_vehicle_rigid_state *s,const rf_vehicle_rigid_parameters *p,
    const rf_vehicle_rigid_command *c,const rf_vehicle_rigid_support *support,float dt,rf_vehicle_rigid_proposal *out)
{
    rf_vehicle_rigid_proposal n;float forward_speed,speed2,alignment,damping,accel,omega_up[3],response,inverse_up,target;
    uint32_t i,j;
    if(!s||!p||!c||!support||!out||!isfinite(dt)||dt<0||dt>.1f)return RF_RANGE;
    if(!finite_array(s->position,3)||!finite_array(s->orientation,9)||!basis_valid(s->orientation)||
       !finite_array(s->velocity,3)||!finite_array(s->momentum,3)||!finite_array(s->inverse_inertia,9)||
       !finite_array(s->force,3)||!finite_array(s->torque,3)||s->skip_forces>1||
       !isfinite(p->mass)||p->mass<=0||!isfinite(p->maximum_speed)||p->maximum_speed<=0||
       !isfinite(p->acceleration)||p->acceleration<0||!isfinite(p->maximum_turn)||p->maximum_turn<=0||
       !isfinite(p->turn_acceleration)||p->turn_acceleration<=0||!isfinite(p->gravity)||p->gravity<0||
       !isfinite(c->throttle)||fabsf(c->throttle)>1||!isfinite(c->turn)||fabsf(c->turn)>1||c->controlled>1||
       support->grounded>1||!isfinite(support->material)||support->material<0||
       !finite_array(support->velocity,3)||!finite_array(support->force,3)||!finite_array(support->torque,3))return RF_RANGE;
    memset(&n,0,sizeof(n));memcpy(n.velocity,s->velocity,12);memcpy(n.momentum,s->momentum,12);
    if(!s->skip_forces){
        if(c->controlled){
            inertia_apply(s,s->orientation+3,omega_up);inverse_up=dot(omega_up,s->orientation+3);
            if(!isfinite(inverse_up)||inverse_up<=0)return RF_RANGE;
            response=1-expf(dt*logf(.05f)/(p->maximum_turn/p->turn_acceleration));
            target=c->turn*p->maximum_turn/inverse_up;
            target=(target-dot(n.momentum,s->orientation+3))*response;
            for(i=0;i<3;i++)n.momentum[i]+=s->orientation[3+i]*target;
        }
        for(i=0;i<3;i++)n.momentum[i]+=(s->torque[i]+support->torque[i])*dt;
        if(support->grounded){
            float throttle=c->controlled?c->throttle:0;
            speed2=dot(n.velocity,n.velocity);forward_speed=dot(n.velocity,s->orientation+6);
            alignment=speed2>0?forward_speed*throttle/sqrtf(speed2):0;
            if(speed2>0 && alignment<=0){damping=fmaxf(0,1-(2-alignment)*dt);for(i=0;i<3;i++)n.velocity[i]*=damping;}
            accel=throttle*(speed2==0?1:p->acceleration*support->material);
            for(i=0;i<3;i++)n.velocity[i]+=s->orientation[6+i]*accel*dt;
            /* First-pass lateral slip damping, independent of forward cap. */
            damping=dot(n.velocity,s->orientation)*(1-expf(-4*dt));
            for(i=0;i<3;i++)n.velocity[i]-=s->orientation[i]*damping;
            forward_speed=dot(n.velocity,s->orientation+6);
            accel=forward_speed>p->maximum_speed?forward_speed-p->maximum_speed:forward_speed< -p->maximum_speed?forward_speed+p->maximum_speed:0;
            for(i=0;i<3;i++)n.velocity[i]-=s->orientation[6+i]*accel;
        }
        for(i=0;i<3;i++)n.velocity[i]+=(s->force[i]+support->force[i])/p->mass*dt;
        n.velocity[1]-=p->gravity*dt;
    }
    for(i=0;i<3;i++)n.position[i]=s->position[i]+(n.velocity[i]+support->velocity[i])*dt;
    inertia_apply(s,n.momentum,n.angular_velocity);
    {float length=sqrtf(dot(n.angular_velocity,n.angular_velocity)),axis[3],angle=length*dt;
     if(!isfinite(length)||!isfinite(angle))return RF_RANGE;
     if(length>0 && dt>0){float co=cosf(angle),si=sinf(angle);
      for(i=0;i<3;i++)axis[i]=n.angular_velocity[i]/length;
      for(j=0;j<3;j++){const float *v=s->orientation+3*j;float a=dot(axis,v);
       for(i=0;i<3;i++)n.orientation[j*3+i]=v[i]*co+(axis[(i+1)%3]*v[(i+2)%3]-axis[(i+2)%3]*v[(i+1)%3])*si+axis[i]*a*(1-co);}
     }else memcpy(n.orientation,s->orientation,36);}
    if(!proposal_valid(&n))return RF_RANGE;
    *out=n;
    return RF_OK;
}
int rf_vehicle_rigid_step(rf_vehicle_rigid_state *s,const rf_vehicle_rigid_parameters *p,
    const rf_vehicle_rigid_command *c,float dt,const rf_vehicle_rigid_backend *backend)
{
    rf_vehicle_rigid_support support={0};rf_vehicle_rigid_proposal proposal,resolved;int status;
    if(!s||!backend||!backend->support||!backend->resolve)return RF_RANGE;
    status=backend->support(backend->context,s,&support);if(status)return status;
    status=rf_vehicle_rigid_propose(s,p,c,&support,dt,&proposal);if(status)return status;
    resolved=proposal;status=backend->resolve(backend->context,s,&proposal,&resolved);if(status)return status;
    if(!proposal_valid(&resolved))return RF_RANGE;
    memcpy(s->position,resolved.position,12);memcpy(s->orientation,resolved.orientation,36);
    memcpy(s->velocity,resolved.velocity,12);memcpy(s->momentum,resolved.momentum,12);
    if(!s->skip_forces){memset(s->force,0,12);memset(s->torque,0,12);}return RF_OK;
}
