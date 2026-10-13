#ifndef RF_SCENE_PLAYER_ROCKET_LOCK_INCLUDED
#define RF_SCENE_PLAYER_ROCKET_LOCK_INCLUDED
/* Handheld Rocket Launcher alt_lock, deliberately separate from generic
 * Homing (authored false). Original4a63f9/4ad470 arms on alternate press;
 *4acec0..4ad0ff scans while held and fires on release, even without a lock.
 * Acquisition4ad110/4ad240: range100, forward30-degree half cone, highest
 * alignment and eye/body LOS.4ad0b5 starts2750ms lock, rescans every75ms.
 * Locked release4acfa5..4acfff binds the target and sets turn5/wakeup~1/3s.
 * First-playable policies:75ms scan is rounded to5 simulation ticks; existing
 * swept projectile occlusion includes other bodies/props/rubble; live static
 * vehicle/turret owners participate; primary cancels the hold; retired, dead
 * or hidden targets permanently leave their shot ballistic. No new damage,
 * ammunition, collision, effect or homing reacquisition. RFAP10 owns the
 * bounded continuation independently of these reconstructed gameplay rules. */
enum {SCENE_ROCKET_LOCK_TICKS=165,SCENE_ROCKET_LOCK_SCAN_TICKS=5};
/* active,target,remaining,locked,scans,guided launches,unlocked releases,
 * steering steps,lost flight targets,live guided,last launch target,status */
uint32_t rf_scene_rocket_lock[12];

static void scene_player_rocket_lock_cancel(scene_stream *s,uint32_t held)
{
    if(!s)return;
    memset(&s->rocket_lock,0,sizeof(s->rocket_lock));s->rocket_lock.held=!!held;
    memset(rf_scene_rocket_lock,0,4*sizeof(*rf_scene_rocket_lock));
}
static void scene_player_rocket_lock_reset(scene_stream *s,uint32_t held)
{
    if(!s)return;
    scene_player_rocket_lock_cancel(s,held);
    memset(s->rocket_targets,0,sizeof(s->rocket_targets));rf_scene_rocket_lock[9]=0;
}
static uint32_t scene_player_rocket_lock_save_pending(const scene_stream *s)
{
    uint32_t i;if(!s)return 0;
    if(s->rocket_lock.active)return 1;
    for(i=0;i<SCENE_ROCKETS;i++)if(s->rockets[i].active && s->rocket_targets[i])return 1;
    return 0;
}
static uint32_t scene_player_rocket_lock_owner(const scene_stream *s)
{
    uint32_t source=campaign_player_object.handle;int32_t descriptor;
    if(!s || !scene_rocket_resources || campaign_equipped_slot!=4 ||
       campaign_rocket_id<0 || campaign_rocket_id>=64 || campaign_explicit_unarmed ||
       campaign_player_form.active || campaign_cutscene_runtime.active ||
       scene_driller_active(s) || scene_turret_player_active() ||
       !campaign_player_inventory.owned[campaign_rocket_id] ||
       !source || source==UINT32_MAX || campaign_player_object.view!=&campaign_player_view ||
       campaign_player_damage.state.effects.handle!=source ||
       (uint32_t)campaign_player_view.handle!=source || campaign_player_view.type!=0 ||
       !scene_actor_body.allocated_bytes ||
       ((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) ||
       (campaign_player_view.flags_810&(1u|0x800u)) ||
       !isfinite(campaign_player_damage.state.effects.health) ||
       campaign_player_damage.state.effects.health<=0 ||
       rf_object_registry_lookup(&campaign_registry,source)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)source)!=&campaign_player_view)return 0;
    descriptor=rf_weapon_name_find(&campaign_weapon_reset.names,"Rocket Launcher");
    return descriptor>=0 && descriptor<64 && (campaign_weapon_reset.definitions[descriptor].flags_264&0x4000u);
}
/* Return copied geometry only. A stored full handle is always re-resolved;
 * an array index, old pointer or reused low handle bits cannot preserve lock. */
static uint32_t scene_player_rocket_target(scene_stream *s,uint32_t handle,float eye[3],float body[3])
{
    uint32_t i;void *registered;float health;
    if(!s || !handle || handle==UINT32_MAX || handle==campaign_player_object.handle ||
       !(registered=rf_object_registry_lookup(&campaign_registry,handle)))return 0;
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *npc=campaign_npc_bodies+i;
        if(npc->registration.handle!=handle || registered!=&npc->registration)continue;
        if(npc->registration.view!=&npc->view || !npc->body.allocated_bytes ||
           rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&npc->view ||
           ((npc->object_flags|npc->view.flags_7c)&(2u|0x4000u)) ||
           (npc->view.flags_810&1u) || !isfinite(npc->damage.effects.health) ||
           npc->damage.effects.health<=0)return 0;
        memcpy(eye,npc->eye_position,12);memcpy(body,npc->body.state.position,12);
        goto finite;
    }
    if(!scene_campaign_vehicle_target(s,handle,eye,&health) || !isfinite(health) || health<=0)return 0;
    memcpy(body,eye,12);
