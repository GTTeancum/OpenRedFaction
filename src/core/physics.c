#include "rf/physics.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
int rf_physics_gravity_set(rf_physics_gravity *state,float acceleration)
{
    if(!state || !isfinite(acceleration))return RF_RANGE;
    state->acceleration=acceleration;state->vector[0]=state->vector[2]=0;
    state->vector[1]=-acceleration;return RF_OK;
}
int rf_physics_ground_propose(rf_physics_body_state *state,float dt,float drag,
    const float steering_acceleration[3],const float support_velocity[3])
{
    rf_physics_body_state value;volatile float half_dt_squared;uint32_t k;
    if(!state || !steering_acceleration || !support_velocity || !isfinite(dt) || dt<0 ||
       !isfinite(drag) || drag<0 || !isfinite(state->mass) || state->mass<=0)return RF_RANGE;
    value=*state;half_dt_squared=(float)((double)dt*dt*.5);
    for(k=0;k<3;++k) {
        volatile float resistance,acceleration,force,increment,combined,travel,base,correction;
        if(!isfinite(state->position[k]) || !isfinite(state->velocity[k]) ||
           !isfinite(state->vector_e0[k]) || !isfinite(steering_acceleration[k]) || !isfinite(support_velocity[k]))return RF_RANGE;
        resistance=(float)((double)drag*state->velocity[k]);
        acceleration=(float)((double)steering_acceleration[k]-resistance);
        force=(float)((double)state->vector_e0[k]/state->mass);
        acceleration=(float)((double)acceleration+force);
        increment=(float)((double)acceleration*dt);
        value.velocity[k]=(float)((double)state->velocity[k]+increment);
        combined=(float)((double)value.velocity[k]+support_velocity[k]);
        travel=(float)((double)combined*dt);base=(float)((double)state->position[k]+travel);
        correction=(float)((double)acceleration*half_dt_squared);
        /* Original grounded branch adds this term after updated-velocity
         * travel. Falling uses subtraction; do not silently unify them. */
        value.next_position[k]=(float)((double)base+correction);
        if(!isfinite(value.velocity[k]) || !isfinite(value.next_position[k]))return RF_RANGE;
    }
    *state=value;return RF_OK;
}
int rf_physics_support_commit(rf_physics_body_state *state,const rf_physics_ground_probe *probe,
    float fraction,uint32_t moving,float contact_y,uint32_t object_handle,uint32_t *support_handle)
{
    rf_physics_body_state value;float candidate;uint32_t k;
    if(!state || !probe || !support_handle || !isfinite(contact_y) || !isfinite(fraction) || fraction<0 || fraction>=1 ||
       !isfinite(probe->start[1]) || !isfinite(probe->end[1]) ||
       !isfinite(state->bounds.radius) || state->bounds.radius<0)return RF_RANGE;
    value=*state;
    candidate=(float)(((double)probe->end[1]-probe->start[1])*fraction+probe->start[1]+.05f);
    if(!isfinite(candidate))return RF_RANGE;
    for(k=0;k<3;++k)if(!isfinite(value.next_position[k]) || !isfinite(value.velocity[k]))return RF_RANGE;
    value.next_position[1]=moving && contact_y>0?candidate:fminf(value.next_position[1],candidate);
    memcpy(value.position,value.next_position,12);
    for(k=0;k<3;++k) {
        value.bounds.minimum[k]=(float)((double)value.position[k]-value.bounds.radius);
        value.bounds.maximum[k]=(float)((double)value.position[k]+value.bounds.radius);
        if(!isfinite(value.bounds.minimum[k]) || !isfinite(value.bounds.maximum[k]))return RF_RANGE;
    }
    if(moving)value.flags|=0x400000u;else value.flags&=~0x400000u;
    *state=value;*support_handle=moving?object_handle:0;return RF_OK;
}
int rf_physics_static_support(rf_physics_body_state *state,const rf_physics_ground_probe *probe,float fraction)
{
    uint32_t handle;
    return rf_physics_support_commit(state,probe,fraction,0,0,0,&handle);
}
int rf_physics_static_land(rf_physics_body_state *state,const rf_physics_ground_probe *probe,float fraction)
{
    int status=rf_physics_static_support(state,probe,fraction);if(status)return status;
    state->velocity[1]=0;state->flags&=~0x200000u;return RF_OK;
}
int rf_physics_landing_velocity(const float velocity[3],const float previous_support[3],
    const float contact_velocity[3],float result[3])
{
    float value[3];uint32_t i;
    if(!velocity || !previous_support || !contact_velocity || !result)return RF_RANGE;
    for(i=0;i<3;i++) {
        volatile float sum;
        if(!isfinite(velocity[i]) || !isfinite(previous_support[i]) || !isfinite(contact_velocity[i]))return RF_FORMAT;
        sum=velocity[i]+previous_support[i];value[i]=sum-contact_velocity[i];
        if(!isfinite(sum) || !isfinite(value[i]))return RF_FORMAT;
    }
    memcpy(result,value,12);return RF_OK;
}
int rf_physics_contact_advance(rf_physics_body_state *state,float dt,float fraction,float *remaining)
{
    float delta[3],position[3],adjusted=fraction,left;double length;uint32_t i;
    if(!state || !remaining || !isfinite(dt) || dt<0 || !isfinite(fraction) || fraction<0 || fraction>=1 || (state->flags&0x4000))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(state->position[i]) || !isfinite(state->next_position[i]))return RF_RANGE;
        delta[i]=(float)((double)state->next_position[i]-state->position[i]);if(!isfinite(delta[i]))return RF_RANGE;
    }
    length=sqrt(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
    if(!isfinite(length) || length==0)return RF_RANGE;
    left=(float)((double)dt-(double)dt*fraction);
    if(!(state->flags&0x400000)) {
        double ratio=(length*fraction-(double).05f)/length;
        adjusted=ratio<0?0:(float)ratio;
    }
    for(i=0;i<3;++i) {
        volatile float travel=(float)((double)delta[i]*adjusted);
        position[i]=(float)((double)state->position[i]+travel);if(!isfinite(position[i]))return RF_RANGE;
    }
    memcpy(state->position,position,sizeof(position));state->scalar_144=adjusted;*remaining=left;return RF_OK;
}
int rf_physics_static_contact(rf_physics_body_state *state,const float normal[3],
    const float support_velocity[3],const float contact_velocity[3],float *impact_speed)
{
    float combined[3],correction[3],velocity[3],contact_dot,impact,scale;double actor_dot,test;uint32_t i;
    if(!state || !normal || !support_velocity || !contact_velocity || !impact_speed || (state->flags&0x80))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(normal[i]) || !isfinite(state->velocity[i]) || !isfinite(support_velocity[i]) || !isfinite(contact_velocity[i]))return RF_RANGE;
        combined[i]=(float)((double)state->velocity[i]+support_velocity[i]);
        if(!isfinite(combined[i]))return RF_RANGE;
    }
    contact_dot=(float)(((double)contact_velocity[2]*normal[2]+(double)contact_velocity[1]*normal[1])+(double)contact_velocity[0]*normal[0]);
    actor_dot=((double)combined[2]*normal[2]+(double)combined[1]*normal[1])+(double)combined[0]*normal[0];
    impact=(float)((double)contact_dot-actor_dot);scale=(float)((double)impact*1.100000023841858f);
    if(!isfinite(impact) || !isfinite(scale))return RF_RANGE;
    for(i=0;i<3;++i) {correction[i]=(float)((double)scale*normal[i]);if(!isfinite(correction[i]))return RF_RANGE;}
    test=((double)correction[2]*normal[2]+(double)correction[1]*normal[1])+(double)correction[0]*normal[0];
    if(test>0) {
        for(i=0;i<3;++i) {velocity[i]=(float)((double)state->velocity[i]+correction[i]);if(!isfinite(velocity[i]))return RF_RANGE;}
        memcpy(state->velocity,velocity,sizeof(velocity));memset(state->vector_c8,0,sizeof(state->vector_c8));
    }
    *impact_speed=impact;return RF_OK;
}
static long double player_contact_dot(const float a[3],const float b[3])
{return ((long double)a[2]*b[2]+(long double)a[1]*b[1])+(long double)a[0]*b[0];}
int rf_physics_player_contact(rf_physics_body_state *state,const float normal[3],
    const float support_velocity[3],const float contact_velocity[3],const float direction[3],
    uint32_t mode,uint32_t free_tangent,float *impact_speed)
{
    float combined[3],tangent[3],velocity[3],normalized[3],world_direction[3];
    float original_dot,contact_dot,impact,scale;uint32_t i;
    if(!state || !normal || !support_velocity || !contact_velocity || !direction || !impact_speed ||
       !(state->flags&0x80) || free_tangent>1)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(state->orientation[i]))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(state->velocity[i]) || !isfinite(normal[i]) || !isfinite(support_velocity[i]) ||
           !isfinite(contact_velocity[i]) || !isfinite(direction[i]))return RF_RANGE;
        combined[i]=(float)((long double)state->velocity[i]+support_velocity[i]);
    }
    original_dot=(float)player_contact_dot(state->velocity,normal);
    contact_dot=(float)player_contact_dot(contact_velocity,normal);
    impact=(float)((long double)contact_dot-player_contact_dot(combined,normal));
    for(i=0;i<3;++i) {
        float projection=(float)((long double)original_dot*normal[i]);
        tangent[i]=(float)((long double)state->velocity[i]-projection);
    }
    if(contact_dot>0 && support_velocity[0]==0 && support_velocity[1]==0 && support_velocity[2]==0) {
        scale=(float)(player_contact_dot(contact_velocity,normal)*1.100000023841858f);
        for(i=0;i<3;++i)velocity[i]=(float)((long double)scale*normal[i]);
    } else if(original_dot>0) {
        for(i=0;i<3;++i)velocity[i]=(float)((long double)original_dot*normal[i]);
    } else memset(velocity,0,sizeof(velocity));
    if(original_dot<0) {
        long double length=sqrtl(((long double)state->velocity[0]*state->velocity[0]+
            (long double)state->velocity[1]*state->velocity[1])+(long double)state->velocity[2]*state->velocity[2]);
        if(length<=0){normalized[0]=1;normalized[1]=normalized[2]=0;}
        else for(i=0;i<3;++i)normalized[i]=(float)((1/length)*state->velocity[i]);
        scale=(float)(((player_contact_dot(normalized,normal)+1)*1.5f)*original_dot);
        for(i=0;i<3;++i) {
            float correction=(float)((long double)scale*normal[i]);
            velocity[i]=(float)((long double)velocity[i]-correction);
        }
    }
    if(tangent[0]!=0 || tangent[1]!=0 || tangent[2]!=0) {
        uint32_t add=free_tangent;
        if(!add) {
            float length=(float)sqrtl(((long double)tangent[0]*tangent[0]+
                (long double)tangent[1]*tangent[1])+(long double)tangent[2]*tangent[2]);
            if(!isfinite(length) || length<=0)return RF_RANGE;
            for(i=0;i<3;++i) {
                normalized[i]=(float)((long double)tangent[i]/length);
                world_direction[i]=(float)(((long double)state->orientation[6+i]*direction[2]+
                    (long double)state->orientation[3+i]*direction[1])+(long double)state->orientation[i]*direction[0]);
            }
            add=player_contact_dot(normalized,world_direction)>=0;
            if(add && mode==1)tangent[1]=fmaxf(tangent[1],0);
        }
        if(add)for(i=0;i<3;++i)velocity[i]=(float)((long double)velocity[i]+tangent[i]);
    }
    if(!isfinite(impact))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(velocity[i]))return RF_RANGE;
    memcpy(state->velocity,velocity,sizeof(velocity));*impact_speed=impact;return RF_OK;
}
int rf_physics_fall_propose(rf_physics_body_state *state,float dt,float gravity,const float support_velocity[3])
{
    float velocity[3],position[3];volatile float half_dt_squared;uint32_t i;
    if(!state || !support_velocity || !isfinite(dt) || dt<0 || !isfinite(gravity) ||
       !isfinite(state->mass) || state->mass<=0)return RF_RANGE;
    half_dt_squared=(float)((double)dt*dt*.5);
    if(!isfinite(half_dt_squared))return RF_RANGE;
    for(i=0;i<3;++i) {
        volatile float acceleration,increment,combined,travel,base,correction;
        if(!isfinite(state->position[i]) || !isfinite(state->velocity[i]) ||
           !isfinite(state->vector_e0[i]) || !isfinite(support_velocity[i]))return RF_RANGE;
        acceleration=0;
        if(!(state->flags&0x1000000)) {
            acceleration=(float)((double)state->vector_e0[i]/state->mass);
            if(i==1)acceleration=(float)((double)acceleration-gravity);
        }
        increment=(float)((double)acceleration*dt);
        velocity[i]=(state->flags&0x1000000)?state->velocity[i]:(float)((double)state->velocity[i]+increment);
        combined=(float)((double)velocity[i]+support_velocity[i]);
        travel=(float)((double)combined*dt);base=(float)((double)state->position[i]+travel);
        correction=(float)((double)acceleration*half_dt_squared);
        position[i]=(float)((double)base-correction);
        if(!isfinite(acceleration) || !isfinite(velocity[i]) || !isfinite(position[i]))return RF_RANGE;
    }
    memcpy(state->velocity,velocity,sizeof(velocity));memcpy(state->next_position,position,sizeof(position));return RF_OK;
}
void rf_physics_body_close(rf_physics_body *body)
{
    if(body) {rf_physics_spheres_close(&body->spheres);memset(body,0,sizeof(*body));}
}
int rf_physics_body_replace_spheres(rf_physics_body *body,
    const rf_physics_sphere *source,uint32_t count,uint32_t budget)
{
    rf_physics_spheres replacement={0};rf_physics_bounds bounds;
    uint64_t old_bytes,new_bytes,peak;uint32_t flags,i;int status;
    if(!body || !body->allocated_bytes)return RF_RANGE;
    old_bytes=(uint64_t)body->spheres.count*sizeof(*source);
    new_bytes=(uint64_t)count*sizeof(*source);peak=sizeof(*body)+old_bytes+new_bytes;
    if(body->allocated_bytes!=sizeof(*body)+old_bytes ||
       body->spheres.allocated_bytes!=sizeof(body->spheres)+old_bytes ||
       (body->spheres.count && !body->spheres.items) || peak>budget)return RF_RANGE;
    status=rf_physics_spheres_bounds(source,count,body->state.position,&bounds);if(status)return status;
    flags=body->state.flags&~0x2000u;
    for(i=0;i<count;++i)if(source[i].parameter_10>0)flags|=0x2000;
    status=rf_physics_spheres_open(source,count,(uint32_t)(sizeof(replacement)+new_bytes),&replacement);
    if(status)return status;
    rf_physics_spheres_close(&body->spheres);body->spheres=replacement;
    body->state.bounds=bounds;body->state.flags=flags;
    body->allocated_bytes=(uint32_t)(sizeof(*body)+new_bytes);return RF_OK;
}
int rf_physics_body_open(const rf_physics_body_parameters *parameters,
    const rf_physics_sphere *source,uint32_t count,uint32_t budget,rf_physics_body *result)
{
    rf_physics_body value={0};rf_physics_body_state *state=&value.state;uint32_t i;uint64_t bytes;int status;
    if(!parameters || !result || result->allocated_bytes || result->spheres.items || result->spheres.count || result->spheres.allocated_bytes)return RF_RANGE;
    if(!(parameters->flags&0x70)) {count=0;source=NULL;}
    bytes=sizeof(value)+(uint64_t)count*sizeof(*source);
    if(bytes>budget || (count && !source) || !isfinite(parameters->mass))return RF_RANGE;
    for(i=0;i<3;++i) {
        double product=(double)parameters->mass*parameters->vector_78[i];
        if(!isfinite(parameters->coefficients[i]) || !isfinite(parameters->velocity[i]) ||
            !isfinite(product) || fabs(product)>FLT_MAX)return RF_RANGE;
        state->coefficients[i]=parameters->coefficients[i];state->velocity[i]=parameters->velocity[i];
        state->vector_c8[i]=parameters->vector_78[i];state->mass_vector_d4[i]=(float)product;
    }
    status=rf_physics_tensor_world(parameters->local_tensor,parameters->orientation,state->world_tensor);if(status)return status;
    status=rf_physics_spheres_bounds(source,count,parameters->position,&state->bounds);if(status)return status;
    state->mass=parameters->mass;state->flags=parameters->flags;
    memcpy(state->local_tensor,parameters->local_tensor,sizeof(state->local_tensor));
    memcpy(state->position,parameters->position,sizeof(state->position));memcpy(state->next_position,state->position,sizeof(state->position));
    memcpy(state->orientation,parameters->orientation,sizeof(state->orientation));memcpy(state->next_orientation,state->orientation,sizeof(state->orientation));
    for(i=0;i<count;++i)if(source[i].parameter_10>0)state->flags|=0x2000;
    state->vector_138[1]=1;state->scalar_144=1;state->reference_15c=-1;
    status=rf_physics_spheres_open(source,count,(uint32_t)(budget-sizeof(value)+sizeof(value.spheres)),&value.spheres);if(status)return status;
    value.allocated_bytes=(uint32_t)bytes;*result=value;return RF_OK;
}
int rf_physics_spheres_bounds(const rf_physics_sphere *source,uint32_t count,
    const float position[3],rf_physics_bounds *result)
{
    rf_physics_bounds value={0};uint32_t i,j;
    if(!position || !result || (count && !source))return RF_RANGE;
    for(j=0;j<3;++j)if(!isfinite(position[j]))return RF_RANGE;
    for(i=0;i<count;++i) {
        double x=source[i].center[0],y=source[i].center[1],z=source[i].center[2],distance,sum;
        volatile float rounded_distance,candidate;
        if(!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(source[i].radius) || source[i].radius<0)return RF_RANGE;
        distance=sqrt((x*x+y*y)+z*z);if(distance>FLT_MAX)return RF_RANGE;
        /* 4a0ce2 stores length before radius addition at 4a0cee. */
        rounded_distance=(float)distance;sum=(double)rounded_distance+source[i].radius;
        if(sum>FLT_MAX)return RF_RANGE;candidate=(float)sum;
        if(candidate>value.radius)value.radius=candidate;
    }
    for(j=0;j<3;++j) {
        double low=(double)position[j]-value.radius,high=(double)value.radius+position[j];
        float a,b;
        if(fabs(low)>FLT_MAX || fabs(high)>FLT_MAX)return RF_RANGE;
        a=(float)low;b=(float)high;
        /* 539460 selects its second endpoint as minimum on equality. */
        if(a<b) {value.minimum[j]=a;value.maximum[j]=b;}
        else {value.minimum[j]=b;value.maximum[j]=a;}
    }
    *result=value;return RF_OK;
}
/* 40ea80 computes right*left when the stored vectors are read as rows.
 * Its term order varies by output element. */
