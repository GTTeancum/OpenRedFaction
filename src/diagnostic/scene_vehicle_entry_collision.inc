#ifndef RF_SCENE_VEHICLE_ENTRY_COLLISION_INCLUDED
#define RF_SCENE_VEHICLE_ENTRY_COLLISION_INCLUDED
/* Include after vehicle_wreck_exit, before vehicle_switch. Only the entry
 * copy receives this provider: chassis motion keeps its existing collision.
 * Own host is omitted during transit from its interior; existing exit math
 * excludes it at the destination. The player must not collide with itself. */
static uint32_t scene_vehicle_entry_passive_live(const scene_passive_vehicle *o)
{return !(o->damage.object_flags&(2u|0x4000u))&&rf_object_registry_lookup(&campaign_registry,o->handle)==o;}
static uint32_t scene_vehicle_entry_npc_live(const campaign_npc_body *o)
{return o->registration.view==&o->view&&!(o->object_flags&(2u|0x4000u))&&
    rf_object_registry_lookup(&campaign_registry,o->registration.handle)==&o->registration&&
    rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)==&o->view;}
static uint32_t scene_vehicle_entry_turret_live(const scene_turret_owner *o)
{return !o->dead&&o->registration.view==&o->view&&!(o->view.flags_7c&(2u|0x4000u))&&
    rf_object_registry_lookup(&campaign_registry,o->registration.handle)==&o->registration&&
    rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)==&o->view;}
static int scene_vehicle_entry_query(void *context,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *found)
{
    scene_stream *s=context;uint32_t actor,host;int status;
    if(!s||!s->driller_runtime||!query||!hit||!found)return RF_RANGE;
    if(!scene_driller_damage_runtime_valid(&s->driller_runtime->damage)||
       !campaign_player_object.view||campaign_player_object.handle!=s->driller_runtime->entry.player.handle)return RF_NOT_FOUND;
    actor=campaign_player_object.handle;host=s->driller_runtime->entry.host.handle;
    status=campaign_body_query_batch_for(s->collision,query,hit,found,NULL,&campaign_movers);if(status)return status;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *o=campaign_passive_vehicles+i;scene_driller_actor_collision body={0};
        uint32_t kind=o->resource_kind;
        if(o->handle==actor||o->handle==host||!scene_vehicle_entry_passive_live(o))continue;
        if(kind>=6||!s->passive_vehicle_sphere_count[kind]||s->passive_vehicle_sphere_count[kind]>8)return RF_FORMAT;
        body.spheres=s->passive_vehicle_spheres[kind];body.count=s->passive_vehicle_sphere_count[kind];
        body.handle=o->handle;body.driver=UINT32_MAX;
        memcpy(body.position,o->pose.position,12);memcpy(body.next_position,o->pose.position,12);
        memcpy(body.orientation,o->pose.input_matrix,36);memcpy(body.next_orientation,o->pose.input_matrix,36);
        status=scene_driller_actor_compose(&body,actor,host,query,hit,found);if(status)return status;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.handle==actor||o->registration.handle==host||!scene_vehicle_entry_npc_live(o))continue;
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
        status=scene_driller_actor_compose(&body,actor,host,query,hit,found);if(status)return status;
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
/* A sweep from a seated head cannot establish standing destination fit, and
 * the shared sphere sweep intentionally skips zero-length motion. Reuse the
 * actual-body placement/pair tests, including props and loose GeoMod pieces.
 * This runs only for exit candidates, never for low seated boarding poses. */
