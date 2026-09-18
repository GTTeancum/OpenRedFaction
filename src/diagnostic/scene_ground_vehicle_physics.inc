#ifndef RF_SCENE_GROUND_VEHICLE_PHYSICS_INCLUDED
#define RF_SCENE_GROUND_VEHICLE_PHYSICS_INCLUDED
#include "rf/vehicle_rigid.h"
#include "rf/vehicle_suspension.h"
#include "rf/collision.h"
#include <math.h>
/* Include after entity/model metadata headers. APC, Jeep01, Driller01, sub and Fighter01.
 * Uses the same core sphere/body/tensor services and inertia policy as Driller.
 * Spring coefficients remain authored, including unequal Jeep front/rear pairs.
 * Retains no model/archive pointer.
 * Sphere centers remain authored body-local (no invented center-of-mass shift).
 * First-pass inertia policy: sphere-generated inertia scaled to positive class
 * mass. Recovered accumulation omits sphere self-inertia; use the existing
 * inverse operation and reject degenerate geometry, rather than zero tensor.
 * A generated-mass class uses sphere density mass directly. The submarine/fighter
 * include solid-sphere self-inertia to avoid singular point-mass tensors;
 * this explicit first-pass policy does not alter the ground vehicle tensor. */
#include "scene_driller_physics.inc"
static int scene_vehicle_body_physics_initialize(const char *class_name,const rf_entity_physics_config *physics,
    const rf_entity_movement_values *movement,const rf_entity_rotation_values *rotation,
    const rf_static_render_resource *render,
    const float position[3],const float basis[9],float gravity,scene_driller_physics *out)
{
    scene_driller_physics value={0};rf_entity_class_sphere resolved[8]={0};
    rf_physics_mass_tensor initial={0},generated;rf_entity_physics_config config;
    rf_physics_body body={0};rf_vehicle_rigid_command command={0};
    rf_vehicle_rigid_support support={0};rf_vehicle_rigid_proposal probe;
    uint32_t i,j,expected_springs;float scale;int status;
    if(!class_name||!physics||!movement||!rotation||!render||!position||!basis||!out||!render->spheres||!render->sphere_count||
       render->sphere_count>8||!isfinite(gravity)||gravity<0)return RF_RANGE;
    if(physics->authored.use_kind!=1||physics->authored.movement_index!=(!strcmp(class_name,"sub")?7:!strcmp(class_name,"Fighter01")?9:5))return RF_FORMAT;
    if(!strcmp(class_name,"Driller01"))expected_springs=6;
    else if(!strcmp(class_name,"APC") || !strcmp(class_name,"Jeep01"))expected_springs=4;
    else if(!strcmp(class_name,"sub") || !strcmp(class_name,"Fighter01"))expected_springs=0;
    else return RF_FORMAT;
    value.sphere_count=render->sphere_count;
    for(i=0;i<value.sphere_count;i++){
        const rf_model_collision_sphere *sphere=render->spheres+i;
        float posed[4];
        if(!memchr(sphere->name,0,sizeof(sphere->name))||strlen(sphere->name)>=sizeof(resolved[i].name)||sphere->parent!=-1)return RF_FORMAT;
        status=rf_model_collision_sphere_pose(sphere,NULL,0,posed);if(status)return status;
        strcpy(resolved[i].name,sphere->name);memcpy(resolved[i].center,posed,12);
        if(physics->authored.flags&0x24000)resolved[i].center[0]=resolved[i].center[2]=0;
        resolved[i].radius=posed[3];resolved[i].selected_scalar=1;resolved[i].parameter_10=-1;resolved[i].model_index=(int32_t)i;
    }
    status=rf_entity_sphere_overrides(resolved,value.sphere_count,physics->spheres.items,physics->spheres.count,0);if(status)return status;
    for(i=0;i<value.sphere_count;i++){
        rf_physics_sphere *sphere=value.spheres+i;float radius;
        memcpy(sphere->center,resolved[i].center,12);sphere->radius=resolved[i].radius;
        sphere->parameter_10=resolved[i].parameter_10;sphere->opaque_14=resolved[i].opaque_14;
        memcpy(value.collision[i].center,sphere->center,12);value.collision[i].radius=sphere->radius;
        radius=sqrtf(sphere->center[0]*sphere->center[0]+sphere->center[1]*sphere->center[1]+sphere->center[2]*sphere->center[2])+sphere->radius;
        if(!isfinite(radius))return RF_RANGE;
        if(radius>value.radius)value.radius=radius;
        if(sphere->parameter_10>0){
            rf_vehicle_spring *spring;
            if(value.spring_count>=RF_VEHICLE_SUSPENSION_CAPACITY)return RF_RANGE;
            spring=value.springs+value.spring_count++;memcpy(spring->center,sphere->center,12);
            spring->radius=sphere->radius;spring->constant=sphere->parameter_10;memcpy(&spring->length,&sphere->opaque_14,4);
            if(!isfinite(spring->length)||spring->length<0)return RF_FORMAT;
        }
    }
    if(value.spring_count!=expected_springs)return RF_FORMAT;
    status=rf_physics_spheres_accumulate(value.spheres,value.sphere_count,physics->material.density,&initial,&generated);if(status)return status;
    if(!isfinite(generated.mass)||generated.mass<=0)return RF_FORMAT;
    /* Actual sub model has one collision sphere despite seven named table
     * overrides. A point-mass-only tensor is singular. First-pass submarine
     * policy includes solid-sphere self inertia; ground behavior stays exact. */
    if(!strcmp(class_name,"sub") || !strcmp(class_name,"Fighter01"))for(i=0;i<value.sphere_count;i++){
        double radius=value.spheres[i].radius;
        double mass=radius*radius*radius*physics->material.density*4.18879032135009765625;
        float self=(float)(.4*mass*radius*radius);
        for(j=0;j<3;j++)generated.tensor[j*3+j]+=self;
    }
    config=*physics;
    if(!isfinite(config.authored.mass))return RF_FORMAT;
    if(config.authored.mass<=0)config.authored.mass=generated.mass;
    scale=config.authored.mass/generated.mass;
    for(j=0;j<9;j++)generated.tensor[j]*=scale;
    /* Positive determinant is required: inverse() intentionally preserves a
     * singular source, which would otherwise masquerade as valid inertia. */
    {const float *m=generated.tensor;double determinant=(double)m[0]*(m[4]*m[8]-m[5]*m[7])-(double)m[1]*(m[3]*m[8]-m[5]*m[6])+(double)m[2]*(m[3]*m[7]-m[4]*m[6]);
     if(!isfinite(determinant)||determinant<=0)return RF_FORMAT;}
    status=rf_physics_tensor_inverse(generated.tensor,value.state.inverse_inertia);if(status)return status;
    status=rf_entity_body_open(&config,value.spheres,value.sphere_count,position,basis,0,4096,&body);if(status)return status;
    value.body_flags=body.state.flags;rf_physics_body_close(&body);
    memcpy(value.state.position,position,12);memcpy(value.state.orientation,basis,36);
    value.parameters.mass=config.authored.mass;value.parameters.gravity=gravity;
    value.parameters.maximum_speed=movement->speed;value.parameters.acceleration=movement->acceleration;
    value.parameters.maximum_turn=rotation->maximum_velocity;value.parameters.turn_acceleration=rotation->acceleration;
    status=rf_vehicle_rigid_propose(&value.state,&value.parameters,&command,&support,0,&probe);if(status)return status;
    *out=value;return RF_OK;
}
/* Ground compatibility wrapper; the shared initializer also supplies authored
 * submarine/fighter hull/inertia but never selects its movement solver. */
static int scene_ground_vehicle_physics_initialize(const char *name,const rf_entity_physics_config *physics,
    const rf_entity_movement_values *movement,const rf_entity_rotation_values *rotation,
    const rf_static_render_resource *render,const float position[3],const float basis[9],float gravity,scene_driller_physics *out)
{
    if(!name || (!strcmp(name,"sub")) || !strcmp(name,"Fighter01"))return RF_FORMAT;
    return scene_vehicle_body_physics_initialize(name,physics,movement,rotation,render,position,basis,gravity,out);
}
#endif
