#ifndef RF_SCENE_SECONDARY_VEHICLE_RUNTIME_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_RUNTIME_INCLUDED
#include "scene_vehicle_static_edges.inc"
#include "scene_secondary_vehicle_order.inc"
/* Independent authored APC/submarine/Fighter/Masako owners keep their kind11 handle, damage
 * and draw identity. A lazy pointer bank uses the existing passive owner
 * bound; only genuine authored orders allocate rigid/navigation state. Slots
 * and runtime addresses remain stable, including paused and retired owners.
 * APC routing retains its recovered graph/four-node-prefix policy. Submarine/Fighter
 * point/player steering uses the actual wet/dry hull motion policy. */
typedef struct scene_secondary_vehicle_query {
    scene_stream *stream;uint32_t handle;
} scene_secondary_vehicle_query;
typedef struct scene_secondary_vehicle_runtime {
    scene_driller_physics physics;
    scene_vehicle_collision collision;
    scene_driller_support_context support;
    scene_fighter_motion fighter;scene_submarine_motion submarine;
    rf_liquid_rooms liquids;
    rf_vehicle_submersible_parameters flight_parameters;
    scene_secondary_vehicle_query query;
    uint32_t uid,handle,slot,profile,retired,last_frame,scripted_physics,steps;
    struct {uint32_t active,event,index,retry,kind;float target[3];} route;
    campaign_npc_route navigation;
} scene_secondary_vehicle_runtime;
static scene_secondary_vehicle_runtime **campaign_secondary_vehicles;
static uint32_t campaign_secondary_vehicle_capacity,campaign_secondary_vehicle_count;
static int scene_secondary_vehicle_carry_clear(scene_stream *,const scene_secondary_vehicle_runtime *,
    const rf_vehicle_rigid_state *,uint32_t *);
uint32_t rf_scene_secondary_vehicle[16]; /* admissions,steps,route requests,found,misses,node advances,arrivals,OFF,paused,removed,UID,handle,last frame,status,count,index */
uint32_t rf_scene_secondary_vehicle_edges[10]; /* queries,hits,UID,handle,room,face,sphere,fraction,status,reserved */
/* Last actual pursuit command: count,UID,handle,player handle,prior frame,
 * active,kind,player present; authored3,sampled3,command6,start3,basis9.
 * Read-only evidence, never an input to steering or checkpoint state. */
uint32_t rf_scene_secondary_vehicle_pursuit[32];

static scene_secondary_vehicle_runtime *scene_secondary_vehicle_find(uint32_t handle)
{
    for(uint32_t i=0;campaign_secondary_vehicles&&i<campaign_secondary_vehicle_capacity;i++)
        if(campaign_secondary_vehicles[i]&&campaign_secondary_vehicles[i]->handle==handle)
            return campaign_secondary_vehicles[i];
    return NULL;
}
static scene_secondary_vehicle_runtime *scene_secondary_vehicle_find_uid(uint32_t uid)
{
    for(uint32_t i=0;campaign_secondary_vehicles&&i<campaign_secondary_vehicle_capacity;i++)
        if(campaign_secondary_vehicles[i]&&campaign_secondary_vehicles[i]->uid==uid)
            return campaign_secondary_vehicles[i];
    return NULL;
}

/* Profile and passive resource-kind are different namespaces. Keep the
 * actual submarine class4/kind0, Fighter class5/kind1 and Masako class6/kind2
 * distinct. Class6 never participates in player boarding/profile exchange. */
static uint32_t scene_secondary_vehicle_profile(uint32_t kind)
{return kind==4?2:kind==0?4:kind==1?5:kind==2?6:0;}
static uint32_t scene_secondary_vehicle_kind(uint32_t profile)
{return profile==2?4:profile==4?0:profile==5?1:profile==6?2:UINT32_MAX;}
static const char *scene_secondary_vehicle_class(uint32_t profile)
{return profile==2?"APC":profile==4?"sub":profile==5?"Fighter01":profile==6?"masako_fighter":"";}

static scene_passive_vehicle *scene_secondary_vehicle_owner(const scene_secondary_vehicle_runtime *r)
{
    scene_passive_vehicle *owner;
    if(!r||!campaign_passive_vehicles||r->slot>=campaign_passive_vehicle_count)return NULL;
    owner=campaign_passive_vehicles+r->slot;
    if(owner->object_kind!=11||owner->handle!=r->handle||owner->uid!=r->uid||
       owner->damage.state.effects.handle!=r->handle||
       (!scene_secondary_vehicle_profile(owner->resource_kind)||scene_secondary_vehicle_profile(owner->resource_kind)!=r->profile)||
       rf_object_registry_lookup(&campaign_registry,r->handle)!=owner)return NULL;
    return owner;
}
static uint32_t scene_secondary_vehicle_retired(uint32_t handle)
{
    const scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find(handle);
    return r&&r->retired&&scene_secondary_vehicle_owner(r)!=NULL;
}
static uint32_t scene_secondary_vehicle_controller_owned(uint32_t handle)
{
    if(campaign_group_runtime.count&&!campaign_controller_views)return 1;
    for(uint32_t i=0;i<campaign_group_runtime.count;i++){
        const rf_group_controller_view *view=campaign_controller_views+i;
        if(view->general_count&&!view->general_handles)return 1;
        for(uint32_t j=0;j<view->general_count;j++)if(view->general_handles[j]==handle)return 1;
    }
    return 0;
}
static uint32_t scene_secondary_vehicle_empty(const scene_passive_vehicle *owner)
{
    /* group_owned is immutable authored provenance, not live ownership.
     * Masako leaves Hanger Lift001 through original Detach18377. Keep RFVA
     * provenance, but require both attached=0 and absence from every actual
     * controller before its independent Goto may own the same chassis. */
    return owner&&(!owner->group_owned||owner->resource_kind==2)&&!owner->attached&&
        (!owner->group_owned||!scene_secondary_vehicle_controller_owned(owner->handle))&&owner->seat_driver==UINT32_MAX&&
        owner->seat_occupants[0]==-1&&owner->seat_occupants[1]==-1&&
        campaign_authored_vehicle_handle!=owner->handle;
}
/* Promotion still needs this conservative dependency check. Independent
 * motion instead transports valid grounded riders through the existing
 * accepted-interval support path; a rider never becomes a second driver. */
