#include "rf/physics.h"
#include "rf/collision.h"
#include "rf/level.h"
#include "rf/effect.h"
#include "rf/entity.h"
#include "rf/geomod.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
int rf_physics_body_segment(const rf_physics_body *body,const float start[3],
    const float delta[3],float limit,float *fraction)
{
    double a=0,nearest=limit;uint32_t i,k;int hit=0;
    if(!body || !start || !delta || !fraction || !isfinite(limit) || limit<0 || limit>1 ||
        (body->spheres.count && !body->spheres.items))return 0;
    for(k=0;k<3;k++){if(!isfinite(start[k]) || !isfinite(delta[k]))return 0;a+=(double)delta[k]*delta[k];}
    for(i=0;i<body->spheres.count;i++) {
        const rf_physics_sphere *s=body->spheres.items+i;double b=0,c=-(double)s->radius*s->radius,t;
        for(k=0;k<3;k++) {
            float center=(float)((double)body->state.position[k]+(double)s->center[0]*body->state.orientation[k]+
                (double)s->center[1]*body->state.orientation[3+k]+(double)s->center[2]*body->state.orientation[6+k]);
            double offset=(double)start[k]-center;b+=offset*delta[k];c+=offset*offset;
        }
        if(c<=0)t=0;
        else {double discriminant=b*b-a*c;if(a==0 || b>=0 || discriminant<0)continue;
            /* Stable near root avoids cancellation for distant/small targets. */
            t=c/(-b+sqrt(discriminant));}
        if(t<=nearest){nearest=t;hit=1;}
    }
    if(hit)*fraction=(float)nearest;return hit;
}
int rf_physics_solid_mass_prepare(const rf_collision_face *faces,uint32_t count,
    const float minimum[3],const float maximum[3],float density,rf_physics_solid_mass *result)
{
    rf_physics_solid_mass out={0};float extent[3],half,base,first,tensor[9]={0};
    uint32_t i,j,k,x,y,z;volatile float px,py,cell_mass;
    /* 4d1a80/87 round products through 40a070; 4d1a90/40a350 stores
     * each accumulated component. Preserve these stores on x87 too. */
    volatile float center_sum[3]={0};
    if(!faces || !count || !minimum || !maximum || !result || !isfinite(density) || density<0)return RF_RANGE;
    for(k=0;k<3;k++) {
        if(!isfinite(minimum[k]) || !isfinite(maximum[k]) || maximum[k]<=minimum[k])return RF_RANGE;
        extent[k]=(float)((double)maximum[k]-minimum[k]);
        if(!isfinite(extent[k]))return RF_RANGE;
        if(extent[k]>out.spacing)out.spacing=extent[k];
    }
    out.spacing=(float)((double)out.spacing*.25);half=(float)((double)out.spacing*.5);
    if(half<=0)return RF_RANGE;
    base=(float)((double)half*-4);first=(float)((double)half*-3);
    cell_mass=(float)((double)out.spacing*out.spacing*out.spacing*density);
    if(!isfinite(cell_mass))return RF_RANGE;
    for(x=0,px=first;px<maximum[0];x++,px=(float)((double)px+out.spacing)) {
        if(x==4)return RF_RANGE;
        for(y=0,py=first;py<maximum[1];y++,py=(float)((double)py+out.spacing)) {
            float start[3]={px,py,base},end[3]={px,py,0},delta[3]={0,0,0};
            uint8_t *row=out.cells+16*x+4*y;
            if(y==4)return RF_RANGE;
            end[2]=(float)((double)out.spacing*4+base);delta[2]=(float)((double)end[2]-base);
            for(i=0;i<count;i++) {
                float scratch[3],hit[4],scaled;uint32_t accepted,cell,bit;int status;
                status=rf_collision_segment_box(faces[i].minimum,faces[i].maximum,start,end,scratch,&accepted);
                if(status)return status;if(!accepted)continue;
                if(!rf_collision_model_ray_plane(start,delta,faces[i].plane,hit))continue;
                status=rf_collision_polygon_contains(faces[i].plane,hit,faces[i].vertices,faces[i].count,&accepted);
                if(status)return status;if(!accepted)continue;
                scaled=(float)((double)hit[3]*4);cell=(uint32_t)floor((double)hit[3]*4);
                bit=(uint32_t)floor(((double)scaled-cell)*4);
                /* The original assumes padded bounds keep hits below the top endpoint. */
                if(cell>=4 || bit>=4)return RF_RANGE;
                row[cell]|=(uint8_t)(1u<<(bit+(faces[i].plane[2]>0?4:0)));
            }
            for(z=0;z<4;z++)for(j=1;j<=8;j<<=1)if((row[z]&j) && !(row[z]&(j<<4))) {
                if(j<8)row[z]|=(uint8_t)(j<<1);else if(z<3)row[z+1]|=1;
            }
            for(z=0;z<4;z++) {
                uint32_t bits=row[z]&15,n=(bits&1)+((bits>>1)&1)+((bits>>2)&1)+((bits>>3)&1);
                volatile float p[3]={px,py,0};double fraction=n*.25;
                if(!n)continue;
                p[2]=(float)((double)(2*z+1)*half+base);
                out.mass=(float)((double)out.mass+fraction*cell_mass);
                for(k=0;k<3;k++) {
                    /* Preserve both original float stores before accumulation;
                     * x87 can otherwise retain the first product across casts. */
                    volatile float weighted=(float)((double)p[k]*cell_mass);
                    weighted=(float)((double)weighted*fraction);
                    center_sum[k]=(float)((double)center_sum[k]+weighted);
                }
            }
        }
    }
    for(k=0;k<3;k++) {
        out.center[k]=out.mass>0?(float)((double)center_sum[k]/out.mass):center_sum[k];
        out.origin[k]=(float)(((double)base+half)-out.center[k]);
    }
    for(i=0;i<64;i++) {
        uint32_t bits=out.cells[i]&15,n=(bits&1)+((bits>>1)&1)+((bits>>2)&1)+((bits>>3)&1);
        float p[3],yy,xx,mz,xz;double m,zz,xy;
        if(!n)continue;m=n*.25*cell_mass;
        for(k=0;k<3;k++)p[k]=(float)((double)((i>>(4-2*k))&3)*out.spacing+out.origin[k]);
        zz=(double)p[2]*p[2];yy=(float)((double)p[1]*p[1]);xx=(float)((double)p[0]*p[0]);
        tensor[0]=(float)((double)tensor[0]+m*((double)p[1]*p[1]+zz));
        xy=m*p[1]*p[0];tensor[1]=(float)((double)tensor[1]-xy);tensor[3]=(float)((double)tensor[3]-xy);
        mz=(float)(m*p[2]);xz=(float)(m*p[2]*p[0]);
        tensor[2]=(float)((double)tensor[2]-xz);tensor[6]=(float)((double)tensor[6]-xz);
        tensor[4]=(float)((double)tensor[4]+m*((double)p[0]*p[0]+zz));
        tensor[5]=(float)((double)tensor[5]-(double)mz*p[1]);tensor[7]=(float)((double)tensor[7]-(double)mz*p[1]);
        tensor[8]=(float)((double)tensor[8]+m*((double)xx+yy));
    }
    if(out.mass<=0)out.mass=(float)((double)extent[0]*extent[1]*extent[2]*density*.5);
    if(!isfinite(out.mass))return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(out.center[k]) || !isfinite(out.origin[k]))return RF_RANGE;
    {int status=rf_physics_tensor_inverse(tensor,out.inverse_tensor);if(status)return status;}
    *result=out;return RF_OK;
}

