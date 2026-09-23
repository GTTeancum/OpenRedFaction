#ifndef RF_SCENE_CHECKPOINT_WORLD_INCLUDED
#define RF_SCENE_CHECKPOINT_WORLD_INCLUDED
#include "rf/checkpoint_placement.h"
#include "rf/clutter.h"
#include "rf/entity.h"
#include <math.h>
#include <float.h>
#include <string.h>
#ifndef RF_IMAGE_XBOX_NATIVE
#include <stdio.h>
#include <stdlib.h>
#endif
static inline int scene_checkpoint_world_diagnostics(void)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");return enabled&&!strcmp(enabled,"1");
#else
    return 0;
#endif
}
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
    uint32_t fresh_boot; /* Fresh owner state is replaced, not required to be independently saveable. */
    uint32_t allow_no_contact; /* Stationary, nonfalling NPC only; default is strict standing. */
    uint32_t authored_static_unchanged; /* Caller proves identity-bound static terrain has no mutations. */
    const float *authored_position,*authored_basis; /* Optional exact NPC spawn baseline, borrowed. */
    uint32_t authored_rotating_npc; /* Stationary authored turret may rotate without translating. */
    rf_clutter_base_owner *const *authored_props; /* Parallel immutable authored-pose owners; caller proves class geometry identity. */
    uint32_t spawn_shell_uid; /* Narrow player-only authored hollow prop exception; zero disables it. */
    const float *spawn_shell_position; /* Borrowed authored player start, not a saved position. */
} scene_checkpoint_world_context;
static int scene_checkpoint_world_authored_pose(const scene_checkpoint_world_context *c,const rf_checkpoint_placement *p)
{
    uint32_t k;
    if(!c->allow_no_contact||!c->authored_static_unchanged||!c->authored_position||!c->authored_basis||
       memcmp(p->position,c->authored_position,12))return 0;
    if(!c->authored_rotating_npc)
        for(k=0;k<9;k++)if(!isfinite(c->authored_basis[k])||fabsf(p->basis[k]-c->authored_basis[k])>1e-5f)return 0;
    return 1;
}
static int scene_checkpoint_world_authored_prop(const scene_checkpoint_world_context *c,uint32_t slot,const rf_clutter_base_owner *p)
{
    const rf_clutter_base_owner *a=c->authored_props?c->authored_props[slot]:NULL;
    if(!a||p->uid!=a->uid||p->state.handle!=a->state.handle||p->state.definition!=a->state.definition||
       p->state.class_index!=a->state.class_index||memcmp(p->state.position,a->state.position,12)||
       memcmp(p->matrix,a->matrix,36)||p->body.spheres.count!=a->body.spheres.count||
       p->body.spheres.items!=a->body.spheres.items)return 0;
    return 1;
}
static int scene_checkpoint_world_spawn_shell(const scene_checkpoint_world_context *c,
    const rf_checkpoint_placement *p,uint32_t slot,const rf_clutter_base_owner *prop)
{
    const float *start=c->spawn_shell_position;
    if(!c->spawn_shell_uid||!start||!c->authored_static_unchanged||
       prop->uid!=c->spawn_shell_uid||!prop->state.definition||
       !prop->state.definition->name||strcmp(prop->state.definition->name,"BustedEscapePod")||
       !scene_checkpoint_world_authored_prop(c,slot,prop))return 0;
    /* L18S1 starts inside this hollow model's coarse collision sphere. Admit
     * only the vertical settling region of that authored start, while real
     * static-world standing, mover, and other-prop checks still apply. */
    return fabsf(p->position[0]-start[0])<=.125f&&
        fabsf(p->position[2]-start[2])<=.125f&&
        p->position[1]>=start[1]-3.f&&p->position[1]<=start[1]+.125f;
}
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
            {rf_checkpoint_placement_result reason={UINT32_MAX,UINT32_MAX,0};
             status=rf_checkpoint_solid_sphere_check(flat->faces,flat->count,p->query_flags,local,p->spheres[i].radius,&reason);
#ifndef RF_IMAGE_XBOX_NATIVE
             if(status&&scene_checkpoint_world_diagnostics())printf("WORLD_RESTORE_PLACEMENT phaseobstacle_mover index%u handle%u sphere%u reason%u retries%u status%d\n",
                j,c->movers->views[j].object_id,i,reason.reason,reason.retries,status);
#endif
             if(status)return status;}
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
                if(distance<radius*radius){
                    /* Conservative sphere unions may overlap in the original
                     * authored layout. Only that exact, proven source pair is
                     * exempt; saved moved/new props retain ordinary rejection. */
                    if(scene_checkpoint_world_authored_pose(c,p)&&scene_checkpoint_world_authored_prop(c,j,prop))continue;
                    if(scene_checkpoint_world_spawn_shell(c,p,j,prop))continue;
#ifndef RF_IMAGE_XBOX_NATIVE
                    if(scene_checkpoint_world_diagnostics())printf("WORLD_RESTORE_PLACEMENT phaseobstacle_prop uid%u sphere%u prop_sphere%u distance2%.12g limit2%.12g status%d\n",
                        prop->uid,i,k,distance,radius*radius,RF_NOT_FOUND);
#endif
                    return RF_NOT_FOUND;
                }
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
       c->allow_no_contact>1||c->authored_static_unchanged>1||(c->prop_count&&!c->props)||c->scratch_count<c->movers->count||
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
    {rf_checkpoint_placement_result reason={UINT32_MAX,UINT32_MAX,0};
     status=c->allow_no_contact?rf_checkpoint_world_placement_check(&p,&reason):
        rf_checkpoint_world_standing_check(&p,c->dt,c->class_speed,NULL,NULL,&reason);
     /* Preserve a level author's stationary spawn even when its unchanged
      * class sphere intersects a static polygon. Never extend this exception
      * to displaced saves, modified terrain, solid/ambiguous classification,
      * or candidate movers/props (checked below). Default/player has no proof. */
     if(status==RF_NOT_FOUND&&reason.reason==RF_CHECKPOINT_PLACEMENT_SURFACE&&scene_checkpoint_world_authored_pose(c,&p)){
         uint32_t k;
         {
             rf_checkpoint_placement centers=p;rf_physics_sphere points[8];
             /* Recheck every center, so an early authored surface overlap
              * cannot hide a later solid/outside/ambiguous center. */
             for(k=0;k<p.count;k++){points[k]=p.spheres[k];points[k].radius=.00201f;}
             centers.spheres=points;status=rf_checkpoint_world_placement_check(&centers,&reason);
         }
     }
#ifndef RF_IMAGE_XBOX_NATIVE
     if(status&&scene_checkpoint_world_diagnostics())printf("WORLD_RESTORE_PLACEMENT phasestanding sphere%u reason%u retries%u room%u dt%.9g speed%.9g flags%u status%d\n",
        reason.sphere,reason.reason,reason.retries,c->reference_room,(double)c->dt,(double)c->class_speed,p.query_flags,status);
#endif
     if(status)return status;}
    status=scene_checkpoint_world_obstacles(c,&p);if(status)return status;
    status=rf_physics_ground_prepare(p.spheres,p.count,p.position,p.query_flags,0,c->dt,c->class_speed,0,&probe);if(status)return status;
    memcpy(sphere.center,probe.sphere.center,12);sphere.radius=probe.sphere.radius;
    memcpy(query.start,probe.start,12);memcpy(query.end,probe.end,12);
    for(i=0;i<3;i++)query.matrix[i][i]=1;
    query.radius=probe.bounds.radius;query.flags=probe.query_flags;query.spheres=&sphere;query.count=1;query.limit=1;
    status=rf_geometry_collision_body_sweep(c->world,c->movers,&query,c->scratch,c->scratch_count,
        c->metadata,c->metadata_context,&hit,&matched);if(status)return status;
    if(matched&&(!isfinite(hit.contact.fraction)||!isfinite(hit.contact.normal[1])||hit.contact.material>INT32_MAX))return RF_FORMAT;
    if(!matched||hit.solid!=UINT32_MAX||hit.contact.fraction>=1||hit.contact.normal[1]<.5f){
        /* Original stationary NPCs need not have run a support query. Absence
         * of floor is allowed only by their caller, after full volume clearance;
         * moving support is never synthesized or silently discarded. */
        if(c->allow_no_contact&&(!matched||hit.solid==UINT32_MAX)){
            next_support.handle=0;next_support.material=-1;goto locate;
        }
#ifndef RF_IMAGE_XBOX_NATIVE
        if(scene_checkpoint_world_diagnostics()){
            if(matched)printf("WORLD_RESTORE_PLACEMENT phasefloor matched1 solid%u fraction%.9g normal_y%.9g material%u start_y%.9g end_y%.9g status%d\n",
                hit.solid,(double)hit.contact.fraction,(double)hit.contact.normal[1],hit.contact.material,(double)probe.start[1],(double)probe.end[1],RF_NOT_FOUND);
            else printf("WORLD_RESTORE_PLACEMENT phasefloor matched0 start_y%.9g end_y%.9g status%d\n",(double)probe.start[1],(double)probe.end[1],RF_NOT_FOUND);
        }
#endif
        return RF_NOT_FOUND;
    }
    next_support.handle=0;next_support.material=(int32_t)hit.contact.material;
locate:
    status=rf_geometry_collision_world_locate(c->world,p.position,&location);if(status)return status;
    if(location.room==UINT32_MAX||location.room>=c->world->room_count)return RF_NOT_FOUND;
    next_room.room=location.room+1;memcpy(next_room.query_position,p.position,12);
    *room=next_room;*support=next_support;return RF_OK;
}
#endif