static uint32_t scene_secondary_vehicle_has_rider(uint32_t handle)
{
    if(campaign_support_handle==handle&&campaign_player_object.view==&campaign_player_view&&
       scene_actor_body.allocated_bytes&&campaign_player_damage.state.effects.health>0&&
       !(campaign_player_view.flags_7c&(2u|0x4000u))&&
       rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object&&
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view)return 1;
    for(uint32_t i=0;campaign_npc_bodies&&i<campaign_npc_body_count;i++) {
        const campaign_npc_body *actor=campaign_npc_bodies+i;
        if(actor->support.handle==handle&&actor->registration.view==&actor->view&&
           actor->body.allocated_bytes&&actor->damage.effects.health>0&&
           !(actor->object_flags&(2u|0x4000u))&&
           rf_object_registry_lookup(&campaign_registry,actor->registration.handle)==&actor->registration&&
           rf_entity_lookup(&campaign_entities,(int32_t)actor->registration.handle)==&actor->view)return 1;
    }
    return 0;
}
static uint32_t scene_secondary_vehicle_supports(uint32_t handle)
{
    const scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find(handle);
    const scene_passive_vehicle *p=r?scene_secondary_vehicle_owner(r):NULL;
    return p&&!r->retired&&scene_secondary_vehicle_empty(p)&&!p->damage.destroyed&&
        p->damage.state.effects.health>0&&!(p->damage.object_flags&(2u|0x4000u));
}
/* Only a complete existing support relationship removes an actor from its
 * own host sweep. Stale handles, airborne actors, seats and frozen NPC bodies
 * remain ordinary obstacles. No proximity-derived passenger exemption. */
static uint32_t scene_secondary_vehicle_rider(uint32_t host,uint32_t actor)
{
    if(!host||host==UINT32_MAX||actor==host)return 0;
    if(!scene_secondary_vehicle_supports(host))return 0;
    if(actor==campaign_player_object.handle)return campaign_support_handle==host&&
        rf_scene_actor_landing[1]==1&&(scene_actor_body.state.flags&0x400000u)&&
        !campaign_piece_support.tag&&scene_actor_body.allocated_bytes&&
        campaign_player_damage.state.effects.health>0&&campaign_player_view.linked_handle==-1&&
        !(campaign_player_view.flags_7c&(2u|0x4000u))&&campaign_player_object.view==&campaign_player_view&&
        rf_object_registry_lookup(&campaign_registry,actor)==&campaign_player_object&&
        rf_entity_lookup(&campaign_entities,(int32_t)actor)==&campaign_player_view;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *p=campaign_npc_bodies+i;
        if(p->registration.handle!=actor)continue;
        return p->support.handle==host&&p->support.material>=0&&p->body.allocated_bytes&&
            (p->body.state.flags&0x400000u)&&!(p->body.state.flags&1u)&&!p->piece_support.tag&&
            p->movement_slot<16&&campaign_modes[p->movement_slot].index==1&&
            !campaign_npc_physics_suspended(p)&&p->damage.effects.health>0&&p->view.linked_handle==-1&&
            !(p->object_flags&(2u|8u|0x4000u))&&p->registration.view==&p->view&&
            rf_object_registry_lookup(&campaign_registry,actor)==&p->registration&&
            rf_entity_lookup(&campaign_entities,(int32_t)actor)==&p->view;
    }
    return 0;
}
static const scene_driller_resources *scene_secondary_vehicle_resource(const scene_stream *stream,uint32_t profile)
{
    uint32_t kind=scene_secondary_vehicle_kind(profile);
    if(!stream||kind==UINT32_MAX)return NULL;
    if(stream->passive_vehicle_resources[kind])return stream->passive_vehicle_resources[kind];
    return campaign_selected_vehicle_resource_kind()==kind?stream->driller:NULL;
}
static int scene_secondary_vehicle_collision_query(void *context,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *found)
{
    const scene_secondary_vehicle_query *owner=context;
    const scene_secondary_vehicle_runtime *runtime;int status;
    if(!owner)return RF_RANGE;
    status=scene_vehicle_owner_collision_query(owner->stream,owner->handle,query,hit,found);if(status)return status;
    runtime=scene_secondary_vehicle_find(owner->handle);
    /* The recovered face sweep only visits edges after a forward plane hit.
     * Independent free motion also needs tangent/outward edge entry to remain a
     * valid full hull at its next save. Keep this practical complement local
     * to the independent submarine/Fighter; ordinary face and other-owner hits stay. */
    if(runtime&&scene_secondary_vehicle_flight_profile(runtime->profile)){
        uint32_t previous=*found,*p=rf_scene_secondary_vehicle_edges;
        float fraction=previous?hit->contact.fraction:query->limit;
        ++p[0];status=scene_vehicle_static_edges_compose(owner->stream->collision,query,hit,found);
        if(status){p[8]=(uint32_t)status;return status;}
        if(*found&&(!previous||hit->contact.fraction<fraction)){
            ++p[1];p[2]=runtime->uid;p[3]=runtime->handle;p[4]=hit->room;p[5]=hit->face;p[6]=hit->sphere;
            memcpy(p+7,&hit->contact.fraction,4);
        }
    }
    return RF_OK;
}
/* Pure pointer rebasing: safe for a staged candidate and for a committed world
 * overlay replacement; cached support and full rigid state remain untouched. */
