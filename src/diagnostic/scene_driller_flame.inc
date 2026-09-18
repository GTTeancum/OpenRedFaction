#ifndef RF_SCENE_DRILLER_FLAME_INC
#define RF_SCENE_DRILLER_FLAME_INC
/* Include after scene_driller_runtime and scene_flame_gameplay. Reuses the
 * current expanding flame volume, authored fire request, actual posed chassis
 * sphere union and retained damage owner. One dose per accepted pulse, never
 * one dose per sphere. Persistent vehicle burns remain unsupported.
 * Host contact tests world/rubble cover ONLY, avoiding self-occlusion. NPC
 * flame cover additionally includes the opaque chassis through the helper
 * below. Source/occupant exclusions are shared with firearm/projectile hits. */
static int scene_driller_flame_world_cover(void *context,const float *start,const float *end,uint32_t *blocked)
{
    float delta[3];uint32_t i;
    for(i=0;i<3;i++)delta[i]=end[i]-start[i];
    return combat_shot_obstructed(context,start,delta,1,blocked);
}
static int scene_driller_flame_cover(scene_stream *stream,uint32_t source,
    const float *start,const float *end,uint32_t *blocked)
{
    rf_weapon_flight_contact hit;float delta[3];uint32_t i,found=0;int status;
    if(!stream||!start||!end||!blocked)return RF_RANGE;
    status=scene_driller_flame_world_cover(stream,start,end,blocked);if(status||*blocked)return status;
    for(i=0;i<3;i++)delta[i]=end[i]-start[i];
    status=scene_driller_projectile_query(source,start,delta,0,1,&hit,&found);if(status)return status;
    *blocked=found;return RF_OK;
}
static int scene_driller_flame_pulse(scene_stream *stream,uint32_t frame,const float origin[3],
    const float forward[3],const rf_damage_request *request,uint32_t *contact,float *amount)
{
    scene_driller_damage_runtime *owner=scene_driller_damage_owner;
    scene_driller_physics *physics;uint32_t i,j,k,inside;int status;
    if(!stream||!origin||!forward||!request||!contact||!amount||request->kind!=4||
       !isfinite(request->amount)||request->amount<0)return RF_RANGE;
    *contact=0;*amount=0;
    if(!scene_driller_damage_runtime_valid(owner)||owner->damage.destroyed||
       scene_driller_projectile_source_excluded(owner,request->source))return RF_OK;
    physics=owner->entry->physics;if(!physics||!physics->sphere_count||physics->sphere_count>8)return RF_RANGE;
    for(i=0;i<physics->sphere_count;i++){
        const rf_physics_sphere *sphere=physics->spheres+i;float center[3];
        for(j=0;j<3;j++){
            center[j]=physics->state.position[j];
            for(k=0;k<3;k++)center[j]+=sphere->center[k]*physics->state.orientation[k*3+j];
        }
        status=rf_flame_contact(origin,forward,SCENE_FLAME_RANGE,SCENE_FLAME_RADIUS,center,sphere->radius,
            scene_driller_flame_world_cover,stream,&inside);if(status)return status;
        if(!inside)continue;
        *contact=1;
        return scene_driller_damage_runtime_receive(owner,owner->entry->registration.handle,request,frame,amount);
    }
    return RF_OK;
}
#endif
