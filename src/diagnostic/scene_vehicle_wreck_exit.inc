#ifndef RF_SCENE_VEHICLE_WRECK_EXIT_INCLUDED
#define RF_SCENE_VEHICLE_WRECK_EXIT_INCLUDED
/* Include after scene_driller_checkpoint_adapter.inc. A live authored Jeep driver leaves a
 * destroyed host only after an actual-shape, grounded exit is admitted. This
 * is a port egress policy, not a recovered original ejection trajectory.
 * No clear exit: retain the real seat and retry next frame. Generic teardown,
 * actor death, Remove_Object and teleport retain their existing unbind path.
 */
uint32_t rf_scene_vehicle_wreck_exit[12]; /* attempts,exits,blocked,errors,frame,actor,host,direction,status,stage,actorUID,hostUID */
uint32_t rf_scene_vehicle_wreck_exit_apply[32]; /* first exit: frame,actor,host,parent,driver,occupant,AI,health,seatXYZ,exitXYZ,velocityXYZ,hostVelocityXYZ,angularXYZ,hostXYZ,direction,seatActive,reserved4 */
static void scene_vehicle_wreck_exit_reset(void)
{memset(rf_scene_vehicle_wreck_exit,0,sizeof(rf_scene_vehicle_wreck_exit));memset(rf_scene_vehicle_wreck_exit_apply,0,sizeof(rf_scene_vehicle_wreck_exit_apply));}
typedef struct scene_vehicle_wreck_query {scene_stream *stream;uint32_t actor,host;} scene_vehicle_wreck_query;
/* The normal static gateway does not include actor bodies. Compose the same
 * actual sphere-union swept contacts used by moving hosts, with no unrelated
 * actor/occupant exclusion. The wreck itself is intentionally excluded from
 * transit out of its interior; final placement must be disjoint from it. */
