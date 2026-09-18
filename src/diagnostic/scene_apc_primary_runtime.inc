#ifndef RF_SCENE_APC_PRIMARY_RUNTIME_INC
#define RF_SCENE_APC_PRIMARY_RUNTIME_INC
/* Include after live vehicle runtime and AI projectile helpers. Eight actual
 * swept flights, AP direct damage only. World/mover/rubble nearest wins ties.
 * Existing body-hit shield receive is retained; independently protruding shield
 * silhouette and player view shield are not added to the projectile service.
 * A retired driver handle is never resolved as a new occupant. The retained
 * source remains the registered firing host for damage credit. No explosions.
 */
uint32_t rf_scene_apc_primary[8]; /* ticks,launches,contacts,expired,live,pool-full,damage,reserve */
typedef struct scene_apc_primary_launch_context {scene_stream *stream;uint32_t driver;} scene_apc_primary_launch_context;
static int scene_apc_primary_launch(void *context,uint32_t source,const scene_vehicle_primary_definition *definition,
    const float pose[12],const float direction[3],uint32_t *accepted)
{
    scene_apc_primary_launch_context *c=context;scene_apc_primary_state *state=&c->stream->apc_primary;
    scene_apc_primary_round round={0};rf_random_state random=state->random;float spread[3];uint32_t i;int status;
    *accepted=0;
    for(i=0;i<SCENE_APC_PRIMARY_CAPACITY&&state->rounds[i].flight.active;i++){}
    if(i==SCENE_APC_PRIMARY_CAPACITY){++rf_scene_apc_primary[5];return RF_OK;}
    status=rf_weapon_spread_ray(direction,definition->spread,&random,spread);if(status)return status;
    status=rf_weapon_flight_launch(&round.flight,pose+9,spread,definition->speed,definition->lifetime,definition->radius);if(status)return status;
    round.source=source;round.driver=c->driver;round.damage=definition->damage;round.kind=definition->damage_kind;
    memcpy(round.previous,pose+9,12);state->rounds[i]=round;state->random=random;
    ++rf_scene_apc_primary[1];*accepted=1;return RF_OK;
}
static int scene_apc_primary_damage(scene_stream *stream,uint32_t frame,const scene_apc_primary_round *round,
    const rf_weapon_flight_contact *contact,const float start[3],const float end[3])
{
    uint32_t tag=contact->object,handled=0,index,target,entered,bits,accepted=0,broken=0;float amount=0,seconds=(float)frame/60;int status;
    rf_damage_request request={round->damage,round->source,round->kind,0,UINT32_MAX,0};
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,combat_random,combat_notify,combat_playing,combat_play,&feedback};
    status=scene_driller_projectile_damage(contact,round->source,round->damage,round->kind,frame,&handled,&amount);if(status)return status;
    if(handled){if(amount>0)++rf_scene_apc_primary[6];return RF_OK;}
    if(tag!=UINT32_MAX&&(tag&SCENE_DETACHED_ROCKET_OWNER))return scene_detached_sources_damage(stream,(tag>>16)&0x7fff,tag&0xffff,round->damage);
    if(tag!=SCENE_AI_PROJECTILE_PLAYER&&(tag&0xffff0000u)!=SCENE_ACTOR_ROCKET_OWNER)return RF_OK;
    memcpy(&bits,&seconds,4);
    if(tag==SCENE_AI_PROJECTILE_PLAYER){
        if(campaign_player_object.handle==round->driver||campaign_player_object.handle==round->source)return RF_OK;
        target=campaign_player_object.handle;status=rf_scene_player_damage(target,&request,1,bits,&effects,&amount);
    }else{
        index=tag&0xffffu;if(index>=campaign_npc_body_count)return RF_RANGE;
        target=campaign_npc_bodies[index].registration.handle;
        if(target==round->driver||target==round->source)return RF_OK;
        status=scene_npc_shield_receive(index,start,end,contact->hit.fraction,request.amount,request.kind,&accepted,&broken);if(status)return status;
        if(accepted)return RF_OK;
        status=rf_scene_npc_damage(target,&request,1,bits,&effects,&amount);
        if(!status&&campaign_npc_bodies[index].damage.effects.health<=0){
            status=rf_scene_npc_death_entry(target,&entered);
            if(!status&&entered)status=combat_death_start(index);
        }
    }
    if(status)return status;
    if(feedback.status)return feedback.status;
    if(amount>0)++rf_scene_apc_primary[6];
    return RF_OK;
}
/* Same shared world/mover/rubble/body composition, with a second excluded
 * handle. The original service has only one exclusion; retaining BOTH host
 * and launch-time driver prevents self hits even after the driver exits. */