static int scene_secondary_vehicle_bind(scene_stream *stream,scene_secondary_vehicle_runtime *r)
{
    if(!stream||!stream->collision||!r||!r->physics.sphere_count)return RF_RANGE;
    if(r->profile==2){if(!r->physics.spring_count)return RF_RANGE;}
    else if(scene_secondary_vehicle_flight_profile(r->profile)){
        if(r->physics.spring_count||!stream->liquid_rooms||!stream->swim_room_count)return RF_RANGE;
    }else return RF_RANGE;
    r->query.stream=stream;r->query.handle=r->handle;
    r->collision.world=stream->collision;r->collision.movers=&campaign_movers;
    r->collision.spheres=r->physics.collision;r->collision.count=r->physics.sphere_count;
    r->collision.radius=r->physics.radius;r->collision.flags=4;
    r->collision.query=scene_secondary_vehicle_collision_query;r->collision.query_context=&r->query;
    r->support.stream=stream;r->support.collision=&r->collision;r->support.physics=&r->physics;
    if(scene_secondary_vehicle_flight_profile(r->profile)){
        r->liquids.items=stream->liquid_rooms;r->liquids.count=stream->swim_room_count;r->liquids.bytes=stream->swim_bytes;
        r->fighter.collision=r->collision;r->fighter.liquids=&r->liquids;
        r->submarine.collision=r->collision;r->submarine.liquids=&r->liquids;
    }
    return RF_OK;
}
static int scene_secondary_vehicle_rebind(scene_stream *stream)
{
    for(uint32_t i=0;campaign_secondary_vehicles&&i<campaign_secondary_vehicle_capacity;i++){
        if(!campaign_secondary_vehicles[i])continue;
        int status=scene_secondary_vehicle_bind(stream,campaign_secondary_vehicles[i]);if(status)return status;
    }
    return RF_OK;
}

static void scene_secondary_vehicle_route_rebase(scene_secondary_vehicle_runtime *to,
    const scene_secondary_vehicle_runtime *from)
{
    uint32_t i;
    for(i=0;i<to->navigation.retained.count&&i<4;i++) {
        if(to->navigation.retained.nodes[i]==&from->navigation.start)
            to->navigation.retained.nodes[i]=&to->navigation.start;
        else if(to->navigation.retained.nodes[i]==&from->navigation.goal)
            to->navigation.retained.nodes[i]=&to->navigation.goal;
    }
}
static int scene_secondary_vehicle_prepare(scene_stream *stream,uint32_t slot,uint32_t restoring,
    scene_secondary_vehicle_runtime *out)
{
    scene_secondary_vehicle_runtime value={0};scene_passive_vehicle *owner;
    const scene_driller_resources *resource;int status;
    if(!stream||!out||restoring>1||slot>=campaign_passive_vehicle_count||!campaign_passive_vehicles)return RF_RANGE;
    owner=campaign_passive_vehicles+slot;
    /* On a fresh boot the authored boss is still lift-attached. Candidate
     * RFSV decode may borrow its class/body without detaching anything. The
     * mandatory RFVA2 join later requires this saved owner to be detached;
     * RFVA publication rebuilds controller membership before RFSV commit. */
    uint32_t candidate=restoring&&owner->resource_kind==2&&owner->group_owned&&
        owner->seat_driver==UINT32_MAX&&owner->seat_occupants[0]==-1&&owner->seat_occupants[1]==-1&&
        campaign_authored_vehicle_handle!=owner->handle;
    if(!scene_secondary_vehicle_profile(owner->resource_kind)||owner->object_kind!=11||
       (!candidate&&!scene_secondary_vehicle_empty(owner))||
       owner->damage.state.effects.handle!=owner->handle||
       rf_object_registry_lookup(&campaign_registry,owner->handle)!=owner)return RF_NOT_FOUND;
    value.uid=owner->uid;value.handle=owner->handle;value.slot=slot;
    value.profile=scene_secondary_vehicle_profile(owner->resource_kind);value.last_frame=UINT32_MAX;
    resource=scene_secondary_vehicle_resource(stream,value.profile);if(!resource)return RF_NOT_FOUND;
    status=scene_vehicle_profile_physics_prepare(value.profile,resource,owner->pose.position,
        owner->pose.input_matrix,scene_gravity.acceleration,&value.physics);if(status)return status;
    if(scene_secondary_vehicle_flight_profile(value.profile))value.flight_parameters=(rf_vehicle_submersible_parameters){resource->physics.authored.mass,
        resource->movement.speed,resource->movement.acceleration,resource->rotation.maximum_velocity,
        resource->rotation.acceleration,2};
    memcpy(value.physics.state.velocity,owner->velocity,12);
    value.scripted_physics=owner->scripted_physics;
    if(value.scripted_physics)value.physics.body_flags=owner->physics_body_flags;
    value.support.material=-1;value.support.support_handle=UINT32_MAX;
    *out=value;return scene_secondary_vehicle_bind(stream,out);
}
/* Require the original parsed record, including its original target UID link.
 * A fabricated event with the same number cannot admit a second simulation. */
