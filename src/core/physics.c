#include "rf/physics.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
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
        acceleration=(float)((double)state->vector_e0[i]/state->mass);
        if(i==1)acceleration=(float)((double)acceleration-gravity);
        increment=(float)((double)acceleration*dt);
        velocity[i]=(float)((double)state->velocity[i]+increment);
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
