#ifndef RF_SCENE_AI_CANE_FLIGHT_INCLUDED
#define RF_SCENE_AI_CANE_FLIGHT_INCLUDED
/* Include after scene_ai_laser.inc. Original Cane homing is independent of
 * AI target/faction. This bounded adapter admits skeletal NPCs and the actual
 * on-foot player eye; separately retained vehicle acquisition is deferred.
 * It does not reuse the approximate vehicle-homing algorithm or its filters. */
typedef struct scene_ai_cane_candidate {
    uint32_t handle,slot;float eye[3],body[3];
} scene_ai_cane_candidate;
/* scans,locks,steered,retained missing,last target,last slot,last frame,status */
uint32_t rf_scene_cane_homing[8];
static uint32_t scene_ai_cane_candidate_read(uint32_t index,uint32_t frame,uint32_t source,
    scene_ai_cane_candidate *out)
{
    scene_ai_cane_candidate c={0};const rf_physics_body *body;uint32_t flags;
    if(index<campaign_npc_body_count){
        campaign_npc_body *npc=campaign_npc_bodies+index;uint32_t cls;
        c.handle=npc->registration.handle;c.slot=index;
        if(c.handle==source || !scene_ai_melee_npc_live(index,c.handle) ||
           !campaign_seeds.items || index>=campaign_seeds.records.count)return 0;
        cls=campaign_seeds.items[index].class_index;if(cls>=campaign_seeds.class_count)return 0;
        flags=campaign_seeds.classes[cls].physics.flags;body=&npc->body;
        memcpy(c.eye,npc->eye_position,12);
    }else{
        if(index!=campaign_npc_body_count)return 0;
        c.handle=campaign_player_object.handle;c.slot=UINT32_MAX;
        if(c.handle==source || !scene_ai_melee_player_identity(c.handle) ||
           !scene_ai_actor_penetration_player_live(c.handle) ||
           !scene_ai_cane_player_eye.valid || scene_ai_cane_player_eye.frame!=frame ||
           scene_ai_cane_player_eye.handle!=c.handle || scene_ai_cane_player_eye.epoch!=scene_ai_cane_epoch)return 0;
        flags=campaign_player_damage.class_flags;body=&scene_actor_body;
        memcpy(c.eye,scene_ai_cane_player_eye.eye,12);
    }
    /*4c6dc8..4c6e0e: actual body radius180 and ambient class724, no faction.*/
    if((flags&0x800000u) || !isfinite(body->state.bounds.radius) || body->state.bounds.radius<.5f)return 0;
    memcpy(c.body,body->state.position,12);
    for(uint32_t k=0;k<3;k++)if(!isfinite(c.eye[k]) || !isfinite(c.body[k]))return 0;
    *out=c;return 1;
}
/*4faf30->4fa7a0; scanner4c6e64 stores the approximate result as binary32
 * before strict range/nearest comparisons. Preserve stable enumeration ties.*/
static float scene_ai_cane_distance(const float delta[3])
{
    float a=fabsf(delta[0]),b=fabsf(delta[1]),c=fabsf(delta[2]),t;long double partial;
    if(a<b){t=a;a=b;b=t;}if(b<c){t=b;b=c;c=t;}if(a<b){t=a;a=b;b=t;}
    partial=(long double)c*.125f+(long double)b*.25f;
    return (float)((partial*.5f+partial)+a);
}
static int scene_ai_cane_acquire(scene_stream *stream,uint32_t frame,
    const scene_ai_laser_projectile *shot,scene_ai_cane_homing *lock)
{
    uint32_t i,k,blocked;float best=scene_ai_cane_definition.scan_range;
    float cosine=cosf(scene_ai_cane_definition.view_cone*.01745329238474369f*.5f);int status;
    ++rf_scene_cane_homing[0];
    for(i=0;i<=campaign_npc_body_count;i++){
        scene_ai_cane_candidate c;float delta[3],distance,dot=0;double length;
        if(!scene_ai_cane_candidate_read(i,frame,shot->source,&c))continue;
        for(k=0;k<3;k++)delta[k]=c.eye[k]-shot->flight.position[k];
        distance=scene_ai_cane_distance(delta);
        if(!isfinite(distance) || distance>=best)continue;
        length=sqrt((double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2]);
        if(!isfinite(length) || length<=0)continue;
        for(k=0;k<3;k++)dot+=(float)(delta[k]/length)*shot->basis[6+k];
        if(!isfinite(dot) || dot<cosine)continue;
        /*4c6e83: ordinary visibility to eye, then body only when blocked.
         * No projectile no-world flag is passed to this independent query. */
        status=rf_geometry_collision_ray(stream->collision,&campaign_movers,
            shot->flight.position,c.eye,3,NULL,&blocked);if(status)return status;
        if(blocked){status=rf_geometry_collision_ray(stream->collision,&campaign_movers,
            shot->flight.position,c.body,3,NULL,&blocked);if(status)return status;}
        if(blocked)continue;
        best=distance;lock->target=c.handle;lock->target_slot=c.slot;
    }
    if(lock->target!=UINT32_MAX){
        ++rf_scene_cane_homing[1];rf_scene_cane_homing[4]=lock->target;
        rf_scene_cane_homing[5]=lock->target_slot;
    }
    return RF_OK;
}
/*4c6f60 only resolves the stored full handle. A missing owner remains locked
 * and ballistic; a hidden/dead but retained owner still supplies body position.
 * Never substitute an index match or reacquire after retirement/reuse. */
