#ifndef SCENE_DRILLER_ENTRY_EXIT_INC
#define SCENE_DRILLER_ENTRY_EXIT_INC
/* Include after driller resources/physics, vehicle possession and collision.
 * Generic single-driver ownership, with a second Jeep slot owned by the
 * NPC-driver/player-gunner adapter. Side/front/back exit order is a port policy,
 * not recovered exit-tag semantics. Full body sweeps use world + movers; the
 * enemy-free profile is required (other actors are not exit obstacles here). */
typedef struct scene_driller_entry_exit {
    rf_object_registry *objects;rf_entity_registry *entities;
    rf_entity_view *player_view;rf_physics_body *player_body;
    const scene_driller_resources *resource;scene_driller_physics *physics;
    scene_vehicle_collision collision;
    rf_entity_view view;rf_registered_entity_view registration;
    scene_vehicle_possession session;scene_vehicle_player_control player;
    scene_vehicle_host_control host;
    union {
        struct {int32_t occupant,gunner_occupant;};
        int32_t occupants[2];
    };
    uint32_t held;float eye_offset[3];
    /* Sub-only admission; provider checks every actual player sphere and the
     * swept wet path. Missing provider rejects boarding/exit, never flies dry. */
    int (*water_path)(void *,const rf_collision_body_sphere *,uint32_t,
        const scene_vehicle_pose *,const scene_vehicle_pose *,uint32_t *);
    void *water_context;
    /* Optional scene admission for the final actual-body exit destination. */
    int (*exit_clear)(void *,const scene_vehicle_pose *,uint32_t *);
    void *exit_context;
} scene_driller_entry_exit;
uint32_t rf_scene_submarine_boarding[8]; /* attempts,range,dry,solid,allowed,player xyz bits */
static void scene_driller_point(const float origin[3],const float basis[9],const float local[3],float out[3])
{
    uint32_t i,j;for(i=0;i<3;++i){out[i]=origin[i];for(j=0;j<3;++j)out[i]+=local[j]*basis[j*3+i];}
}
/* Binary occupant basis-lock: interface_2 supplies position, not orientation.
 * Keep the player's independent body basis while the host moves underneath. */
static void scene_driller_seat_basis(const scene_driller_entry_exit *c,int32_t seat,
    const float tag[12],float basis[9])
{
    if(!strcmp(c->resource->model,"Jeep01.v3m") && seat>=0 &&
       (uint32_t)seat<c->resource->tags.count &&
       !strcmp(c->resource->tags.items[seat].name,"interface_2"))
        memcpy(basis,c->player_body->state.orientation,36);
    else memcpy(basis,tag,36);
}
static int scene_driller_entry_seat(void *context,uint32_t host,scene_vehicle_pose *pose)
{
    scene_driller_entry_exit *c=context;float tag[12],offset[3];uint32_t i;int status;
    if(host!=c->registration.handle || rf_entity_lookup(c->entities,(int32_t)host)!=&c->view)return RF_NOT_FOUND;
    status=scene_driller_seat_pose(c->resource,c->physics->state.position,c->physics->state.orientation,tag);if(status)return status;
    scene_driller_seat_basis(c,c->resource->seat,tag,pose->basis);memcpy(pose->position,tag+9,12);
    scene_driller_point((const float[3]){0,0,0},pose->basis,c->eye_offset,offset);
    for(i=0;i<3;++i)pose->position[i]-=offset[i];
    return RF_OK;
}
static int scene_driller_player_shape(scene_driller_entry_exit *c,scene_vehicle_collision *query,
    rf_collision_body_sphere spheres[64])
{
    uint32_t i;float r=0;
    if(!c->player_body->spheres.items || !c->player_body->spheres.count || c->player_body->spheres.count>64)return RF_RANGE;
    *query=c->collision;
    for(i=0;i<c->player_body->spheres.count;++i){float distance;
        memcpy(spheres[i].center,c->player_body->spheres.items[i].center,12);spheres[i].radius=c->player_body->spheres.items[i].radius;
        distance=sqrtf(spheres[i].center[0]*spheres[i].center[0]+spheres[i].center[1]*spheres[i].center[1]+spheres[i].center[2]*spheres[i].center[2])+spheres[i].radius;
        if(distance>r)r=distance;}
    query->spheres=spheres;query->count=c->player_body->spheres.count;query->radius=r;
    return scene_vehicle_collision_valid(query);
}
#include "scene_vehicle_seated_clearance.inc"
static uint32_t scene_driller_seated_head(const scene_driller_entry_exit *c)
{return !strcmp(c->resource->model,"Jeep01.v3m")||!strcmp(c->resource->model,"APC.v3m");}
/* First Jeep admission: attempts, stage(range1/path2), allowed, status,
 * distance squared, radius, player xyz, host xyz. Floats are bitcasts. */
