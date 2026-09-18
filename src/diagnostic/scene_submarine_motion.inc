/* Borrowed actual hull, liquid metadata and shared world/mover collision.
 * Include after scene_vehicle_collision.inc. No allocation or scene globals.
 * Water admission uses wet endpoint rooms, full sphere top/surface clearance
 * and actual swept liquid faces; nonconvex and connected wet rooms are allowed.
 * Rotation follows the shared collision adapter's endpoint-center chord policy.
 * Solid contacts are resolved separately; this is not a static overlap solver. */
#ifndef SCENE_SUBMARINE_MOTION_INC
#define SCENE_SUBMARINE_MOTION_INC
#include "rf/vehicle_submersible.h"
#include "rf/liquid_damage.h"
typedef struct scene_submarine_motion {
    scene_vehicle_collision collision;
    const rf_liquid_rooms *liquids;
} scene_submarine_motion;
static int scene_submarine_motion_wet(const scene_submarine_motion *c,
    const float center[3],float radius,uint32_t *wet)
{
    rf_collision_room_location room;const rf_liquid_room *liquid;float surface;int status;
    status=rf_geometry_collision_world_locate(c->collision.world,center,&room);
    if(status)return status;
    *wet=0;
    if(room.room>=c->liquids->count)return RF_OK;
    liquid=c->liquids->items+room.room;
    if(liquid->type!=1 || !isfinite(liquid->depth) || !isfinite(liquid->minimum_y))return RF_OK;
    surface=liquid->minimum_y+liquid->depth;
    if(isfinite(surface) && center[1]+radius<=surface-.001f)*wet=1;
    return RF_OK;
}
/* Also usable for the real player body during boarding/exit. Ownership,
 * destination overlap and static transit clearance remain caller obligations. */
static int scene_submarine_water_body_path(const scene_submarine_motion *c,
    const rf_collision_body_sphere *spheres,uint32_t count,
    const float start[3],const float start_basis[9],const float end[3],const float end_basis[9],
    uint32_t *allowed)
{
    uint32_t i,j,k,wet,found,is_liquid;int status;
    if(!c || !c->collision.world || !c->liquids || !c->liquids->items ||
       !spheres || !count || count>64 || !start || !start_basis || !end || !end_basis || !allowed)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(start[i]) || !isfinite(end[i]))return RF_FORMAT;
    for(i=0;i<9;++i)if(!isfinite(start_basis[i]) || !isfinite(end_basis[i]))return RF_FORMAT;
    for(i=0;i<count;++i){
        float a[3],b[3],delta[3];rf_geometry_world_sweep_hit hit;
        if(!isfinite(spheres[i].radius) || spheres[i].radius<=0)return RF_FORMAT;
        for(k=0;k<3;++k)if(!isfinite(spheres[i].center[k]))return RF_FORMAT;
        for(j=0;j<3;++j){
            a[j]=start[j];b[j]=end[j];
            for(k=0;k<3;++k){a[j]+=spheres[i].center[k]*start_basis[k*3+j];b[j]+=spheres[i].center[k]*end_basis[k*3+j];}
            delta[j]=b[j]-a[j];
            if(!isfinite(a[j]) || !isfinite(b[j]) || !isfinite(delta[j]))return RF_FORMAT;
        }
        status=scene_submarine_motion_wet(c,a,spheres[i].radius,&wet);if(status)return status;
        if(!wet){*allowed=0;return RF_OK;}
        status=scene_submarine_motion_wet(c,b,spheres[i].radius,&wet);if(status)return status;
        if(!wet){*allowed=0;return RF_OK;}
        found=is_liquid=0;
        /* 0x1000 includes liquid faces; 4 uses ordinary solid face policy.
         * Do not add 0x100: it requires an unavailable alpha texture provider. */
        status=rf_geometry_collision_world_sweep_liquid(c->collision.world,0x1004u,
            a,delta,spheres[i].radius,1,&hit,&found,&is_liquid);
        if(status)return status;
        if(found && is_liquid){*allowed=0;return RF_OK;}
    }
    *allowed=1;return RF_OK;
}
static int scene_submarine_motion_water_path(void *context,const rf_vehicle_rigid_state *current,
    const rf_vehicle_rigid_proposal *proposed,uint32_t *allowed)
{
    const scene_submarine_motion *c=context;
    if(!c || !current || !proposed)return RF_RANGE;
    return scene_submarine_water_body_path(c,c->collision.spheres,c->collision.count,
        current->position,current->orientation,proposed->position,proposed->orientation,allowed);
}
static int scene_submarine_motion_resolve(void *context,const rf_vehicle_rigid_state *current,
    const rf_vehicle_rigid_proposal *proposed,rf_vehicle_rigid_proposal *resolved)
{
    const scene_submarine_motion *c=context;
    if(!c)return RF_RANGE;
    return scene_vehicle_collision_resolve((void *)&c->collision,current,proposed,resolved);
}
static inline int scene_submarine_motion_backend(scene_submarine_motion *c,rf_vehicle_submersible_backend *backend)
{
    int status;
    if(!c || !backend || !c->collision.world || !c->liquids || !c->liquids->items)return RF_RANGE;
    status=scene_vehicle_collision_valid(&c->collision);if(status)return status;
    backend->context=c;backend->water_path=scene_submarine_motion_water_path;
    backend->resolve=scene_submarine_motion_resolve;return RF_OK;
}
#endif