static uint32_t scene_secondary_vehicle_event(const rf_level_event *event,const scene_passive_vehicle *owner)
{
    uint32_t i,kind=SCENE_SECONDARY_ORDER_NONE;
    if(!event||!owner)return SCENE_SECONDARY_ORDER_NONE;
    for(i=0;i<campaign_events.count;i++) {
        const rf_level_owned_event *authored=campaign_events.items[i].authored;
        if(authored && &authored->record==event) {
            if(scene_secondary_vehicle_order_event(campaign_events.items+i,scene_secondary_vehicle_profile(owner->resource_kind),
                owner->uid,owner->handle,&kind))return SCENE_SECONDARY_ORDER_NONE;
            return kind;
        }
    }
    return SCENE_SECONDARY_ORDER_NONE;
}
static int scene_secondary_vehicle_script_move(scene_stream *stream,uint32_t handle,
    const rf_level_event *event,uint32_t on,uint32_t *handled)
{
    scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find(handle);scene_passive_vehicle *owner=NULL;
    uint32_t i,kind;int status;
    if(!handled||on>1||!event)return RF_RANGE;*handled=0;
    for(i=0;campaign_passive_vehicles&&i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==handle){owner=campaign_passive_vehicles+i;break;}
    if(!owner||!scene_secondary_vehicle_profile(owner->resource_kind)||
       !(kind=scene_secondary_vehicle_event(event,owner)))return RF_NOT_FOUND;
    *handled=1;
    if(!scene_secondary_vehicle_empty(owner)||rf_object_registry_lookup(&campaign_registry,handle)!=owner)return RF_NOT_FOUND;
    if(!on){if(r&&r->route.event==event->uid){r->route.active=0;++rf_scene_secondary_vehicle[7];}return RF_OK;}
    if((r&&r->retired)||owner->damage.destroyed||owner->damage.state.effects.health<=0||
       (owner->damage.object_flags&2u))return RF_OK;
    for(i=0;i<3;i++)if(!isfinite(event->position[i]))return RF_FORMAT;
    if(!r){
        uint32_t slot=(uint32_t)(owner-campaign_passive_vehicles),allowed=0;
        if(campaign_secondary_vehicles&&(campaign_secondary_vehicle_capacity!=campaign_passive_vehicle_count||
           slot>=campaign_secondary_vehicle_capacity||campaign_secondary_vehicles[slot]))return RF_FORMAT;
        r=calloc(1,sizeof(*r));if(!r)return RF_IO;
        status=scene_secondary_vehicle_prepare(stream,slot,0,r);
        if(!status)status=scene_vehicle_switch_motion_borrow(stream,owner->uid,handle,r->profile,&r->physics.state,&r->physics.body_flags);
        if(status){free(r);return status;}
        if(scene_secondary_vehicle_flight_profile(r->profile)){
            if(r->profile==4)status=scene_submarine_water_body_path(&r->submarine,r->physics.collision,r->physics.sphere_count,
                r->physics.state.position,r->physics.state.orientation,r->physics.state.position,r->physics.state.orientation,&allowed);
            else status=scene_fighter_dry_body_path(&r->fighter,r->physics.collision,r->physics.sphere_count,
                r->physics.state.position,r->physics.state.orientation,r->physics.state.position,r->physics.state.orientation,&allowed);
            if(status||!allowed){free(r);return status?status:RF_NOT_FOUND;}
        }
        if(!campaign_secondary_vehicles){
            scene_secondary_vehicle_runtime **pool=calloc(campaign_passive_vehicle_count,sizeof(*pool));
            if(!pool){free(r);return RF_IO;}
            campaign_secondary_vehicles=pool;campaign_secondary_vehicle_capacity=campaign_passive_vehicle_count;
        }
        owner->physics_body_flags=r->physics.body_flags;owner->scripted_physics=r->scripted_physics;
        campaign_secondary_vehicles[slot]=r;++campaign_secondary_vehicle_count;
        scene_vehicle_switch_motion_commit(owner->uid,handle);
        scene_vehicle_owner_collision_bind(stream,1);++rf_scene_secondary_vehicle[0];
    }
    if(kind!=SCENE_SECONDARY_ORDER_ATTACK)scene_vehicle_attack_cancel(handle);
    r->route.active=1;r->route.event=event->uid;r->route.index=1;r->route.retry=0;r->route.kind=kind;
    memcpy(r->route.target,event->position,12);memset(&r->navigation,0,sizeof(r->navigation));
    rf_scene_secondary_vehicle[10]=r->uid;rf_scene_secondary_vehicle[11]=r->handle;
    return RF_OK;
}
static uint32_t scene_secondary_vehicle_frozen(const scene_secondary_vehicle_runtime *r)
{return r->scripted_physics&&!(r->physics.body_flags&0x80000000u);}
static uint32_t scene_secondary_vehicle_paused(const scene_secondary_vehicle_runtime *r,const scene_passive_vehicle *owner)
{
    return r->retired||scene_secondary_vehicle_frozen(r)||!scene_secondary_vehicle_empty(owner)||
        owner->damage.destroyed||owner->damage.state.effects.health<=0||
        (owner->damage.object_flags&(2u|0x4000u));
}
static int scene_secondary_vehicle_physics_sync(uint32_t handle)
{
    scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find(handle);scene_passive_vehicle *owner;
    if(!r)return RF_OK;
    owner=scene_secondary_vehicle_owner(r);if(!owner)return RF_NOT_FOUND;
    r->scripted_physics=owner->scripted_physics;r->physics.body_flags=owner->physics_body_flags;
    if(scene_secondary_vehicle_frozen(r)) {
        memset(r->physics.state.velocity,0,12);memset(r->physics.state.momentum,0,12);
        memset(owner->velocity,0,12);memset(owner->pose.velocity,0,12);owner->motion_valid=0;
    }
    return RF_OK;
}
static int scene_secondary_vehicle_remove(uint32_t handle,uint32_t *handled)
{
    scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find(handle);scene_passive_vehicle *owner;
    if(!handled)return RF_RANGE;*handled=0;
    if(!r)return RF_OK;*handled=1;
    owner=scene_secondary_vehicle_owner(r);if(!owner)return RF_NOT_FOUND;
    scene_vehicle_loop_audio_remove(handle);
    if(!r->retired)++rf_scene_secondary_vehicle[9];
    scene_vehicle_attack_cancel(handle);scene_vehicle_ai_cancel(handle);
    r->retired=1;r->route.active=0;owner->damage.object_flags|=2u|0x4000u;
    owner->pose.flags|=2u|0x4000u;
    memset(r->physics.state.velocity,0,12);memset(r->physics.state.momentum,0,12);
    memset(r->physics.state.force,0,12);memset(r->physics.state.torque,0,12);
    r->support.material=-1;r->support.support_handle=UINT32_MAX;r->support.hits=0;memset(r->support.velocity,0,12);
    memset(owner->velocity,0,12);memset(owner->pose.velocity,0,12);owner->motion_valid=0;
    return RF_OK;
}
static void scene_secondary_vehicle_nav_changed(const rf_entity_navigation_candidate *candidate,uint32_t on)
{
    for(uint32_t slot=0;campaign_secondary_vehicles&&slot<campaign_secondary_vehicle_capacity;slot++){
        scene_secondary_vehicle_runtime *r=campaign_secondary_vehicles[slot];
        if(!r||r->profile!=2||r->retired||!r->route.active)continue;
        if(on)r->route.retry=0;
        for(uint32_t i=r->route.index;i<r->navigation.retained.count&&i<4;i++)
            if(r->navigation.retained.nodes[i]==candidate){r->navigation.retained.count=0;r->route.index=1;r->route.retry=0;break;}
    }
}
static void scene_secondary_vehicle_close(void)
{
    uint32_t n=campaign_navigation_workspace.count;
    for(uint32_t i=0;campaign_secondary_vehicles&&i<campaign_secondary_vehicle_capacity;i++){
        scene_secondary_vehicle_runtime *r=campaign_secondary_vehicles[i];if(!r)continue;
        if(campaign_navigation_workspace.references) {
            if(campaign_navigation_workspace.references[n].candidate==&r->navigation.goal)
                campaign_navigation_workspace.references[n].candidate=NULL;
            if(campaign_navigation_workspace.references[n+1].candidate==&r->navigation.start)
                campaign_navigation_workspace.references[n+1].candidate=NULL;
        }
        if(r->query.stream)scene_vehicle_owner_collision_bind(r->query.stream,0);
        free(r);
    }
    free(campaign_secondary_vehicles);campaign_secondary_vehicles=NULL;
    campaign_secondary_vehicle_capacity=campaign_secondary_vehicle_count=0;
}
/* The staged bank has already passed every admission and pointer rebase.
 * Publication owns no fallible work and never replays an authored order. */