static int tensor_product(const float left[9],const float right[9],float result[9])
{
    static const unsigned char order[9][3]={{0,2,1},{2,1,0},{1,0,2},{2,1,0},{2,1,0},{0,1,2},{2,1,0},{2,0,1},{1,0,2}};
    uint32_t row,col,i;
    for(row=0;row<3;++row)for(col=0;col<3;++col) {
        const unsigned char *k=order[row*3+col];double v=(double)right[row*3+k[0]]*left[k[0]*3+col];
        for(i=1;i<3;++i)v+=(double)right[row*3+k[i]]*left[k[i]*3+col];
        if(!isfinite(v) || fabs(v)>FLT_MAX)return RF_RANGE;
        result[row*3+col]=(float)v;
    }
    return RF_OK;
}
int rf_physics_tensor_world(const float local[9],const float orientation[9],float result[9])
{
    float transpose[9],intermediate[9],value[9];uint32_t i,j;int status;
    if(!local || !orientation || !result)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(local[i]) || !isfinite(orientation[i]))return RF_RANGE;
    for(i=0;i<3;++i)for(j=0;j<3;++j)transpose[i*3+j]=orientation[j*3+i];
    status=tensor_product(orientation,local,intermediate);if(status)return status;
    status=tensor_product(intermediate,transpose,value);if(status)return status;
    memcpy(result,value,sizeof(value));return RF_OK;
}
/* Original 4fc4c0 term order, retaining extended precision until the caller's
 * float determinant store. Save/restore the host control word. */
