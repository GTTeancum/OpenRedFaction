#ifndef SCENE_DRILLER_RUNTIME_INC
#define SCENE_DRILLER_RUNTIME_INC
#include <float.h>
#include "scene_driller_physics.inc"
#include "scene_ground_vehicle_physics.inc"
#include "scene_vehicle_possession.inc"
#include "scene_vehicle_collision.inc"
#include "scene_submarine_motion.inc"
#include "scene_fighter_motion.inc"
#include "scene_driller_support.inc"
#include "scene_driller_entry_exit.inc"
#include "scene_driller_player_adapter.inc"
#include "scene_jeep_npc_gunner.inc"
#include "scene_jeep_seats.inc"
#include "scene_driller_audio.inc"
#include "scene_driller_actor_collision.inc"
#include "scene_driller_damage_runtime.inc"
#include "scene_apc_exit_transition.inc"
#include "scene_driller_projectile_contact.inc"
struct scene_driller_runtime {
    scene_driller_physics physics;scene_vehicle_collision collision;scene_jeep_seat_state jeep_seat;
    scene_submarine_motion submarine;scene_fighter_motion fighter;rf_liquid_rooms submarine_liquids;
    rf_vehicle_submersible_parameters submarine_parameters;
    scene_driller_support_context support;scene_driller_entry_exit entry;
    scene_driller_damage_runtime damage;scene_driller_audio audio;rf_geomod_template shapes[2];scene_driller_live_contact contact;
    scene_driller_player_adapter player;uint32_t last_frame;float driver_seconds;
};
#include "scene_driller_blast.inc"
uint32_t rf_scene_vehicle_state[16];
uint32_t rf_scene_jeep_seats[8]; /* ticks,driver frames,gunner frames,switches,role,seat,steer,fire */
uint32_t rf_scene_vehicle_damage[8]; /* health bits,amount bits,calls,destroyed,pending,exits,source,alive */
uint32_t rf_scene_drill_state[8]; /* ticks,contacts,damage,cuts,requests,rejections,postwork,status */
static uint32_t scene_campaign_vehicle_target(const scene_stream *s,uint32_t handle,
    float position[3],float *health)
{
    const scene_driller_runtime *r=s?s->driller_runtime:NULL;
    uint32_t i;
    if(!s||!handle||!position||!health)return 0;
    if(r&&handle==r->entry.host.handle){
        memcpy(position,r->physics.state.position,12);
        *health=r->damage.damage.state.effects.health;
        return r->entry.host.alive && *health>0;
    }
    for(i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *owner=campaign_passive_vehicles+i;
        if(owner->handle!=handle)continue;
        if(rf_object_registry_lookup(&campaign_registry,handle)!=owner)return 0;
        memcpy(position,owner->pose.position,12);
        *health=owner->damage.state.effects.health;
        return !owner->damage.destroyed && *health>0 &&
            !(owner->damage.object_flags&(2u|0x4000u));
    }
    {uint32_t affiliation,alive;scene_turret_owner *o=scene_turret_lookup(handle);
     if(o&&!scene_turret_target(handle,position,&affiliation,&alive)){*health=o->damage.effects.health;return alive;}}
    return 0;
}
static int scene_driller_runtime_cut(void *context,const float *center,const float *basis,
    const char *name,float radius,uint32_t flags,uint32_t source,uint32_t *accepted)
{
    scene_driller_live_contact *c=context;scene_driller_runtime *r=c->stream->driller_runtime;
    uint32_t index=!strcmp(name,"bit_driller_double.v3d");int status,post=0;
    *accepted=0;if(!c->has_world || source!=c->source)return RF_OK;
    ++rf_scene_drill_state[4];
    status=scene_driller_excavate(c->stream,c->frame,r->shapes+index,&c->world_contact,
        center,basis,radius,flags,accepted,&post);
    rf_scene_drill_state[6]=(uint32_t)post;rf_scene_drill_state[7]=(uint32_t)status;
    printf("DRILL_CUT %u %u %d %u %.9g %.9g %.9g %u %u\n",c->frame,index,status,*accepted,
        center[0],center[1],center[2],c->world_contact.room,c->world_contact.face);
    if(status==RF_NOT_FOUND){++rf_scene_drill_state[5];return RF_OK;}
    return status;
}
static uint32_t scene_driller_active(const scene_stream *s)
{return s && s->driller_runtime && s->driller_runtime->entry.session.active;}
/* Player movement is queried against the committed host pose, not the whole
 * previous frame displacement on every solver substep. Roof riding and host
 * pushing a stationary actor require a separate pair scheduler. */
static int scene_driller_player_collision(scene_stream *s,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *matched)
{
    scene_driller_actor_collision host={0};scene_driller_runtime *r;int status;
    if(!s || !(r=s->driller_runtime))return RF_OK;
    host.spheres=r->physics.collision;host.count=r->physics.sphere_count;
    host.handle=r->entry.host.handle;host.driver=r->entry.host.driver;host.material=s->driller->physics.material.index;
    memcpy(host.position,r->physics.state.position,12);memcpy(host.next_position,host.position,12);
    memcpy(host.orientation,r->physics.state.orientation,36);memcpy(host.next_orientation,host.orientation,36);
    memcpy(host.velocity,r->physics.state.velocity,12);
    ++rf_scene_vehicle_state[14];
    status=scene_driller_actor_compose(&host,campaign_player_object.handle,(uint32_t)campaign_player_view.linked_handle,query,hit,matched);
    if(!status && *matched && hit->contact.object_id==host.handle)++rf_scene_vehicle_state[15];
    return status;
}
/* One authored collision-sphere set per static vehicle class. The shape is
 * body-local, so every group-owned instance can reuse it at its live pose. */
