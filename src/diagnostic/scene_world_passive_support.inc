#ifndef RF_SCENE_WORLD_PASSIVE_SUPPORT_INCLUDED
#define RF_SCENE_WORLD_PASSIVE_SUPPORT_INCLUDED
/* Validate the saved actor against the candidate attached chassis, before
 * either actor or vehicle state is published. World geometry, movers and
 * props still own clearance; only this exact UID supplies ground contact. */
static int scene_world_passive_support_place(scene_stream *stream,
    scene_checkpoint_world_context *context,const rf_checkpoint_placement *placement,
    const scene_passive_vehicle_checkpoint_stage *passive,uint32_t index,uint32_t actor_handle,uint32_t linked_handle,
    rf_entity_room_state *room,rf_physics_support_contact *support)
{
    const scene_passive_vehicle *owner;const scene_passive_vehicle_saved *saved;
    const scene_driller_resources *resource;scene_driller_actor_collision host={0};
    rf_checkpoint_placement p;rf_collision_room_location location;
    rf_physics_ground_probe probe;rf_collision_body_query query={0};
    rf_collision_body_sphere sphere;rf_geometry_body_hit hit={0};
    uint32_t kind,matched,i;int status;
    if(!stream||!context||!placement||!passive||!room||!support||
       index>=passive->count||index>=campaign_passive_vehicle_count)return RF_RANGE;
    owner=campaign_passive_vehicles+index;saved=passive->rows+index;kind=owner->resource_kind;
    if(saved->uid!=owner->uid||kind>=6||
       (saved->has_damage&&(saved->destroyed||saved->health<=0||(saved->object_flags&(2u|0x4000u))))||
       !stream->passive_vehicle_sphere_count[kind])return RF_FORMAT;
    resource=stream->passive_vehicle_resources[kind];
    if(!resource&&stream->driller&&campaign_selected_vehicle_resource_kind()==kind)
        resource=stream->driller;
    if(!resource)return RF_FORMAT;
    status=rf_geometry_collision_world_locate(context->world,placement->position,&location);
    if(status)return status;
    if(location.room==UINT32_MAX||location.room>=context->world->room_count)return RF_NOT_FOUND;
    context->reference_room=location.room;
    p=*placement;p.world=context->world;p.replaced_room=location.room;
    status=rf_checkpoint_world_placement_check(&p,NULL);if(status)return status;
    status=scene_checkpoint_world_obstacles(context,&p);if(status)return status;
    status=rf_physics_ground_prepare(p.spheres,p.count,p.position,p.query_flags,0,
        context->dt,context->class_speed,0,&probe);if(status)return status;
    memcpy(sphere.center,probe.sphere.center,12);sphere.radius=probe.sphere.radius;
    memcpy(query.start,probe.start,12);memcpy(query.end,probe.end,12);
    for(i=0;i<3;i++)query.matrix[i][i]=1;
    query.radius=probe.bounds.radius;query.flags=probe.query_flags;
    query.spheres=&sphere;query.count=1;query.limit=1;
    host.spheres=stream->passive_vehicle_spheres[kind];
    host.count=stream->passive_vehicle_sphere_count[kind];
    host.handle=owner->handle;host.material=resource->physics.material.index;
    memcpy(host.position,saved->position,12);memcpy(host.next_position,saved->position,12);
    memcpy(host.orientation,saved->basis,36);memcpy(host.next_orientation,saved->basis,36);
    status=scene_driller_actor_query(&host,actor_handle,linked_handle,&query,&hit,&matched);if(status)return status;
    if(!matched||hit.contact.object_id!=owner->handle||hit.contact.fraction>=1||
       hit.contact.normal[1]<.5f)return RF_NOT_FOUND;
    memset(room,0,sizeof(*room));room->room=location.room+1;
    memcpy(room->query_position,p.position,12);
    support->handle=owner->handle;support->material=(int32_t)hit.contact.material;
    return RF_OK;
}
#endif