uint32_t rf_scene_jeep_entry_probe[12];
/* Last ordinary entry admission: calls,frame,profile,host,allowed,status,
 * distance squared,use radius,playerXYZ,seatXYZ,stage(range1/path2),reserved.
 * Observational only; retain through close and reset when a new entry opens. */
uint32_t rf_scene_vehicle_entry_probe[16];
static int scene_driller_entry_allowed(void *context,uint32_t player,uint32_t host,uint32_t *allowed)
{
    scene_driller_entry_exit *c=context;scene_vehicle_pose seat;scene_vehicle_collision query;
    rf_collision_body_sphere spheres[64];float d2=0,range=c->resource->physics.authored.use_radius;uint32_t i;int status;
    uint32_t *trace=rf_scene_vehicle_entry_probe;++trace[0];memset(trace+1,0,15*sizeof(*trace));
#ifdef RF_IMAGE_XBOX_NATIVE
    trace[1]=rf_scene_profile_stage[0];trace[2]=rf_scene_vehicle_enabled;
#else
    /* Standalone adapter consumers have no scene clock/profile globals. */
    trace[1]=UINT32_MAX;
    trace[2]=!strcmp(c->resource->model,"APC.v3m")?2:!strcmp(c->resource->model,"Jeep01.v3m")?3:
        !strcmp(c->resource->model,"sub.v3m")?4:!strcmp(c->resource->model,"Fighter01.v3m")?5:1;
#endif
    trace[3]=host;memcpy(trace+7,&range,4);memcpy(trace+8,c->player_body->state.position,12);
    *allowed=0;
    if(rf_entity_lookup(c->entities,(int32_t)player)!=c->player_view || host!=c->registration.handle){trace[5]=(uint32_t)RF_NOT_FOUND;return RF_NOT_FOUND;}
    if(c->view.flags_7c&0x4000u)return RF_OK;
    if(c->resource->physics.authored.movement_index==7){
        ++rf_scene_submarine_boarding[0];
        memcpy(rf_scene_submarine_boarding+5,c->player_body->state.position,12);
    }
    for(i=0;i<3;++i){float d=c->player_body->state.position[i]-c->physics->state.position[i];d2+=d*d;}
    trace[14]=1;memcpy(trace+6,&d2,4);
    uint32_t sample=!strcmp(c->resource->model,"Jeep01.v3m") && !rf_scene_jeep_entry_probe[0];
    if(!strcmp(c->resource->model,"Jeep01.v3m"))++rf_scene_jeep_entry_probe[0];
    if(sample){rf_scene_jeep_entry_probe[1]=1;memcpy(rf_scene_jeep_entry_probe+4,&d2,4);memcpy(rf_scene_jeep_entry_probe+5,&range,4);
        memcpy(rf_scene_jeep_entry_probe+6,c->player_body->state.position,12);memcpy(rf_scene_jeep_entry_probe+9,c->physics->state.position,12);}
    if(!strcmp(c->resource->model,"Jeep01.v3m"))printf("JEEP_USE distance %.6g radius %.6g player %.6g %.6g %.6g host %.6g %.6g %.6g\n",sqrtf(d2),range,c->player_body->state.position[0],c->player_body->state.position[1],c->player_body->state.position[2],c->physics->state.position[0],c->physics->state.position[1],c->physics->state.position[2]);
    if(!isfinite(range) || range<=0 || d2>range*range){
        if(c->resource->physics.authored.movement_index==7)++rf_scene_submarine_boarding[1];
        return RF_OK;
    }
    status=scene_driller_entry_seat(c,host,&seat);if(status){trace[5]=(uint32_t)status;return status;}
    memcpy(trace+11,seat.position,12);
    status=scene_driller_player_shape(c,&query,spheres);if(status){trace[5]=(uint32_t)status;return status;}
    trace[14]=2;
    if(c->resource->physics.authored.movement_index==7){
        scene_vehicle_pose from;uint32_t wet=0;
        if(!c->water_path){trace[5]=(uint32_t)RF_RANGE;return RF_RANGE;}
        memcpy(from.position,c->player_body->state.position,12);memcpy(from.basis,c->player_body->state.orientation,36);
        status=c->water_path(c->water_context,spheres,query.count,&from,&seat,&wet);
        if(status || !wet){if(!wet)++rf_scene_submarine_boarding[2];trace[5]=(uint32_t)status;return status;}
    }
    if(scene_driller_seated_head(c)){
        scene_vehicle_pose from;memcpy(from.position,c->player_body->state.position,12);memcpy(from.basis,c->player_body->state.orientation,36);
        status=scene_vehicle_seated_path(c,&from,&seat,allowed);
        if(sample){rf_scene_jeep_entry_probe[1]=2;rf_scene_jeep_entry_probe[2]=*allowed;rf_scene_jeep_entry_probe[3]=(uint32_t)status;}
    }else status=scene_vehicle_collision_exit_path(&query,c->player_body->state.position,seat.position,c->player_body->state.orientation,allowed);
    trace[4]=*allowed;trace[5]=(uint32_t)status;
    if(c->resource->physics.authored.movement_index==7 && !status){
        ++rf_scene_submarine_boarding[*allowed?4:3];
    }
    if(!strcmp(c->resource->model,"Jeep01.v3m"))printf("JEEP_ENTRY_SWEEP %d %u target %.6g %.6g %.6g\n",status,*allowed,seat.position[0],seat.position[1],seat.position[2]);
    return status;
}
static int scene_driller_excludes_host(scene_driller_entry_exit *c,const scene_vehicle_collision *body,const scene_vehicle_pose *pose)
{
    uint32_t i,j,k;
    for(i=0;i<body->count;++i){float player[3];scene_driller_point(pose->position,pose->basis,body->spheres[i].center,player);
        for(j=0;j<c->physics->sphere_count;++j){float host[3],d2=0,r=body->spheres[i].radius+c->physics->collision[j].radius+.02f;
            scene_driller_point(c->physics->state.position,c->physics->state.orientation,c->physics->collision[j].center,host);
            for(k=0;k<3;++k){float d=player[k]-host[k];d2+=d*d;}if(d2<r*r)return 0;}}
    return 1;
}
static int scene_driller_exit_pose(void *context,uint32_t player,uint32_t host,scene_vehicle_pose *pose,uint32_t *clear)
{
    scene_driller_entry_exit *c=context;scene_vehicle_collision body;rf_collision_body_sphere spheres[64];
    scene_vehicle_pose seat,candidate;uint32_t direction,i,found;int status;
    *clear=0;if(player!=c->player.handle || host!=c->host.handle)return RF_NOT_FOUND;
    status=scene_driller_player_shape(c,&body,spheres);if(status)return status;
    status=scene_driller_entry_seat(c,host,&seat);if(status)return status;
    /* Use an upright saved body basis for exit; the seat camera may pitch. */
    memcpy(candidate.basis,c->session.saved.pose.basis,36);
    printf("DRILLER_EXIT_BEGIN seat %.6g %.6g %.6g bodyradius %.6g host %.6g %.6g %.6g\n",seat.position[0],seat.position[1],seat.position[2],body.radius,c->physics->state.position[0],c->physics->state.position[1],c->physics->state.position[2]);
    for(direction=0;direction<4;++direction){
        uint32_t axis=direction<2?0:2;float sign=(direction&1)?-1:1,extent=0,local[3]={0},end[3];
        rf_collision_body_query q;rf_geometry_body_hit hit={0};float drop;
        for(i=0;i<c->physics->sphere_count;++i){float edge=sign*c->physics->collision[i].center[axis]+c->physics->collision[i].radius;if(edge>extent)extent=edge;}
        local[axis]=sign*(extent+body.radius+.15f);
        scene_driller_point(c->physics->state.position,c->physics->state.orientation,local,candidate.position);
        candidate.position[1]=seat.position[1];
        if(scene_driller_seated_head(c)){
            uint32_t head_clear;
            /* Low seated body origins are below standing floor height. Probe
             * a standing exit from seat-eye height, then establish full-body
             * floor support below; head transit still rejects walls/ceilings. */
            candidate.position[1]+=fabsf(c->eye_offset[1]);
            status=scene_vehicle_seated_path(c,&seat,&candidate,&head_clear);if(status)return status;
            found=!head_clear;
        }else{
            scene_vehicle_collision_body(&body,seat.position,candidate.position,candidate.basis,&q);
            status=scene_vehicle_collision_query(&body,&q,&hit,&found);if(status)return status;
        }
        printf("DRILLER_EXIT_PATH %u candidate %.6g %.6g %.6g blocked %u fraction %.6g normal %.6g %.6g %.6g sphere %u\n",direction,candidate.position[0],candidate.position[1],candidate.position[2],found,hit.contact.fraction,hit.contact.normal[0],hit.contact.normal[1],hit.contact.normal[2],hit.sphere);
        if(found)continue;
        if(c->resource->physics.authored.movement_index==7){
            uint32_t wet=0;
            if(!c->water_path)return RF_RANGE;
            status=c->water_path(c->water_context,spheres,body.count,&seat,&candidate,&wet);
            if(status)return status;
            if(!wet || !scene_driller_excludes_host(c,&body,&candidate))continue;
            /* Swimming exit needs clear submerged body space, not a floor.
             * The ordinary swim update reacquires liquid after publication. */
            if(c->exit_clear){status=c->exit_clear(c->exit_context,&candidate,&found);if(status)return status;if(!found)continue;}
            *pose=candidate;*clear=1;return RF_OK;
        }
        /* Bounded full-body downward sweep establishes walkable support before
         * publishing an exit. No floor means no exit, never a blind teleport. */
        memcpy(end,candidate.position,12);drop=c->physics->radius*2+4;end[1]-=drop;
        scene_vehicle_collision_body(&body,candidate.position,end,candidate.basis,&q);
        status=scene_vehicle_collision_query(&body,&q,&hit,&found);if(status)return status;
        printf("DRILLER_EXIT_FLOOR %u found %u fraction %.6g normal %.6g %.6g %.6g\n",direction,found,hit.contact.fraction,hit.contact.normal[0],hit.contact.normal[1],hit.contact.normal[2]);
        if(!found || hit.contact.normal[1]<.5f || hit.contact.fraction<0 || hit.contact.fraction>=1 ||
           (hit.solid==UINT32_MAX&&hit.room==UINT32_MAX))continue; /* Only world/mover support. */
        candidate.position[1]-=drop*hit.contact.fraction;candidate.position[1]+=.01f;
        if(!scene_driller_excludes_host(c,&body,&candidate)){printf("DRILLER_EXIT_HOST_BLOCK %u\n",direction);continue;}
        if(c->exit_clear){status=c->exit_clear(c->exit_context,&candidate,&found);if(status)return status;if(!found)continue;}
        printf("DRILLER_EXIT_ACCEPT %u %.6g %.6g %.6g\n",direction,candidate.position[0],candidate.position[1],candidate.position[2]);
        *pose=candidate;*clear=1;return RF_OK;
    }
    return RF_OK;
}
static int scene_driller_entry_exit_open(scene_driller_entry_exit *c,rf_object_registry *objects,
    rf_entity_registry *entities,rf_entity_view *player,rf_physics_body *body,
    const scene_driller_resources *resource,scene_driller_physics *physics,
    const scene_vehicle_collision *collision,const float eye_offset[3])
{
    int status;
    if(!c || !objects || !entities || !player || !body || !resource || !physics || !collision || !eye_offset || c->registration.view)return RF_RANGE;
    if(rf_entity_lookup(entities,player->handle)!=player)return RF_NOT_FOUND;
    memset(rf_scene_vehicle_entry_probe,0,sizeof(rf_scene_vehicle_entry_probe));
    memset(c,0,sizeof(*c));c->objects=objects;c->entities=entities;c->player_view=player;c->player_body=body;
    if(resource->physics.authored.movement_index==7)memset(rf_scene_submarine_boarding,0,sizeof(rf_scene_submarine_boarding));
    c->resource=resource;c->physics=physics;c->collision=*collision;memcpy(c->eye_offset,eye_offset,12);
    c->view.handle=-1;c->view.type=0;c->view.class_type=(int32_t)resource->physics.authored.use_kind;
    c->view.linked_handle=-1;c->view.weapons[0]=c->view.weapons[1]=-1;c->view.base_speed=resource->movement.speed;
    c->occupant=c->gunner_occupant=-1;c->view.occupants=c->occupants;
    c->view.occupant_count=!strcmp(resource->model,"Jeep01.v3m")?2:1;
    status=rf_entity_view_register(objects,entities,&c->view,&c->registration);if(status)return status;
    c->host.handle=c->registration.handle;c->host.driver=UINT32_MAX;c->host.alive=1;
    c->player.handle=(uint32_t)player->handle;c->player.host=UINT32_MAX;c->player.control=(uint32_t)player->handle;
    return RF_OK;
}
/* Call once per input sample. changed=1 means publish returned player.pose,
 * zero body velocity, and cancel player impact/support accumulators. On entry
 * skip ordinary actor physics; follow pose() after each rigid host step. */
