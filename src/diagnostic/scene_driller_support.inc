#ifndef RF_SCENE_DRILLER_SUPPORT_INCLUDED
#define RF_SCENE_DRILLER_SUPPORT_INCLUDED
/* Include after scene_vehicle_collision.inc and scene_driller_physics.inc.
 * Collision gateway supplies current committed world/mover faces, material
 * IDs and actual support velocity. No assumed floor/height is introduced.
 * First-pass scheduling probes every upright frame, including initial rest;
 * it does not wait for the not-yet-integrated rigid previous-contact flags. */
typedef struct scene_driller_support_context {
    scene_stream *stream;
    const scene_vehicle_collision *collision;
    const scene_driller_physics *physics;
    int32_t material;
    uint32_t support_handle,hits;
    float velocity[3];
} scene_driller_support_context;
static int scene_driller_support_initialize(scene_driller_support_context *context,scene_stream *stream,
    const scene_vehicle_collision *collision,const scene_driller_physics *physics)
{
    scene_driller_support_context value={0};
    if(!context||!stream||!collision||!physics||!stream->collision||collision->world!=stream->collision||
       (!physics->spring_count || physics->spring_count>RF_VEHICLE_SUSPENSION_CAPACITY)||!campaign_surface_palette)return RF_RANGE;
    value.stream=stream;value.collision=collision;value.physics=physics;
    value.material=-1;value.support_handle=UINT32_MAX;*context=value;return RF_OK;
}
static int scene_driller_spring_query(void *opaque,const float start[3],const float end[3],float radius,rf_vehicle_spring_hit *out)
{
    const scene_driller_support_context *context=opaque;rf_collision_body_sphere sphere={{0,0,0},0};
    rf_collision_body_query query={0};rf_geometry_body_hit hit={0};rf_vehicle_spring_hit result={0};
    uint32_t found=0,material,i;int status;
    if(!context||!context->collision||!out||!campaign_surface_palette)return RF_RANGE;
    sphere.radius=radius;query.radius=radius;query.spheres=&sphere;query.count=1;query.flags=4;query.limit=1;
    memcpy(query.start,start,12);memcpy(query.end,end,12);
    for(i=0;i<3;i++)query.matrix[i][i]=1;
    status=scene_vehicle_collision_query(context->collision,&query,&hit,&found);if(status)return status;
    result.support_handle=UINT32_MAX;result.fraction=1;
    if(found){
        /*4688a0: out-of-range material indices use Default, never a guessed
         * constant coefficient. The gateway already resolves face textures. */
        material=hit.contact.material;
        if(material<1||material>9)material=0;
        result.matched=1;result.fraction=hit.contact.fraction;result.material_id=material;
        result.material=campaign_surface_palette->materials[material].traction;
        if(hit.solid!=UINT32_MAX){
            if(!context->collision->movers||hit.solid>=context->collision->movers->count)return RF_FORMAT;
            result.support_handle=hit.contact.object_id;
            memcpy(result.velocity,hit.contact.velocity,12);
        }
    }
    *out=result;return RF_OK;
}
/* Matches rf_vehicle_rigid_backend.support. If its combined backend context
 * also owns collision resolution, parent forwards to this subcontext. */
static int scene_driller_support_sample(void *opaque,const rf_vehicle_rigid_state *state,rf_vehicle_rigid_support *out)
{
    scene_driller_support_context *context=opaque;rf_vehicle_suspension_result result;
    rf_vehicle_rigid_support retained={0};int32_t material;int status;
    if(!context||!state||!out||!context->physics||!context->stream||!context->collision||
       context->collision->world!=context->stream->collision||!campaign_surface_palette)return RF_RANGE;
    material=context->material;
    if(!rf_physics_surface_probe_gate(state->orientation[4],context->physics->body_flags|0x18000000u,&material)){
        if(material>=0){
            uint32_t index=(uint32_t)material;
            if(index>9)index=0;
            retained.grounded=1;retained.material=campaign_surface_palette->materials[index].traction;
            memcpy(retained.velocity,context->velocity,12);
        }else{context->support_handle=UINT32_MAX;memset(context->velocity,0,12);}
        context->material=material;context->hits=0;*out=retained;return RF_OK;
    }
    status=rf_vehicle_suspension_sample(state,context->physics->parameters.mass,context->physics->springs,
        context->physics->spring_count,scene_driller_spring_query,context,&result);if(status)return status;
    context->material=result.support.grounded?(int32_t)result.last_material:-1;
    context->support_handle=result.support_handle;context->hits=result.hits;
    memcpy(context->velocity,result.support.velocity,12);*out=result.support;return RF_OK;
}
#endif