static const float *scene_ai_cane_target_body(const scene_ai_cane_homing *lock)
{
    if(lock->target_slot==UINT32_MAX)
        return scene_ai_melee_player_identity(lock->target)?scene_actor_body.state.position:NULL;
    {campaign_npc_body *npc=scene_ai_melee_npc_identity(lock->target_slot,lock->target);
     return npc?npc->body.state.position:NULL;}
}
static int scene_ai_cane_guide(scene_stream *stream,uint32_t frame,float dt,
    scene_ai_laser_projectile *shot,scene_ai_cane_homing *lock)
{
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);int due,status;
    const float *body;float delta[3],velocity[3],scale,speed;
    double length;uint32_t k;
    /* Authored Wakeup Time0: the first flight tick can immediately scan. */
    if(lock->target==UINT32_MAX){
        status=rf_timer_expired(lock->scan_due,now,&due);if(status)return status;
        if(due){
            status=scene_ai_cane_acquire(stream,frame,shot,lock);if(status)return status;
            status=rf_timer_set(&lock->scan_due,now,200);if(status)return status;
        }
        /*4c6a2b is the OTHER branch: a newly acquired target is first steered
         * on the following update, not in this acquisition pass. */
        return RF_OK;
    }
    body=scene_ai_cane_target_body(lock);
    if(!body){++rf_scene_cane_homing[3];return RF_OK;}
    for(k=0;k<3;k++){if(!isfinite(body[k]))return RF_RANGE;delta[k]=body[k]-shot->flight.position[k];}
    length=sqrt((double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2]);
    if(!isfinite(length))return RF_RANGE;if(length<=0)return RF_OK;
    /*4c6fca..4c7064: vector addition followed by speed renormalization,
     * NOT an angular cap, interpolation, predictive lead or vehicle policy. */
    speed=(float)sqrt((double)shot->flight.velocity[0]*shot->flight.velocity[0]+
        (double)shot->flight.velocity[1]*shot->flight.velocity[1]+(double)shot->flight.velocity[2]*shot->flight.velocity[2]);
    if(!isfinite(speed) || speed<=0)return RF_RANGE;
    scale=speed*dt*30.00029945373535f/scene_ai_cane_definition.turn_time;
    for(k=0;k<3;k++)velocity[k]=shot->flight.velocity[k]+(float)(delta[k]/length)*scale;
    length=sqrt((double)velocity[0]*velocity[0]+(double)velocity[1]*velocity[1]+(double)velocity[2]*velocity[2]);
    if(!isfinite(length) || length<=0)return RF_RANGE;
    for(k=0;k<3;k++)shot->flight.velocity[k]=(float)(velocity[k]/length)*speed;
    ++rf_scene_cane_homing[2];return RF_OK;
}
/* Original4c4f9b presents physical shields before durability; ordinary
 *4c6463 audio follows direct damage. Nano4c5a90..4c5b5d consumes silently.
 * Snapshot retained identity/material once, then call the existing direct
 * consumer once. No second collision query, Nano prepass or radial action.
 *4c98b0 excludes ENERGY5, so even shield presentation always uses dry groups. */
