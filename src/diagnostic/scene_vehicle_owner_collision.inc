#ifndef RF_SCENE_VEHICLE_OWNER_COLLISION_INCLUDED
#define RF_SCENE_VEHICLE_OWNER_COLLISION_INCLUDED
/* Independent chassis use the same committed world. Queries identify the
 * chassis, never its player adapter. Bodies step sequentially against the
 * latest committed other pose; this is not a coupled impulse solver. */
static int scene_vehicle_owner_collision_query(scene_stream *s,uint32_t actor,
    const rf_collision_body_query *query,rf_geometry_body_hit *hit,uint32_t *found)
{
    int status;
    if(!s||!s->collision||!query||!hit||!found||actor==UINT32_MAX)return RF_RANGE;
    status=campaign_body_query_batch_for(s->collision,query,hit,found,NULL,&campaign_movers);if(status)return status;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *o=campaign_passive_vehicles+i;scene_driller_actor_collision body={0};
        uint32_t kind=o->resource_kind;
        /* Visible wrecks remain obstacles, matching ordinary passive hulls. */
        if(o->handle==actor||(o->damage.object_flags&0x4000u))continue;
        if(rf_object_registry_lookup(&campaign_registry,o->handle)!=o)return RF_FORMAT;
        if(kind>=6||!s->passive_vehicle_sphere_count[kind]||s->passive_vehicle_sphere_count[kind]>8)return RF_FORMAT;
        body.spheres=s->passive_vehicle_spheres[kind];body.count=s->passive_vehicle_sphere_count[kind];
        body.handle=o->handle;body.driver=UINT32_MAX;
        const scene_driller_resources *resource=s->passive_vehicle_resources[kind];
        if(!resource&&campaign_selected_vehicle_resource_kind()==kind)resource=s->driller;
        if(!resource)return RF_NOT_FOUND;body.material=resource->physics.material.index;
        memcpy(body.position,o->pose.position,12);memcpy(body.next_position,o->pose.position,12);
        memcpy(body.orientation,o->pose.input_matrix,36);memcpy(body.next_orientation,o->pose.input_matrix,36);
        memcpy(body.velocity,o->velocity,12);
        status=scene_driller_actor_compose(&body,actor,UINT32_MAX,query,hit,found);if(status)return status;
    }
    if(s->driller_runtime){
        const scene_driller_runtime *r=s->driller_runtime;
        if(r->entry.host.handle!=actor&&!(r->damage.damage.object_flags&0x4000u)){
            scene_driller_actor_collision body={0};
            if(!scene_driller_damage_runtime_valid(&r->damage))return RF_FORMAT;
            body.spheres=r->physics.collision;body.count=r->physics.sphere_count;
            body.handle=r->entry.host.handle;body.driver=UINT32_MAX;
            body.material=s->driller->physics.material.index;
            memcpy(body.position,r->physics.state.position,12);memcpy(body.next_position,r->physics.state.position,12);
            memcpy(body.orientation,r->physics.state.orientation,36);memcpy(body.next_orientation,r->physics.state.orientation,36);
            memcpy(body.velocity,r->physics.state.velocity,12);
            status=scene_driller_checkpoint_rotate_tensor(r->physics.state.orientation,r->physics.state.inverse_inertia,
                r->physics.state.momentum,body.angular_velocity);if(status)return status;
            status=scene_driller_actor_compose(&body,actor,UINT32_MAX,query,hit,found);if(status)return status;
        }
    }
    if(campaign_player_object.view&&scene_actor_body.allocated_bytes&&
       !(campaign_player_view.flags_7c&(2u|0x4000u))&&
       !(s->driller_runtime&&s->driller_runtime->entry.host.handle==actor&&
         s->driller_runtime->entry.session.active&&s->driller_runtime->entry.player.handle==campaign_player_object.handle)){
        status=scene_vehicle_wreck_actor_sweep(&scene_actor_body,campaign_player_object.handle,actor,query,hit,found);if(status)return status;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(!scene_vehicle_entry_npc_live(o)||scene_npc_seat_related(o->registration.handle,actor))continue;
        status=scene_vehicle_wreck_actor_sweep(&o->body,o->registration.handle,actor,query,hit,found);if(status)return status;
    }
    for(uint32_t i=0;i<scene_turret_count;i++){
        const scene_turret_owner *o=scene_turrets+i;rf_physics_sphere raw[8]={{0}};
        rf_collision_body_sphere spheres[8]={{0}};scene_driller_actor_collision body={0};
        if(!scene_vehicle_entry_turret_live(o))continue;
        const rf_static_render_resource *model=&scene_turret_models[o->live_model].render;uint32_t n=model->sphere_count;
        if(n>8)return RF_RANGE;
        if(n){status=rf_model_creation_spheres(model->spheres,n,1,NULL,0,raw,8);if(status)return status;}
        else{n=1;memcpy(raw[0].center,model->bound,12);raw[0].radius=model->bound[3];}
        for(uint32_t j=0;j<n;j++){memcpy(spheres[j].center,raw[j].center,12);spheres[j].radius=raw[j].radius;}
        body.spheres=spheres;body.count=n;body.handle=o->registration.handle;body.driver=UINT32_MAX;
        memcpy(body.position,o->position,12);memcpy(body.next_position,o->position,12);
        memcpy(body.orientation,o->basis,36);memcpy(body.next_orientation,o->basis,36);
        status=scene_driller_actor_compose(&body,actor,UINT32_MAX,query,hit,found);if(status)return status;
    }
    for(uint32_t i=0;i<query->count;i++){
        float start[3],end[3],delta[3];rf_geomod_registry_hit piece;uint32_t yes=0,liquid=0;rf_weapon_flight_contact prop={0};
        scene_driller_point(query->start,(const float*)query->matrix,query->spheres[i].center,start);
        scene_driller_point(query->end,(const float*)query->matrix,query->spheres[i].center,end);
        for(uint32_t k=0;k<3;k++)delta[k]=end[k]-start[k];
        status=scene_detached_sources_sweep(s,query->flags,start,delta,query->spheres[i].radius,
            *found?hit->contact.fraction:query->limit,&piece,&yes);if(status)return status;
        if(yes){memset(hit,0,sizeof(*hit));hit->contact.fraction=piece.piece.hit.fraction;
            memcpy(hit->contact.normal,piece.piece.hit.normal,12);memcpy(hit->contact.point,piece.piece.hit.point,12);
            hit->contact.object_id=UINT32_MAX;hit->solid=hit->room=hit->face=UINT32_MAX;hit->sphere=i;*found=1;}
        yes=*found;prop.hit.fraction=yes?hit->contact.fraction:query->limit;
        status=scene_clutter_projectile_compose(actor,start,delta,query->spheres[i].radius,&prop,&liquid,&yes);if(status)return status;
        if(yes&&(!*found||prop.hit.fraction<hit->contact.fraction)){
            memset(hit,0,sizeof(*hit));hit->contact.fraction=prop.hit.fraction;
            memcpy(hit->contact.normal,prop.hit.normal,12);memcpy(hit->contact.point,prop.hit.point,12);
            hit->contact.object_id=prop.face;hit->solid=hit->room=hit->face=UINT32_MAX;hit->sphere=i;*found=1;}
    }
    return RF_OK;
}
static int scene_vehicle_owner_selected_query(void *context,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *found)
{
    scene_stream *s=context;
    if(!s||!s->driller_runtime)return RF_RANGE;
    return scene_vehicle_owner_collision_query(s,s->driller_runtime->entry.host.handle,query,hit,found);
}
static void scene_vehicle_owner_collision_bind(scene_stream *s,uint32_t enabled)
{
    if(!s||!s->driller_runtime)return;
    scene_driller_runtime *r=s->driller_runtime;
    r->collision.query=enabled?scene_vehicle_owner_selected_query:NULL;
    r->collision.query_context=enabled?s:NULL;
    r->submarine.collision.query=r->fighter.collision.query=r->collision.query;
    r->submarine.collision.query_context=r->fighter.collision.query_context=r->collision.query_context;
}
#endif