static void scene_secondary_vehicle_publish(scene_stream *stream,scene_secondary_vehicle_runtime **pool,
    uint32_t capacity,uint32_t count)
{
    scene_secondary_vehicle_close();campaign_secondary_vehicles=pool;
    campaign_secondary_vehicle_capacity=capacity;campaign_secondary_vehicle_count=count;
    scene_vehicle_owner_collision_bind(stream,count!=0);
}

typedef struct scene_secondary_vehicle_visibility {const rf_collision_solid_view *solid;float radius;} scene_secondary_vehicle_visibility;
static int scene_secondary_vehicle_route_visible(void *context,const float start[3],const float end[3],float radius,uint32_t *blocked)
{
    const scene_secondary_vehicle_visibility *view=context;float delta[3];rf_collision_sweep_room_hit hit;uint32_t i;
    (void)radius;for(i=0;i<3;i++)delta[i]=end[i]-start[i];
    return rf_collision_solid_preferred(view->solid,NULL,0x45,start,delta,view->radius,1,&hit,blocked);
}
static int scene_secondary_vehicle_route_request(scene_stream *stream,scene_secondary_vehicle_runtime *r,uint32_t *found)
{
    rf_level_navigation_workspace *workspace=&campaign_navigation_workspace;
    rf_entity_navigation_graph_request request={0};rf_entity_navigation_selection begin,end;
    rf_collision_solid_view solid={0};scene_secondary_vehicle_visibility visibility;
    const scene_driller_resources *resource=scene_secondary_vehicle_resource(stream,r->profile);
    rf_entity_navigation_candidate *saved_goal,*saved_start;
    float low=0,high=0;uint32_t i,n=workspace->count;int status;
    *found=0;++rf_scene_secondary_vehicle[2];
    if(!workspace->storage||!n||!resource){++rf_scene_secondary_vehicle[4];return RF_OK;}
    for(i=0;i<r->physics.sphere_count;i++) {
        const rf_physics_sphere *sphere=r->physics.spheres+i;
        if(sphere->radius>request.radius)request.radius=sphere->radius;
        if(!i||sphere->center[1]-sphere->radius<low)low=sphere->center[1]-sphere->radius;
        if(!i||sphere->center[1]+sphere->radius>high)high=sphere->center[1]+sphere->radius;
    }
    if(resource->movement.radius>0)request.radius=resource->movement.radius;
    request.height=high-low;
    solid.rooms=stream->collision->views;solid.room_count=stream->collision->room_count;
    solid.primary=stream->collision->primary;solid.primary_count=stream->collision->primary_count;
    solid.children=stream->collision->children;solid.child_count=stream->collision->child_count;
    visibility.solid=&solid;visibility.radius=request.radius;
    memset(&r->navigation.start,0,sizeof(r->navigation.start));memset(&r->navigation.goal,0,sizeof(r->navigation.goal));
    memcpy(r->navigation.start.position,r->physics.state.position,12);
    memcpy(r->navigation.start.query_point,r->physics.state.position,12);
    memcpy(r->navigation.goal.position,r->route.target,12);memcpy(r->navigation.goal.query_point,r->route.target,12);
    status=rf_entity_navigation_select(workspace->references,n,r->navigation.start.position,
        request.radius,request.height,0,1,scene_secondary_vehicle_route_visible,&visibility,&begin);if(status)return status;
    status=rf_entity_navigation_select(workspace->references,n,r->navigation.goal.position,
        request.radius,request.height,0,1,scene_secondary_vehicle_route_visible,&visibility,&end);if(status)return status;
    if(begin.first==UINT32_MAX||end.first==UINT32_MAX){++rf_scene_secondary_vehicle[4];return RF_OK;}
    request.first_start=begin.first;request.second_start=begin.second;request.first_end=end.first;request.second_end=end.second;
    request.search_mode=1;request.solid=&solid;request.references=workspace->references;request.adjacency=workspace->adjacency;
    request.global_count=n;request.route=&r->navigation.retained;request.scratch=workspace->scratch;request.scratch_capacity=n+2;
    saved_goal=workspace->references[n].candidate;saved_start=workspace->references[n+1].candidate;
    workspace->references[n].candidate=&r->navigation.goal;workspace->references[n+1].candidate=&r->navigation.start;
    status=rf_entity_navigation_graph_request_run(&request,found);
    /* No borrowed endpoint may retain a stack candidate after this request. */
    workspace->references[n].candidate=saved_goal;workspace->references[n+1].candidate=saved_start;
    if(status)return status;
    if(*found&&r->navigation.retained.count>1) {
        r->navigation.cost=request.cost;r->route.index=1;++rf_scene_secondary_vehicle[3];
    } else {*found=0;r->navigation.retained.count=0;++rf_scene_secondary_vehicle[4];}
    return RF_OK;
}
static int scene_secondary_vehicle_command(scene_stream *stream,scene_secondary_vehicle_runtime *r,rf_vehicle_rigid_command *command)
{
    uint32_t found;const rf_entity_navigation_candidate *node;float dx,dz,distance,angle;int status;
    command->controlled=1;
    /* A retained idle owner releases throttle and steering through the same
     * rigid command response, so the last route turn is not permanent yaw. */
    if(!r->route.active)return scene_vehicle_ai_ground_steer(r->handle,&r->physics.state,command);
    if(r->route.retry)--r->route.retry;
    if(!r->navigation.retained.count&&!r->route.retry) {
        status=scene_secondary_vehicle_route_request(stream,r,&found);if(status)return status;
        if(!found){r->route.retry=60;return RF_OK;}
    }
    if(!r->navigation.retained.count)return RF_OK; /* Never invent a direct fallback. */
    if(r->navigation.retained.count>4||!r->route.index||r->route.index>r->navigation.retained.count)return RF_FORMAT;
    while(r->route.index<r->navigation.retained.count) {
        node=r->navigation.retained.nodes[r->route.index];if(!node)return RF_FORMAT;
        dx=node->position[0]-r->physics.state.position[0];dz=node->position[2]-r->physics.state.position[2];
        distance=sqrtf(dx*dx+dz*dz);if(!isfinite(distance))return RF_FORMAT;
        if(distance>1.5f)break;
        ++r->route.index;++rf_scene_secondary_vehicle[5];
        if(node==&r->navigation.goal){r->route.active=0;++rf_scene_secondary_vehicle[6];return RF_OK;}
    }
    if(r->route.index>=r->navigation.retained.count) {
        /* Four pointers are only a prefix: ask the graph again from the new
         * committed position, never mistake prefix exhaustion for arrival. */
        r->navigation.retained.count=0;r->route.index=1;r->route.retry=0;return RF_OK;
    }
    node=r->navigation.retained.nodes[r->route.index];
    dx=node->position[0]-r->physics.state.position[0];dz=node->position[2]-r->physics.state.position[2];
    distance=sqrtf(dx*dx+dz*dz);
    angle=atan2f(r->physics.state.orientation[8]*dx-r->physics.state.orientation[6]*dz,
        r->physics.state.orientation[6]*dx+r->physics.state.orientation[8]*dz);
    if(!isfinite(angle)||!isfinite(distance))return RF_FORMAT;
    command->turn=fmaxf(-1.f,fminf(1.f,angle/.7f));
    command->throttle=fabsf(angle)>1.5f?0.f:fabsf(angle)>.8f?.2f:fminf(.65f,distance/6.f);
    return RF_OK;
}
/* Keep the immutable authored point for exact checkpoint identity. Only the
 * Goto_Player command samples the registered player's current body position;
 * transient absence/death releases controls without losing the saved order. */
