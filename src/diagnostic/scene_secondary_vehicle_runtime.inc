#ifndef RF_SCENE_SECONDARY_VEHICLE_RUNTIME_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_RUNTIME_INCLUDED
/* One independent authored APC ground owner. The existing kind11 owner keeps
 * its handle, damage and draw
 * identity. Only rigid/navigation state lives here; resources stay borrowed.
 * Routing uses the recovered graph request and four-node retained prefix.
 * Steering tolerances/speed reduction are explicit first-pass driving policy. */
typedef struct scene_secondary_vehicle_query {
    scene_stream *stream;uint32_t handle;
} scene_secondary_vehicle_query;
typedef struct scene_secondary_vehicle_runtime {
    scene_driller_physics physics;
    scene_vehicle_collision collision;
    scene_driller_support_context support;
    scene_secondary_vehicle_query query;
    uint32_t uid,handle,slot,profile,retired,last_frame,scripted_physics;
    struct {uint32_t active,event,index,retry;float target[3];} route;
    campaign_npc_route navigation;
} scene_secondary_vehicle_runtime;
static scene_secondary_vehicle_runtime *campaign_secondary_vehicle;
uint32_t rf_scene_secondary_vehicle[16]; /* admissions,steps,route requests,found,misses,node advances,arrivals,OFF,paused,removed,UID,handle,last frame,status,count,index */

static uint32_t scene_secondary_vehicle_owns(uint32_t handle)
{return campaign_secondary_vehicle&&campaign_secondary_vehicle->handle==handle;}

static scene_passive_vehicle *scene_secondary_vehicle_owner(const scene_secondary_vehicle_runtime *r)
{
    scene_passive_vehicle *owner;
    if(!r||!campaign_passive_vehicles||r->slot>=campaign_passive_vehicle_count)return NULL;
    owner=campaign_passive_vehicles+r->slot;
    if(owner->object_kind!=11||owner->handle!=r->handle||owner->uid!=r->uid||
       owner->damage.state.effects.handle!=r->handle||
       owner->resource_kind!=4||r->profile!=2||
       rf_object_registry_lookup(&campaign_registry,r->handle)!=owner)return NULL;
    return owner;
}
static uint32_t scene_secondary_vehicle_retired(uint32_t handle)
{
    const scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;
    return r&&r->handle==handle&&r->retired&&scene_secondary_vehicle_owner(r)!=NULL;
}
static uint32_t scene_secondary_vehicle_empty(const scene_passive_vehicle *owner)
{
    return owner&&!owner->group_owned&&!owner->attached&&owner->seat_driver==UINT32_MAX&&
        owner->seat_occupants[0]==-1&&owner->seat_occupants[1]==-1&&
        campaign_authored_vehicle_handle!=owner->handle;
}
/* Seats are not the only attachment: a living actor can already own the roof
 * as ground support. Keep that relation intact and suspend this independent
 * solver until the actor naturally leaves; no carry or support theft. */
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
static const scene_driller_resources *scene_secondary_vehicle_resource(const scene_stream *stream)
{
    if(!stream)return NULL;
    if(stream->passive_vehicle_resources[4])return stream->passive_vehicle_resources[4];
    return campaign_selected_vehicle_resource_kind()==4?stream->driller:NULL;
}
static int scene_secondary_vehicle_collision_query(void *context,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *found)
{
    const scene_secondary_vehicle_query *owner=context;
    if(!owner)return RF_RANGE;
    return scene_vehicle_owner_collision_query(owner->stream,owner->handle,query,hit,found);
}
/* Pure pointer rebasing: safe for a staged candidate and for a committed world
 * overlay replacement; cached support and full rigid state remain untouched. */