int rf_physics_grid_spheres(const uint8_t cells[64],float spacing,const float origin[3],
    rf_physics_sphere *spheres,uint32_t capacity,uint32_t *count,float *radius)
{
    rf_physics_sphere output[64]={{0}};uint32_t i,k,n=0;float half,maximum=0,bound;
    if(!cells || !origin || !spheres || !count || !radius || !isfinite(spacing) || spacing<=0)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(origin[k]))return RF_RANGE;
    half=(float)((double)spacing*.5);
    for(i=0;i<64;i++) {
        uint32_t bits=cells[i]&15,occupancy=(bits&1)+((bits>>1)&1)+((bits>>2)&1)+((bits>>3)&1);
        double squared;rf_physics_sphere *sphere;
        if(occupancy<2)continue;
        if(n==capacity)return RF_RANGE;
        sphere=output+n++;
        for(k=0;k<3;k++) {
            uint32_t coordinate=(i>>(4-2*k))&3;
            sphere->center[k]=(float)((double)coordinate*spacing+origin[k]);
            if(!isfinite(sphere->center[k]))return RF_RANGE;
        }
        sphere->radius=(float)((double)half*(occupancy*.25));sphere->parameter_10=-1;
        squared=(((double)sphere->center[0]*sphere->center[0]+(double)sphere->center[1]*sphere->center[1])+
            (double)sphere->center[2]*sphere->center[2])+(double)sphere->radius*sphere->radius;
        if(squared>maximum)maximum=(float)squared;
    }
    bound=(float)sqrt((double)maximum);if(!isfinite(bound))return RF_RANGE;
    memcpy(spheres,output,n*sizeof(*spheres));*count=n;*radius=bound;return RF_OK;
}