static int scene_secondary_vehicle_fighter_command(scene_secondary_vehicle_runtime *r,
    rf_vehicle_submersible_command *command)
{
    const float *player=NULL;uint32_t complete=0;int status;
    command->controlled=1;
    if(r->route.active&&r->route.kind==SCENE_SECONDARY_ORDER_PLAYER&&scene_vehicle_pursuit_finished(r->handle,r->physics.state.position))r->route.active=0;
    if(!r->route.active)return scene_vehicle_ai_steer(r->handle,&r->physics.state,command);
    if(r->route.kind==SCENE_SECONDARY_ORDER_ATTACK){
        status=scene_vehicle_attack_steer(r->handle,&r->physics.state,command);
        if(!scene_vehicle_attack_active(r->handle))r->route.active=0;
        return status;
    }
    /* A seated player can be render-hidden (4000) while still a live target;
     * removal, death and broken ownership invalidate the sample. */
    if(r->route.kind==SCENE_SECONDARY_ORDER_PLAYER&&campaign_player_object.view==&campaign_player_view&&
       scene_actor_body.allocated_bytes&&campaign_player_damage.state.effects.health>0&&
       !(campaign_player_view.flags_7c&2u)&&
       rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object&&
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view)
        player=scene_actor_body.state.position;
    status=scene_secondary_vehicle_point_steer(r->profile,&r->physics.state,r->route.kind,r->route.target,
        player,command,&complete);if(status)return status;
    if(r->profile==5&&r->route.kind==SCENE_SECONDARY_ORDER_PLAYER){
        uint32_t *p=rf_scene_secondary_vehicle_pursuit;++p[0];p[1]=r->uid;p[2]=r->handle;
        p[3]=campaign_player_object.handle;p[4]=r->last_frame;p[5]=r->route.active;p[6]=r->route.kind;p[7]=player!=NULL;
        memcpy(p+8,r->route.target,12);memset(p+11,0,12);if(player)memcpy(p+11,player,12);
        memcpy(p+14,command,24);memcpy(p+20,r->physics.state.position,12);memcpy(p+23,r->physics.state.orientation,36);
    }
    if(complete){r->route.active=0;++rf_scene_secondary_vehicle[6];}
    return RF_OK;
}
static int scene_secondary_vehicle_support(void *context,const rf_vehicle_rigid_state *state,rf_vehicle_rigid_support *out)
{return scene_driller_support_sample(&((scene_secondary_vehicle_runtime *)context)->support,state,out);}
static int scene_secondary_vehicle_resolve(void *context,const rf_vehicle_rigid_state *state,
    const rf_vehicle_rigid_proposal *proposal,rf_vehicle_rigid_proposal *out)
{return scene_vehicle_collision_resolve(&((scene_secondary_vehicle_runtime *)context)->collision,state,proposal,out);}