static int scene_passive_vehicle_collision_open(scene_stream *s,uint32_t kind,
    const scene_driller_resources *resource)
{
    rf_entity_class_sphere resolved[8]={0};uint32_t i,count;int status;
    if(!s || kind>=6 || !resource || !resource->render.spheres ||
       !(count=resource->render.sphere_count) || count>8 || s->passive_vehicle_sphere_count[kind])return RF_RANGE;
    for(i=0;i<count;++i) {
        const rf_model_collision_sphere *sphere=resource->render.spheres+i;
        float posed[4];
        if(!memchr(sphere->name,0,sizeof(sphere->name)) ||
           strlen(sphere->name)>=sizeof(resolved[i].name) || sphere->parent!=-1)return RF_FORMAT;
        status=rf_model_collision_sphere_pose(sphere,NULL,0,posed);if(status)return status;
        strcpy(resolved[i].name,sphere->name);memcpy(resolved[i].center,posed,12);
        if(resource->physics.authored.flags&0x24000)
            resolved[i].center[0]=resolved[i].center[2]=0;
        resolved[i].radius=posed[3];resolved[i].selected_scalar=1;
        resolved[i].parameter_10=-1;resolved[i].model_index=(int32_t)i;
    }
    status=rf_entity_sphere_overrides(resolved,count,resource->physics.spheres.items,
        resource->physics.spheres.count,0);if(status)return status;
    for(i=0;i<count;++i) {
        if(!isfinite(resolved[i].radius) || resolved[i].radius<=0)return RF_FORMAT;
        memcpy(s->passive_vehicle_spheres[kind][i].center,resolved[i].center,12);
        s->passive_vehicle_spheres[kind][i].radius=resolved[i].radius;
    }
    s->passive_vehicle_sphere_count[kind]=count;return RF_OK;
}
static int scene_passive_vehicle_actor_collision(scene_stream *s,uint32_t actor,uint32_t linked,
    const rf_collision_body_query *query,rf_geometry_body_hit *hit,uint32_t *matched)
{
    uint32_t index;int status;
    if(!s)return RF_OK;
    for(index=0;index<campaign_passive_vehicle_count;++index) {
        const scene_passive_vehicle *owner=campaign_passive_vehicles+index;
        scene_driller_actor_collision body={0};const scene_driller_resources *resource;
        uint32_t kind=owner->resource_kind;
        if(kind>=6 || !s->passive_vehicle_sphere_count[kind]) {
            rf_scene_passive_collision[7]=RF_FORMAT;return RF_FORMAT;
        }
        resource=s->passive_vehicle_resources[kind];
        if(!resource && s->driller && campaign_selected_vehicle_resource_kind()==kind)resource=s->driller;
        if(!resource){rf_scene_passive_collision[7]=RF_NOT_FOUND;return RF_NOT_FOUND;}
        body.spheres=s->passive_vehicle_spheres[kind];body.count=s->passive_vehicle_sphere_count[kind];
        body.handle=owner->handle;body.material=resource->physics.material.index;
        memcpy(body.position,owner->pose.position,12);memcpy(body.next_position,body.position,12);
        memcpy(body.orientation,owner->pose.input_matrix,36);
        memcpy(body.next_orientation,body.orientation,36);
        memcpy(body.velocity,owner->velocity,12);
        ++rf_scene_passive_collision[0];
        status=scene_driller_actor_compose(&body,actor,linked,query,hit,matched);
        if(status){rf_scene_passive_collision[7]=(uint32_t)status;return status;}
        if(*matched && hit->contact.object_id==body.handle) {
            ++rf_scene_passive_collision[1];rf_scene_passive_collision[4]=owner->uid;
            rf_scene_passive_collision[5]=body.handle;rf_scene_passive_collision[6]=body.count;
        }
    }
    for(index=0;index<scene_turret_count;index++){
        scene_turret_owner *o=scene_turrets+index;scene_driller_actor_collision body={0};
        rf_physics_sphere raw[8]={{0}};rf_collision_body_sphere spheres[8]={{0}};
        const rf_static_render_resource *r=&scene_turret_models[o->live_model].render;
        uint32_t n=r->sphere_count,j;
        if(o->dead||(o->view.flags_7c&(2u|0x4000u)))continue;
        if(n>8)return RF_RANGE;
        if(n){status=rf_model_creation_spheres(r->spheres,n,1,NULL,0,raw,8);if(status)return status;}
        else{n=1;memcpy(raw[0].center,r->bound,12);raw[0].radius=r->bound[3];}
        for(j=0;j<n;j++){memcpy(spheres[j].center,raw[j].center,12);spheres[j].radius=raw[j].radius;}
        body.spheres=spheres;body.count=n;body.handle=o->registration.handle;body.material=o->material;
        memcpy(body.position,o->position,12);memcpy(body.next_position,o->position,12);
        memcpy(body.orientation,o->basis,36);memcpy(body.next_orientation,o->basis,36);
        status=scene_driller_actor_compose(&body,actor,linked,query,hit,matched);if(status)return status;
    }
    return RF_OK;
}
/* Sweep each translated secondary chassis through the stationary player body.
 * A side contact moves the player by the remaining chassis translation, gated
 * by the ordinary compiled-world, mover and detached-piece clearance. The
 * pair's current chassis pose is excluded from that clearance sweep. */
static int scene_passive_vehicle_player_push(scene_stream *s,rf_physics_body_state *actor)
{
    rf_collision_body_sphere spheres[8];rf_collision_body_query query={0};
    uint32_t index,i,found;int status;
    if(!s || !actor)return RF_RANGE;
    if(!campaign_passive_vehicle_count || !scene_actor_body.spheres.count)return RF_OK;
    if(scene_actor_body.spheres.count>8)return RF_RANGE;
    for(i=0;i<scene_actor_body.spheres.count;i++) {
        memcpy(spheres[i].center,scene_actor_body.spheres.items[i].center,12);
        spheres[i].radius=scene_actor_body.spheres.items[i].radius;
    }
    memcpy(query.matrix,actor->orientation,36);query.spheres=spheres;
    query.count=scene_actor_body.spheres.count;query.radius=actor->bounds.radius;
    query.flags=0x460;query.limit=1;
    for(index=0;index<campaign_passive_vehicle_count;index++) {
        const scene_passive_vehicle *owner=campaign_passive_vehicles+index;
        scene_driller_actor_collision host={0};const scene_driller_resources *resource;
        rf_geometry_body_hit contact={0},obstacle={0};rf_physics_body_state motion;
        float delta[3],approach=0,limit;uint32_t kind=owner->resource_kind;
        if(!owner->attached || campaign_support_handle==owner->handle ||
           (uint32_t)campaign_player_view.linked_handle==owner->handle)continue;
        if(kind>=6 || !s->passive_vehicle_sphere_count[kind])return RF_FORMAT;
        resource=s->passive_vehicle_resources[kind];
        if(!resource && s->driller && campaign_selected_vehicle_resource_kind()==kind)resource=s->driller;
        if(!resource)return RF_NOT_FOUND;
        for(i=0;i<3;i++)delta[i]=owner->velocity[i]*scene_step_seconds;
        if(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]<1e-8f)continue;
        host.spheres=s->passive_vehicle_spheres[kind];host.count=s->passive_vehicle_sphere_count[kind];
        host.handle=owner->handle;host.material=resource->physics.material.index;
        memcpy(host.next_position,owner->pose.position,12);
        for(i=0;i<3;i++)host.position[i]=host.next_position[i]-delta[i];
        memcpy(host.orientation,owner->pose.input_matrix,36);
        memcpy(host.next_orientation,host.orientation,36);
        memcpy(host.velocity,owner->velocity,12);
        memcpy(query.start,actor->position,12);memcpy(query.end,actor->position,12);
        ++rf_scene_passive_side_push[0];
        status=scene_driller_actor_query(&host,campaign_player_object.handle,
            (uint32_t)campaign_player_view.linked_handle,&query,&contact,&found);if(status)return status;
        if(!found || fabsf(contact.contact.normal[1])>=.5f)continue;
        for(i=0;i<3;i++)approach+=delta[i]*contact.contact.normal[i];
        if(approach<=0)continue;
        ++rf_scene_passive_side_push[1];rf_scene_passive_side_push[4]=owner->uid;
        motion=*actor;limit=fminf(1.f,1.f-contact.contact.fraction+.01f);
        for(i=0;i<3;i++)motion.next_position[i]=motion.position[i]+delta[i]*limit;
        status=rf_physics_body_prepare_sweep(&motion);if(status)return status;
        memcpy(query.start,motion.position,12);memcpy(query.end,motion.next_position,12);
        found=0;query.limit=1;
        status=campaign_body_query_batch_for(s->collision,&query,&obstacle,&found,NULL,&campaign_movers);
        if(status)return status;
        status=campaign_player_piece_query(s->collision,&query,&motion,&obstacle,&found,NULL);
        if(status)return status;
        status=scene_driller_player_collision(s,&query,&obstacle,&found);if(status)return status;
        if(found) {
            ++rf_scene_passive_side_push[3];
            limit=fmaxf(0.f,obstacle.contact.fraction-.001f);
        } else limit=1;
        for(i=0;i<3;i++)actor->position[i]=actor->next_position[i]=
            motion.position[i]+(motion.next_position[i]-motion.position[i])*limit;
        status=rf_physics_body_prepare_sweep(actor);if(status)return status;
        if(limit>0)++rf_scene_passive_side_push[2];
        memcpy(rf_scene_passive_side_push+5,actor->position,12);
    }
    return RF_OK;
}
/* Xbox-only process-local fixture. Place the player on a live attached
 * chassis after its authored controller has advanced. Seed the already
 * supported state so this checks carry independently of landing acquisition. */