static int scene_secondary_vehicle_bind(scene_stream *stream,scene_secondary_vehicle_runtime *r)
{
    if(!stream||!stream->collision||!r||!r->physics.sphere_count||!r->physics.spring_count)return RF_RANGE;
    r->query.stream=stream;r->query.handle=r->handle;
    r->collision.world=stream->collision;r->collision.movers=&campaign_movers;
    r->collision.spheres=r->physics.collision;r->collision.count=r->physics.sphere_count;
    r->collision.radius=r->physics.radius;r->collision.flags=4;
    r->collision.query=scene_secondary_vehicle_collision_query;r->collision.query_context=&r->query;
    r->support.stream=stream;r->support.collision=&r->collision;r->support.physics=&r->physics;
    return RF_OK;
}
static int scene_secondary_vehicle_rebind(scene_stream *stream)
{return campaign_secondary_vehicle?scene_secondary_vehicle_bind(stream,campaign_secondary_vehicle):RF_OK;}

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
static int scene_secondary_vehicle_prepare(scene_stream *stream,uint32_t slot,scene_secondary_vehicle_runtime *out)
{
    scene_secondary_vehicle_runtime value={0};scene_passive_vehicle *owner;
    const scene_driller_resources *resource;int status;
    if(!stream||!out||slot>=campaign_passive_vehicle_count||!campaign_passive_vehicles)return RF_RANGE;
    owner=campaign_passive_vehicles+slot;
    if(owner->resource_kind!=4||owner->object_kind!=11||!scene_secondary_vehicle_empty(owner)||
       owner->damage.state.effects.handle!=owner->handle||
       rf_object_registry_lookup(&campaign_registry,owner->handle)!=owner)return RF_NOT_FOUND;
    resource=scene_secondary_vehicle_resource(stream);if(!resource)return RF_NOT_FOUND;
    value.uid=owner->uid;value.handle=owner->handle;value.slot=slot;value.profile=2;value.last_frame=UINT32_MAX;
    status=scene_vehicle_profile_physics_prepare(2,resource,owner->pose.position,
        owner->pose.input_matrix,scene_gravity.acceleration,&value.physics);if(status)return status;
    memcpy(value.physics.state.velocity,owner->velocity,12);
    value.scripted_physics=owner->scripted_physics;
    if(value.scripted_physics)value.physics.body_flags=owner->physics_body_flags;
    value.support.material=-1;value.support.support_handle=UINT32_MAX;
    *out=value;return scene_secondary_vehicle_bind(stream,out);
}
/* Require the original parsed record, including its original target UID link.
 * A fabricated event with the same number cannot admit a second simulation. */