static double tensor_determinant(const float *source,float *rounded)
{
    double value;
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    unsigned short saved,control;
#if defined(_MSC_VER)
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,source
        mov edx,rounded
        lea eax,value
        fld dword ptr [ecx+32]
        fmul dword ptr [ecx]
        fmul dword ptr [ecx+16]
        fld dword ptr [ecx+12]
        fmul dword ptr [ecx+8]
        fmul dword ptr [ecx+28]
        faddp st(1),st(0)
        fld dword ptr [ecx+20]
        fmul dword ptr [ecx+4]
        fmul dword ptr [ecx+24]
        faddp st(1),st(0)
        fld dword ptr [ecx+8]
        fmul dword ptr [ecx+24]
        fmul dword ptr [ecx+16]
        fsubp st(1),st(0)
        fld dword ptr [ecx+20]
        fmul dword ptr [ecx]
        fmul dword ptr [ecx+28]
        fsubp st(1),st(0)
        fld dword ptr [ecx+12]
        fmul dword ptr [ecx+4]
        fmul dword ptr [ecx+32]
        fsubp st(1),st(0)
        fst dword ptr [edx]
        fstp qword ptr [eax]
        fldcw saved
    }
#else
    __asm__ volatile("fnstcw %0":"=m"(saved));
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile(".intel_syntax noprefix\n\t"
        "fld dword ptr [ecx+32]\n\t"
        "fmul dword ptr [ecx]\n\t"
        "fmul dword ptr [ecx+16]\n\t"
        "fld dword ptr [ecx+12]\n\t"
        "fmul dword ptr [ecx+8]\n\t"
        "fmul dword ptr [ecx+28]\n\t"
        "faddp st(1),st(0)\n\t"
        "fld dword ptr [ecx+20]\n\t"
        "fmul dword ptr [ecx+4]\n\t"
        "fmul dword ptr [ecx+24]\n\t"
        "faddp st(1),st(0)\n\t"
        "fld dword ptr [ecx+8]\n\t"
        "fmul dword ptr [ecx+24]\n\t"
        "fmul dword ptr [ecx+16]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fld dword ptr [ecx+20]\n\t"
        "fmul dword ptr [ecx]\n\t"
        "fmul dword ptr [ecx+28]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fld dword ptr [ecx+12]\n\t"
        "fmul dword ptr [ecx+4]\n\t"
        "fmul dword ptr [ecx+32]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fst dword ptr [edx]\n\t"
        "fstp qword ptr [eax]\n\t"
        ".att_syntax prefix"
        ::"c"(source),"d"(rounded),"a"(&value):"memory","st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#endif
#else
    const float *m=source;
    value=(((((long double)m[8]*m[0]*m[4]+(long double)m[3]*m[2]*m[7])+
        (long double)m[5]*m[1]*m[6])-(long double)m[2]*m[6]*m[4])-
        (long double)m[5]*m[0]*m[7])-(long double)m[3]*m[1]*m[8];
    *rounded=(float)value;
#endif
    return value;
}
int rf_physics_tensor_inverse(const float source[9],float result[9])
{
    double m[9],det,last;float value[9];float denominator;volatile float minor[8];uint32_t i;
    if(!source || !result)return RF_RANGE;
    for(i=0;i<9;++i) {if(!isfinite(source[i]))return RF_RANGE;m[i]=source[i];}
    /* 4fc4c0 keeps this exact term order; 4fccf0 compares the unrounded
     * determinant with zero, but divides by its stored float value. */
    det=tensor_determinant(source,&denominator);
    if(det==0) {memmove(result,source,9*sizeof(float));return RF_OK;}
    if(!isfinite(denominator) || denominator==0)return RF_RANGE;
    minor[0]=(float)(m[4]*m[8]-m[5]*m[7]);
    minor[1]=(float)(-(m[1]*m[8]-m[2]*m[7]));
    minor[2]=(float)(m[1]*m[5]-m[2]*m[4]);
    minor[3]=(float)(-(m[3]*m[8]-m[5]*m[6]));
    minor[4]=(float)(m[0]*m[8]-m[2]*m[6]);
    minor[5]=(float)(-(m[0]*m[5]-m[2]*m[3]));
    minor[6]=(float)(m[3]*m[7]-m[4]*m[6]);
    minor[7]=(float)(-(m[0]*m[7]-m[1]*m[6]));
    /* The final 505260 result stays in the FPU through the first eight stores. */
    last=m[0]*m[4]-m[1]*m[3];
    for(i=0;i<9;++i) {
        double divided=(i==8?last:(double)minor[i])/(double)denominator;
        if(!isfinite(divided) || fabs(divided)>FLT_MAX)return RF_RANGE;
        value[i]=(float)divided;
    }
    memcpy(result,value,sizeof(value));return RF_OK;
}
int rf_physics_spheres_prepare(const rf_physics_sphere *source,uint32_t count,float density,
    const rf_physics_mass_tensor *initial,rf_physics_mass_tensor *result)
{
    rf_physics_mass_tensor value;int status;
    if(!result)return RF_RANGE;
    status=rf_physics_spheres_accumulate(source,count,density,initial,&value);if(status)return status;
    status=rf_physics_tensor_inverse(value.tensor,value.tensor);if(status)return status;
    *result=value;return RF_OK;
}
int rf_physics_spheres_accumulate(const rf_physics_sphere *source,uint32_t count,float density,
    const rf_physics_mass_tensor *initial,rf_physics_mass_tensor *result)
{
    rf_physics_mass_tensor value;uint32_t i,j;
    if(!source || !count || !initial || !result || !isfinite(density) || density<0 || !isfinite(initial->mass))return RF_RANGE;
    for(j=0;j<9;++j)if(!isfinite(initial->tensor[j]))return RF_RANGE;
    value=*initial;
    for(i=0;i<count;++i) {
        double x=source[i].center[0],y=source[i].center[1],z=source[i].center[2],r=source[i].radius;
        double generated,xy,yz;volatile float mass,xx,yy,xz;
        if(!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(r) || r<0)return RF_RANGE;
        generated=((r*r)*r)*(double)density*(double)4.18879032135009765625f;
        if(generated>FLT_MAX)return RF_RANGE;
        mass=(float)generated;xx=(float)(x*x);yy=(float)(y*y);
        xy=x*y*(double)mass;xz=(float)(x*z*(double)mass);yz=z*y*(double)mass;
        /* The x87 loop retains products except its x*z spill and the x*x,
         * y*y values reloaded for the final diagonal (49ed68/49edc2). */
        value.tensor[0]=(float)((y*y+z*z)*(double)mass+value.tensor[0]);
        value.tensor[1]=(float)(value.tensor[1]-xy);
        value.tensor[2]=(float)((double)value.tensor[2]-xz);
        value.tensor[3]=(float)(value.tensor[3]-xy);
        value.tensor[4]=(float)((x*x+z*z)*(double)mass+value.tensor[4]);
        value.tensor[5]=(float)(value.tensor[5]-yz);
        value.tensor[6]=(float)((double)value.tensor[6]-xz);
        value.tensor[7]=(float)(value.tensor[7]-yz);
        value.tensor[8]=(float)(((double)xx+yy)*(double)mass+value.tensor[8]);
        value.mass=(float)((double)mass+value.mass);
        if(!isfinite(value.mass))return RF_RANGE;
        for(j=0;j<9;++j)if(!isfinite(value.tensor[j]))return RF_RANGE;
    }
    *result=value;return RF_OK;
}
void rf_physics_spheres_close(rf_physics_spheres *spheres)
{
    if(spheres) {free(spheres->items);memset(spheres,0,sizeof(*spheres));}
}
int rf_physics_spheres_open(const rf_physics_sphere *source,uint32_t count,uint32_t budget,rf_physics_spheres *result)
{
    rf_physics_spheres value={0};uint32_t i,j;uint64_t bytes=sizeof(value)+(uint64_t)count*sizeof(*source);
    if(!result || result->items || result->count || result->allocated_bytes || (count && !source) || bytes>budget)return RF_RANGE;
    for(i=0;i<count;++i) {
        if(!isfinite(source[i].radius) || source[i].radius<0)return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(source[i].center[j]))return RF_RANGE;
    }
    if(count) {
        value.items=malloc((size_t)count*sizeof(*source));if(!value.items)return RF_RANGE;
        memcpy(value.items,source,(size_t)count*sizeof(*source));
    }
    value.count=count;value.allocated_bytes=(uint32_t)bytes;*result=value;return RF_OK;
}
int rf_physics_ground_prepare(const rf_physics_sphere *spheres,uint32_t count,
    const float next_position[3],uint32_t collision_flags,int falling,float dt,
    float class_speed,float support_y,rf_physics_ground_probe *result)
{
    rf_physics_ground_probe value={0};uint32_t i,k,selected=0;volatile float lifted;
    if(!spheres || !count || !next_position || !result || !isfinite(dt) || dt<0 ||
       !isfinite(class_speed) || class_speed<0 || !isfinite(support_y))return RF_RANGE;
    for(k=0;k<3;++k)if(!isfinite(next_position[k]))return RF_RANGE;
    for(i=0;i<count;++i) {
        if(!isfinite(spheres[i].radius) || spheres[i].radius<0)return RF_RANGE;
        for(k=0;k<3;++k)if(!isfinite(spheres[i].center[k]))return RF_RANGE;
        if(spheres[i].center[1]<spheres[selected].center[1])selected=i;
    }
    value.sphere=spheres[selected];value.sphere_index=selected;
    value.bounds.radius=(float)(fabs((double)value.sphere.center[1])+value.sphere.radius);
    memcpy(value.start,next_position,12);memcpy(value.end,next_position,12);
    lifted=(float)((double)next_position[1]+.05f);
    value.start[1]=support_y>0?(float)((double)lifted+(double)dt*support_y):lifted;
    value.end[1]=(float)((double)next_position[1]-(falling?(double).1f:(double)dt*class_speed+.05f));
    value.query_flags=(collision_flags&~0x1000u)|4;
    if(value.bounds.radius<.05f)value.query_flags|=0x100;
    for(k=0;k<3;++k) {
        value.bounds.minimum[k]=(float)((double)fminf(value.start[k],value.end[k])-value.bounds.radius);
        value.bounds.maximum[k]=(float)((double)fmaxf(value.start[k],value.end[k])+value.bounds.radius);
        if(!isfinite(value.start[k]) || !isfinite(value.end[k]) ||
           !isfinite(value.bounds.minimum[k]) || !isfinite(value.bounds.maximum[k]))return RF_RANGE;
    }
    *result=value;return RF_OK;
}
int rf_physics_fallback_prepare(float density,float radius,float mass,rf_physics_fallback *result)
{
    rf_physics_fallback value={0};double generated;
    if(!result || !isfinite(density) || !isfinite(radius) || !isfinite(mass) || density<0 || radius<0)return RF_RANGE;
    value.mass=mass;
    if(mass<=0) {
        /* Original keeps intermediate products in x87 before one float store. */
        generated=(double)density*(double)radius*(double)radius;
        if(generated>FLT_MAX)return RF_RANGE;
        value.mass=(float)generated;
    }
    value.radius=radius;value.parameter_10=-1;
    *result=value;return RF_OK;
}