static int scene_vehicle_entry_exit_placement(scene_stream *s,const scene_vehicle_pose *pose)
{
    rf_checkpoint_placement p={0};rf_collision_room_location room;
    scene_checkpoint_world_context obstacles={0};int status;
    p.world=s->collision;p.query_flags=4;p.spheres=scene_actor_body.spheres.items;p.count=scene_actor_body.spheres.count;
    if(!scene_actor_body.allocated_bytes||!p.spheres||!p.count)return RF_NOT_FOUND;
    memcpy(p.position,pose->position,12);memcpy(p.basis,pose->basis,36);
    status=rf_geometry_collision_world_locate(p.world,p.position,&room);if(status)return status;
    if(room.room>=p.world->room_count)return RF_NOT_FOUND;p.replaced_room=room.room;
    status=rf_checkpoint_world_placement_check(&p,NULL);if(status)return status;
    obstacles.movers=&campaign_movers;status=scene_checkpoint_world_obstacles(&obstacles,&p);if(status)return status;
    {rf_geometry_collision_movers no_movers={0};obstacles.movers=&no_movers;
     for(uint32_t i=0;campaign_clutter_bodies&&i<campaign_clutter_records.count;i++){
        if(!campaign_clutter_bodies[i])continue;
        obstacles.props=campaign_clutter_bodies+i;obstacles.prop_count=1;
        status=scene_checkpoint_world_obstacles(&obstacles,&p);if(status)return status;
     }}
    for(uint32_t i=0,n=s->terrain_source_count?s->terrain_source_count:1;i<n;i++){
        status=rf_geomod_piece_registry_placement_check(scene_detached_source_registry(s,i),&p);if(status)return status;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.handle==campaign_player_object.handle||!o->body.allocated_bytes||!scene_vehicle_entry_npc_live(o))continue;
        status=scene_vehicle_wreck_pair(&p,o->body.spheres.items,o->body.spheres.count,
            o->body.state.position,o->body.state.orientation);if(status)return status;
    }
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *o=campaign_passive_vehicles+i;rf_physics_sphere spheres[8]={{0}};
        uint32_t kind=o->resource_kind,n;
        if(o->handle==campaign_player_object.handle||o->handle==s->driller_runtime->entry.host.handle||!scene_vehicle_entry_passive_live(o))continue;
        if(kind>=6)return RF_FORMAT;n=s->passive_vehicle_sphere_count[kind];if(!n||n>8)return RF_RANGE;
        for(uint32_t j=0;j<n;j++){memcpy(spheres[j].center,s->passive_vehicle_spheres[kind][j].center,12);spheres[j].radius=s->passive_vehicle_spheres[kind][j].radius;}
        status=scene_vehicle_wreck_pair(&p,spheres,n,o->pose.position,o->pose.input_matrix);if(status)return status;
    }
    for(uint32_t i=0;i<scene_turret_count;i++){
        const scene_turret_owner *o=scene_turrets+i;rf_physics_sphere spheres[8]={{0}};
        if(!scene_vehicle_entry_turret_live(o))continue;
        const rf_static_render_resource *model=&scene_turret_models[o->live_model].render;uint32_t n=model->sphere_count;
        if(n>8)return RF_RANGE;
        if(n){status=rf_model_creation_spheres(model->spheres,n,1,NULL,0,spheres,8);if(status)return status;}
        else{n=1;memcpy(spheres[0].center,model->bound,12);spheres[0].radius=model->bound[3];}
        status=scene_vehicle_wreck_pair(&p,spheres,n,o->position,o->basis);if(status)return status;
    }
    return RF_OK;
}
static int scene_vehicle_entry_exit_clear(void *context,const scene_vehicle_pose *pose,uint32_t *clear)
{
    scene_stream *s=context;int status;
    if(!s||!s->driller_runtime||!s->collision||!pose||!clear)return RF_RANGE;
    *clear=0;
    status=scene_vehicle_entry_exit_placement(s,pose);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;*clear=1;return RF_OK;
}
static void scene_vehicle_entry_collision_bind(scene_stream *s)
{
    if(!s||!s->driller_runtime)return;
    scene_driller_entry_exit *e=&s->driller_runtime->entry;
    e->collision.world=s->collision;e->collision.movers=&campaign_movers;
    e->collision.query=scene_vehicle_entry_query;e->collision.query_context=s;
    e->exit_clear=scene_vehicle_entry_exit_clear;e->exit_context=s;
}
#endif