static int scene_vehicle_wreck_actor_sweep(const rf_physics_body *body,uint32_t handle,
    uint32_t actor,const rf_collision_body_query *query,rf_geometry_body_hit *hit,uint32_t *found)
{
    rf_collision_body_sphere spheres[64];scene_driller_actor_collision other={0};
    if(!body->allocated_bytes||!body->spheres.count)return RF_OK;
    if(!body->spheres.items||body->spheres.count>64)return RF_RANGE;
    for(uint32_t i=0;i<body->spheres.count;i++){
        memcpy(spheres[i].center,body->spheres.items[i].center,12);spheres[i].radius=body->spheres.items[i].radius;
    }
    other.spheres=spheres;other.count=body->spheres.count;other.handle=handle;other.driver=UINT32_MAX;
    memcpy(other.position,body->state.position,12);memcpy(other.next_position,other.position,12);
    memcpy(other.orientation,body->state.orientation,36);memcpy(other.next_orientation,other.orientation,36);
    return scene_driller_actor_compose(&other,actor,UINT32_MAX,query,hit,found);
}
static int scene_vehicle_wreck_query_body(void *context,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *found)
{
    scene_vehicle_wreck_query *c=context;int status;
    status=campaign_body_query_batch_for(c->stream->collision,query,hit,found,NULL,&campaign_movers);if(status)return status;
    status=scene_passive_vehicle_actor_collision(c->stream,c->actor,c->host,query,hit,found);if(status)return status;
    /* A parked wreck's occupant must still respect the independently selected
     * chassis. Use this NPC identity, never the player's collision adapter. */
    if(scene_driller_visible(c->stream)&&c->stream->driller_runtime->entry.host.handle!=c->host){
        const scene_driller_runtime *r=c->stream->driller_runtime;scene_driller_actor_collision body={0};
        body.spheres=r->physics.collision;body.count=r->physics.sphere_count;
        body.handle=r->entry.host.handle;body.driver=r->entry.host.driver;
        memcpy(body.position,r->physics.state.position,12);memcpy(body.next_position,body.position,12);
        memcpy(body.orientation,r->physics.state.orientation,36);memcpy(body.next_orientation,body.orientation,36);
        status=scene_driller_actor_compose(&body,c->actor,c->host,query,hit,found);if(status)return status;
    }
    if(campaign_player_object.view && !(campaign_player_view.flags_7c&(2u|0x4000u))){
        status=scene_vehicle_wreck_actor_sweep(&scene_actor_body,campaign_player_object.handle,c->actor,query,hit,found);if(status)return status;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(!o->registration.view||o->registration.handle==c->actor||(o->object_flags&(2u|0x4000u)))continue;
        status=scene_vehicle_wreck_actor_sweep(&o->body,o->registration.handle,c->actor,query,hit,found);if(status)return status;
    }
    for(uint32_t i=0;i<query->count;i++){
        float start[3],end[3],delta[3];rf_geomod_registry_hit piece;uint32_t yes=0,liquid=0;
        rf_weapon_flight_contact prop={0};
        scene_driller_point(query->start,(const float*)query->matrix,query->spheres[i].center,start);
        scene_driller_point(query->end,(const float*)query->matrix,query->spheres[i].center,end);
        for(uint32_t k=0;k<3;k++)delta[k]=end[k]-start[k];
        status=scene_detached_sources_sweep(c->stream,query->flags,start,delta,query->spheres[i].radius,
            *found?hit->contact.fraction:query->limit,&piece,&yes);if(status)return status;
        if(yes){memset(hit,0,sizeof(*hit));hit->contact.fraction=piece.piece.hit.fraction;
            memcpy(hit->contact.normal,piece.piece.hit.normal,12);memcpy(hit->contact.point,piece.piece.hit.point,12);
            hit->contact.object_id=UINT32_MAX;hit->solid=hit->room=hit->face=UINT32_MAX;hit->sphere=i;*found=1;}
        yes=*found;prop.hit.fraction=yes?hit->contact.fraction:query->limit;
        status=scene_clutter_projectile_compose(c->actor,start,delta,query->spheres[i].radius,&prop,&liquid,&yes);if(status)return status;
        if(yes&&(!*found||prop.hit.fraction<hit->contact.fraction)){
            memset(hit,0,sizeof(*hit));hit->contact.fraction=prop.hit.fraction;
            memcpy(hit->contact.normal,prop.hit.normal,12);memcpy(hit->contact.point,prop.hit.point,12);
            hit->contact.object_id=prop.face;hit->solid=hit->room=hit->face=UINT32_MAX;hit->sphere=i;*found=1;
        }
    }
    return RF_OK;
}
static int scene_vehicle_wreck_pair(const rf_checkpoint_placement *p,const rf_physics_sphere *spheres,
    uint32_t count,const float position[3],const float basis[9])
{
    rf_checkpoint_placement other={0};float first[8][3],second[8][3];int status;
    if(!count)return RF_OK;
    other.spheres=spheres;other.count=count;memcpy(other.position,position,12);memcpy(other.basis,basis,36);
    status=scene_npc_checkpoint_pair_centers(p,first);if(status)return status;
    status=scene_npc_checkpoint_pair_centers(&other,second);if(status)return status;
    for(uint32_t a=0;a<p->count;a++)for(uint32_t b=0;b<count;b++){
        double d2=0,r=(double)p->spheres[a].radius+spheres[b].radius-.002;
        for(uint32_t k=0;k<3;k++){double d=(double)first[a][k]-second[b][k];d2+=d*d;}
        if(d2<r*r)return RF_NOT_FOUND;
    }
    return RF_OK;
}
static int scene_vehicle_wreck_destination(scene_stream *s,campaign_npc_body *actor,
    const scene_vehicle_pose *pose)
{
    rf_checkpoint_placement p={0};rf_collision_room_location room;scene_checkpoint_world_context obstacles={0};int status;
    p.world=s->collision;p.query_flags=4;p.spheres=actor->body.spheres.items;p.count=actor->body.spheres.count;
    memcpy(p.position,pose->position,12);memcpy(p.basis,pose->basis,36);
    status=rf_geometry_collision_world_locate(p.world,p.position,&room);if(status)return status;
    if(room.room==UINT32_MAX||room.room>=p.world->room_count)return RF_NOT_FOUND;p.replaced_room=room.room;
    status=rf_checkpoint_world_placement_check(&p,NULL);if(status)return status;
    obstacles.movers=&campaign_movers;
    status=scene_checkpoint_world_obstacles(&obstacles,&p);if(status)return status;
    /* The live prop array can contain absent owners; the restore helper takes
     * compact non-null rows, so supply one admitted live row at a time. */
    {rf_geometry_collision_movers no_movers={0};obstacles.movers=&no_movers;
     for(uint32_t i=0;campaign_clutter_bodies&&i<campaign_clutter_records.count;i++){
        if(!campaign_clutter_bodies[i])continue;
        obstacles.props=campaign_clutter_bodies+i;obstacles.prop_count=1;
        status=scene_checkpoint_world_obstacles(&obstacles,&p);if(status)return status;
     }}
    for(uint32_t i=0,n=s->terrain_source_count?s->terrain_source_count:1;i<n;i++){
        status=rf_geomod_piece_registry_placement_check(scene_detached_source_registry(s,i),&p);if(status)return status;
    }
    if(scene_driller_visible(s)){
        const scene_driller_physics *host=&s->driller_runtime->physics;
        status=scene_vehicle_wreck_pair(&p,host->spheres,host->sphere_count,host->state.position,host->state.orientation);if(status)return status;
    }
    if(campaign_player_object.view && !(campaign_player_view.flags_7c&(2u|0x4000u)) && scene_actor_body.allocated_bytes){
        status=scene_vehicle_wreck_pair(&p,scene_actor_body.spheres.items,scene_actor_body.spheres.count,
            scene_actor_body.state.position,scene_actor_body.state.orientation);if(status)return status;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o==actor||!o->registration.view||!o->body.allocated_bytes||(o->object_flags&(2u|0x4000u)))continue;
        status=scene_vehicle_wreck_pair(&p,o->body.spheres.items,o->body.spheres.count,o->body.state.position,o->body.state.orientation);if(status)return status;
    }
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *o=campaign_passive_vehicles+i;rf_physics_sphere spheres[8]={{0}};uint32_t kind=o->resource_kind,n;
        if(o->damage.object_flags&0x4000u)continue;
        if(kind>=6)return RF_FORMAT;n=s->passive_vehicle_sphere_count[kind];if(n>8)return RF_RANGE;
        for(uint32_t j=0;j<n;j++){memcpy(spheres[j].center,s->passive_vehicle_spheres[kind][j].center,12);spheres[j].radius=s->passive_vehicle_spheres[kind][j].radius;}
        status=scene_vehicle_wreck_pair(&p,spheres,n,o->pose.position,o->pose.input_matrix);if(status)return status;
    }
    for(uint32_t i=0;i<scene_turret_count;i++){
        const scene_turret_owner *o=scene_turrets+i;rf_physics_sphere spheres[8]={{0}};
        if(o->dead||(o->view.flags_7c&(2u|0x4000u)))continue;
        const rf_static_render_resource *model=&scene_turret_models[o->live_model].render;uint32_t n=model->sphere_count;
        if(n>8)return RF_RANGE;
        if(n){status=rf_model_creation_spheres(model->spheres,n,1,NULL,0,spheres,8);if(status)return status;}
        else{n=1;memcpy(spheres[0].center,model->bound,12);spheres[0].radius=model->bound[3];}
        status=scene_vehicle_wreck_pair(&p,spheres,n,o->position,o->basis);if(status)return status;
    }
    return RF_OK;
}
/* API used only in seat tick after final Jeep movement. released0 means a
 * valid but blocked request: caller must retain/update the seat this frame. */