static int scene_driller_entry_exit_use(scene_driller_entry_exit *c,uint32_t held,uint32_t alive,uint32_t *changed)
{
    scene_vehicle_possession_ops ops={scene_driller_entry_allowed,scene_driller_entry_seat,scene_driller_exit_pose,c};
    uint32_t edge;int status=RF_OK;
    if(!c || !changed || !c->registration.view)return RF_RANGE;
    edge=held && !c->held;c->held=held!=0;*changed=0;
    if(!c->session.active){
        if(!edge || !alive)return RF_OK;
        if(c->player_view->linked_handle!=-1)return RF_OK;
        memcpy(c->player.pose.position,c->player_body->state.position,12);memcpy(c->player.pose.basis,c->player_body->state.orientation,36);
        c->player.weapon=c->player_view->weapons[0];c->player.control_flags=c->player_view->flags_810;
        status=scene_vehicle_possess(&c->session,&c->player,&c->host,alive,&ops,changed);
    }else if(!alive || edge || !c->host.alive){
        if(edge && alive && c->host.alive && (c->view.flags_814&0x80u))return RF_OK;
        status=scene_vehicle_unpossess(&c->session,&c->player,&c->host,alive?0:2,&ops,changed);
    }
    if(status)return status;
    if(*changed){c->player_view->linked_handle=c->session.active?(int32_t)c->host.handle:-1;
        c->occupant=c->session.active?(int32_t)c->player.handle:-1;}
    return RF_OK;
}
static int scene_driller_entry_exit_pose(scene_driller_entry_exit *c,scene_vehicle_pose *body,float camera[12])
{
    int status;if(!c || !body || !camera || !c->session.active)return RF_NOT_FOUND;
    status=scene_driller_entry_seat(c,c->host.handle,body);if(status)return status;
    status=scene_driller_seat_pose(c->resource,c->physics->state.position,c->physics->state.orientation,camera);if(status)return status;
    memcpy(camera,body->basis,36);c->player.pose=*body;return RF_OK;
}
/* Teardown only after safe exit/death, or when the whole level is discarded.
 * It never supplies a living player's in-level exit position. */
static int scene_driller_entry_exit_close(scene_driller_entry_exit *c,uint32_t level_discard)
{
    int status;if(!c)return RF_RANGE;if(!c->registration.view)return RF_OK;
    if(c->session.active && !level_discard)return RF_RANGE;
    if(c->player_view && c->player_view->linked_handle==(int32_t)c->host.handle)c->player_view->linked_handle=-1;
    status=rf_entity_view_unregister(c->objects,c->entities,&c->registration);if(status)return status;
    memset(c,0,sizeof(*c));return RF_OK;
}
#endif