static int scene_secondary_vehicle_owner_tick(scene_stream *stream,scene_secondary_vehicle_runtime *r,uint32_t frame)
{
    scene_secondary_vehicle_runtime next;
    scene_passive_vehicle *owner;rf_group_attached_pose pose;rf_vehicle_rigid_command command={0};
    rf_vehicle_rigid_backend backend;float accepted[3];uint32_t i;int status;
    if(!r||r->last_frame==frame)return RF_OK;
    owner=scene_secondary_vehicle_owner(r);if(!owner)return RF_NOT_FOUND;
    /* The controller interval has already zeroed passive velocity. A paused
     * frame publishes no displacement, without replacing rigid coasting state. */
    if(!frame||scene_secondary_vehicle_paused(r,owner)) {
        r->last_frame=frame;memset(owner->velocity,0,12);memset(owner->pose.velocity,0,12);owner->motion_valid=0;
        if(frame)++rf_scene_secondary_vehicle[8];return RF_OK;
    }
    next=*r;scene_secondary_vehicle_route_rebase(&next,r);
    status=scene_secondary_vehicle_bind(stream,&next);if(status)goto failed;
    if(scene_secondary_vehicle_flight_profile(next.profile)){
        rf_vehicle_submersible_command flight={0};rf_vehicle_submersible_backend flight_backend;
        rf_vehicle_submersible_result flight_result;
        status=scene_secondary_vehicle_fighter_command(&next,&flight);if(status)goto failed;
        status=next.profile==4?scene_submarine_motion_backend(&next.submarine,&flight_backend):
            scene_fighter_motion_backend(&next.fighter,&flight_backend);if(status)goto failed;
        status=rf_vehicle_submersible_step(&next.physics.state,&next.flight_parameters,&flight,
            scene_step_seconds,&flight_backend,&flight_result);if(status)goto failed;
    }else{
        next.physics.parameters.gravity=scene_gravity.acceleration;
        status=scene_secondary_vehicle_command(stream,&next,&command);if(status)goto failed;
        backend.context=&next;backend.support=scene_secondary_vehicle_support;backend.resolve=scene_secondary_vehicle_resolve;
        status=rf_vehicle_rigid_step(&next.physics.state,&next.physics.parameters,&command,scene_step_seconds,&backend);if(status)goto failed;
    }
    /* One rigid authority: admit the rider paths before publishing the hull.
     * If a wall/ceiling/person blocks any rider, keep this complete hull pose
     * and stop its velocity rather than let the hull crush an uncarried body. */
    {uint32_t allowed=0;
     status=scene_secondary_vehicle_carry_clear(stream,r,&next.physics.state,&allowed);if(status)goto failed;
     if(!allowed){
         memcpy(next.physics.state.position,r->physics.state.position,12);
         memcpy(next.physics.state.orientation,r->physics.state.orientation,36);
         memset(next.physics.state.velocity,0,12);memset(next.physics.state.momentum,0,12);
     }}
    pose=owner->pose;
    status=rf_group_pose_set_position(&pose,next.physics.state.position);if(status)goto failed;
    memcpy(pose.base_position,next.physics.state.position,12);memcpy(pose.base_matrix,next.physics.state.orientation,36);
    memcpy(pose.input_matrix,next.physics.state.orientation,36);memcpy(pose.output_matrix,next.physics.state.orientation,36);
    memcpy(pose.pending_matrix,next.physics.state.orientation,36);
    for(i=0;i<3;i++){
        accepted[i]=(next.physics.state.position[i]-r->physics.state.position[i])/scene_step_seconds;
        if(!isfinite(accepted[i])){status=RF_FORMAT;goto failed;}
    }
    memcpy(pose.velocity,accepted,12);next.last_frame=frame;++next.steps;
    memcpy(owner->previous_position,r->physics.state.position,12);memcpy(owner->previous_basis,r->physics.state.orientation,36);
    owner->pose=pose;memcpy(owner->velocity,accepted,12);
    /* The controller pass opened this same physics interval. Existing player
     * and NPC support consume these exact accepted transforms once, after all
     * hull owners step. Never extrapolate requested velocity or pretranslate. */
    owner->motion_valid=1;
    owner->physics_body_flags=next.physics.body_flags;owner->scripted_physics=next.scripted_physics;
    *r=next;scene_secondary_vehicle_route_rebase(r,&next);
    (void)scene_secondary_vehicle_bind(stream,r); /* Same validated world and embedded shapes. */
    ++rf_scene_secondary_vehicle[1];rf_scene_secondary_vehicle[12]=frame;rf_scene_secondary_vehicle[13]=0;
    rf_scene_secondary_vehicle[14]=r->navigation.retained.count;rf_scene_secondary_vehicle[15]=r->route.index;
    return RF_OK;
failed:
    rf_scene_secondary_vehicle[13]=(uint32_t)status;return status;
}
static int scene_secondary_vehicle_tick(scene_stream *stream,uint32_t frame)
{
    /* Authored owner order is deterministic. An idle, hidden, frozen or
     * retired slot consumes only its own frame; other owners keep stepping
     * against the most recently committed poses, without coupled impulses. */
    for(uint32_t slot=0;campaign_secondary_vehicles&&slot<campaign_secondary_vehicle_capacity;slot++){
        scene_secondary_vehicle_runtime *r=campaign_secondary_vehicles[slot];if(!r)continue;
        if(r->slot!=slot)return RF_FORMAT;
        int status=scene_secondary_vehicle_owner_tick(stream,r,frame);if(status)return status;
    }
    return RF_OK;
}
#endif
