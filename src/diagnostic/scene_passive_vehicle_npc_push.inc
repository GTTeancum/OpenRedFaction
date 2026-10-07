#ifndef SCENE_PASSIVE_VEHICLE_NPC_PUSH_INC
#define SCENE_PASSIVE_VEHICLE_NPC_PUSH_INC
/* First-pass translated attached-chassis response for unseated living NPCs.
 * The caller consumes one freshly published controller interval before events
 * and ordinary NPC movement. Rotation, crush damage and vehicle/vehicle response are not
 * approximated here. This path never repeats angular integration. */
uint32_t rf_scene_passive_npc_push[8]; /* sweeps,hits,moves,blocked,actor,pusher,blocker,error */
uint32_t rf_scene_passive_npc_push_history[8][36],rf_scene_passive_npc_push_last[36];
static void scene_passive_vehicle_npc_push_reset(void)
{
    memset(rf_scene_passive_npc_push,0,sizeof(rf_scene_passive_npc_push));
    memset(rf_scene_passive_npc_push_history,0,sizeof(rf_scene_passive_npc_push_history));
    memset(rf_scene_passive_npc_push_last,0,sizeof(rf_scene_passive_npc_push_last));
    memset(rf_scene_passive_npc_blocker_fixture,0,sizeof(rf_scene_passive_npc_blocker_fixture));
    memset(rf_scene_passive_npc_blocker_separation,0,sizeof(rf_scene_passive_npc_blocker_separation));
}
static void scene_passive_vehicle_npc_push_capture(void)
{
    uint32_t i;
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *npc=campaign_npc_bodies+i;
        npc->passive_push_handle=npc->registration.handle;
        memcpy(npc->passive_push_position,npc->body.state.position,12);
        memcpy(npc->passive_push_basis,npc->body.state.orientation,36);
    }
}
static int scene_passive_vehicle_npc_push(scene_stream *s,campaign_npc_body *npc,uint32_t frame)
{
    rf_collision_body_sphere spheres[8];rf_collision_body_query query={0};
    uint32_t index,i,slot,actor,found;int status=RF_OK;
    if(!s||!npc)return RF_RANGE;
    if(!npc->registration.view||npc->damage.effects.health<=0||!npc->body.spheres.count||
       npc->body.spheres.count>8||(npc->object_flags&(2u|8u|0x4000u))||
       npc->view.linked_handle!=-1||npc->movement_slot>=16||
       (campaign_modes[npc->movement_slot].index!=1&&campaign_modes[npc->movement_slot].index!=3)||
       !isfinite(npc->body.state.mass)||npc->body.state.mass<=0||campaign_npc_physics_suspended(npc))return RF_OK;
    slot=(uint32_t)(npc-campaign_npc_bodies);actor=npc->registration.handle;
    /* A controller arrival callback can teleport an NPC during this interval.
     * Never sweep the earlier chassis motion against that newly placed body. */
    if(npc->passive_push_handle!=actor||memcmp(npc->passive_push_position,npc->body.state.position,12)||
       memcmp(npc->passive_push_basis,npc->body.state.orientation,36))return RF_OK;
    if(slot>=campaign_npc_body_count||slot>=campaign_seeds.records.count||
       rf_object_registry_lookup(&campaign_registry,actor)!=&npc->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)actor)!=&npc->view)return RF_NOT_FOUND;
    for(i=0;i<npc->body.spheres.count;i++){
        memcpy(spheres[i].center,npc->body.spheres.items[i].center,12);
        spheres[i].radius=npc->body.spheres.items[i].radius;
    }
    memcpy(query.matrix,npc->body.state.orientation,36);query.spheres=spheres;
    query.count=npc->body.spheres.count;query.radius=npc->body.state.bounds.radius;query.flags=0x460;
    for(index=0;index<campaign_passive_vehicle_count;index++){
        const scene_passive_vehicle *owner=campaign_passive_vehicles+index;
        const scene_driller_resources *resource;scene_driller_actor_collision host={0};
        rf_geometry_body_hit contact={0},obstacle={0};rf_physics_body_state motion;
        float delta[3],approach=0,remaining,limit,health=npc->damage.effects.health;
        uint32_t kind=owner->resource_kind,record[36]={0},moved;int32_t saved_link;
        if(!owner->attached||!owner->motion_valid||(owner->damage.object_flags&0x4000u)||
           npc->support.handle==owner->handle||
           memcmp(owner->previous_basis,owner->pose.input_matrix,36))continue;
        if(rf_object_registry_lookup(&campaign_registry,owner->handle)!=owner){status=RF_NOT_FOUND;goto fail;}
        if(kind>=6||!s->passive_vehicle_sphere_count[kind]){status=RF_FORMAT;goto fail;}
        resource=s->passive_vehicle_resources[kind];
        if(!resource&&s->driller&&campaign_selected_vehicle_resource_kind()==kind)resource=s->driller;
        if(!resource){status=RF_NOT_FOUND;goto fail;}
        for(i=0;i<3;i++)delta[i]=owner->pose.position[i]-owner->previous_position[i];
        if(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]<1e-8f)continue;
        host.spheres=s->passive_vehicle_spheres[kind];host.count=s->passive_vehicle_sphere_count[kind];
        host.handle=owner->handle;host.material=resource->physics.material.index;
        memcpy(host.position,owner->previous_position,12);memcpy(host.next_position,owner->pose.position,12);
        memcpy(host.orientation,owner->previous_basis,36);memcpy(host.next_orientation,owner->pose.input_matrix,36);
        memcpy(host.velocity,owner->velocity,12);
        memcpy(query.start,npc->body.state.position,12);memcpy(query.end,query.start,12);query.limit=1;
        ++rf_scene_passive_npc_push[0];
        status=scene_driller_actor_query(&host,actor,(uint32_t)npc->view.linked_handle,&query,&contact,&found);
        if(status)goto fail;
        if(!found||fabsf(contact.contact.normal[1])>=.5f)continue;
        for(i=0;i<3;i++)approach+=delta[i]*contact.contact.normal[i];
        if(approach<=0)continue;
        ++rf_scene_passive_npc_push[1];motion=npc->body.state;
        remaining=fminf(1.f,1.f-contact.contact.fraction+.01f);
        for(i=0;i<3;i++)motion.next_position[i]=motion.position[i]+delta[i]*remaining;
        status=rf_physics_body_prepare_sweep(&motion);if(status)goto fail;
        /* Synchronous collision readers only: ignore exactly the pushing host,
         * retaining compiled world, movers, fragments and other passive hulls. */
        saved_link=npc->view.linked_handle;npc->view.linked_handle=(int32_t)owner->handle;
        status=rf_scene_npc_body_sweep(s->collision,actor,&motion,0x460,spheres,8,&obstacle,&found);
        npc->view.linked_handle=saved_link;
        if(status)goto fail;
        /* The ordinary NPC sweep has no active-host overlay. Keep that host
         * as a clearance blocker too, using this NPC's actual identity. */
        if(s->driller_runtime){
            const scene_driller_runtime *r=s->driller_runtime;scene_driller_actor_collision active={0};
            active.spheres=r->physics.collision;active.count=r->physics.sphere_count;
            active.handle=r->entry.host.handle;active.driver=r->entry.host.driver;
            active.material=s->driller->physics.material.index;
            memcpy(active.position,r->physics.state.position,12);memcpy(active.next_position,active.position,12);
            memcpy(active.orientation,r->physics.state.orientation,36);memcpy(active.next_orientation,active.orientation,36);
            memcpy(query.start,motion.position,12);memcpy(query.end,motion.next_position,12);query.limit=1;
            status=scene_driller_actor_compose(&active,actor,(uint32_t)saved_link,&query,&obstacle,&found);
            if(status)goto fail;
        }
        limit=found?fmaxf(0.f,obstacle.contact.fraction-.001f):1.f;
        record[0]=frame;record[1]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
        record[2]=actor;record[3]=owner->uid;record[4]=owner->handle;
        record[5]=found?obstacle.contact.object_id:0;
        memcpy(record+6,&contact.contact.fraction,4);memcpy(record+7,&limit,4);
        memcpy(record+8,motion.position,12);memcpy(record+11,motion.next_position,12);
        memcpy(record+17,owner->previous_position,12);memcpy(record+20,owner->pose.position,12);
        record[23]=found;memcpy(record+25,&health,4);record[27]=npc->support.handle;record[29]=(uint32_t)saved_link;
        for(i=0;i<3;i++)motion.position[i]=motion.next_position[i]=
            motion.position[i]+(motion.next_position[i]-motion.position[i])*limit;
        moved=memcmp(motion.position,record+8,12)!=0;
        status=rf_physics_body_prepare_sweep(&motion);if(status)goto fail;
        npc->body.state=motion;
        status=rf_scene_npc_publish_position(actor);if(status)goto fail;
        memcpy(record+14,npc->body.state.position,12);record[24]=moved;
        memcpy(record+26,&npc->damage.effects.health,4);record[28]=npc->support.handle;
        record[30]=(uint32_t)npc->view.linked_handle;record[31]=
            rf_object_registry_lookup(&campaign_registry,actor)==&npc->registration&&
            rf_object_registry_lookup(&campaign_registry,owner->handle)==owner;
        memcpy(record+32,npc->published,12);record[35]=npc->object_flags;
        rf_scene_passive_npc_push[2]+=moved;rf_scene_passive_npc_push[3]+=found;
        rf_scene_passive_npc_push[4]=actor;rf_scene_passive_npc_push[5]=owner->handle;
        rf_scene_passive_npc_push[6]=record[5];
        if(rf_scene_passive_npc_push[1]<=8)memcpy(rf_scene_passive_npc_push_history[rf_scene_passive_npc_push[1]-1],record,sizeof(record));
        memcpy(rf_scene_passive_npc_push_last,record,sizeof(record));
    }
    return RF_OK;
fail:rf_scene_passive_npc_push[7]=(uint32_t)status;return status;
}
static int scene_passive_vehicle_npc_push_tick(scene_stream *s,uint32_t frame)
{
    uint32_t i;int status;
    if(campaign_passive_npc_push_epoch==campaign_passive_vehicle_motion_epoch)return RF_OK;
    campaign_passive_npc_push_epoch=campaign_passive_vehicle_motion_epoch;
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *npc=campaign_npc_bodies+i;
        if(scene_npc_seat_attached(npc->registration.handle))continue;
        status=scene_passive_vehicle_npc_push(s,npc,frame);if(status)return status;
    }
    return RF_OK;
}
#endif