finite:
    for(i=0;i<3;i++)if(!isfinite(eye[i]) || !isfinite(body[i]))return 0;
    return 1;
}
/* Source4faf30 approximate distance, already reconstructed for Cane scans. */
static float scene_player_rocket_lock_distance(const float delta[3])
{
    float a=fabsf(delta[0]),b=fabsf(delta[1]),c=fabsf(delta[2]),t;
    if(a<b){t=a;a=b;b=t;}if(b<c){t=b;b=c;c=t;}if(a<b){t=a;a=b;b=t;}
    return a+b*.375f+c*.1875f;
}
static int scene_player_rocket_lock_sight(scene_stream *s,const float start[3],
    const float end[3],uint32_t target,uint32_t *clear)
{
    rf_weapon_flight_contact contact={0};float delta[3];uint32_t i,liquid=0,matched=0;int status;
    for(i=0;i<3;i++)delta[i]=end[i]-start[i];
    status=scene_rocket_sweep(s,start,delta,0,0x104u,&contact,&liquid,&matched);if(status)return status;
    *clear=!matched;
    if(matched && !liquid && (contact.object&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
        i=contact.object&0xffffu;
        if(i<campaign_npc_body_count && campaign_npc_bodies[i].registration.handle==target)*clear=1;
    }
    if(matched && !liquid &&
       (contact.object==SCENE_DRILLER_PROJECTILE_OWNER || contact.object==SCENE_TURRET_PROJECTILE_OWNER) &&
       contact.face==target)*clear=1;
    return RF_OK;
}
static int scene_player_rocket_lock_candidate(scene_stream *s,uint32_t target,
    const float position[3],const float forward[3],float *alignment,uint32_t *accepted)
{
    float eye[3],body[3],delta[3];double length=0,forward_length=0,dot=0;
    uint32_t i,clear=0;int status;*accepted=0;
    if(!scene_player_rocket_target(s,target,eye,body))return RF_OK;
    for(i=0;i<3;i++){
        if(!isfinite(position[i]) || !isfinite(forward[i]))return RF_RANGE;
        delta[i]=eye[i]-position[i];length+=(double)delta[i]*delta[i];
        forward_length+=(double)forward[i]*forward[i];
    }
    if(!isfinite(length) || !isfinite(forward_length) || forward_length<=0)return RF_RANGE;
    if(length<=0 || scene_player_rocket_lock_distance(delta)>=100.f)return RF_OK;
    length=sqrt(length);forward_length=sqrt(forward_length);
    for(i=0;i<3;i++)dot+=(delta[i]/length)*(forward[i]/forward_length);
    /* Original double5898c8 is approximately pi/6; normalize the supplied
     * basis defensively instead of treating a malformed direction as a lock. */
    if(dot<0.8660253964992068)return RF_OK;
    status=scene_player_rocket_lock_sight(s,position,eye,target,&clear);if(status)return status;
    if(!clear){status=scene_player_rocket_lock_sight(s,position,body,target,&clear);if(status)return status;}
    if(clear){*alignment=(float)dot;*accepted=1;}return RF_OK;
}
static int scene_player_rocket_lock_scan(scene_stream *s,const float position[3],
    const float forward[3],uint32_t *target)
{
    uint32_t i,total=campaign_npc_body_count+campaign_passive_vehicle_count+scene_turret_count,best=0;
    float best_alignment=-1;int status;
    ++rf_scene_rocket_lock[4];
    for(i=0;i<total;i++){
        uint32_t handle,accepted;float alignment;
        if(i<campaign_npc_body_count)handle=campaign_npc_bodies[i].registration.handle;
        else if(i<campaign_npc_body_count+campaign_passive_vehicle_count)
            handle=campaign_passive_vehicles[i-campaign_npc_body_count].handle;
        else handle=scene_turrets[i-campaign_npc_body_count-campaign_passive_vehicle_count].registration.handle;
        status=scene_player_rocket_lock_candidate(s,handle,position,forward,&alignment,&accepted);if(status)return status;
        if(accepted && alignment>=best_alignment){best=handle;best_alignment=alignment;}
    }
    *target=best;return RF_OK;
}
/* Call once after incoming combat/damage, before weapon-specific early-outs.
 * A release is a frame-local request to the existing primary trigger. It may
 * still be declined by its real cooldown, reload or empty-ammo gate. */
static int scene_player_rocket_lock_input(scene_stream *s,uint32_t frame,
    const float position[3],const float forward[3],uint32_t *release,uint32_t *target)
{
    scene_player_rocket_lock_state *lock;uint32_t held=!!player_input.alt_fire,edge;int status=RF_OK;
    if(!s || !position || !forward || !release || !target)return RF_RANGE;
    lock=&s->rocket_lock;edge=held&&!lock->held;*release=*target=0;
    if(!scene_player_rocket_lock_owner(s) || player_input.fire || rf_scene_combat[6] ||
       rf_scene_player_swim[2] || (player_input.reload && rf_scene_combat[5]<campaign_pistol.magazine && rf_scene_player_ammo[1]) ||
       (lock->active && lock->source!=campaign_player_object.handle)){
        scene_player_rocket_lock_cancel(s,held);return RF_OK;
    }
    /* A chord or hotplug emits neutral gameplay input without a physical
     * release. Retain the hold and freeze its remaining acquisition; after a
     * load or suppression, only a fresh effective held sample can rearm it. */
#ifdef RF_IMAGE_XBOX_NATIVE
    if(player_poll && rf_xbox_input_alt_sample_suppressed()){
        if(lock->active && !lock->resume_pending){lock->resume_pending=1;lock->resume_frame=frame;}
        return RF_OK;
    }
#endif
    if(lock->resume_pending){
        if(!held)return RF_OK;
        lock->scan_due+=frame-lock->resume_frame;
        if(lock->target&&!lock->locked)lock->lock_due+=frame-lock->resume_frame;
        lock->resume_pending=lock->resume_frame=0;edge=0;
    }
    lock->held=held;
    if(edge){lock->active=1;lock->source=campaign_player_object.handle;
        lock->target=lock->locked=0;lock->scan_due=frame;}
    if(!lock->active)return RF_OK;
    if(!held){
        if(lock->locked && lock->target){uint32_t accepted;float alignment;
            status=scene_player_rocket_lock_candidate(s,lock->target,position,forward,&alignment,&accepted);
            if(status)return status;if(accepted)*target=lock->target;}
        *release=1;if(!*target)++rf_scene_rocket_lock[6];
        scene_player_rocket_lock_cancel(s,held);return RF_OK;
    }
    if((int32_t)(frame-lock->scan_due)>=0){
        uint32_t selected;
        status=scene_player_rocket_lock_scan(s,position,forward,&selected);if(status)return status;
        lock->scan_due=frame+SCENE_ROCKET_LOCK_SCAN_TICKS;
        if(selected!=lock->target){lock->target=selected;lock->locked=0;
            lock->lock_due=selected?frame+SCENE_ROCKET_LOCK_TICKS:0;}
    }
    if(lock->target && (int32_t)(frame-lock->lock_due)>=0)lock->locked=1;
    rf_scene_rocket_lock[0]=lock->active;rf_scene_rocket_lock[1]=lock->target;
    rf_scene_rocket_lock[2]=lock->target&&!lock->locked?lock->lock_due-frame:0;
    rf_scene_rocket_lock[3]=lock->locked;return RF_OK;
}
/* Called only after a successful flight publication. Zero target is the
 * unchanged ordinary primary/unlocked alternate shot. No ammo mutation. */
static void scene_player_rocket_lock_launch(scene_stream *s,uint32_t slot,uint32_t target)
{
    float eye[3],body[3];s->rocket_targets[slot]=0;
    if(target && scene_player_rocket_target(s,target,eye,body)){
        s->rocket_targets[slot]=target;++rf_scene_rocket_lock[5];
        rf_scene_rocket_lock[10]=target;
    }
}
static int scene_player_rocket_lock_guide(scene_stream *s,uint32_t slot)
{
    rf_weapon_flight *flight=s->rockets+slot;float eye[3],body[3],delta[3],velocity[3],speed,scale;
    double length=0;uint32_t i,target=s->rocket_targets[slot];int status;
    if(!target || !flight->active)return RF_OK;
    if(!scene_player_rocket_target(s,target,eye,body)){
        s->rocket_targets[slot]=0;++rf_scene_rocket_lock[8];return RF_OK;
    }
    /* Original locked release writes3eaaaa3b at descriptor13c. */
    if(campaign_rocket.lifetime-flight->remaining<0.3333300054073334)return RF_OK;
    for(i=0;i<3;i++){delta[i]=body[i]-flight->position[i];length+=(double)delta[i]*delta[i];}
    if(!isfinite(length))return RF_RANGE;if(length<=0)return RF_OK;length=sqrt(length);
    speed=(float)sqrt((double)flight->velocity[0]*flight->velocity[0]+
        (double)flight->velocity[1]*flight->velocity[1]+(double)flight->velocity[2]*flight->velocity[2]);
    if(!isfinite(speed) || speed<=0)return RF_RANGE;
    /* Original4c6fca..4c7064 steering, also used by existing Cane owner:
     * normalized velocity-plus-target acceleration, preserving actual speed.
     * Target prediction, thrust acceleration and near-miss destruction defer. */
    scale=speed*(1.f/60)*30.00029945373535f/5.f;
    for(i=0;i<3;i++)velocity[i]=flight->velocity[i]+(float)(delta[i]/length)*scale;
    length=sqrt((double)velocity[0]*velocity[0]+(double)velocity[1]*velocity[1]+(double)velocity[2]*velocity[2]);
    if(!isfinite(length) || length<=0)return RF_RANGE;
    for(i=0;i<3;i++)velocity[i]=(float)(velocity[i]/length)*speed;
    status=rf_entity_navigation_basis(velocity,(float (*)[3])s->rocket_basis[slot]);if(status)return status;
    memcpy(flight->velocity,velocity,12);++rf_scene_rocket_lock[7];return RF_OK;
}
#endif