int rf_physics_creation_body_open(const rf_physics_creation_seed *seed,float elasticity,float friction,
    float density,uint32_t budget,rf_physics_body *result)
{
    const float zero[3]={0};
    return rf_physics_creation_body_open_moving(seed,zero,zero,elasticity,friction,density,budget,result);
}
int rf_physics_creation_body_open_moving(const rf_physics_creation_seed *seed,
    const float velocity[3],const float angular[3],float elasticity,float friction,
    float density,uint32_t budget,rf_physics_body *result)
{
    rf_physics_body_parameters parameters={0};rf_physics_mass_tensor initial={0},prepared;
    rf_physics_sphere fallback={0};rf_physics_fallback values;
    const rf_physics_sphere *spheres;uint32_t count;int status;
    if(!seed || !velocity || !angular || !result)return RF_RANGE;
    count=(seed->flags&0x70)?seed->sphere_count:0;
    if(count && !seed->spheres)return RF_RANGE;
    if(result->allocated_bytes || result->spheres.items || result->spheres.count || result->spheres.allocated_bytes ||
       sizeof(*result)+(uint64_t)((seed->flags&0x70)?(count?count:1):0)*sizeof(*spheres)>budget)return RF_RANGE;
    memcpy(&parameters.coefficients[1],&seed->word_0c,4);memcpy(&parameters.mass,&seed->word_14,4);
    parameters.coefficients[0]=elasticity;parameters.coefficients[2]=friction;parameters.flags=seed->flags;
    memcpy(parameters.position,seed->position,12);memcpy(parameters.orientation,seed->basis,36);
    memcpy(parameters.velocity,velocity,12);memcpy(parameters.vector_78,angular,12);
    spheres=count?seed->spheres:NULL;
    if((seed->flags&0x70) && !count) {
        status=rf_physics_fallback_prepare(density,seed->radius,parameters.mass,&values);if(status)return status;
        parameters.mass=values.mass;fallback.radius=values.radius;fallback.parameter_10=values.parameter_10;
        parameters.local_tensor[0]=parameters.local_tensor[4]=parameters.local_tensor[8]=1;
        spheres=&fallback;count=1;
    } else if(count && parameters.mass<=0) {
        initial.mass=parameters.mass;
        status=rf_physics_spheres_prepare(spheres,count,density,&initial,&prepared);if(status)return status;
        parameters.mass=prepared.mass;memcpy(parameters.local_tensor,prepared.tensor,36);
    }
    return rf_physics_body_open(&parameters,spheres,count,budget,result);
}
int rf_physics_body_prepare_contact(rf_physics_body_state *state)
{
    if(!state)return RF_RANGE;
    state->scalar_144=1;state->reference_15c=-1;
    memset(state->vector_e0,0,sizeof(state->vector_e0));
    memset(state->vector_ec,0,sizeof(state->vector_ec));
    state->flags|=0x01000000u;
    return RF_OK;
}
int rf_physics_body_prepare_sweep(rf_physics_body_state *state)
{
    float minimum[3],maximum[3],radius;uint32_t i;
    if(!state || !isfinite(state->bounds.radius))return RF_RANGE;
    radius=state->bounds.radius;
    for(i=0;i<3;++i) {
        float a=state->position[i],b=state->next_position[i],low,high;
        if(!isfinite(a) || !isfinite(b))return RF_RANGE;
        low=a<b?a:b;high=a<b?b:a;
        minimum[i]=(float)((double)low-radius);maximum[i]=(float)((double)radius+high);
        if(!isfinite(minimum[i]) || !isfinite(maximum[i]))return RF_RANGE;
    }
    memcpy(state->bounds.minimum,minimum,12);memcpy(state->bounds.maximum,maximum,12);
    return rf_physics_body_prepare_contact(state);
}
int rf_physics_surface_probe_gate(float up_y,uint32_t flags,int32_t *surface)
{
    if(!surface)return 0;
    if(!(up_y>=0)){*surface=-1;return 0;}
    return up_y>=.85f && (*surface!=-1 || (flags&0x18000000u)!=0);
}
int rf_physics_surface_reset_gate(float field_1b0,uint32_t flags,int32_t *surface)
{
    if(!surface)return 0;
    if((field_1b0==0 || isnan(field_1b0)) && !(flags&0x10000000u)) {
        *surface=-1;return 1;
    }
    return 0;
}
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
int rf_physics_support_accept(rf_physics_body_state *state,const rf_physics_ground_probe *probe,
    float fraction,uint32_t moving,float contact_y,uint32_t object_handle,int32_t material,
    rf_physics_support_contact *support,float published[3])
{
    rf_physics_support_contact value;int status;
    if(!support || !published)return RF_RANGE;
    status=rf_physics_support_commit(state,probe,fraction,moving,contact_y,object_handle,&value.handle);
    if(status)return status;
    memcpy(published,state->position,12);
    value.material=material;*support=value;return RF_OK;
}
int rf_physics_support_finish(rf_physics_support_actor *actor,const rf_physics_ground_probe *probe,
    const rf_collision_actor_contact *contact,const rf_physics_support_backend *backend)
{
    rf_physics_support_object object={0};double up;int route,status;uint32_t falling;
    if(!actor || !actor->body || !actor->extra || !actor->support || !actor->published || !probe || !contact ||
        !backend || !backend->lookup || !backend->fall || !backend->impact || !backend->land || !backend->relative)return RF_RANGE;
    up=((double)contact->normal[2]*0+(double)contact->normal[1])+(double)contact->normal[0]*0;
    if(!(contact->time>=1) && up>=.5) {
        status=backend->lookup(backend->context,contact->handle,&object);if(status)return status;
    }
    falling=rf_entity_falling((int32_t)actor->mode,actor->use_kind,actor->support->material);
    route=rf_entity_support_contact_route(contact->time,up,object.present,object.type,object.body_flags,falling);
    if(route==RF_ENTITY_CONTACT_FALL)return backend->fall(backend->context,actor);
    if(route==RF_ENTITY_CONTACT_NONE)return RF_OK;
    status=rf_physics_support_accept(actor->body,probe,contact->time,object.present,contact->velocity[1],object.handle,
        (int32_t)contact->material,actor->support,actor->published);if(status)return status;
    status=rf_collision_contact_write(actor->body,actor->extra,contact);if(status)return status;
    if(rf_entity_falling((int32_t)actor->mode,actor->use_kind,actor->support->material)) {
        float impact=(float)-(((double)actor->body->velocity[2]*contact->normal[2]+(double)actor->body->velocity[1]*contact->normal[1])+
            (double)actor->body->velocity[0]*contact->normal[0]);
        status=backend->impact(backend->context,actor,impact);if(status)return status;
        status=backend->land(backend->context,actor);if(status)return status;
    }
    if(contact->word_1f0 && (contact->handle&0x80000000u)) {
        uint32_t value;status=backend->relative(backend->context,contact->word_1f0,contact,&value);if(status)return status;
        actor->relative_contact=value;
    }
    return RF_OK;
}
void rf_physics_support_refresh(uint32_t mode,const float resolved_velocity[3],
    float cached_velocity[3],uint32_t *body_flags,uint32_t *object_flags)
{
    if((mode!=1 && mode!=3) || !resolved_velocity)return;
    memmove(cached_velocity,resolved_velocity,12);
    *body_flags|=0x80000000u;*object_flags|=0x06000000u;
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
int rf_physics_weapon_contact_advance(rf_physics_body_state *state,float dt,float fraction,float *remaining)
{
    rf_physics_body_state value;float ignored;volatile float consumed;int status;
    if(!state || !remaining)return RF_RANGE;
    value=*state;
    status=rf_physics_contact_advance(&value,dt,fraction,&ignored);if(status)return status;
    /* 4a01b0 stores consumed time before invoking the weapon callback. The
     * older 49ffd2 translation path retains its distinct x87 expression. */
    consumed=(float)((double)dt*fraction);
    *remaining=dt-consumed;*state=value;return RF_OK;
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
int rf_physics_rotating_contact(rf_physics_body_state *state,const float point[3],
    const float normal[3],const float support_velocity[3],const float contact_velocity[3],
    int32_t sphere_count,float *impact_speed)
{
    float offset[3],local[3],angular[3],combined[3],correction[3],velocity[3],impact,scale,contact_dot;uint32_t i;
    if(!state || !point || !normal || !support_velocity || !contact_velocity || !impact_speed || (state->flags&0x80))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(state->orientation[i]))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(point[i]) || !isfinite(state->position[i]) || !isfinite(state->velocity[i]) ||
            !isfinite(state->vector_c8[i]) || !isfinite(normal[i]) || !isfinite(support_velocity[i]) || !isfinite(contact_velocity[i]))return RF_RANGE;
        offset[i]=(float)((long double)point[i]-state->position[i]);local[i]=state->vector_c8[i];
    }
    local[0]=(float)((long double)local[0]*-1.0f);
    for(i=0;i<3;++i)angular[i]=(float)(((long double)local[2]*state->orientation[6+i]+(long double)local[1]*state->orientation[3+i])+(long double)local[0]*state->orientation[i]);
    for(i=0;i<3;++i) {
        uint32_t j=(i+1)%3,k=(i+2)%3;
        float spin=(float)((long double)angular[j]*offset[k]-(long double)angular[k]*offset[j]);
        float sum=(float)((long double)spin+state->velocity[i]);combined[i]=(float)((long double)sum+support_velocity[i]);
        if(!isfinite(combined[i]))return RF_RANGE;
    }
    contact_dot=(float)player_contact_dot(contact_velocity,normal);
    impact=(float)((long double)contact_dot-player_contact_dot(combined,normal));scale=(float)((long double)impact*1.100000023841858f);
    if(!isfinite(impact) || !isfinite(scale))return RF_RANGE;
    for(i=0;i<3;++i){correction[i]=(float)((long double)scale*normal[i]);if(!isfinite(correction[i]))return RF_RANGE;}
    if(player_contact_dot(correction,normal)>0) {
        for(i=0;i<3;++i){velocity[i]=(float)((long double)state->velocity[i]+correction[i]);if(!isfinite(velocity[i]))return RF_RANGE;}
        memcpy(state->velocity,velocity,12);
        if(sphere_count>1)for(i=0;i<3;++i)state->vector_c8[i]=(float)((long double)state->vector_c8[i]*.5f);
    }
    *impact_speed=impact;return RF_OK;
}
int rf_physics_contact_process_sp(rf_physics_contact_actor *actor,const rf_physics_contact_backend *backend)
{
    rf_physics_contact_object object={0};rf_physics_contact_context input;rf_physics_contact_route route;
    float impact;int status;
    if(!actor || !actor->body || !actor->contact || !backend || !backend->lookup || !backend->crouch ||
        !backend->speed || !backend->motion || !backend->player_flag || !backend->crush || !backend->impact)return RF_RANGE;
    status=backend->lookup(backend->context,(uint32_t)actor->body->reference_15c,&object);if(status)return status;
    input.mode=actor->mode;input.object_present=object.present;input.object_radius=object.radius;
    input.inverse_mass=actor->contact->inverse_mass;memcpy(input.contact_velocity,actor->contact->velocity,12);
    memcpy(input.support_velocity,actor->support_velocity,12);input.field_964=actor->field_964;input.field_974=actor->field_974;input.actor_flags_810=actor->flags_810;
    status=rf_physics_contact_select(actor->body,&input,&route,&impact);if(status)return status;
    if(route==RF_PHYSICS_CONTACT_CRUSH) {
        rf_damage_request request={9999.0f,(uint32_t)actor->body->reference_15c,-1,0,UINT32_MAX,0};
        status=backend->crush(backend->context,actor,&request);if(status)return status;
        memset(actor->body->velocity,0,12);return RF_OK;
    }
    if(route==RF_PHYSICS_CONTACT_STANCE) {
        uint8_t *flag=NULL;
        status=backend->crouch(backend->context,actor);if(status)return status;
        status=backend->speed(backend->context,actor,0);if(status)return status;
        status=backend->motion(backend->context,actor,9,.25f);if(status)return status;
        status=backend->player_flag(backend->context,actor->handle,&flag);if(status)return status;
        if(flag)*flag=1;
        route=(actor->body->flags&0x80)?RF_PHYSICS_CONTACT_FLAG80:RF_PHYSICS_CONTACT_STATIC;
    }
    if(route==RF_PHYSICS_CONTACT_DYNAMIC)
        status=rf_physics_dynamic_contact(actor->body,actor->body->vector_138,actor->support_velocity,
            actor->contact->velocity,actor->mode,object.present,(object.flags>>3)&1,(actor->object_flags>>3)&1,&impact);
    else if(route==RF_PHYSICS_CONTACT_FLAG80)
        status=rf_physics_player_contact(actor->body,actor->body->vector_138,actor->support_velocity,
            actor->contact->velocity,actor->direction,actor->mode,
            rf_entity_falling((int32_t)actor->mode,actor->use_kind,actor->support_material),&impact);
    else if(route==RF_PHYSICS_CONTACT_STATIC) {
        if(actor->use_kind==1)status=rf_physics_rotating_contact(actor->body,actor->contact->point,actor->body->vector_138,
            actor->support_velocity,actor->contact->velocity,actor->sphere_count,&impact);
        else status=rf_physics_static_contact(actor->body,actor->body->vector_138,actor->support_velocity,actor->contact->velocity,&impact);
    }
    if(status)return status;return backend->impact(backend->context,actor,impact);
}
int rf_physics_contact_select(rf_physics_body_state *state,
    const rf_physics_contact_context *context,rf_physics_contact_route *route,float *impact)
{
    rf_physics_contact_route selected;uint32_t i;
    if(!state || !context || !route || !impact)return RF_RANGE;
    if(state->word_164) {
        float velocity[3];
        for(i=0;i<3;++i){if(!isfinite(state->velocity[i]))return RF_RANGE;velocity[i]=(float)((long double)state->velocity[i]*.8500000238418579f);}
        memcpy(state->velocity,velocity,12);state->word_164=0;state->state_124&=~0x1000u;
        *impact=velocity[1];*route=RF_PHYSICS_CONTACT_DAMPED;return RF_OK;
    }
    if(!isfinite(context->inverse_mass))return RF_RANGE;
    if(context->inverse_mass>0)selected=RF_PHYSICS_CONTACT_DYNAMIC;
    else {
        selected=(state->flags&0x80)?RF_PHYSICS_CONTACT_FLAG80:RF_PHYSICS_CONTACT_STATIC;
        if(context->mode==1 && context->object_present) {
            if(!isfinite(context->object_radius) || !isfinite(state->vector_138[1]) || !isfinite(context->contact_velocity[1]))return RF_RANGE;
            if(context->object_radius>1 && state->vector_138[1]<-.5f && context->contact_velocity[1]<0) {
                float delta[3];long double squared;
                for(i=0;i<3;++i) {
                    if(!isfinite(context->contact_velocity[i]) || !isfinite(context->support_velocity[i]))return RF_RANGE;
                    delta[i]=(float)((long double)context->contact_velocity[i]-context->support_velocity[i]);
                    if(!isfinite(delta[i]))return RF_RANGE;
                }
                squared=((long double)delta[0]*delta[0]+(long double)delta[1]*delta[1])+(long double)delta[2]*delta[2];
                if(squared>.10000000149011612f)selected=(context->field_974!=-1 || context->field_964!=-1) &&
                    !(context->actor_flags_810&0x400)?RF_PHYSICS_CONTACT_STANCE:RF_PHYSICS_CONTACT_CRUSH;
            }
        }
    }
    *route=selected;*impact=0;return RF_OK;
}
int rf_physics_dynamic_contact(rf_physics_body_state *state,float normal[3],
    const float support_velocity[3],const float contact_velocity[3],uint32_t mode,
    uint32_t object_present,uint32_t object_player,uint32_t actor_player,float *impact_speed)
{
    float n[3],relative[3],velocity[3],actor_dot,contact_dot,impact,scale;uint32_t i;
    if(!state || !normal || !support_velocity || !contact_velocity || !impact_speed)return RF_RANGE;
    if(!object_present || ((object_player&255) && !(actor_player&255))){*impact_speed=0;return RF_OK;}
    for(i=0;i<3;++i)if(!isfinite(normal[i]) || !isfinite(state->velocity[i]) ||
        !isfinite(support_velocity[i]) || !isfinite(contact_velocity[i]))return RF_RANGE;
    memcpy(n,normal,12);
    if(mode==1 && n[1]!=0 && n[1]<.95f && n[1]>-.95f) {
        long double length;n[1]=0;
        length=sqrtl(((long double)n[0]*n[0]+(long double)n[1]*n[1])+(long double)n[2]*n[2]);
        if(length>0)for(i=0;i<3;++i)n[i]=(float)((1/length)*n[i]);
        else {n[0]=1;n[1]=n[2]=0;}
    }
    actor_dot=(float)player_contact_dot(state->velocity,n);
    actor_dot=0<actor_dot?0:actor_dot;
    for(i=0;i<3;++i)relative[i]=(float)((long double)contact_velocity[i]-support_velocity[i]);
    contact_dot=(float)player_contact_dot(relative,n);contact_dot=0>contact_dot?0:contact_dot;
    impact=(float)((long double)contact_dot-actor_dot);scale=(float)((long double)impact*1.0499999523162842f);
    if(!isfinite(impact) || !isfinite(scale))return RF_RANGE;
    for(i=0;i<3;++i) {
        float correction=(float)((long double)scale*n[i]);
        velocity[i]=(float)((long double)state->velocity[i]+correction);if(!isfinite(velocity[i]))return RF_RANGE;
    }
    memcpy(state->velocity,velocity,12);memcpy(normal,n,12);*impact_speed=impact;return RF_OK;
}
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
int rf_physics_forces_set_state(rf_physics_force_region *regions,uint32_t count,
    const uint32_t *uids,uint32_t uid_count,uint32_t action)
{
    uint32_t i,j;
    if((count && !regions) || (uid_count && !uids) || action>1)return RF_RANGE;
    for(i=0;i<uid_count;++i)for(j=0;j<count;++j)if(regions[j].uid==uids[i]) {
        regions[j].active=(regions[j].active&0xffffff00u)|action;break;
    }
    return RF_OK;
}
void rf_physics_forces_close(rf_physics_force_collection *forces)
{
    if(forces) {free(forces->items);memset(forces,0,sizeof(*forces));}
}
int rf_physics_forces_open(const rf_level *level,uint32_t budget,rf_physics_force_collection *result)
{
    rf_physics_force_collection value={0};rf_level_force_reader reader;rf_level_force_region record;
    uint64_t bytes;uint32_t i;int status;
    if(!level || !result || result->items || result->count || result->allocated_bytes)return RF_RANGE;
    status=rf_level_forces_begin(level,&reader);
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    value.count=status==RF_NOT_FOUND?0:reader.count;
    bytes=sizeof(value)+(uint64_t)value.count*sizeof(*value.items);if(bytes>budget)return RF_RANGE;
    value.allocated_bytes=(uint32_t)bytes;
    if(value.count) {
        value.items=calloc(value.count,sizeof(*value.items));if(!value.items)return RF_RANGE;
        for(i=0;i<value.count;++i) {
            status=rf_level_force_next(&reader,&record);if(!status)status=rf_physics_force_region_build(&record,value.items+i);
            if(status) {rf_physics_forces_close(&value);return status;}
        }
    }
    *result=value;return RF_OK;
}
int rf_physics_force_region_build(const rf_level_force_region *source,rf_physics_force_region *result)
{
    rf_physics_force_region value={0};float half[3];uint32_t i,j;
    if(!source || !result || source->shape<1 || source->shape>3 || !isfinite(source->strength))return RF_RANGE;
    value.shape=source->shape;value.uid=source->uid;value.flags=source->flags;
    value.strength=source->strength;value.active=1;
    for(i=0;i<9;++i) {
        if(!isfinite(source->orientation_disk[i]))return RF_RANGE;
        value.matrix[((i/3+2)%3)*3+i%3]=source->orientation_disk[i];
    }
    for(i=0;i<3;++i) {
        if(!isfinite(source->position[i]) || !isfinite(source->extent[source->shape==1?0:i]))return RF_RANGE;
        value.center[i]=source->position[i];
        half[i]=source->shape==1?source->extent[0]:(float)((double)source->extent[i]*.5);
        if(source->shape!=1)value.size[i]=source->extent[i];
        value.minimum[i]=(float)((double)value.center[i]-half[i]);
        value.maximum[i]=(float)((double)value.center[i]+half[i]);
    }
    value.radius_squared=source->shape==1?(float)((double)half[0]*half[0]):
        (float)((double)half[0]*half[0]+(double)half[1]*half[1]+(double)half[2]*half[2]);
    if(source->shape==3)for(i=0;i<3;++i) {
        value.minimum[i]=value.maximum[i]=value.center[i];
        for(j=0;j<3;++j) {
            volatile float low=(float)((double)value.matrix[i*3+j]*-half[j]);
            double high=(double)value.matrix[i*3+j]*half[j];
            double minimum=high<=low?high:low,maximum=high<=low?low:high;
            value.minimum[i]=(float)((double)value.minimum[i]+minimum);
            value.maximum[i]=(float)((double)value.maximum[i]+maximum);
        }
    }
    if(!isfinite(value.radius_squared))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(value.minimum[i]) || !isfinite(value.maximum[i]))return RF_RANGE;
    *result=value;return RF_OK;
}
int rf_physics_force_region_influence(const rf_physics_force_region *region,
    const float physics_position[3],float body_radius,float mass,rf_physics_force_influence *result)
{
    rf_physics_force_influence value;float delta[3];double squared,ratio;uint32_t i;
    if(!region || !physics_position || !result || !isfinite(region->strength))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(physics_position[i]) || !isfinite(region->center[i]))return RF_RANGE;
        delta[i]=(float)((double)physics_position[i]-region->center[i]);
        if(!isfinite(delta[i]))return RF_RANGE;
    }
    squared=(double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2];
    if(region->flags&0x10) {
        if(squared==0)return RF_RANGE;
        ratio=1/sqrt(squared);
        for(i=0;i<3;++i)value.direction[i]=(float)(ratio*delta[i]);
    } else for(i=0;i<3;++i)value.direction[i]=region->matrix[6+i];
    value.strength=region->strength;
    if(region->flags&12) {
        if(!isfinite(region->radius_squared) || region->radius_squared==0)return RF_RANGE;
        ratio=squared/region->radius_squared;
        if(!(region->flags&8))ratio=1-ratio;
        value.strength=(float)(ratio*value.strength);
    }
    if(!(region->flags&3)) {
        if(!isfinite(body_radius) || !isfinite(mass) || mass<=0)return RF_RANGE;
        ratio=(double)body_radius*body_radius/mass;
        if(ratio<1)value.strength=(float)(ratio*value.strength);
    }
    if(!isfinite(value.strength))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(value.direction[i]))return RF_RANGE;
    *result=value;return RF_OK;
}
int rf_physics_force_actor_carry(float support_velocity[3],uint32_t *body_flags,
    const rf_physics_force_influence *influence,uint32_t mode,uint32_t class_kind,int32_t attachment)
{
    float value[3];uint32_t i;double length;
    if(!support_velocity || !body_flags || !influence || !isfinite(influence->strength))return RF_RANGE;
    for(i=0;i<3;++i) {
        volatile float increment;float direction=influence->direction[i];
        if(!isfinite(support_velocity[i]) || !isfinite(direction))return RF_RANGE;
        if(i==1 && (mode==1 || mode==2))direction=0;
        increment=(float)((double)direction*influence->strength);
        value[i]=(float)((double)support_velocity[i]+increment);
    }
    if(mode==3 || mode==8 || (class_kind==1 && attachment==-1)) {
        length=sqrt((double)value[0]*value[0]+(double)value[1]*value[1]+(double)value[2]*value[2]);
        if(length>influence->strength) {
            volatile float scale=(float)((double)influence->strength/length);
            for(i=0;i<3;++i)value[i]=(float)((double)value[i]*scale);
        }
    }
    for(i=0;i<3;++i)if(!isfinite(value[i]))return RF_RANGE;
    memcpy(support_velocity,value,sizeof(value));*body_flags|=0x80000000u;return RF_OK;
}
int rf_physics_force_air_cap(const float velocity[3],float class_speed,
    float *alternate_cap,uint32_t *body_flags)
{
    double speed;float cap;
    if(!velocity || !alternate_cap || !body_flags || !isfinite(class_speed) || class_speed<0 ||
        !isfinite(velocity[0]) || !isfinite(velocity[1]) || !isfinite(velocity[2]))return RF_RANGE;
    speed=sqrt((double)velocity[0]*velocity[0]+(double)velocity[2]*velocity[2]);
    cap=speed>class_speed?(float)(speed+1):class_speed;
    if(!isfinite(cap))return RF_RANGE;
    *alternate_cap=cap;*body_flags|=0x200000;return RF_OK;
}
int rf_physics_force_turbulence(rf_physics_force_influence *influence,uint32_t flags,
    float dt,rf_random_state *random,float *shake_amplitude)
{
    uint32_t amount=(flags>>16)&15;float amplitude,cosine,direction[3];rf_random_state next;int status;
    if(!influence || !random || !shake_amplitude)return RF_RANGE;
    if(!amount) {*shake_amplitude=0;return RF_OK;}
    if(!isfinite(dt) || dt<0 || !isfinite(influence->strength))return RF_RANGE;
    amplitude=dt==0?0:(float)fabs(((double)amount/(150.0/dt))*influence->strength);
    if(!isfinite(amplitude))return RF_RANGE;
    cosine=(float)(1.0-amplitude);if(cosine < -1)cosine=-1;if(cosine>1)cosine=1;
    next=*random;status=rf_particle_cone_oriented(influence->direction,cosine,&next,direction);if(status)return status;
    memcpy(influence->direction,direction,sizeof(direction));*random=next;*shake_amplitude=amplitude;return RF_OK;
}
uint32_t rf_physics_force_eligible(uint32_t body_flags,uint32_t region_present,
    uint32_t region_flags,uint32_t local_related,uint32_t actor_present,uint32_t actor_mode)
{
    if(!(body_flags&8) || region_present!=1 || local_related>1 || actor_present>1)return 0;
    if((region_flags&0x20) && local_related)return 0;
    if(!(region_flags&2))return 1;
    if(actor_present)return actor_mode==1 || actor_mode==5;
    return (body_flags&0x18000000)!=0;
}
int rf_physics_force_region_select(const rf_physics_force_region *regions,uint32_t count,
    const float position[3],uint32_t *index)
{
    uint32_t i,k,inside;int status;
    if((count && !regions) || !position || !index)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(position[k]))return RF_RANGE;
    for(i=0;i<count;i++) {
        const rf_physics_force_region *r=regions+i;
        if(!(r->active&255u))continue;
        inside=0;
        if(r->shape==1) {
            float delta[3],distance;
            if(!isfinite(r->radius_squared))return RF_RANGE;
            for(k=0;k<3;k++) {
                if(!isfinite(r->center[k]))return RF_RANGE;
                delta[k]=(float)((double)position[k]-r->center[k]);
            }
            distance=(float)(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
            inside=distance<r->radius_squared;
        } else if(r->shape==2) {
            inside=1;
            for(k=0;k<3;k++) {
                if(!isfinite(r->minimum[k]) || !isfinite(r->maximum[k]))return RF_RANGE;
                if(position[k]<r->minimum[k] || position[k]>r->maximum[k])inside=0;
            }
        } else if(r->shape==3) {
            status=rf_collision_point_oriented_box(position,r->center,(const float (*)[3])r->matrix,r->size,&inside);
            if(status)return status;
        }
        if(inside){*index=i;return RF_OK;}
    }
    *index=UINT32_MAX;return RF_OK;
}
int rf_physics_force_suppresses_damage(const rf_physics_force_region *regions,uint32_t count,
    const float position[3],uint32_t *suppressed)
{
    uint32_t i,index;int status;
    if((count && !regions) || !position || !suppressed)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(position[i]))return RF_RANGE;
    for(i=0;i<count;++i) {
        if((regions[i].active&255u)!=1 || !(regions[i].flags&0x40u))continue;
        status=rf_physics_force_region_select(regions+i,1,position,&index);if(status)return status;
        if(index==0){*suppressed=1;return RF_OK;}
    }
    *suppressed=0;return RF_OK;
}
int rf_physics_air_steer(rf_physics_body_state *state,float dt,float air_control,
    float acceleration_limit,float speed_limit,const float world_acceleration[3])
{
    float desired[3],horizontal[2],length;double magnitude;uint32_t i;
    volatile float scale;
    if(!state || !world_acceleration || !isfinite(dt) || dt<0 ||
       !isfinite(air_control) || air_control<0 || !isfinite(acceleration_limit) || acceleration_limit<0 ||
       !isfinite(speed_limit) || speed_limit<0)return RF_RANGE;
    if(state->flags&0x1000000)return RF_OK;
    for(i=0;i<3;i++) {
        if(!isfinite(world_acceleration[i]) || !isfinite(state->velocity[i]))return RF_RANGE;
        desired[i]=world_acceleration[i];
    }
    magnitude=sqrt(((double)desired[0]*desired[0]+(double)desired[1]*desired[1])+(double)desired[2]*desired[2]);
    if(magnitude>acceleration_limit) {
        scale=(float)((double)acceleration_limit/magnitude);
        for(i=0;i<3;i++)desired[i]=(float)((double)desired[i]*scale);
    }
    scale=(float)((double)air_control*dt);
    for(i=0;i<3;i++)desired[i]=(float)((double)desired[i]*scale);
    horizontal[0]=(float)((double)desired[0]+state->velocity[0]);
    horizontal[1]=(float)((double)desired[2]+state->velocity[2]);
    length=(float)sqrt((double)horizontal[0]*horizontal[0]+(double)horizontal[1]*horizontal[1]);
    if(length>speed_limit) {
        double ratio=(double)speed_limit/length;
        for(i=0;i<2;i++)horizontal[i]=(float)((double)horizontal[i]*ratio);
    }
    if(!isfinite(horizontal[0]) || !isfinite(horizontal[1]))return RF_RANGE;
    state->velocity[0]=horizontal[0];state->velocity[2]=horizontal[1];return RF_OK;
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
int rf_physics_solid_propose(rf_physics_body_state *state,float dt,float gravity,
    uint32_t object_flags,float acceleration[3])
{
    float velocity[3],position[3],a[3],midpoint[3];uint32_t i,pass;
    volatile float half_dt_squared;
    if(!state || !acceleration || !isfinite(dt) || dt<0 || !isfinite(gravity) ||
       !isfinite(state->mass) || state->mass<=0 || !isfinite(state->coefficients[1]))return RF_RANGE;
    half_dt_squared=(float)((double)dt*dt*.5);
    if(!isfinite(half_dt_squared))return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(state->position[i]) || !isfinite(state->velocity[i]) ||
           !isfinite(state->vector_e0[i]) || !isfinite(acceleration[i]))return RF_RANGE;
        velocity[i]=midpoint[i]=state->velocity[i];a[i]=acceleration[i];
    }
    if(!(state->flags&0x1000000))for(pass=0;pass<2;pass++) {
        float drag=0;
        if((state->flags&2) && (object_flags&0x80000)) {
            long double speed=sqrtl(((long double)midpoint[0]*midpoint[0]+(long double)midpoint[1]*midpoint[1])+(long double)midpoint[2]*midpoint[2]);
            drag=(float)(speed*state->coefficients[1]*2);
            if(!isfinite(drag))return RF_RANGE;
        }
        for(i=0;i<3;i++) {
            volatile float resistance=(float)((double)drag*midpoint[i]);
            volatile float force=(float)((double)state->vector_e0[i]-resistance),increment;
            a[i]=(float)((double)force/state->mass);
            if(i==1 && (state->flags&1))a[i]=(float)((double)a[i]-gravity);
            increment=(float)((double)a[i]*dt);
            if(!pass){volatile float half=(float)((double)increment*.5);midpoint[i]=(float)((double)velocity[i]+half);}
            else velocity[i]=(float)((double)velocity[i]+increment);
            if(!isfinite(a[i]) || !isfinite(midpoint[i]) || !isfinite(velocity[i]))return RF_RANGE;
        }
    }
    for(i=0;i<3;i++) {
        volatile float travel=(float)((double)velocity[i]*dt),base=(float)((double)state->position[i]+travel);
        volatile float correction=(float)((double)a[i]*half_dt_squared);
        position[i]=(float)((double)base-correction);if(!isfinite(position[i]))return RF_RANGE;
    }
    memcpy(state->velocity,velocity,12);memcpy(state->next_position,position,12);memcpy(acceleration,a,12);return RF_OK;
}
static long double solid_length(const float v[3])
{return sqrtl(((long double)v[0]*v[0]+(long double)v[1]*v[1])+(long double)v[2]*v[2]);}
static void solid_transform(const float m[9],const float v[3],float out[3])
{uint32_t i;for(i=0;i<3;i++)out[i]=(float)(((long double)v[2]*m[i*3+2]+(long double)v[1]*m[i*3+1])+(long double)v[0]*m[i*3]);}
static void solid_cross(const float a[3],const float b[3],float out[3])
{uint32_t i;for(i=0;i<3;i++){uint32_t j=(i+1)%3,k=(i+2)%3;out[i]=(float)((long double)a[j]*b[k]-(long double)a[k]*b[j]);}}
int rf_physics_solid_angular_propose(rf_physics_body_state *state,float dt)
{
    float momentum[3],angular[3],delta[3],axis[3],basis[9],next[9],angle;long double length;uint32_t i;int status;
    if(!state || !isfinite(dt) || dt<0 || !isfinite(state->coefficients[1]))return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(state->world_tensor[i]) || !isfinite(state->orientation[i]))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(state->vector_c8[i]) || !isfinite(state->mass_vector_d4[i]) || !isfinite(state->vector_ec[i]))return RF_RANGE;
    memcpy(momentum,state->mass_vector_d4,12);
    if(!(state->flags&0x1000000)) {
        float drag=(state->flags&2)?(float)(solid_length(state->vector_c8)*state->coefficients[1]):0;
        if(!isfinite(drag))return RF_RANGE;
        for(i=0;i<3;i++) {
            volatile float resistance=(float)((double)drag*state->vector_c8[i]);
            volatile float torque=(float)((double)state->vector_ec[i]-resistance),step=(float)((double)torque*dt);
            momentum[i]=(float)((double)momentum[i]+step);if(!isfinite(momentum[i]))return RF_RANGE;
        }
    }
    solid_transform(state->world_tensor,momentum,angular);length=solid_length(angular);
    if(!isfinite(length))return RF_RANGE;
    if(length>15) {
        float scale=(float)(15/length);
        for(i=0;i<3;i++){angular[i]=(float)((double)angular[i]*scale);momentum[i]=(float)((double)momentum[i]*scale);}
    }
    for(i=0;i<3;i++)delta[i]=(float)((double)angular[i]*dt);
    solid_transform(state->orientation,delta,axis);angle=(float)solid_length(axis);
    if(!isfinite(angle))return RF_RANGE;
    if(angle>0)for(i=0;i<3;i++)axis[i]=(float)((double)axis[i]/angle);
    status=rf_geomod_debris_rotate(state->orientation,axis,angle,1,basis);if(status)return status;
    /*4fc960's ordinary valid-basis path: normalize forward/up, rebuild right/up. */
    length=solid_length(basis+6);if(!isfinite(length) || length<=0)return RF_RANGE;
    for(i=0;i<3;i++)next[6+i]=(float)((long double)basis[6+i]/length);
    length=solid_length(basis+3);if(!isfinite(length) || length<=0)return RF_RANGE;
    for(i=0;i<3;i++)next[3+i]=(float)((long double)basis[3+i]/length);
    solid_cross(next+3,next+6,next);
    if(solid_length(next)<=0)return RF_RANGE;
    solid_cross(next+6,next,next+3);
    for(i=0;i<9;i++)if(!isfinite(next[i]))return RF_RANGE;
    memcpy(state->mass_vector_d4,momentum,12);memcpy(state->vector_c8,angular,12);memcpy(state->next_orientation,next,36);return RF_OK;
}
int rf_physics_solid_advance(rf_physics_body_state *state,float dt,float fraction,
    float published_basis[9],float *remaining)
{
    rf_physics_body_state value;float left=0;uint32_t i;int status;
    if(!state || !published_basis || !remaining || !isfinite(dt) || dt<0 ||
       !isfinite(fraction) || fraction<0 || fraction>1 || (state->flags&0x4000))return RF_RANGE;
    value=*state;
    if(fraction<1) {
        status=rf_physics_weapon_contact_advance(&value,dt,fraction,&left);if(status)return status;
    } else {
        if(!isfinite(value.bounds.radius) || value.bounds.radius<0)return RF_RANGE;
        for(i=0;i<3;i++) {
            float p=value.next_position[i],r=value.bounds.radius;
            if(!isfinite(p))return RF_RANGE;
            value.position[i]=p;value.bounds.minimum[i]=p-r;value.bounds.maximum[i]=p+r;
            if(!isfinite(value.bounds.minimum[i]) || !isfinite(value.bounds.maximum[i]))return RF_RANGE;
        }
        value.scalar_144=1;
    }
    memcpy(value.orientation,value.next_orientation,36);
    status=rf_physics_tensor_world(value.local_tensor,value.orientation,value.world_tensor);if(status)return status;
    if(fraction==1)memcpy(published_basis,value.orientation,36);
    *state=value;*remaining=left;return RF_OK;
}
int rf_physics_solid_step(rf_physics_body_state *state,float dt,float gravity,
    uint32_t *object_flags,float published_position[3],float published_basis[9],
    rf_physics_solid_query_fn query,void *context,rf_physics_solid_step_report *report)
{
    rf_physics_body_state value;rf_physics_solid_step_report result={0};
    float acceleration[3]={0},position[3],basis[9],left=dt,g[3]={0,0,0};
    uint32_t flags;int status;
    if(!state || !object_flags || !published_position || !published_basis || !query || !report ||
       !isfinite(dt) || dt<0 || !isfinite(gravity))return RF_RANGE;
    if(dt==0 || !(state->flags&0x80000000u)){*report=result;return RF_OK;}
    if(state->flags&(0x4000|0x100))return RF_NOT_FOUND;
    value=*state;flags=*object_flags;memcpy(basis,published_basis,36);g[1]=-gravity;
    value.flags&=~0x01000000u;
    while(left>0 && result.steps<10) {
        rf_physics_solid_hit hit={0};rf_physics_solid_response response;uint32_t found=0;
        status=rf_physics_solid_propose(&value,left,gravity,flags,acceleration);if(status)return status;
        status=rf_physics_solid_angular_propose(&value,left);if(status)return status;
        status=rf_physics_body_prepare_sweep(&value);if(status)return status;
        if(value.position[0]!=value.next_position[0] || value.position[1]!=value.next_position[1] ||
           value.position[2]!=value.next_position[2]) {
            status=query(&value,&hit,&found,context);if(status)return status;
        }
        if(found && (!isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>=1))return RF_RANGE;
        status=rf_physics_solid_advance(&value,left,found?hit.fraction:1,basis,&left);if(status)return status;
        result.steps++;
        if(found) {
            status=rf_physics_solid_contact(&value,hit.point,hit.normal,g,hit.elasticity,hit.friction,&response);
            if(status)return status;
            result.contacts++;
            if(response==RF_SOLID_CONTACT_STOPPED){result.stopped=1;left=0;break;}
        }
    }
    result.remaining=left;result.limited=left>0;
    status=rf_physics_publish_position(&value,position,&flags);if(status)return status;
    *state=value;*object_flags=flags;memcpy(published_position,position,12);
    memcpy(published_basis,basis,36);*report=result;return RF_OK;
}
int rf_physics_solid_contact(rf_physics_body_state *state,const float point[3],
    const float normal[3],const float gravity[3],float elasticity,float friction,
    rf_physics_solid_response *response)
{
    rf_physics_body_state value;float r[3],spin[3],vp[3],tp[3],tangent[3],jn[3],impulse[3],a[3],b[3];
    float vn,gn,mu,e,j,budget,len,scale;long double dot,decay,denominator;uint32_t i;
    if(!state || !point || !normal || !gravity || !response)return RF_RANGE;
    value=*state;
    if(value.word_164){value.word_164=0;value.state_124&=~0x1000u;*state=value;*response=RF_SOLID_CONTACT_IGNORED;return RF_OK;}
    if(value.flags&0x200){*response=RF_SOLID_CONTACT_IGNORED;return RF_OK;}
    if(value.flags&(0x4000|0x100))return RF_NOT_FOUND;
    if(!isfinite(value.mass) || value.mass<=0 || !isfinite(elasticity) || elasticity<0 ||
       !isfinite(friction) || friction<0 || !isfinite(value.coefficients[0]) || value.coefficients[0]<0 ||
       !isfinite(value.coefficients[2]) || value.coefficients[2]<0)return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(value.world_tensor[i]))return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(point[i]) || !isfinite(normal[i]) || !isfinite(gravity[i]) || !isfinite(value.position[i]) ||
           !isfinite(value.velocity[i]) || !isfinite(value.vector_c8[i]) || !isfinite(value.mass_vector_d4[i]))return RF_RANGE;
        r[i]=(float)((double)point[i]-value.position[i]);
        spin[i]=(float)(((long double)value.mass_vector_d4[2]*value.world_tensor[6+i]+(long double)value.mass_vector_d4[1]*value.world_tensor[3+i])+(long double)value.mass_vector_d4[0]*value.world_tensor[i]);
    }
    solid_cross(spin,r,a);
    for(i=0;i<3;i++)vp[i]=(float)((double)value.velocity[i]+a[i]);
    dot=player_contact_dot(normal,vp);if(!isfinite(dot))return RF_RANGE;
    if(dot>0){*response=RF_SOLID_CONTACT_IGNORED;return RF_OK;}vn=(float)dot;
    for(i=0;i<3;i++){volatile float n=(float)((double)normal[i]*vn);tp[i]=(float)((double)vp[i]-n);}
    gn=(float)player_contact_dot(gravity,normal);mu=(float)(((double)friction+value.coefficients[2])*.5);
    e=(float)((double)elasticity*value.coefficients[0]);decay=(long double)value.coefficients[0]*.8f;
    value.coefficients[0]=(float)decay;
    if(decay<.05f || (player_contact_dot(value.velocity,value.velocity)<.25f && player_contact_dot(value.vector_c8,value.vector_c8)<.5f)) {
        value.flags=(value.flags&0x67ffffffu)|0x18000000u;
        memset(value.velocity,0,12);memset(value.vector_c8,0,12);memset(value.mass_vector_d4,0,12);
        *state=value;*response=RF_SOLID_CONTACT_STOPPED;return RF_OK;
    }
    solid_cross(r,normal,a);solid_transform(value.world_tensor,a,b);solid_cross(b,r,a);
    denominator=1/(long double)value.mass+player_contact_dot(normal,a);
    if(!isfinite(denominator) || denominator<=0)return RF_RANGE;
    j=(float)(-((1+(long double)e)*vn)/denominator);if(!isfinite(j))return RF_RANGE;
    for(i=0;i<3;i++)jn[i]=(float)((double)normal[i]*j);
    budget=(float)(-(long double)mu*value.mass*gn);
    dot=player_contact_dot(normal,value.velocity);vn=(float)dot;
    for(i=0;i<3;i++){volatile float n=(float)((double)normal[i]*vn);tangent[i]=(float)((double)value.velocity[i]-n);}
    len=(float)solid_length(tangent);memcpy(impulse,jn,12);
    if(len>0) {
        float cap=(float)((double)len*value.mass);if(!(budget<cap))budget=cap;
        scale=(float)(-(long double)budget/len);
        for(i=0;i<3;i++){volatile float f=(float)((double)tangent[i]*scale);impulse[i]=(float)((double)jn[i]+f);}
    }
    for(i=0;i<3;i++){volatile float change=(float)((double)impulse[i]/value.mass);value.velocity[i]=(float)((double)value.velocity[i]+change);}
    len=(float)solid_length(tp);memcpy(impulse,jn,12);
    if(len>0) {
        float cap=(float)((double)len*value.mass),amount=budget<cap?budget:cap;
        scale=(float)(-(long double)amount/len);
        for(i=0;i<3;i++){volatile float f=(float)((double)tp[i]*scale);impulse[i]=(float)((double)jn[i]+f);}
    }
    solid_cross(r,impulse,a);
    for(i=0;i<3;i++)value.mass_vector_d4[i]=(float)((double)value.mass_vector_d4[i]+a[i]);
    solid_transform(value.world_tensor,value.mass_vector_d4,value.vector_c8);
    for(i=0;i<3;i++)if(!isfinite(value.velocity[i]) || !isfinite(value.mass_vector_d4[i]) || !isfinite(value.vector_c8[i]))return RF_RANGE;
    if(!isfinite(value.coefficients[0]) || !isfinite(budget))return RF_RANGE;
    *state=value;*response=RF_SOLID_CONTACT_IMPULSE;return RF_OK;
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

