#ifndef RF_SCENE_VEHICLE_PROFILE_PHYSICS_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_PHYSICS_INCLUDED
/* Prepare a value, never a registered runtime. The caller may discard a failed
 * candidate without changing the current host, chassis seat or global profile.
 * All arrays are embedded in scene_driller_physics; assignment retains no
 * pointers into this stack frame or the borrowed resource. */
static int scene_vehicle_profile_physics_prepare(uint32_t profile,
    const scene_driller_resources *resource,const float position[3],
    const float basis[9],float gravity,scene_driller_physics *out)
{
    if(!resource||!position||!basis||!out||profile<1||profile>5)return RF_RANGE;
    if(profile==1)return scene_driller_physics_initialize(resource,position,basis,gravity,out);
    if(profile<=3)return scene_ground_vehicle_physics_initialize(profile==3?"Jeep01":"APC",
        &resource->physics,&resource->movement,&resource->rotation,&resource->render,
        position,basis,gravity,out);
    return scene_vehicle_body_physics_initialize(profile==5?"Fighter01":"sub",
        &resource->physics,&resource->movement,&resource->rotation,&resource->render,
        position,basis,0,out);
}
#endif