static int scene_vehicle_wreck_exit_try(scene_stream *s,uint32_t actor,uint32_t host,
    uint32_t frame,int32_t now,uint32_t *released)
{
    static const float directions[8][2]={{1,0},{-1,0},{0,1},{0,-1},{.707106781f,.707106781f},
        {-.707106781f,.707106781f},{.707106781f,-.707106781f},{-.707106781f,-.707106781f}};
    scene_npc_seat_binding *binding=NULL;scene_npc_seat_host h;campaign_npc_body *o;
    scene_vehicle_collision collision;scene_vehicle_wreck_query context={s,actor,host};rf_collision_body_sphere spheres[8];
    scene_driller_physics parked_physics={0};const scene_driller_physics *physics=NULL;
    scene_passive_vehicle *passive=NULL;
    scene_vehicle_pose from,to;rf_physics_body_state next;float published[3],velocity[3],angular[3],offset[3],yaw;
    uint32_t flags,cls,attached,stage=1,direction;int status;
    if(!released)return RF_RANGE;*released=0;
    ++rf_scene_vehicle_wreck_exit[0];rf_scene_vehicle_wreck_exit[4]=frame;
    rf_scene_vehicle_wreck_exit[5]=actor;rf_scene_vehicle_wreck_exit[6]=host;
    if(!s||!s->driller_runtime||!s->driller||now<0||now>RF_TIMER_PERIOD){status=RF_RANGE;goto fail;}
    for(uint32_t i=0;i<scene_npc_seat_count;i++)if(scene_npc_seats[i].active&&scene_npc_seats[i].kind==SCENE_NPC_SEAT_JEEP&&
        scene_npc_seats[i].actor==actor&&scene_npc_seats[i].host==host){binding=scene_npc_seats+i;break;}
    if(!binding||!(o=scene_npc_seat_actor(binding))){status=RF_NOT_FOUND;goto fail;}
    status=scene_npc_seat_host_read(binding,&h);if(status)goto fail;
    status=scene_npc_seat_pair(host,actor,binding->tag,&attached);if(status)goto fail;
    if(!attached||h.alive||!(o->damage.effects.health>0)||(o->view.flags_810&1u)||
       !o->body.allocated_bytes||!o->body.spheres.count||o->body.spheres.count>8||
       binding->slot>=campaign_model_owner_count||!campaign_npc_eyes){status=RF_NOT_FOUND;goto fail;}
    if(h.view){
        if(s->driller_runtime->entry.host.handle!=host||strcmp(s->driller->model,"Jeep01.v3m")){status=RF_NOT_FOUND;goto fail;}
        physics=&s->driller_runtime->physics;
    }else{
        passive=scene_npc_seat_passive_jeep_host(binding);
        if(!passive||!s->passive_vehicle_sphere_count[5]||s->passive_vehicle_sphere_count[5]>8){status=RF_NOT_FOUND;goto fail;}
        /* This owner has a fixed unparented pose, not another rigid solver.
         * Reuse its admitted class hull and inherit no selected-host motion. */
        parked_physics.sphere_count=s->passive_vehicle_sphere_count[5];
        memcpy(parked_physics.state.position,passive->pose.position,12);
        memcpy(parked_physics.state.orientation,passive->pose.input_matrix,36);
        for(uint32_t i=0;i<parked_physics.sphere_count;i++){
            const rf_collision_body_sphere *shape=s->passive_vehicle_spheres[5]+i;
            float extent=sqrtf(shape->center[0]*shape->center[0]+shape->center[1]*shape->center[1]+shape->center[2]*shape->center[2])+shape->radius;
            if(!isfinite(extent)||!isfinite(shape->radius)||shape->radius<=0){status=RF_FORMAT;goto fail;}
            parked_physics.collision[i]=*shape;
            memcpy(parked_physics.spheres[i].center,shape->center,12);parked_physics.spheres[i].radius=shape->radius;
            if(extent>parked_physics.radius)parked_physics.radius=extent;
        }
        physics=&parked_physics;
    }
    cls=campaign_seeds.items[binding->slot].class_index;if(cls>=campaign_seeds.class_count){status=RF_RANGE;goto fail;}
    rf_scene_vehicle_wreck_exit[10]=(uint32_t)campaign_seeds.records.items[binding->slot].record.uid;
    rf_scene_vehicle_wreck_exit[11]=binding->host_uid;
    /* Recompute the seat at this final host pose, not last frame's NPC cache. */
    {float tag[12];status=rf_static_model_tag_place(h.tags,binding->tag,h.basis,h.position,tag);if(status)goto fail;
     memcpy(from.position,tag+9,12);memcpy(from.basis,tag,36);}
    yaw=atan2f(from.basis[6],from.basis[8]);memset(to.basis,0,36);
    to.basis[0]=to.basis[8]=cosf(yaw);to.basis[2]=-sinf(yaw);to.basis[6]=sinf(yaw);to.basis[4]=1;
    collision=s->driller_runtime->collision;collision.query=scene_vehicle_wreck_query_body;collision.query_context=&context;
    collision.spheres=spheres;collision.count=o->body.spheres.count;collision.radius=0;
    for(uint32_t i=0;i<collision.count;i++){
        memcpy(spheres[i].center,o->body.spheres.items[i].center,12);spheres[i].radius=o->body.spheres.items[i].radius;
        float radius=sqrtf(spheres[i].center[0]*spheres[i].center[0]+spheres[i].center[1]*spheres[i].center[1]+spheres[i].center[2]*spheres[i].center[2])+spheres[i].radius;
        if(radius>collision.radius)collision.radius=radius;
    }
    status=scene_vehicle_collision_valid(&collision);if(status)goto fail;
    for(direction=0;direction<8;direction++){
        float axis[3],extent=0,actor_min=FLT_MAX,end[3],drop=physics->radius*2+4;
        rf_collision_body_query query;rf_geometry_body_hit hit={0};uint32_t clear,found;
        for(uint32_t k=0;k<3;k++)axis[k]=directions[direction][0]*to.basis[k]+directions[direction][1]*to.basis[6+k];
        for(uint32_t i=0;i<physics->sphere_count;i++){
            float center[3],edge=physics->collision[i].radius;scene_driller_point((const float[3]){0,0,0},h.basis,physics->collision[i].center,center);
            for(uint32_t k=0;k<3;k++)edge+=center[k]*axis[k];if(edge>extent)extent=edge;
        }
        for(uint32_t i=0;i<collision.count;i++){
            float center[3],edge=-spheres[i].radius;scene_driller_point((const float[3]){0,0,0},to.basis,spheres[i].center,center);
            for(uint32_t k=0;k<3;k++)edge+=center[k]*axis[k];if(edge<actor_min)actor_min=edge;
        }
        for(uint32_t k=0;k<3;k++)to.position[k]=h.position[k]+axis[k]*(extent-actor_min+.15f);
        to.position[1]=from.position[1]+fabsf(campaign_npc_eyes[cls].offsets[1]);
        stage=2;status=scene_vehicle_seated_clearance(&collision,&o->body,campaign_npc_eyes[cls].offsets,
            from.position,from.basis,to.position,to.basis,&clear);if(status)goto fail;if(!clear)continue;
        stage=3;memcpy(end,to.position,12);end[1]-=drop;
        scene_vehicle_collision_body(&collision,to.position,end,to.basis,&query);
        status=scene_vehicle_wreck_query_body(&context,&query,&hit,&found);if(status)goto fail;
        if(!found||hit.contact.normal[1]<.5f||hit.contact.fraction<0||hit.contact.fraction>=1||
           (hit.solid==UINT32_MAX&&hit.room==UINT32_MAX))continue; /* World/mover floor, not another actor or loose piece. */
        to.position[1]-=drop*hit.contact.fraction;to.position[1]+=.01f;
        stage=4;status=scene_vehicle_wreck_destination(s,o,&to);if(status==RF_NOT_FOUND)continue;if(status)goto fail;
        break;
    }
    if(direction==8){++rf_scene_vehicle_wreck_exit[2];rf_scene_vehicle_wreck_exit[7]=UINT32_MAX;
        rf_scene_vehicle_wreck_exit[8]=0;rf_scene_vehicle_wreck_exit[9]=stage;return RF_OK;}
    stage=5;next=o->body.state;flags=o->object_flags;
    if(passive)memset(angular,0,sizeof(angular));
    else {status=scene_driller_checkpoint_rotate_tensor(physics->state.orientation,physics->state.inverse_inertia,physics->state.momentum,angular);if(status)goto fail;}
    for(uint32_t k=0;k<3;k++)offset[k]=from.position[k]-h.position[k];
    velocity[0]=physics->state.velocity[0]+angular[1]*offset[2]-angular[2]*offset[1];
    velocity[1]=physics->state.velocity[1]+angular[2]*offset[0]-angular[0]*offset[2];
    velocity[2]=physics->state.velocity[2]+angular[0]*offset[1]-angular[1]*offset[0];
    for(uint32_t k=0;k<3;k++)if(!isfinite(velocity[k])){status=RF_FORMAT;goto fail;}
    memcpy(next.position,to.position,12);memcpy(next.next_position,to.position,12);
    memcpy(next.orientation,to.basis,36);memcpy(next.next_orientation,to.basis,36);memcpy(next.velocity,velocity,12);
    memset(next.vector_c8,0,12);memset(next.vector_138,0,12);next.flags&=~0x400000u;next.reference_15c=-1;next.scalar_144=1;
    status=rf_physics_tensor_world(next.local_tensor,next.orientation,next.world_tensor);if(status)goto fail;
    status=rf_physics_publish_position(&next,published,&flags);if(status)goto fail;
    /* No further collision or placement decisions follow ownership release. */
    status=scene_npc_seat_release(binding,now);if(status)goto fail;
    o->body.state=next;o->object_flags=o->view.flags_7c=flags;memcpy(o->published,published,12);memcpy(o->previous,published,12);
    memcpy(campaign_model_owners[binding->slot].position,published,12);memcpy(campaign_model_owners[binding->slot].basis,to.basis,36);
    memset(o->look.body_angles,0,12);o->look.body_angles[1]=yaw;memset(&o->look.angles,0,sizeof(o->look.angles));
    memcpy(o->look.orientation,to.basis,36);memset(o->look.command_708,0,12);memset(o->command_714,0,12);
    o->support.handle=0;o->support.material=-1;memset(o->support_velocity,0,12);
    memset(&o->piece_support,0,sizeof(o->piece_support));o->piece_reacquire=0;o->previous_region_13ec=NULL;
    memset(&o->collision_contact,0,sizeof(o->collision_contact));o->collision_contact.material=UINT32_MAX;
    status=campaign_npc_eye_update(binding->slot);if(!status)status=rf_scene_npc_refresh_room(actor);if(status)goto fail;
    *released=1;++rf_scene_vehicle_wreck_exit[1];rf_scene_vehicle_wreck_exit[7]=direction;
    rf_scene_vehicle_wreck_exit[8]=0;rf_scene_vehicle_wreck_exit[9]=stage;
    if(rf_scene_vehicle_wreck_exit[1]==1){uint32_t *p=rf_scene_vehicle_wreck_exit_apply;
        p[0]=frame;p[1]=actor;p[2]=host;p[3]=(uint32_t)o->view.linked_handle;p[4]=*h.driver;p[5]=(uint32_t)*h.occupant;
        p[6]=(uint32_t)o->ai_mode.action_280;memcpy(p+7,&o->damage.effects.health,4);memcpy(p+8,from.position,12);
        memcpy(p+11,to.position,12);memcpy(p+14,velocity,12);memcpy(p+17,physics->state.velocity,12);
        memcpy(p+20,angular,12);memcpy(p+23,h.position,12);p[26]=direction;p[27]=binding->active;
    }
    return RF_OK;
fail:++rf_scene_vehicle_wreck_exit[3];rf_scene_vehicle_wreck_exit[8]=(uint32_t)status;rf_scene_vehicle_wreck_exit[9]=stage;return status;
}
#endif
