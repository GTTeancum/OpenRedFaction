#ifndef RF_SCENE_VEHICLE_PROFILE_PHYSICS_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_PHYSICS_INCLUDED
#include "rf/prepare_memory.h"
/* Prepare a value, never a registered runtime. The caller may discard a failed
 * candidate without changing the current host, chassis seat or global profile.
 * All arrays are embedded in scene_driller_physics; assignment retains no
 * pointers into this stack frame or the borrowed resource. Profile6 is the
 * independent masako_fighter body only; player/boarding profiles remain1..5. */
static int scene_vehicle_profile_physics_prepare_accounted(uint32_t profile,
    const scene_driller_resources *resource,const float position[3],
    const float basis[9],float gravity,scene_driller_physics *out,const rf_prepare_memory *memory)
{
    if(!resource||!position||!basis||!out||profile<1||profile>6)return RF_RANGE;
    if(profile==1)return scene_driller_physics_initialize_accounted(resource,position,basis,gravity,out,memory);
    if(profile<=3)return scene_ground_vehicle_physics_initialize_accounted(profile==3?"Jeep01":"APC",
        &resource->physics,&resource->movement,&resource->rotation,&resource->render,
        position,basis,gravity,out,memory);
    /* Never admit a Fighter01 chassis under the boss's distinct profile. */
    if(profile==6&&strcmp(resource->model,"Fighter02.v3m"))return RF_FORMAT;
    return scene_vehicle_body_physics_initialize_accounted(profile==6?"masako_fighter":profile==5?"Fighter01":"sub",
        &resource->physics,&resource->movement,&resource->rotation,&resource->render,
        position,basis,0,out,memory);
}
static int scene_vehicle_profile_physics_prepare(uint32_t profile,
    const scene_driller_resources *resource,const float position[3],
    const float basis[9],float gravity,scene_driller_physics *out)
{return scene_vehicle_profile_physics_prepare_accounted(profile,resource,position,basis,gravity,out,NULL);}
#endif