typedef struct scene_apc_primary_query {scene_stream *stream;uint32_t source,driver;} scene_apc_primary_query;
static int scene_apc_primary_sweep_all(void *context,const float *start,const float *delta,float radius,uint32_t flags,
    rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    scene_apc_primary_query *q=context;scene_stream *s=q->stream;rf_geometry_world_sweep_hit hit;
    rf_geomod_registry_hit piece;uint32_t i,found;int status;
    status=rf_geometry_collision_world_sweep_liquid(s->collision,flags,start,delta,radius,1,&hit,matched,liquid);if(status)return status;
    if(*matched){out->hit=hit.hit;out->room=hit.room;out->face=hit.face;out->object=UINT32_MAX;}
    for(i=0;i<campaign_movers.count;i++){
        const rf_collision_solid_view *solid=campaign_movers.views+i;rf_collision_sweep_room_hit local;rf_collision_ray_hit world;
        status=rf_collision_solid_preferred(solid,NULL,flags&~0x1004u,start,delta,radius,*matched?out->hit.fraction:1,&local,&found);if(status)return status;
        if(!found || (*matched && local.tree.hit.fraction>=out->hit.fraction))continue;
        status=rf_collision_contact_world(&local.tree.hit,solid->output_origin,solid->output_matrix,&world);if(status)return status;
        out->hit=world;out->room=UINT32_MAX;out->face=local.tree.face_index;out->object=SCENE_MOVER_ROCKET_OWNER|i;*matched=1;*liquid=0;
    }
    status=scene_detached_sources_sweep(s,flags,start,delta,radius,*matched?out->hit.fraction:1,&piece,&found);if(status)return status;
    if(found && (!*matched || piece.piece.hit.fraction<out->hit.fraction)){
        rf_collision_room_location room;status=rf_geometry_collision_world_locate(s->collision,piece.piece.hit.point,&room);if(status)return status;
        out->hit=piece.piece.hit;out->room=room.room;out->face=piece.piece.face;out->object=SCENE_DETACHED_ROCKET_OWNER|(piece.batch<<16)|piece.piece.piece;*matched=1;*liquid=0;
    }
    for(i=0;i<=campaign_npc_body_count;i++){
        const rf_physics_body *body;uint32_t tag,room;rf_collision_ray_hit body_hit;
        if(i==campaign_npc_body_count){
            if(!campaign_player_object.view || campaign_player_object.handle==q->source || campaign_player_object.handle==q->driver || campaign_player_damage.state.effects.health<=0)continue;
            body=&scene_actor_body;tag=SCENE_AI_PROJECTILE_PLAYER;room=rf_scene_player_swim[0];
        }else{
            const campaign_npc_body *npc=campaign_npc_bodies+i;
            if(!npc->registration.view || npc->registration.handle==q->source || npc->registration.handle==q->driver || !npc->body.allocated_bytes ||
               npc->damage.effects.health<=0 || (npc->object_flags&(2u|0x4000u)) || (npc->view.flags_810&1u))continue;
            body=&npc->body;tag=SCENE_ACTOR_ROCKET_OWNER|i;room=npc->room.room?npc->room.room-1:UINT32_MAX;
        }
        if(!scene_rocket_body_hit(body,start,delta,radius,*matched?out->hit.fraction:1,&body_hit) || (*matched && body_hit.fraction>=out->hit.fraction))continue;
        out->hit=body_hit;out->object=tag;out->room=room;out->face=UINT32_MAX;*matched=1;*liquid=0;
    }
    return scene_driller_projectile_compose(q->source,start,delta,radius,out,liquid,matched);
}
static int scene_apc_primary_sweep(void *context,const float *start,const float *delta,float radius,
    rf_weapon_flight_contact *contact,uint32_t *matched)
{
    uint32_t liquid=0;
    /* 0x100 requests indexed texture/alpha admission, not a small-radius
     * collision optimization. This scene sweep supplies no texture backend;
     * collision.c sweep_rooms_common rejects that request with NOT_FOUND.
     * Keep the authored .02 sphere radius with supported solid flag4. Alpha
     * cutout transparency remains the shared projectile service limitation. */
    return scene_apc_primary_sweep_all(context,start,delta,radius,4u,contact,&liquid,matched);
}
static uint32_t scene_apc_primary_pending(const scene_stream *stream)
{
    uint32_t i;for(i=0;i<SCENE_APC_PRIMARY_CAPACITY;i++)if(stream->apc_primary.rounds[i].flight.active)return 1;
    return 0;
}
static int scene_apc_primary_tick(scene_stream *stream,uint32_t frame)
{
    scene_apc_primary_state *state=&stream->apc_primary;scene_driller_runtime *runtime=stream->driller_runtime;
    uint32_t i,k,fired;float dt=frame?scene_step_seconds:0;int status;
    if(rf_scene_vehicle_enabled!=2||!runtime)return RF_OK;
    ++rf_scene_apc_primary[0];rf_scene_apc_primary[4]=0;
    for(i=0;i<SCENE_APC_PRIMARY_CAPACITY;i++){
        scene_apc_primary_round *round=state->rounds+i;rf_weapon_flight_event event;float end[3];scene_apc_primary_query query;
        if(!round->flight.active)continue;
        memcpy(round->previous,round->flight.position,12);
        for(k=0;k<3;k++)end[k]=round->flight.position[k]+round->flight.velocity[k]*fminf(dt,(float)round->flight.remaining);
        query.stream=stream;query.source=round->source;query.driver=round->driver;
        status=rf_weapon_flight_step(&round->flight,dt,scene_apc_primary_sweep,&query,&event);if(status)return status;
        if(event.kind==1){++rf_scene_apc_primary[2];status=scene_apc_primary_damage(stream,frame,round,&event.contact,round->previous,end);if(status)return status;}
        else if(event.kind==2)++rf_scene_apc_primary[3];
        if(round->flight.active)++rf_scene_apc_primary[4];
    }
    {
        scene_apc_primary_launch_context launch={stream,runtime->entry.host.driver};
        uint32_t allowed=runtime->entry.session.active&&runtime->entry.host.alive&&campaign_player_damage.state.effects.health>0;
        status=scene_vehicle_primary_tick(&state->scheduler,&state->definition,&stream->driller->tags,state->muzzle,
            runtime->physics.state.position,runtime->physics.state.orientation,NULL,runtime->entry.host.handle,
            player_input.fire,allowed,dt,&state->reserve,scene_apc_primary_launch,&launch,&fired);
        if(status)return status;
        rf_scene_apc_primary[4]+=fired;
    }
    rf_scene_apc_primary[7]=(uint32_t)state->reserve;return RF_OK;
}
#endif