static int scene_passive_vehicle_roof_stimulus(scene_stream *s,rf_physics_body_state *actor,
    uint32_t frame)
{
    const scene_passive_vehicle *owner=NULL;const rf_physics_spheres *body_spheres;
    campaign_npc_body *npc=NULL;uint32_t i,j,kind;
    float top=-FLT_MAX,bottom=FLT_MAX,center[3]={0},point[3],zero[3]={0};
    float side_center[3]={0},side_offset[3]={0},side_host_radius=0,side_actor_radius=0;
    if(!rf_scene_passive_roof_fixture[0] || frame!=rf_scene_passive_roof_fixture[1])return RF_OK;
    if(!s || !actor)return RF_RANGE;
    for(i=0;i<campaign_passive_vehicle_count;++i)
        if(campaign_passive_vehicles[i].uid==rf_scene_passive_roof_fixture[0]) {
            owner=campaign_passive_vehicles+i;break;
        }
    if(!owner || !owner->attached)return RF_NOT_FOUND;
    kind=owner->resource_kind;
    if(kind>=6 || !s->passive_vehicle_sphere_count[kind])return RF_FORMAT;
    body_spheres=&scene_actor_body.spheres;
    if(rf_scene_passive_roof_fixture[9]==3 || rf_scene_passive_roof_fixture[9]==5) {
        for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.view &&
            campaign_npc_bodies[i].damage.effects.health>0 && campaign_npc_bodies[i].body.spheres.count &&
            campaign_npc_bodies[i].view.linked_handle==-1 &&
            !(campaign_npc_bodies[i].object_flags&(2u|8u|0x4000u))) {
            npc=campaign_npc_bodies+i;break;
        }
        if(!npc)return RF_NOT_FOUND;
        actor=&npc->body.state;body_spheres=&npc->body.spheres;
    }
    if(!body_spheres->count)return RF_RANGE;
    for(i=0;i<s->passive_vehicle_sphere_count[kind];++i) {
        const rf_collision_body_sphere *sphere=s->passive_vehicle_spheres[kind]+i;
        scene_driller_actor_point(owner->pose.position,owner->pose.input_matrix,sphere->center,point);
        if(point[1]+sphere->radius>top) {
            top=point[1]+sphere->radius;memcpy(center,point,sizeof(center));
        }
        if(sphere->radius>side_host_radius){
            side_host_radius=sphere->radius;memcpy(side_center,point,12);
        }
    }
    for(j=0;j<body_spheres->count;++j) {
        const rf_physics_sphere *sphere=body_spheres->items+j;
        scene_driller_actor_point(zero,actor->orientation,sphere->center,point);
        if(point[1]-sphere->radius<bottom)bottom=point[1]-sphere->radius;
        if(sphere->radius>side_actor_radius){
            side_actor_radius=sphere->radius;memcpy(side_offset,point,12);
        }
    }
    if(rf_scene_passive_roof_fixture[9]==4) {
        float direction=owner->velocity[0]<0?-1.f:1.f;
        actor->position[0]=actor->next_position[0]=side_center[0]+direction*
            (side_host_radius+side_actor_radius+.08f)-side_offset[0];
        actor->position[1]=actor->next_position[1]=side_center[1]-side_offset[1];
        actor->position[2]=actor->next_position[2]=side_center[2]-side_offset[2];
        ++rf_scene_passive_side_push[8];
    } else {
        actor->position[0]=actor->next_position[0]=center[0];
        actor->position[1]=actor->next_position[1]=top-bottom+.06f;
        actor->position[2]=actor->next_position[2]=center[2];
    }
    memset(actor->velocity,0,sizeof(actor->velocity));
    if(npc) {
        uint32_t handle=npc->registration.handle;int status;
        npc->support.handle=owner->handle;
        memcpy(npc->support_velocity,owner->velocity,sizeof(npc->support_velocity));
        memset(&npc->piece_support,0,sizeof(npc->piece_support));
        status=rf_scene_npc_normal(s->collision,handle,NULL);if(status)return status;
        if(rf_scene_passive_roof_fixture[9]==5){
            campaign_pursuit_stop(npc);memset(&npc->script_move,0,sizeof(npc->script_move));
            npc->combat_alert=npc->combat_scripted=npc->combat_target=npc->combat_due=npc->combat_navigation_due=0;
        }else{
            npc->script_move.active=1;npc->script_move.follow=0;
            npc->script_move.retry=UINT32_MAX;
            npc->script_move.target[0]=actor->position[0]+100.f;
            npc->script_move.target[1]=actor->position[1];
            npc->script_move.target[2]=actor->position[2];
        }
        rf_scene_passive_npc_fixture[0]=handle;
    } else if(rf_scene_passive_roof_fixture[9]==2 || rf_scene_passive_roof_fixture[9]==4) {
        actor->flags|=1u;rf_scene_actor_landing[1]=3;campaign_support_handle=0;
        memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));
    } else {
        actor->flags&=~1u;rf_scene_actor_landing[1]=1;
        campaign_support_handle=owner->handle;
        memcpy(campaign_support_velocity,owner->velocity,sizeof(campaign_support_velocity));
    }
    if(!npc)memset(&campaign_piece_support,0,sizeof(campaign_piece_support));
    if(rf_physics_body_prepare_sweep(actor))return RF_FORMAT;
    if(npc){int status=rf_scene_npc_publish_position(npc->registration.handle);if(status)return status;}
    ++rf_scene_passive_roof_fixture[2];
    memcpy(rf_scene_passive_roof_fixture+3,actor->position,12);
    memcpy(rf_scene_passive_roof_fixture+6,owner->pose.position,12);
    return RF_OK;
}
static void scene_passive_vehicle_npc_fixture_probe(void)
{
    uint32_t i,handle=rf_scene_passive_npc_fixture[0];
    if(!handle)for(i=0;i<campaign_npc_body_count&&!handle;i++){
        const campaign_npc_body *npc=campaign_npc_bodies+i;uint32_t j;
        if(!npc->registration.view||npc->damage.effects.health<=0||!npc->support.handle)continue;
        for(j=0;j<campaign_passive_vehicle_count;j++)if(npc->support.handle==campaign_passive_vehicles[j].handle){
            handle=npc->registration.handle;rf_scene_passive_npc_fixture[0]=handle;break;}
    }
    if(!handle)return;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==handle) {
        const campaign_npc_body *npc=campaign_npc_bodies+i;
        rf_scene_passive_npc_fixture[1]=npc->support.handle;
        memcpy(rf_scene_passive_npc_fixture+2,npc->body.state.position,12);
        rf_scene_passive_npc_fixture[5]=npc->script_move.active;
        memcpy(rf_scene_passive_npc_fixture+6,&npc->damage.effects.health,4);
        rf_scene_passive_npc_fixture[8]=npc->movement_slot<16?
            campaign_modes[npc->movement_slot].index:UINT32_MAX;
        ++rf_scene_passive_npc_fixture[7];return;
    }
}
/* Keep a previously attached NPC grounded when its downward sweep misses a
 * translating spherical roof by a small gap. Reacquire only the exact
 * registered support handle and a close, upward-facing surface. */