static int scene_ai_cane_contact(scene_stream *stream,uint32_t frame,
    const scene_ai_laser_projectile *shot,const scene_ai_laser_query *query,
    const rf_weapon_flight_contact *contact)
{
    uint64_t epoch=scene_ai_cane_epoch;uint32_t consumed=0,audible=0,shield;
    int32_t material=-1;int status;
    if(scene_weapon_impact_audio_cane_ready(shot->weapon))
        audible=scene_ai_contact_material(stream,shot,query,contact,&material);
    shield=query->kind==SCENE_LASER_NPC_SHIELD || query->kind==SCENE_LASER_PLAYER_SHIELD;
    if(audible && shield)scene_weapon_impact_audio_contact(shot->weapon,material,contact->hit.point);
    if(epoch!=scene_ai_cane_epoch)return RF_OK;
    status=scene_ai_laser_contact_result(stream,frame,shot,query,contact,
        rf_scene_ai_cane,&scene_ai_cane_epoch,&consumed);
    if(status || consumed || epoch!=scene_ai_cane_epoch)return status;
    /* Keep the captured material for lethal/retiring contacts. Optional audio
     * cannot veto gameplay, and source death/switch never rebinds this shot. */
    if(audible && !shield)scene_weapon_impact_audio_contact(shot->weapon,material,contact->hit.point);
    return RF_OK;
}
static int scene_ai_cane_tick(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    uint32_t i,live=0;uint64_t epoch=scene_ai_cane_epoch;int status=RF_OK;
    if(!stream || !player_eye)return RF_RANGE;if(scene_ai_cane_updating || scene_ai_cane_servicing || scene_ai_cane_presenting)return RF_OK;
    scene_ai_cane_updating=1;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++){
        scene_ai_laser_projectile shot=scene_ai_canes[i];scene_ai_cane_homing lock=scene_ai_cane_locks[i];
        uint64_t ticket=scene_ai_cane_flight_tickets[i];
        rf_weapon_flight_liquid_event move;rf_weapon_flight_liquid_policy policy={0,0,-1,-1};
        scene_ai_laser_query q={0};
        if(!shot.flight.active){
            scene_cane_flight_audio_stop(i,shot.source,ticket);
            if(epoch!=scene_ai_cane_epoch)break;
            continue;
        }
        status=scene_ai_cane_guide(stream,frame,1.f/60,&shot,&lock);if(status)break;
        if(epoch!=scene_ai_cane_epoch)break;
        /*4c7080 uses4fcea0 to rebuild the moving projectile basis. */
        status=rf_entity_navigation_basis(shot.flight.velocity,(float (*)[3])shot.basis);if(status)break;
        q.stream=stream;q.source=shot.source;q.player_eye=player_eye;q.no_world_collide=1;
        if(scene_ai_cane_player_eye.valid && scene_ai_cane_player_eye.frame==frame &&
           scene_ai_cane_player_eye.epoch==epoch &&
           scene_ai_cane_player_eye.handle==campaign_player_object.handle &&
           scene_ai_melee_player_identity(campaign_player_object.handle))q.player_eye=scene_ai_cane_player_eye.eye;
        /* physics10 clear skips49bb70's world/movers/liquid. Independent
         * physics20 object pairs retain actors/shields, detached solids,
         * clutter and vehicles; only static geometry and type9 movers skip.
         * ENERGY5 bypasses the generic near-target miss self-destruction. */
        status=rf_weapon_flight_step_liquid(&shot.flight,1.f/60,&shot.liquid,&policy,
            scene_ai_laser_sweep,&q,&move);if(status)break;
        if(epoch!=scene_ai_cane_epoch)break;
        scene_ai_canes[i]=shot;scene_ai_cane_locks[i]=lock; /* Publish terminal before callbacks. */
        scene_cane_flight_audio_step(i,shot.source,ticket);
        if(epoch!=scene_ai_cane_epoch)break;
        if(move.terminal.kind==2){
            ++rf_scene_ai_cane[2];
            /* Homing0x800000 passes4c6bb5; radius0 skips radial4c6c55,
             * but4c6cd5 selects Default0 and4c6cf9 requests presentation.
             * Expiry owns the published final center, not an invented hit. */
            if(scene_weapon_impact_audio_cane_ready(shot.weapon))
                scene_weapon_impact_audio_contact(shot.weapon,0,shot.flight.position);
            if(epoch!=scene_ai_cane_epoch)break;
        }
        if(move.terminal.kind==1){
            ++rf_scene_ai_cane[1];
            status=scene_ai_cane_contact(stream,frame,&shot,&q,&move.terminal.contact);
            if(status || epoch!=scene_ai_cane_epoch)break;
        }
    }
    if(status && epoch==scene_ai_cane_epoch)scene_cane_flight_audio_forget();
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++)live+=scene_ai_canes[i].flight.active!=0;
    rf_scene_ai_cane[3]=live;rf_scene_ai_cane[11]=(uint32_t)status;
    rf_scene_cane_homing[6]=frame;rf_scene_cane_homing[7]=(uint32_t)status;
    scene_ai_cane_updating=0;return status;
}
#endif
