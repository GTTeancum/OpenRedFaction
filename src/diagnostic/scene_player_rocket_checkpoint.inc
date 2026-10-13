#ifndef RF_SCENE_PLAYER_ROCKET_CHECKPOINT_INCLUDED
#define RF_SCENE_PLAYER_ROCKET_CHECKPOINT_INCLUDED
/* RFAP10 kind23: local-player input/cooldown plus typed acquisition target.
 * Kind24: guidance bound to one already-paid kind3 flight slot. No debit,
 * launch, source pointer or generation handle is transported. */
static uint32_t scene_player_rocket_checkpoint_pending(const scene_stream *s)
{
    if(!s)return 0;
    return scene_player_rocket_lock_save_pending(s) ||
        (campaign_equipped_slot==4 && (s->rocket_lock.held||combat_trigger.cooldown||combat_trigger.held));
}
/* Identity resolution deliberately ignores current health/visibility on load:
 * the complete saved candidate, not its constructor, owns those properties. */
static int scene_player_rocket_checkpoint_resolve(scene_player_rocket_checkpoint_target *target)
{
    uint32_t i,found=0,handle=0;
    if(!target||target->kind<1||target->kind>3||!target->uid||target->uid>(uint32_t)INT32_MAX)return RF_FORMAT;
    if(target->kind==1){
        if(!campaign_seeds.records.items||campaign_seeds.records.count<campaign_npc_body_count)return RF_FORMAT;
        for(i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==target->uid){
            const campaign_npc_body *o=campaign_npc_bodies+i;
            if(found++||o->registration.view!=&o->view||!o->body.allocated_bytes||
               rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration||
               rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)!=&o->view)return RF_FORMAT;
            handle=o->registration.handle;
        }
    }else if(target->kind==2){
        for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].uid==target->uid){
            const scene_passive_vehicle *o=campaign_passive_vehicles+i;
            /* A promoted/switched host no longer has this passive registry owner. */
            if(found++||rf_object_registry_lookup(&campaign_registry,o->handle)!=o)return RF_FORMAT;
            handle=o->handle;
        }
    }else{
        if(scene_turret_authored_count>scene_turret_count)return RF_FORMAT;
        for(i=0;i<scene_turret_authored_count;i++)if(scene_turrets[i].uid==target->uid){
            const scene_turret_owner *o=scene_turrets+i;
            if(found++||scene_turret_lookup(o->registration.handle)!=o)return RF_FORMAT;
            handle=o->registration.handle;
        }
    }
    if(found!=1||!handle||handle==UINT32_MAX||handle==campaign_player_object.handle)return RF_NOT_FOUND;
    target->handle=handle;return RF_OK;
}
static int scene_player_rocket_checkpoint_target_live(const scene_stream *s,uint32_t handle,
    scene_player_rocket_checkpoint_target *target)
{
    uint32_t i,found=0;float eye[3],body[3];int status;
    if(!s||!target||!scene_player_rocket_target((scene_stream *)s,handle,eye,body))return RF_NOT_FOUND;
    memset(target,0,sizeof(*target));
    if(!campaign_seeds.records.items||campaign_seeds.records.count<campaign_npc_body_count)return RF_FORMAT;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==handle){
        uint32_t uid;status=scene_ai_fusion_checkpoint_source(handle,&uid);if(status)return status;
        if(found++)return RF_FORMAT;target->kind=1;target->uid=uid;
    }
    for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==handle){
        if(found++)return RF_FORMAT;target->kind=2;target->uid=campaign_passive_vehicles[i].uid;
    }
    for(i=0;i<scene_turret_authored_count;i++)if(scene_turrets[i].registration.handle==handle){
        if(found++)return RF_FORMAT;target->kind=3;target->uid=scene_turrets[i].uid;
    }
    if(found!=1)return RF_NOT_FOUND; /* Generated heads/active hosts need another identity lane. */
    status=scene_player_rocket_checkpoint_resolve(target);if(status)return status;
    return target->handle==handle?RF_OK:RF_FORMAT;
}
static int scene_player_rocket_checkpoint_flight(const scene_stream *s,const rf_weapon_flight *f,
    const rf_weapon_flight_liquid_state *liquid,const float basis[9])
{
    uint32_t i,j,k,flags=4u|(campaign_rocket.collision_radius<.05f?0x100u:0);
    double speed2=0;
    if(!s||!scene_rocket_resources||f->active!=1||f->radius!=campaign_rocket.collision_radius||
       !isfinite(campaign_rocket.speed)||campaign_rocket.speed<=0||
       !isfinite(f->remaining)||f->remaining<=0||f->remaining>campaign_rocket.lifetime||
       (liquid->query_flags!=flags&&liquid->query_flags!=(flags|0x1000u)))return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(f->position[i])||fabsf(f->position[i])>100000||!isfinite(f->velocity[i]))return RF_FORMAT;
        speed2+=(double)f->velocity[i]*f->velocity[i];
        for(j=0;j<3;j++)if(!isfinite(basis[i*3+j]))return RF_FORMAT;
    }
    if(fabs(sqrt(speed2)/campaign_rocket.speed-1)>.001)return RF_FORMAT;
    for(i=0;i<3;i++){
        if(fabsf(f->velocity[i]/campaign_rocket.speed-basis[6+i])>.001f)return RF_FORMAT;
        for(j=i;j<3;j++){
            double dot=0;for(k=0;k<3;k++)dot+=(double)basis[i*3+k]*basis[j*3+k];
            if(fabs(dot-(i==j?1.0:0.0))>.001)return RF_FORMAT;
        }
        j=(i+1)%3;k=(i+2)%3;
        if(fabsf(basis[j]*basis[3+k]-basis[k]*basis[3+j]-basis[6+i])>.001f)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_player_rocket_checkpoint_capture(const scene_stream *s,unsigned char *rows,uint32_t *count)
{
    const scene_player_rocket_lock_state *lock;uint32_t n=0,controller,now,scan=0,remaining=0;
    scene_player_rocket_checkpoint_target target={0};int status;
    if(!s||!count)return RF_RANGE;
    if(scene_player_rocket_checkpoint_service_depth)return RF_NOT_FOUND;
    lock=&s->rocket_lock;
    controller=lock->active||(campaign_equipped_slot==4&&(lock->held||combat_trigger.cooldown||combat_trigger.held));
    if(lock->active>1||lock->held>1||lock->locked>1||lock->resume_pending>1||
       (!lock->active&&(lock->source||lock->target||lock->locked||lock->scan_due||lock->lock_due||lock->resume_pending||lock->resume_frame)))return RF_FORMAT;
    if(controller){
        if(combat_frame==UINT32_MAX||campaign_equipped_slot!=4||campaign_explicit_unarmed||campaign_rocket_id<0||campaign_rocket_id>=64||
           !campaign_player_inventory.owned[campaign_rocket_id]||!s->player_weapon[4]||
           combat_trigger.cooldown>1200||combat_trigger.remaining||combat_trigger.delay||combat_trigger.held>1)return RF_NOT_FOUND;
        if(lock->active){
            if(!scene_player_rocket_lock_owner(s)||scene_player_weapon_holstered()||!lock->held||
               lock->source!=campaign_player_object.handle||combat_trigger.held||rf_scene_combat[6]||rf_scene_player_swim[2])return RF_NOT_FOUND;
            now=lock->resume_pending?lock->resume_frame:combat_frame;
            scan=(int32_t)(lock->scan_due-now)>0?lock->scan_due-now:0;
            if(lock->target){
                status=scene_player_rocket_checkpoint_target_live(s,lock->target,&target);if(status)return status;
                if(!lock->locked)remaining=(int32_t)(lock->lock_due-now)>0?lock->lock_due-now:0;
            }else if(lock->locked||lock->lock_due)return RF_FORMAT;
            if(scan>SCENE_ROCKET_LOCK_SCAN_TICKS||remaining>SCENE_ROCKET_LOCK_TICKS)return RF_FORMAT;
        }
        if(rows){unsigned char *r=rows+112*n;
            checkpoint_put(r,23);checkpoint_put(r+12,target.kind);checkpoint_put(r+16,target.uid);
            checkpoint_put(r+20,lock->active);checkpoint_put(r+24,lock->held);checkpoint_put(r+28,lock->locked);
            checkpoint_put(r+32,scan);checkpoint_put(r+36,remaining);
            checkpoint_put(r+40,combat_trigger.cooldown);checkpoint_put(r+44,combat_trigger.remaining);
            checkpoint_put(r+48,combat_trigger.delay);checkpoint_put(r+52,combat_trigger.held);
        }
        ++n;
    }
    for(uint32_t i=0;i<SCENE_ROCKETS;i++)if(s->rocket_targets[i]){
        status=scene_player_rocket_checkpoint_flight(s,s->rockets+i,s->rocket_liquid+i,s->rocket_basis[i]);if(status)return status;
        status=scene_player_rocket_checkpoint_target_live(s,s->rocket_targets[i],&target);if(status)return status;
        if(rows){unsigned char *r=rows+112*n;
            checkpoint_put(r,24);checkpoint_put(r+4,i);checkpoint_put(r+8,target.uid);checkpoint_put(r+12,target.kind);
        }
        ++n;
    }
    *count=n;return RF_OK;
}
static int scene_player_rocket_checkpoint_read(scene_ai_projectile_checkpoint_stage *stage,const unsigned char *r)
{
    uint32_t kind=checkpoint_u32(r),slot=checkpoint_u32(r+4),k;
    if(kind==24){
        scene_player_rocket_checkpoint_target *target;
        if(slot>=SCENE_ROCKETS||stage->player_targets[slot].kind)return RF_FORMAT;
        for(k=16;k<112;k++)if(r[k])return RF_FORMAT;
        target=stage->player_targets+slot;target->kind=checkpoint_u32(r+12);target->uid=checkpoint_u32(r+8);
        return scene_player_rocket_checkpoint_resolve(target);
    }
    if(kind==23){
        scene_player_rocket_lock_state *lock=&stage->player_lock;rf_weapon_trigger_state trigger;
        scene_player_rocket_checkpoint_target *target=&stage->player_lock_target;
        if(slot||checkpoint_u32(r+8)||stage->player_lock_present)return RF_FORMAT;
        for(k=56;k<112;k++)if(r[k])return RF_FORMAT;
        target->kind=checkpoint_u32(r+12);target->uid=checkpoint_u32(r+16);
        lock->active=checkpoint_u32(r+20);lock->held=checkpoint_u32(r+24);lock->locked=checkpoint_u32(r+28);
        lock->scan_due=checkpoint_u32(r+32);lock->lock_due=checkpoint_u32(r+36);
        trigger.cooldown=checkpoint_u32(r+40);trigger.remaining=checkpoint_u32(r+44);
        trigger.delay=checkpoint_u32(r+48);trigger.held=checkpoint_u32(r+52);
        if(lock->active>1||lock->held>1||lock->locked>1||lock->scan_due>SCENE_ROCKET_LOCK_SCAN_TICKS||
           lock->lock_due>SCENE_ROCKET_LOCK_TICKS||trigger.cooldown>1200||trigger.remaining||trigger.delay||trigger.held>1||
           (stage->player_trigger_present&&memcmp(&trigger,&stage->player_trigger,sizeof(trigger))))return RF_FORMAT;
        if(lock->active){
            if(!lock->held||trigger.held||!campaign_player_object.handle||campaign_player_object.handle==UINT32_MAX)return RF_FORMAT;
            lock->source=campaign_player_object.handle;
            if(target->kind){int status=scene_player_rocket_checkpoint_resolve(target);if(status)return status;lock->target=target->handle;}
            else if(target->uid||lock->locked||lock->lock_due)return RF_FORMAT;
            if(lock->locked&&lock->lock_due)return RF_FORMAT;
        }else if(target->kind||target->uid||lock->locked||lock->scan_due||lock->lock_due||
            (!lock->held&&!trigger.cooldown&&!trigger.held))return RF_FORMAT;
        stage->player_lock_present=1;stage->player_trigger_present=1;stage->player_trigger=trigger;return RF_OK;
    }
    return RF_FORMAT;
}
static int scene_player_rocket_checkpoint_complete(const scene_stream *s,const scene_ai_projectile_checkpoint_stage *stage)
{
    for(uint32_t i=0;i<SCENE_ROCKETS;i++)if(stage->player_targets[i].kind){
        int status=scene_player_rocket_checkpoint_flight(s,stage->player_rockets+i,stage->player_liquid+i,stage->player_basis[i]);
        if(status)return status; /* Every binding must own its exact paid flight slot. */
    }
    return RF_OK;
}
static int scene_player_rocket_checkpoint_target_admit(const scene_player_rocket_checkpoint_target *target,
    const scene_npc_checkpoint_restore_stage *npcs,const scene_passive_vehicle_checkpoint_stage *passive,
    const scene_turret_checkpoint_stage *turrets)
{
    uint32_t found=0;scene_player_rocket_checkpoint_target resolved;
    if(!target->kind)return target->uid||target->handle?RF_FORMAT:RF_OK;
    resolved=*target;if(scene_player_rocket_checkpoint_resolve(&resolved)||resolved.handle!=target->handle)return RF_FORMAT;
    if(target->kind==1){
        if(!npcs)return RF_FORMAT;
        for(uint32_t i=0;i<npcs->count;i++)if(npcs->entries[i].saved.uid==target->uid){
            const scene_npc_checkpoint_restore_entry *e=npcs->entries+i;
            if(found++||!e->registered||e->handle!=target->handle||e->saved.retired||e->saved.dead_pose||
               !isfinite(e->saved.health)||e->saved.health<=0||(e->saved.flags&(2u|0x4000u))||
               e->slot>=campaign_npc_body_count||(uint32_t)campaign_seeds.records.items[e->slot].record.uid!=target->uid||
               (campaign_npc_bodies[e->slot].view.flags_810&1u))return RF_FORMAT;
        }
    }else if(target->kind==2){
        if(!passive||passive->version!=2||passive->count!=campaign_passive_vehicle_count)return RF_FORMAT;
        for(uint32_t i=0;i<passive->count;i++)if(passive->rows[i].uid==target->uid){
            const scene_passive_vehicle_saved *saved=passive->rows+i;
            if(found++||!saved->loaded||!saved->has_damage||saved->destroyed||!isfinite(saved->health)||saved->health<=0||
               (saved->object_flags&(2u|0x4000u))||campaign_passive_vehicles[i].uid!=target->uid||
               campaign_passive_vehicles[i].handle!=target->handle)return RF_FORMAT;
        }
    }else if(target->kind==3){
        if(!turrets||turrets->count!=scene_turret_authored_count)return RF_FORMAT;
        for(uint32_t i=0;i<turrets->count;i++)if(turrets->rows[i].uid==target->uid){
            const scene_turret_saved *saved=turrets->rows+i;
            if(found++||saved->handle!=target->handle||saved->dead||!isfinite(saved->health)||saved->health<=0||
               (saved->flags&(2u|0x4000u))||(saved->flags_810&1u))return RF_FORMAT;
        }
    }else return RF_FORMAT;
    return found==1?RF_OK:RF_FORMAT;
}
static int scene_player_rocket_checkpoint_admit(const scene_ai_projectile_checkpoint_stage *stage,
    const scene_npc_checkpoint_restore_stage *npcs,const scene_passive_vehicle_checkpoint_stage *passive,
    const scene_turret_checkpoint_stage *turrets,const scene_world_player_stage *player,uint32_t blocked)
{
    int status;if(!stage)return RF_OK;
    if(stage->player_lock_present){
        if(!player||campaign_rocket_id<0||campaign_rocket_id>=64||
           player->player.player.weapon!=(uint32_t)campaign_rocket_id||
           !player->player.player.inventory.owned[campaign_rocket_id])return RF_FORMAT;
        if(stage->player_lock.active&&(blocked||player->player.player.health<=0||
           (player->modes.flags&RF_WEAPON_MODES_PLAYER_HOLSTERED)))return RF_FORMAT;
    }
    status=scene_player_rocket_checkpoint_target_admit(&stage->player_lock_target,npcs,passive,turrets);if(status)return status;
    for(uint32_t i=0;i<SCENE_ROCKETS;i++){
        status=scene_player_rocket_checkpoint_target_admit(stage->player_targets+i,npcs,passive,turrets);if(status)return status;
    }
    return RF_OK;
}
/* Final-success publication is assignment-only, paired with the unchanged
 * paid pool and trigger. Never launch, debit, reacquire or dispatch an effect. */
static void scene_player_rocket_checkpoint_finish(scene_stream *s,const scene_ai_projectile_checkpoint_stage *stage)
{
    if(!s)return;
    scene_player_rocket_lock_reset(s,!!player_input.alt_fire);
    if(!stage)return;
    memcpy(s->rockets,stage->player_rockets,sizeof(stage->player_rockets));
    memcpy(s->rocket_liquid,stage->player_liquid,sizeof(stage->player_liquid));
    memcpy(s->rocket_basis,stage->player_basis,sizeof(stage->player_basis));
    if(stage->player_trigger_present)combat_trigger=stage->player_trigger;
    if(stage->player_lock_present){
        s->rocket_lock=stage->player_lock;
        if(s->rocket_lock.active){
            s->rocket_lock.scan_due+=combat_frame;
            if(s->rocket_lock.target&&!s->rocket_lock.locked)s->rocket_lock.lock_due+=combat_frame;
            s->rocket_lock.resume_pending=1;s->rocket_lock.resume_frame=combat_frame;
        }
    }
    rf_scene_rockets[3]=stage->player_rockets_live;
    rf_scene_rocket_lock[0]=s->rocket_lock.active;rf_scene_rocket_lock[1]=s->rocket_lock.target;
    rf_scene_rocket_lock[2]=stage->player_lock.lock_due;rf_scene_rocket_lock[3]=s->rocket_lock.locked;
    for(uint32_t i=0;i<SCENE_ROCKETS;i++)if(stage->player_targets[i].kind){
        s->rocket_targets[i]=stage->player_targets[i].handle;++rf_scene_rocket_lock[9];
    }
}
#endif