static int scene_passive_vehicle_npc_retain(scene_stream *s,campaign_npc_body *npc,uint32_t *held)
{
    const scene_passive_vehicle *owner=NULL;const rf_physics_spheres *body;
    rf_physics_body_state *actor;float foot[3]={0},foot_y=FLT_MAX,best=FLT_MAX;
    uint32_t i,j,kind;int status;
    if(!s || !npc || !held)return RF_RANGE;*held=0;
    if(!npc->support.handle || !npc->body.spheres.count)return RF_OK;
    for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==npc->support.handle &&
        rf_object_registry_lookup(&campaign_registry,npc->support.handle)==campaign_passive_vehicles+i) {
        owner=campaign_passive_vehicles+i;break;
    }
    if(!owner || !owner->attached)return RF_OK;
    kind=owner->resource_kind;
    if(kind>=6 || !s->passive_vehicle_sphere_count[kind])return RF_OK;
    actor=&npc->body.state;body=&npc->body.spheres;
    for(i=0;i<body->count;i++) {
        const rf_physics_sphere *sphere=body->items+i;float point[3];
        scene_driller_actor_point(actor->position,actor->orientation,sphere->center,point);
        if(point[1]-sphere->radius<foot_y) {
            memcpy(foot,point,sizeof(foot));foot_y=point[1]-sphere->radius;
        }
    }
    for(j=0;j<s->passive_vehicle_sphere_count[kind];j++) {
        const rf_collision_body_sphere *sphere=s->passive_vehicle_spheres[kind]+j;
        float center[3],dx,dz,lateral,gap;
        scene_driller_actor_point(owner->pose.position,owner->pose.input_matrix,sphere->center,center);
        dx=foot[0]-center[0];dz=foot[2]-center[2];lateral=dx*dx+dz*dz;
        if(lateral>sphere->radius*sphere->radius*.64f)continue;
        gap=foot_y-(center[1]+sqrtf(sphere->radius*sphere->radius-lateral));
        if(gap<-.4f || gap>.4f)continue;
        if(fabsf(gap)<fabsf(best))best=gap;
    }
    if(best==FLT_MAX)return RF_OK;
    actor->position[1]=(float)((double)actor->position[1]-(double)best+.02);
    actor->next_position[1]=actor->position[1];actor->velocity[1]=0;
    actor->flags|=0x400000u;
    memcpy(npc->support_velocity,owner->velocity,sizeof(npc->support_velocity));
    npc->script_move.fall_speed=0;
    status=rf_physics_body_prepare_sweep(actor);if(status)return status;
    status=rf_scene_npc_normal(s->collision,npc->registration.handle,NULL);if(status)return status;
    status=rf_scene_npc_publish_position(npc->registration.handle);if(status)return status;
    *held=1;return RF_OK;
}
/* Port first pass for a rising chassis reaching an airborne player's feet.
 * Existing ground queries cover downward landings; this narrow proximity
 * gate supplies the missing moving-host acquisition before actor integration. */