int rf_physics_stance_centers(rf_physics_spheres *spheres,const float (*centers)[3],
    uint32_t count,uint32_t *actor_flags,int crouching)
{
    float saved[8][3];uint32_t i,k;
    if(!spheres || !actor_flags || spheres->count>8 || count<spheres->count ||
       (spheres->count && (!spheres->items || !centers)) || (crouching!=0 && crouching!=1))return RF_RANGE;
    for(i=0;i<spheres->count;++i)for(k=0;k<3;++k) {
        if(!isfinite(centers[i][k]))return RF_RANGE;
        saved[i][k]=centers[i][k];
    }
    for(i=0;i<spheres->count;++i)memcpy(spheres->items[i].center,saved[i],12);
    if(crouching)*actor_flags|=0x400;else *actor_flags&=~0x400u;
    return RF_OK;
}
int rf_physics_stand_endpoint(const float position[3],float height_difference,float end[3])
{
    float value[3];uint32_t k;
    if(!position || !end || !isfinite(height_difference))return RF_RANGE;
    for(k=0;k<3;++k)if(!isfinite(position[k]))return RF_RANGE;
    memcpy(value,position,12);
    value[1]=(float)((double)height_difference+position[1]+(double).1f);
    if(!isfinite(value[1]))return RF_RANGE;
    memcpy(end,value,12);return RF_OK;
}