static uint32_t scene_secondary_vehicle_event(const rf_level_event *event,uint32_t uid)
{
    uint32_t i,j;
    if(!event||strcmp(event->type,"Goto"))return 0;
    for(i=0;i<campaign_events.count;i++) {
        const rf_level_owned_event *authored=campaign_events.items[i].authored;
        if(authored && &authored->record==event) {
            if(!authored->links)return 0;
            for(j=0;j<authored->record.link_count;j++)if(authored->links[j]==uid)return 1;
            return 0;
        }
    }
    return 0;
}
static int scene_secondary_vehicle_script_move(scene_stream *stream,uint32_t handle,
    const rf_level_event *event,uint32_t on,uint32_t *handled)
{
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;scene_passive_vehicle *owner=NULL;
    uint32_t i;int status;
    if(!handled||on>1||!event)return RF_RANGE;*handled=0;
    for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==handle){owner=campaign_passive_vehicles+i;break;}
    if(!owner||owner->resource_kind!=4||!scene_secondary_vehicle_event(event,owner->uid))return RF_NOT_FOUND;
    *handled=1;
    if(r&&r->handle!=handle)return RF_NOT_FOUND;
    if(!scene_secondary_vehicle_empty(owner)||rf_object_registry_lookup(&campaign_registry,handle)!=owner)return RF_NOT_FOUND;
    if(!on){if(r&&r->route.event==event->uid){r->route.active=0;++rf_scene_secondary_vehicle[7];}return RF_OK;}
    if((r&&r->retired)||owner->damage.destroyed||owner->damage.state.effects.health<=0||
       (owner->damage.object_flags&2u))return RF_OK;
    if(!r&&scene_secondary_vehicle_has_rider(handle))return RF_NOT_FOUND;
    for(i=0;i<3;i++)if(!isfinite(event->position[i]))return RF_FORMAT;
    if(!r){
        r=calloc(1,sizeof(*r));if(!r)return RF_IO;
        status=scene_secondary_vehicle_prepare(stream,(uint32_t)(owner-campaign_passive_vehicles),r);
        if(status){free(r);return status;}
        owner->physics_body_flags=r->physics.body_flags;owner->scripted_physics=r->scripted_physics;
        campaign_secondary_vehicle=r;scene_vehicle_owner_collision_bind(stream,1);++rf_scene_secondary_vehicle[0];
    }
    r->route.active=1;r->route.event=event->uid;r->route.index=1;r->route.retry=0;
    memcpy(r->route.target,event->position,12);memset(&r->navigation,0,sizeof(r->navigation));
    rf_scene_secondary_vehicle[10]=r->uid;rf_scene_secondary_vehicle[11]=r->handle;
    return RF_OK;
}
static uint32_t scene_secondary_vehicle_frozen(const scene_secondary_vehicle_runtime *r)
{return r->scripted_physics&&!(r->physics.body_flags&0x80000000u);}
static uint32_t scene_secondary_vehicle_paused(const scene_secondary_vehicle_runtime *r,const scene_passive_vehicle *owner)
{
    return r->retired||scene_secondary_vehicle_frozen(r)||!scene_secondary_vehicle_empty(owner)||
        scene_secondary_vehicle_has_rider(r->handle)||
        owner->damage.destroyed||owner->damage.state.effects.health<=0||
        (owner->damage.object_flags&(2u|0x4000u));
}
static int scene_secondary_vehicle_physics_sync(uint32_t handle)
{
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;scene_passive_vehicle *owner;
    if(!r||r->handle!=handle)return RF_OK;
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
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;scene_passive_vehicle *owner;
    if(!handled)return RF_RANGE;*handled=0;
    if(!r||r->handle!=handle)return RF_OK;*handled=1;
    owner=scene_secondary_vehicle_owner(r);if(!owner)return RF_NOT_FOUND;
    if(!r->retired)++rf_scene_secondary_vehicle[9];
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
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;uint32_t i;
    if(!r||r->retired||!r->route.active)return;
    if(on)r->route.retry=0;
    for(i=r->route.index;i<r->navigation.retained.count&&i<4;i++)
        if(r->navigation.retained.nodes[i]==candidate){r->navigation.retained.count=0;r->route.index=1;r->route.retry=0;break;}
}
static void scene_secondary_vehicle_close(void)
{
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;uint32_t n=campaign_navigation_workspace.count;
    if(r&&campaign_navigation_workspace.references) {
        if(campaign_navigation_workspace.references[n].candidate==&r->navigation.goal)
            campaign_navigation_workspace.references[n].candidate=NULL;
        if(campaign_navigation_workspace.references[n+1].candidate==&r->navigation.start)
            campaign_navigation_workspace.references[n+1].candidate=NULL;
    }
    if(r&&r->query.stream)scene_vehicle_owner_collision_bind(r->query.stream,0);
    free(r);campaign_secondary_vehicle=NULL;
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
    const scene_driller_resources *resource=scene_secondary_vehicle_resource(stream);
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
    if(!r->route.active)return RF_OK;
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
static int scene_secondary_vehicle_support(void *context,const rf_vehicle_rigid_state *state,rf_vehicle_rigid_support *out)
{return scene_driller_support_sample(&((scene_secondary_vehicle_runtime *)context)->support,state,out);}
static int scene_secondary_vehicle_resolve(void *context,const rf_vehicle_rigid_state *state,
    const rf_vehicle_rigid_proposal *proposal,rf_vehicle_rigid_proposal *out)
{return scene_vehicle_collision_resolve(&((scene_secondary_vehicle_runtime *)context)->collision,state,proposal,out);}

static int scene_secondary_vehicle_tick(scene_stream *stream,uint32_t frame)
{
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle,next;
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
    next.physics.parameters.gravity=scene_gravity.acceleration;
    status=scene_secondary_vehicle_command(stream,&next,&command);if(status)goto failed;
    backend.context=&next;backend.support=scene_secondary_vehicle_support;backend.resolve=scene_secondary_vehicle_resolve;
    status=rf_vehicle_rigid_step(&next.physics.state,&next.physics.parameters,&command,scene_step_seconds,&backend);if(status)goto failed;
    pose=owner->pose;
    status=rf_group_pose_set_position(&pose,next.physics.state.position);if(status)goto failed;
    memcpy(pose.base_position,next.physics.state.position,12);memcpy(pose.base_matrix,next.physics.state.orientation,36);
    memcpy(pose.input_matrix,next.physics.state.orientation,36);memcpy(pose.output_matrix,next.physics.state.orientation,36);
    memcpy(pose.pending_matrix,next.physics.state.orientation,36);
    for(i=0;i<3;i++){
        accepted[i]=(next.physics.state.position[i]-r->physics.state.position[i])/scene_step_seconds;
        if(!isfinite(accepted[i])){status=RF_FORMAT;goto failed;}
    }
    memcpy(pose.velocity,accepted,12);next.last_frame=frame;
    memcpy(owner->previous_position,r->physics.state.position,12);memcpy(owner->previous_basis,r->physics.state.orientation,36);
    owner->pose=pose;memcpy(owner->velocity,accepted,12);
    /* Independent chassis contact uses accepted velocity; controller interval
     * rider/push transport remains excluded from this bounded owner. */
    owner->motion_valid=0;
    owner->physics_body_flags=next.physics.body_flags;owner->scripted_physics=next.scripted_physics;
    *r=next;scene_secondary_vehicle_route_rebase(r,&next);
    (void)scene_secondary_vehicle_bind(stream,r); /* Same validated world and embedded shapes. */
    ++rf_scene_secondary_vehicle[1];rf_scene_secondary_vehicle[12]=frame;rf_scene_secondary_vehicle[13]=0;
    rf_scene_secondary_vehicle[14]=r->navigation.retained.count;rf_scene_secondary_vehicle[15]=r->route.index;
    return RF_OK;
failed:
    rf_scene_secondary_vehicle[13]=(uint32_t)status;return status;
}
#endif