static int scene_passive_vehicle_player_rising_support(scene_stream *s,
    rf_physics_body_state *actor)
{
    const scene_passive_vehicle *selected=NULL;float foot[3]={0},foot_offset=FLT_MAX;
    float best=FLT_MAX;uint32_t i,j;
    if(!s || !actor)return RF_RANGE;
    if(rf_scene_actor_landing[1]!=3 || actor->velocity[1]>0 || !campaign_passive_vehicle_count)
        return RF_OK;
    ++rf_scene_passive_rising_support[0];
    for(i=0;i<scene_actor_body.spheres.count;++i) {
        const rf_physics_sphere *sphere=scene_actor_body.spheres.items+i;
        float point[3];
        scene_driller_actor_point(actor->position,actor->orientation,sphere->center,point);
        if(point[1]-sphere->radius<foot_offset) {
            memcpy(foot,point,sizeof(foot));foot_offset=point[1]-sphere->radius;
        }
    }
    if(foot_offset==FLT_MAX)return RF_OK;
    for(i=0;i<campaign_passive_vehicle_count;++i) {
        const scene_passive_vehicle *owner=campaign_passive_vehicles+i;
        uint32_t kind=owner->resource_kind;
        if(!owner->attached || owner->velocity[1]<=.1f || kind>=6 ||
           !s->passive_vehicle_sphere_count[kind])continue;
        for(j=0;j<s->passive_vehicle_sphere_count[kind];++j) {
            const rf_collision_body_sphere *sphere=s->passive_vehicle_spheres[kind]+j;
            float center[3],dx,dz,lateral,vertical,gap;
            scene_driller_actor_point(owner->pose.position,owner->pose.input_matrix,sphere->center,center);
            dx=foot[0]-center[0];dz=foot[2]-center[2];lateral=dx*dx+dz*dz;
            if(lateral>sphere->radius*sphere->radius*.64f)continue; /* upward normal >= .6 */
            vertical=sqrtf(sphere->radius*sphere->radius-lateral);
            gap=foot_offset-(center[1]+vertical);
            if(gap<-.25f || gap>.12f)continue;
            ++rf_scene_passive_rising_support[1];
            if(fabsf(gap)<fabsf(best)){best=gap;selected=owner;}
        }
    }
    if(!selected)return RF_OK;
    actor->position[1]=(float)((double)actor->position[1]-(double)best+.02);
    actor->next_position[1]=actor->position[1];
    actor->velocity[1]=0;actor->flags&=~1u;rf_scene_actor_landing[1]=1;
    campaign_support_handle=selected->handle;
    memcpy(campaign_support_velocity,selected->velocity,sizeof(campaign_support_velocity));
    memset(&campaign_piece_support,0,sizeof(campaign_piece_support));
    {int status=rf_physics_body_prepare_sweep(actor);if(status)return status;}
    ++rf_scene_passive_rising_support[2];
    rf_scene_passive_rising_support[3]=selected->uid;
    rf_scene_passive_rising_support[4]=selected->handle;
    memcpy(rf_scene_passive_rising_support+5,&best,4);
    return RF_OK;
}
static int scene_driller_runtime_support(void *context,const rf_vehicle_rigid_state *state,rf_vehicle_rigid_support *out)
{return scene_driller_support_sample(&((scene_driller_runtime*)context)->support,state,out);}
static int scene_driller_runtime_resolve(void *context,const rf_vehicle_rigid_state *state,const rf_vehicle_rigid_proposal *proposal,rf_vehicle_rigid_proposal *out)
{return scene_vehicle_collision_resolve(&((scene_driller_runtime*)context)->collision,state,proposal,out);}
/* Campaign vehicles initialize before terrain ownership. Switch their shared
 * collision gateway to the committed overlay once it replaces a room. */
static void scene_driller_runtime_rebind_collision(scene_stream *s)
{if(s && s->driller_runtime && s->collision)s->driller_runtime->collision.world=s->collision;}
static int scene_submarine_player_water(void *context,const rf_collision_body_sphere *spheres,uint32_t count,
    const scene_vehicle_pose *from,const scene_vehicle_pose *to,uint32_t *allowed)
{
    return scene_submarine_water_body_path(context,spheres,count,
        from->position,from->basis,to->position,to->basis,allowed);
}
static int scene_driller_runtime_open(scene_stream *s)
{
    scene_driller_runtime *r;int status;uint32_t stage=0;
    if(!s->driller)return RF_OK;
    if(s->driller_runtime)return RF_OK;
    if((!rf_scene_dev_room_enabled && !campaign_authored_vehicle_uid) ||
       (rf_scene_dev_npc_enabled && !(rf_scene_vehicle_enabled==4 && rf_scene_dev_npc_enabled==7)))return RF_RANGE;
    r=calloc(1,sizeof(*r));if(!r)return RF_IO;r->last_frame=UINT32_MAX;
    if(rf_scene_vehicle_enabled==1){
#ifdef RF_IMAGE_XBOX_NATIVE
    status=rf_geomod_template_load("D:\\driller-single.bin",r->shapes);
    if(!status)status=rf_geomod_template_load("D:\\driller-double.bin",r->shapes+1);
#else
    status=rf_geomod_template_load("build/data/driller-single.bin",r->shapes);
    if(!status)status=rf_geomod_template_load("build/data/driller-double.bin",r->shapes+1);
#endif
    if(status)goto fail;
    }
    stage=1;if(rf_scene_vehicle_enabled==4 || rf_scene_vehicle_enabled==5)status=scene_vehicle_body_physics_initialize(rf_scene_vehicle_enabled==5?"Fighter01":"sub",&s->driller->physics,&s->driller->movement,&s->driller->rotation,&s->driller->render,s->driller_position,s->driller_basis,0,&r->physics);
    else if(rf_scene_vehicle_enabled>=2)status=scene_ground_vehicle_physics_initialize(rf_scene_vehicle_enabled==3?"Jeep01":"APC",&s->driller->physics,&s->driller->movement,&s->driller->rotation,&s->driller->render,s->driller_position,s->driller_basis,scene_gravity.acceleration,&r->physics);
    else status=scene_driller_physics_initialize(s->driller,s->driller_position,s->driller_basis,scene_gravity.acceleration,&r->physics);
    if(status)goto fail;
    r->collision.world=s->collision;r->collision.movers=&campaign_movers;
    r->collision.spheres=r->physics.collision;r->collision.count=r->physics.sphere_count;
    r->collision.radius=r->physics.radius;r->collision.flags=4;
    stage=2;
    if(rf_scene_vehicle_enabled==4 || rf_scene_vehicle_enabled==5){
        r->submarine_liquids.items=s->liquid_rooms;r->submarine_liquids.count=s->swim_room_count;r->submarine_liquids.bytes=s->swim_bytes;
        r->submarine.collision=r->collision;r->submarine.liquids=&r->submarine_liquids;
        r->fighter.collision=r->collision;r->fighter.liquids=&r->submarine_liquids;
        r->submarine_parameters=(rf_vehicle_submersible_parameters){s->driller->physics.authored.mass,
            s->driller->movement.speed,s->driller->movement.acceleration,s->driller->rotation.maximum_velocity,
            s->driller->rotation.acceleration,2};
    }else{status=scene_driller_support_initialize(&r->support,s,&r->collision,&r->physics);if(status)goto fail;}
    stage=3;status=scene_driller_entry_exit_open(&r->entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,s->driller,&r->physics,&r->collision,scene_driller_player_eye_offset());if(status)goto fail;
    if(rf_scene_vehicle_enabled==4){r->entry.water_context=&r->submarine;r->entry.water_path=scene_submarine_player_water;}
    if(rf_scene_vehicle_enabled==3){status=scene_jeep_seats_open(&r->jeep_seat,s->driller);if(status)goto fail;}
    stage=4;status=scene_driller_damage_runtime_open(&r->damage,&r->entry,&r->player,&s->driller_damage_prototype);if(status)goto fail;
    /* Ambient slots are initialized later in level startup. Bind the idle
     * voice only after that reset and its initial authored scheduling. */
    s->driller_runtime=r;memset(rf_scene_jeep_seats,0,sizeof(rf_scene_jeep_seats));memset(rf_scene_drill_state,0,sizeof(rf_scene_drill_state));memset(rf_scene_vehicle_state,0,sizeof(rf_scene_vehicle_state));
    memset(rf_scene_jeep_npc_gunner,0,sizeof(rf_scene_jeep_npc_gunner));
    memset(rf_scene_jeep_entry_probe,0,sizeof(rf_scene_jeep_entry_probe));
    memset(rf_scene_apc_exit_transition,0,sizeof(rf_scene_apc_exit_transition));
    memset(rf_scene_apc_exit_continuation,0,sizeof(rf_scene_apc_exit_continuation));return RF_OK;
fail:fprintf(stderr,"VEHICLE_OPEN_ERROR stage%u status%d\n",stage,status);scene_driller_damage_runtime_close(&r->damage);(void)scene_driller_entry_exit_close(&r->entry,1);free(r);return status;
}
static void scene_driller_runtime_audio_start(scene_stream *s)
{
    scene_driller_runtime *r=s?s->driller_runtime:NULL;
    if(!r||r->audio.active)return;
    /* Audio unavailable/full slots must not prevent driving. */
    if(rf_scene_vehicle_enabled==2)(void)scene_vehicle_audio_open(&r->audio,r->physics.state.position,"APC_idle.wav",8,.5f,.4f);
    else if(rf_scene_vehicle_enabled==3)(void)scene_vehicle_audio_open(&r->audio,r->physics.state.position,"Jeep_idle.wav",6,.8f,.6f);
    else if(rf_scene_vehicle_enabled==1)(void)scene_driller_audio_open(&r->audio,r->physics.state.position);
}
#include "scene_vehicle_ai_mode.inc"
/* Shared rigid owner follows the event's authored waypoint path. This is a
 * bounded first-pass driver command; the rigid solver still owns contact,
 * suspension and speed. It does not teleport between authored nodes. */
