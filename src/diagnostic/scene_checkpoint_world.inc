#ifndef RF_SCENE_CHECKPOINT_WORLD_INCLUDED
#define RF_SCENE_CHECKPOINT_WORLD_INCLUDED
#include "rf/checkpoint_placement.h"
#include "rf/clutter.h"
#include "rf/entity.h"
#include <math.h>
#include <float.h>
#include <string.h>
/* Every pointer belongs to the candidate being restored, not implicitly to the
 * current scene. metadata must be read-only. Scratch belongs to the caller.
 * This ordinary adapter cannot represent detached GeoMod pieces, vehicles or
 * other candidate actors: composed admission must check those separately. */
typedef struct scene_checkpoint_world_context {
    const rf_geometry_collision_world *world;
    const rf_geometry_collision_movers *movers;
    rf_collision_body_mover *scratch;uint32_t scratch_count;
    rf_clutter_base_owner *const *props;uint32_t prop_count;
    rf_geometry_body_metadata metadata;void *metadata_context;
    uint32_t reference_room;float dt,class_speed;
} scene_checkpoint_world_context;
static int scene_checkpoint_world_point(const float local[3],const float basis[9],const float position[3],float out[3])
{
    uint32_t i,j;float result[3];
    for(i=0;i<3;i++){
        double v=position[i];for(j=0;j<3;j++)v+=(double)local[j]*basis[j*3+i];
        if(!isfinite(v)||fabs(v)>FLT_MAX)return RF_FORMAT;result[i]=(float)v;
    }
    memcpy(out,result,12);return RF_OK;
}
static int scene_checkpoint_world_obstacles(const scene_checkpoint_world_context *c,const rf_checkpoint_placement *p)
{
    uint32_t i,j,k,l;int status;float center[3];
    for(i=0;i<p->count;i++){
        status=scene_checkpoint_world_point(p->spheres[i].center,p->basis,p->position,center);if(status)return status;
        for(j=0;j<c->movers->count;j++){
            const rf_group_attached_pose *pose=c->movers->poses+j;
            const rf_geometry_collision_flat *flat=c->movers->owned+j;float local[3];
            if(pose->flags&0x40000u)continue; /* Same mover exclusion as body sweep. */
            for(k=0;k<3;k++){
                double v=0;for(l=0;l<3;l++)v+=((double)center[l]-pose->position[l])*pose->input_matrix[k*3+l];
                if(!isfinite(v)||fabs(v)>FLT_MAX)return RF_FORMAT;local[k]=(float)v;
            }
            status=rf_checkpoint_solid_sphere_check(flat->faces,flat->count,p->query_flags,local,p->spheres[i].radius,NULL);
            if(status)return status;
        }
        for(j=0;j<c->prop_count;j++){
            const rf_clutter_base_owner *prop=c->props[j];
            if(!prop)return RF_FORMAT;
            if(prop->state.flags&(2u|0x4000u))continue;
            if(!prop->state.definition)return RF_FORMAT;
            if(!(prop->state.definition->flags&6u))continue;
            if(!prop->body.spheres.items||!prop->body.spheres.count)return RF_RANGE;
            for(k=0;k<prop->body.spheres.count;k++){
                float other[3];double distance=0,radius;
                status=scene_checkpoint_world_point(prop->body.spheres.items[k].center,prop->matrix,prop->state.position,other);if(status)return status;
                if(!isfinite(prop->body.spheres.items[k].radius)||prop->body.spheres.items[k].radius<=0)return RF_FORMAT;
                for(l=0;l<3;l++){double d=(double)center[l]-other[l];distance+=d*d;}
                radius=(double)p->spheres[i].radius+prop->body.spheres.items[k].radius-.002;
                if(distance<radius*radius)return RF_NOT_FOUND;
            }
        }
    }
    return RF_OK;
}
/* Actual static-world fit, support, material and room. Outputs publish only
 * after every query succeeds. Prop spheres deliberately conservatively reject
 * overlap; props/movers are not accepted as load-bearing support in RFNC1. */
static inline int scene_checkpoint_world_place(void *context,const rf_checkpoint_placement *input,
    rf_entity_room_state *room,rf_physics_support_contact *support)
{
    const scene_checkpoint_world_context *c=context;rf_checkpoint_placement p;
    rf_physics_ground_probe probe;rf_collision_body_query query={0};rf_collision_body_sphere sphere;
    rf_geometry_body_hit hit;rf_collision_room_location location;rf_entity_room_state next_room={0};
    rf_physics_support_contact next_support;uint32_t i,matched;int status;
    if(!c||!input||!room||!support||!c->world||!c->movers||!c->metadata||
       (c->prop_count&&!c->props)||c->scratch_count<c->movers->count||
       (c->movers->count&&(!c->scratch||!c->movers->poses||!c->movers->owned||!c->movers->views)))return RF_RANGE;
    p=*input;p.world=c->world;p.replaced_room=c->reference_room;
    if(!p.count){
        /* Authored Cutter/fish bodies have no collision spheres. Preserve that
         * class behavior: validate their point's real room, without inventing
         * a collision volume or ground contact for these actors. */
        if(p.spheres)return RF_FORMAT;
        for(i=0;i<3;i++)if(!isfinite(p.position[i]))return RF_FORMAT;
        for(i=0;i<9;i++)if(!isfinite(p.basis[i]))return RF_FORMAT;
        status=rf_geometry_collision_world_locate(c->world,p.position,&location);if(status)return status;
        if(location.room==UINT32_MAX||location.room>=c->world->room_count)return RF_NOT_FOUND;
        next_room.room=location.room+1;memcpy(next_room.query_position,p.position,12);
        next_support.handle=0;next_support.material=-1; /* Explicit no-contact sentinel. */
        *room=next_room;*support=next_support;return RF_OK;
    }
    status=rf_checkpoint_world_standing_check(&p,c->dt,c->class_speed,NULL,NULL,NULL);if(status)return status;
    status=scene_checkpoint_world_obstacles(c,&p);if(status)return status;
    status=rf_physics_ground_prepare(p.spheres,p.count,p.position,p.query_flags,0,c->dt,c->class_speed,0,&probe);if(status)return status;
    memcpy(sphere.center,probe.sphere.center,12);sphere.radius=probe.sphere.radius;
    memcpy(query.start,probe.start,12);memcpy(query.end,probe.end,12);
    for(i=0;i<3;i++)query.matrix[i][i]=1;
    query.radius=probe.bounds.radius;query.flags=probe.query_flags;query.spheres=&sphere;query.count=1;query.limit=1;
    status=rf_geometry_collision_body_sweep(c->world,c->movers,&query,c->scratch,c->scratch_count,
        c->metadata,c->metadata_context,&hit,&matched);if(status)return status;
    if(!matched||hit.solid!=UINT32_MAX||!isfinite(hit.contact.fraction)||hit.contact.fraction>=1||
       hit.contact.normal[1]<.5f||hit.contact.material>INT32_MAX)return RF_NOT_FOUND;
    status=rf_geometry_collision_world_locate(c->world,p.position,&location);if(status)return status;
    if(location.room==UINT32_MAX||location.room>=c->world->room_count)return RF_NOT_FOUND;
    next_room.room=location.room+1;memcpy(next_room.query_position,p.position,12);
    next_support.handle=0;next_support.material=(int32_t)hit.contact.material;
    *room=next_room;*support=next_support;return RF_OK;
}
#endif