/* Preserve 49e4ab..49e4ea x87 transcendental rounding on NXDK. The caller
 * control word is restored; binary32 stores remain explicit below. */
static float run_blend(float ratio,float traction,float dt)
{
#if (defined(__i386__) || defined(_M_IX86)) && (defined(__clang__) || defined(__GNUC__))
    const double decay=.05;float result;unsigned short saved,extended=0x37f;
    __asm__ volatile("fnstcw %0":"=m"(saved));
    __asm__ volatile("fldcw %0"::"m"(extended));
    __asm__ volatile(
        "flds %1; fdivs %2; fldln2; fldl %3; fyl2x; fdivrp; fdivrs %4; "
        "fldl2e; fmulp; fld %%st(0); frndint; fxch %%st(1); fsub %%st(1); "
        "f2xm1; fld1; faddp; fscale; fstp %%st(1); fld1; fsubp; fstps %0"
        :"=m"(result):"m"(ratio),"m"(traction),"m"(decay),"m"(dt):"st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));return result;
#else
    return (float)(1.0-pow(.05,(double)dt/((double)ratio/traction)));
#endif
}
/* 49e49c: climb keeps speed/acceleration in x87 through the exponential. */
static float climb_blend(float speed,float acceleration,float dt)
{
#if (defined(__i386__) || defined(_M_IX86)) && (defined(__clang__) || defined(__GNUC__))
    const double decay=.05;float result;unsigned short saved,extended=0x37f;
    __asm__ volatile("fnstcw %0":"=m"(saved));
    __asm__ volatile("fldcw %0"::"m"(extended));
    __asm__ volatile(
        "flds %1; fdivs %2; fldln2; fldl %3; fyl2x; fdivrp; fdivrs %4; "
        "fldl2e; fmulp; fld %%st(0); frndint; fxch %%st(1); fsub %%st(1); "
        "f2xm1; fld1; faddp; fscale; fstp %%st(1); fld1; fsubp; fstps %0"
        :"=m"(result):"m"(speed),"m"(acceleration),"m"(decay),"m"(dt):"st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));return result;
#else
    return (float)(1.0-pow(.05,(double)dt/((double)speed/acceleration)));
#endif
}
static int driven_propose(rf_physics_body_state *state,float dt,float speed,float acceleration,
    float traction,const float input[3],const float normal[3],const float support[3],int climb)
{
    rf_physics_body_state value;volatile float delta[3]={0},desired[3],projected[3];uint32_t i;
    if(!state || !input || !normal || !support || !isfinite(dt) || dt<0 ||
       !isfinite(speed) || speed<=0 || !isfinite(acceleration) || acceleration<=0 ||
       !isfinite(traction) || traction<=0 || !isfinite(state->mass) || state->mass<=0)return RF_RANGE;
    value=*state;
    for(i=0;i<3;++i)if(!isfinite(input[i]) || !isfinite(normal[i]) || !isfinite(support[i]) ||
        !isfinite(value.velocity[i]) || !isfinite(value.position[i]) || !isfinite(value.vector_e0[i]))return RF_RANGE;
    if(!(value.flags&0x1000000)) {
        volatile float ratio=(float)((double)speed/acceleration);
        volatile float blend=climb?climb_blend(speed,acceleration,dt):run_blend(ratio,traction,dt);
        double length;for(i=0;i<3;++i)desired[i]=input[i];
        if(normal[1]!=0) {
            volatile float dot=(float)(((double)input[0]*normal[0]+(double)input[1]*normal[1])+(double)input[2]*normal[2]);
            volatile float y;
            for(i=0;i<3;++i) {volatile float component=(float)((double)normal[i]*dot);projected[i]=(float)((double)input[i]-component);}
            y=(float)((double)projected[1]/normal[1]);
            if(y>=0) {
                volatile float factor=(float)(1.0-(double)y*y);
                for(i=0;i<3;++i)desired[i]=(float)((double)projected[i]*factor);
            }
        }
        length=sqrt(((double)desired[0]*desired[0]+(double)desired[1]*desired[1])+(double)desired[2]*desired[2]);
        if(length>1) {
            double reciprocal=1.0/(float)length;
            for(i=0;i<3;++i)desired[i]=(float)((double)desired[i]*reciprocal);
        }
        for(i=0;i<3;++i) {
            volatile float target=(float)((double)desired[i]*speed),difference=(float)((double)target-value.velocity[i]);
            volatile float force=(float)((double)value.vector_e0[i]/value.mass),impulse=(float)((double)force*dt);
            delta[i]=(float)((double)difference*blend);delta[i]=(float)((double)delta[i]+impulse);
            value.velocity[i]=(float)((double)value.velocity[i]+delta[i]);
        }
    }
    for(i=0;i<3;++i) {
        volatile float half=(float)((double)dt*dt*.5),correction=(float)((double)delta[i]*half);
        volatile float combined=(float)((double)value.velocity[i]+support[i]),travel=(float)((double)combined*dt);
        volatile float position=(float)((double)value.position[i]+travel);
        value.next_position[i]=(float)((double)position-correction);
        if(!isfinite(value.velocity[i]) || !isfinite(value.next_position[i]))return RF_RANGE;
    }
    *state=value;return RF_OK;
}

int rf_physics_run_propose(rf_physics_body_state *state,float dt,float speed,float acceleration,
    float traction,const float input[3],const float normal[3],const float support[3])
{return driven_propose(state,dt,speed,acceleration,traction,input,normal,support,0);}
int rf_physics_climb_propose(rf_physics_body_state *state,float dt,float speed,float acceleration,
    const float input[3],const float support[3])
{
    const float normal[3]={0};
    return driven_propose(state,dt,speed,acceleration,1,input,normal,support,1);
}