static int scene_campaign_vehicle_route_command(const scene_driller_runtime *r,rf_vehicle_rigid_command *command)
{
    uint32_t visited=0,node;const float *target;float dx,dz,distance,forward_x,forward_z,angle;
    if((r->entry.session.active&&!scene_jeep_npc_gunner_active(&r->entry))||
       !scene_npc_seat_driver_admits(r->entry.host.handle))return RF_OK;
    if(!campaign_vehicle_route.active || campaign_vehicle_route.pursuit ||
       r->entry.host.handle!=campaign_authored_vehicle_handle)return RF_OK;
    if(!campaign_vehicle_route.path.count || !campaign_vehicle_route.path.indices)return RF_FORMAT;
    while(visited++<campaign_vehicle_route.path.count){
        node=rf_level_waypoint_node(&campaign_vehicle_route.path,campaign_vehicle_route.index);
        if(node>=campaign_navigation.count)return RF_FORMAT;
        target=campaign_navigation.nodes[node].candidate.position;
        dx=target[0]-r->physics.state.position[0];dz=target[2]-r->physics.state.position[2];
        distance=sqrtf(dx*dx+dz*dz);
        if(!isfinite(distance))return RF_FORMAT;
        if(distance>1.5f)break;
        ++rf_scene_vehicle_route_state[4];
        if(campaign_vehicle_route.reverse){
            if(campaign_vehicle_route.index) --campaign_vehicle_route.index;
            else {campaign_vehicle_route.reverse=0;campaign_vehicle_route.index=1;}
        }else ++campaign_vehicle_route.index;
        if(campaign_vehicle_route.index>=campaign_vehicle_route.path.count){
            if(campaign_vehicle_route.mode==1)campaign_vehicle_route.index=0;
            else if(campaign_vehicle_route.mode==2 && campaign_vehicle_route.path.count>1){
                campaign_vehicle_route.reverse=1;campaign_vehicle_route.index=campaign_vehicle_route.path.count-2;
            }else {campaign_vehicle_route.active=0;break;}
        }
    }
    rf_scene_vehicle_route_state[0]=campaign_vehicle_route.active;
    rf_scene_vehicle_route_state[1]=campaign_vehicle_route.index;
    if(!campaign_vehicle_route.active)return RF_OK;
    node=rf_level_waypoint_node(&campaign_vehicle_route.path,campaign_vehicle_route.index);
    if(node>=campaign_navigation.count)return RF_FORMAT;
    target=campaign_navigation.nodes[node].candidate.position;
    dx=target[0]-r->physics.state.position[0];dz=target[2]-r->physics.state.position[2];
    distance=sqrtf(dx*dx+dz*dz);
    if(!isfinite(distance))return RF_FORMAT;
    if(distance<=1.5f){command->controlled=1;command->turn=command->throttle=0;return RF_OK;}
    forward_x=r->physics.state.orientation[6];forward_z=r->physics.state.orientation[8];
    angle=atan2f(forward_z*dx-forward_x*dz,forward_x*dx+forward_z*dz);
    if(!isfinite(angle))return RF_FORMAT;
    command->controlled=1;
    command->turn=fmaxf(-1.f,fminf(1.f,angle/.7f));
    command->throttle=fabsf(angle)>1.5f?0.f:fabsf(angle)>.8f?.2f:fminf(1.f,distance/4.f);
    ++rf_scene_vehicle_route_state[3];return RF_OK;
}
/* Authored Goto and Goto_Player orders drive the same submerged body as the
 * player. This is a direct steering first pass; collision and the full-hull
 * water gate remain in the submersible solver. */
static int scene_campaign_submarine_script_command(const scene_driller_runtime *r,
    rf_vehicle_submersible_command *command)
{
    const float *target;float dx,dy,dz,horizontal,forward_x,forward_z,angle;
    if(!campaign_vehicle_route.active || !campaign_vehicle_route.pursuit ||
       r->entry.host.handle!=campaign_authored_vehicle_handle || r->entry.session.active ||
       !r->entry.host.alive)return RF_OK;
    target=campaign_vehicle_route.pursuit==2?scene_actor_body.state.position:
        campaign_vehicle_route.target;
    dx=target[0]-r->physics.state.position[0];
    dy=target[1]-r->physics.state.position[1];
    dz=target[2]-r->physics.state.position[2];
    horizontal=sqrtf(dx*dx+dz*dz);
    if(!isfinite(horizontal)||!isfinite(dy))return RF_FORMAT;
    command->controlled=1;
    if(horizontal<=1.5f && fabsf(dy)<=1.5f){
        if(campaign_vehicle_route.pursuit==1){
            campaign_vehicle_route.active=0;rf_scene_vehicle_route_state[0]=0;
            ++rf_scene_vehicle_route_state[4];
        }
        return RF_OK;
    }
    forward_x=r->physics.state.orientation[6];forward_z=r->physics.state.orientation[8];
    angle=atan2f(forward_z*dx-forward_x*dz,forward_x*dx+forward_z*dz);
    if(!isfinite(angle))return RF_FORMAT;
    command->yaw=fmaxf(-1.f,fminf(1.f,angle/.7f));
    command->throttle=fabsf(angle)>1.5f?0.f:fabsf(angle)>.8f?.2f:
        fminf(1.f,horizontal/4.f);
    command->rise=fmaxf(-1.f,fminf(1.f,dy/3.f));
    ++rf_scene_vehicle_route_state[3];return RF_OK;
}
/* Fighter Follow_Waypoints uses authored 3D nodes and the same dry-hull
 * solver as player flight. A player taking the seat temporarily owns controls. */