int rf_physics_try_stand(rf_physics_spheres *spheres,const rf_physics_stance_cache *cache,
    const float published[3],uint32_t *actor_flags,const rf_physics_stand_ops *ops,
    void *context,int *stood)
{
    float end[3];uint32_t blocked,i,k,copy_flags=0;uint8_t *player;int status;
    if(!spheres || !cache || !actor_flags || !ops || !ops->clearance || !ops->refresh_ground ||
       !stood || spheres->count>8 || cache->count<spheres->count ||
       (spheres->count && !spheres->items))return RF_RANGE;
    for(i=0;i<spheres->count;++i)for(k=0;k<3;++k)
        if(!isfinite(cache->centers[0][i][k]))return RF_RANGE;
    status=rf_physics_stand_endpoint(published,cache->height_difference,end);if(status)return status;
    status=ops->clearance(context,published,end,&blocked);if(status)return status;
    if((uint8_t)blocked){*stood=0;return RF_OK;}
    *actor_flags&=~0x400u;
    player=ops->player_crouch?ops->player_crouch(context):NULL;
    if(player)*player=0;
    status=rf_physics_stance_centers(spheres,cache->centers[0],cache->count,&copy_flags,0);if(status)return status;
    status=ops->refresh_ground(context);if(status)return status;
    *stood=1;return RF_OK;
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

int rf_physics_publish_position(rf_physics_body_state *state,float published[3],uint32_t *object_flags)
{
    float minimum[3],maximum[3];uint32_t i;
    if(!state || !published || !object_flags)return RF_RANGE;
    if(!isfinite(state->bounds.radius))return RF_RANGE;
    for(i=0;i<3;++i) {
        float p=state->position[i],r=state->bounds.radius;
        if(!isfinite(p))return RF_RANGE;
        minimum[i]=r>0?p-r:p;maximum[i]=r>0?p+r:p;
        if(!isfinite(minimum[i]) || !isfinite(maximum[i]))return RF_RANGE;
    }
    memcpy(published,state->position,12);memcpy(state->next_position,state->position,12);
    memcpy(state->bounds.minimum,minimum,12);memcpy(state->bounds.maximum,maximum,12);
    *object_flags|=0x04000000u;state->flags&=~0x40000000u;
    return RF_OK;
}