static int scene_campaign_fighter_route_command(const scene_driller_runtime *r,
    rf_vehicle_submersible_command *command)
{
    uint32_t visited=0,node;const float *target;
    float dx,dy,dz,horizontal,distance,forward_x,forward_z,angle;
    if(!campaign_vehicle_route.active || campaign_vehicle_route.pursuit ||
       r->entry.host.handle!=campaign_authored_vehicle_handle || r->entry.session.active ||
       !r->entry.host.alive)return RF_OK;
    if(!campaign_vehicle_route.path.count || !campaign_vehicle_route.path.indices)return RF_FORMAT;
    while(visited++<campaign_vehicle_route.path.count){
        node=rf_level_waypoint_node(&campaign_vehicle_route.path,campaign_vehicle_route.index);
        if(node>=campaign_navigation.count)return RF_FORMAT;
        target=campaign_navigation.nodes[node].candidate.position;
        dx=target[0]-r->physics.state.position[0];
        dy=target[1]-r->physics.state.position[1];
        dz=target[2]-r->physics.state.position[2];
        distance=sqrtf(dx*dx+dy*dy+dz*dz);
        if(!isfinite(distance))return RF_FORMAT;
        if(distance>3.f)break;
        ++rf_scene_vehicle_route_state[4];
        if(campaign_vehicle_route.reverse){
            if(campaign_vehicle_route.index)--campaign_vehicle_route.index;
            else {campaign_vehicle_route.reverse=0;campaign_vehicle_route.index=1;}
        }else ++campaign_vehicle_route.index;
        if(campaign_vehicle_route.index>=campaign_vehicle_route.path.count){
            if(campaign_vehicle_route.mode==1)campaign_vehicle_route.index=0;
            else if(campaign_vehicle_route.mode==2 && campaign_vehicle_route.path.count>1){
                campaign_vehicle_route.reverse=1;
                campaign_vehicle_route.index=campaign_vehicle_route.path.count-2;
            }else {campaign_vehicle_route.active=0;break;}
        }
    }
    rf_scene_vehicle_route_state[0]=campaign_vehicle_route.active;
    rf_scene_vehicle_route_state[1]=campaign_vehicle_route.index;
    if(!campaign_vehicle_route.active)return RF_OK;
    node=rf_level_waypoint_node(&campaign_vehicle_route.path,campaign_vehicle_route.index);
    if(node>=campaign_navigation.count)return RF_FORMAT;
    target=campaign_navigation.nodes[node].candidate.position;
    dx=target[0]-r->physics.state.position[0];
    dy=target[1]-r->physics.state.position[1];
    dz=target[2]-r->physics.state.position[2];
    horizontal=sqrtf(dx*dx+dz*dz);
    if(!isfinite(horizontal)||!isfinite(dy))return RF_FORMAT;
    forward_x=r->physics.state.orientation[6];forward_z=r->physics.state.orientation[8];
    angle=atan2f(forward_z*dx-forward_x*dz,forward_x*dx+forward_z*dz);
    if(!isfinite(angle))return RF_FORMAT;
    command->controlled=1;
    command->yaw=fmaxf(-1.f,fminf(1.f,angle/.7f));
    command->throttle=fabsf(angle)>1.5f?0.f:fabsf(angle)>.8f?.2f:
        fminf(1.f,horizontal/8.f);
    command->rise=fmaxf(-1.f,fminf(1.f,dy/6.f));
    ++rf_scene_vehicle_route_state[3];return RF_OK;
}
static int scene_driller_runtime_tick(scene_stream *s,uint32_t frame)
{
    scene_driller_runtime *r;rf_vehicle_rigid_command command={0};rf_vehicle_rigid_backend backend;
    scene_apc_exit_sample apc_exit;
    uint32_t changed=0,was,use_edge,stage=0;int status;
    if(!s->driller)return RF_OK;
    status=scene_driller_runtime_open(s);if(status)return status;r=s->driller_runtime;
    if(r->last_frame==frame)return RF_OK;
    /* The campaign player class finishes loading after the vehicle host opens.
     * Refresh its seat eye offset until boarding, then keep it fixed for the
     * occupied session and any checkpoint derived from that session. */
    if(!r->entry.session.active)
        memcpy(r->entry.eye_offset,rf_scene_actor_initial_eye_offsets,12);
    was=r->entry.session.active;use_edge=player_input.use&&!r->entry.held;
    scene_apc_exit_begin(&apc_exit,&r->entry,frame);
    stage=1;
    if(!apc_exit.deferred){
        status=scene_driller_damage_runtime_tick(&r->damage,&changed);if(status)goto fail;
        if(r->entry.host.alive){
            uint32_t handled=0;
            status=scene_jeep_npc_gunner_use(&r->entry,s->driller,&r->jeep_seat.role,
                player_input.use,campaign_player_damage.state.effects.health>0,&changed,&handled);
            if(!status&&!handled)status=scene_driller_entry_exit_use(&r->entry,
                player_input.use,campaign_player_damage.state.effects.health>0,&changed);
            if(status)goto fail;
        }
        if(campaign_spawn){
            if(changed && r->entry.session.active)campaign_triggers.vehicle_pulses|=0x400u;
            if(was && use_edge && r->entry.host.alive && campaign_player_damage.state.effects.health>0)
                campaign_triggers.vehicle_pulses|=0x800u;
        }
        if(changed){if(r->entry.session.active)++rf_scene_vehicle_state[1];else ++rf_scene_vehicle_state[2];}
    }
    if(rf_scene_vehicle_enabled==3){uint32_t seat_changed=0;
        status=scene_jeep_seats_tick(&r->jeep_seat,s->driller,&r->entry,&r->player,player_input.cycle_weapon,&seat_changed);if(status)goto fail;
        if(seat_changed){s->apc_aim_active=0;++rf_scene_jeep_seats[3];}
    }
    if(frame && r->entry.session.active)r->driver_seconds+=scene_step_seconds;
    command.controlled=r->entry.session.active && r->entry.host.alive &&
        (!apc_exit.deferred || campaign_player_damage.state.effects.health>0) &&
        (rf_scene_vehicle_enabled!=3 || scene_jeep_can_steer(&r->jeep_seat));command.throttle=command.controlled?player_input.move[2]:0;
    command.turn=command.controlled?player_input.move[0]:0;
    if(campaign_authored_vehicle_uid && r->entry.host.alive && rf_scene_vehicle_enabled!=5){
        status=scene_campaign_vehicle_route_command(r,&command);
        rf_scene_vehicle_route_state[7]=(uint32_t)status;if(status)goto fail;
    }
    backend.context=r;backend.support=scene_driller_runtime_support;backend.resolve=scene_driller_runtime_resolve;
    r->physics.parameters.gravity=scene_gravity.acceleration;
    stage=2;if(frame){
        if(rf_scene_vehicle_enabled==4 || rf_scene_vehicle_enabled==5){
            rf_vehicle_submersible_command sub={0};rf_vehicle_submersible_result result;
            rf_vehicle_submersible_backend wet_backend={&r->submarine,scene_submarine_motion_water_path,scene_submarine_motion_resolve};
            if(rf_scene_vehicle_enabled==5){wet_backend.context=&r->fighter;wet_backend.water_path=scene_fighter_motion_air_path;wet_backend.resolve=scene_fighter_motion_resolve;}
            sub.controlled=r->entry.session.active && r->entry.host.alive;
            if(sub.controlled){sub.throttle=player_input.move[2];sub.strafe=player_input.move[0];
                sub.rise=(float)player_input.jump-(float)player_input.crouch;
                sub.yaw=player_input.look[1];sub.pitch=player_input.look[0];}
            else if(rf_scene_vehicle_enabled==4 && campaign_authored_vehicle_uid){
                status=scene_campaign_submarine_script_command(r,&sub);
                rf_scene_vehicle_route_state[7]=(uint32_t)status;if(status)goto fail;
            }
            else if(rf_scene_vehicle_enabled==5 && campaign_authored_vehicle_uid){
                status=scene_campaign_fighter_route_command(r,&sub);
                rf_scene_vehicle_route_state[7]=(uint32_t)status;if(status)goto fail;
            }
            status=rf_vehicle_submersible_step(&r->physics.state,&r->submarine_parameters,&sub,scene_step_seconds,&wet_backend,&result);
        }else status=rf_vehicle_rigid_step(&r->physics.state,&r->physics.parameters,&command,scene_step_seconds,&backend);
        if(status)goto fail;
    }
    stage=3;
    if(apc_exit.deferred){
        /* End-pose clearance must follow the accepted chassis step. Keep
         * physical ownership until then; never publish an old-pose exit. */
        status=scene_apc_exit_finish(&apc_exit,&r->damage,player_input.use,
            campaign_player_damage.state.effects.health>0,&changed);if(status)goto fail;
        if(campaign_spawn){
            if(changed && r->entry.session.active)campaign_triggers.vehicle_pulses|=0x400u;
            if(was && use_edge && r->entry.host.alive && campaign_player_damage.state.effects.health>0)
                campaign_triggers.vehicle_pulses|=0x800u;
        }
        if(changed){if(r->entry.session.active)++rf_scene_vehicle_state[1];else ++rf_scene_vehicle_state[2];}
    }
    status=scene_driller_player_publish(&r->player,&r->entry,changed);if(status)goto fail;
    if(rf_scene_vehicle_enabled==3 && was && !r->entry.session.active){status=scene_jeep_seats_open(&r->jeep_seat,s->driller);if(status)goto fail;}
    stage=4;if(r->audio.active){status=scene_driller_audio_tick(&r->audio,r->physics.state.position,r->physics.state.velocity,r->physics.parameters.maximum_speed,!r->entry.host.alive);if(status)goto fail;}
    stage=5;
    r->contact.stream=s;r->contact.frame=frame;r->contact.source=r->entry.host.handle;r->contact.has_world=0;
    if(rf_scene_vehicle_enabled==1){scene_driller_weapon_backend weapon_backend={&r->contact,scene_driller_live_trace,scene_driller_live_damage,scene_driller_runtime_cut};
     status=scene_driller_weapon_tick(&s->driller_weapon,&s->driller->tags,r->physics.state.position,
        r->physics.state.orientation,r->entry.host.handle,r->entry.session.active && r->entry.host.alive && player_input.fire,
        frame?scene_step_seconds:0,&weapon_backend);if(status)goto fail;}
    ++rf_scene_drill_state[0];rf_scene_drill_state[1]=s->driller_weapon.contacts;
    rf_scene_drill_state[2]=s->driller_weapon.damage_calls;rf_scene_drill_state[3]=s->driller_weapon.accepted_cuts;
    memcpy(s->driller_position,r->physics.state.position,12);memcpy(s->driller_basis,r->physics.state.orientation,36);
    memcpy(rf_scene_vehicle_damage,&r->damage.damage.state.effects.health,4);
    memcpy(rf_scene_vehicle_damage+1,&r->damage.last_amount,4);rf_scene_vehicle_damage[2]=r->damage.damage_calls;
    rf_scene_vehicle_damage[3]=r->damage.damage.destroyed;rf_scene_vehicle_damage[4]=r->damage.eject_pending;
    rf_scene_vehicle_damage[5]=r->damage.exit_count;rf_scene_vehicle_damage[6]=r->damage.damage.state.responsible_handle;
    rf_scene_vehicle_damage[7]=r->entry.host.alive;
    if(rf_scene_vehicle_enabled==3){
        ++rf_scene_jeep_seats[0];
        if(r->entry.session.active)++rf_scene_jeep_seats[scene_jeep_can_fire(&r->jeep_seat)?2:1];
        rf_scene_jeep_seats[4]=r->jeep_seat.role;rf_scene_jeep_seats[5]=(uint32_t)s->driller->seat;
        rf_scene_jeep_seats[6]=command.controlled;
        rf_scene_jeep_seats[7]=r->entry.session.active&&r->entry.host.alive&&scene_jeep_can_fire(&r->jeep_seat);
        scene_jeep_npc_gunner_probe(&r->entry,r->jeep_seat.role);
    }
    r->last_frame=frame;++rf_scene_vehicle_state[0];rf_scene_vehicle_state[3]=r->entry.session.active;
    rf_scene_vehicle_state[4]=r->support.hits;rf_scene_vehicle_state[5]=0;
    memcpy(rf_scene_vehicle_state+6,r->physics.state.position,12);memcpy(rf_scene_vehicle_state+9,r->physics.state.velocity,12);
    rf_scene_vehicle_state[12]=r->entry.registration.handle;rf_scene_vehicle_state[13]=was;
    scene_apc_exit_observe(&r->entry,frame);
    return RF_OK;
fail:rf_scene_vehicle_state[5]=(uint32_t)status;fprintf(stderr,"DRILLER_RUNTIME_ERROR frame%u stage%u status%d\n",frame,stage,status);return status;
}
static int scene_driller_runtime_close(scene_stream *s)
{
    int status=RF_OK;if(!s->driller_runtime)return RF_OK;
    status=scene_driller_audio_close(&s->driller_runtime->audio);
    scene_driller_damage_runtime_close(&s->driller_runtime->damage);
    {int closed=scene_driller_entry_exit_close(&s->driller_runtime->entry,1);if(!status)status=closed;}
    free(s->driller_runtime);s->driller_runtime=NULL;return status;
}
#endif
