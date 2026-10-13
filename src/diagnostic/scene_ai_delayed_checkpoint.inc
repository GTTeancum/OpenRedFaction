#ifndef RF_SCENE_AI_DELAYED_CHECKPOINT_INCLUDED
#define RF_SCENE_AI_DELAYED_CHECKPOINT_INCLUDED
/* Original426197..426273 prepays and publishes cadence/+4c0;4093aa clears
 * the delay before factory4c77a0. The pending receipt intentionally has no
 * onset ray: TriBeam/Cane read the restored current eye basis, and Spit reads
 * the restored current primary_1 pose and current qualified combat target.
 * RFAP is this port's wire owner, not a reconstruction of RF.exe save bytes. */
static uint32_t scene_ai_delayed_checkpoint_busy(void)
{
    return scene_ai_tribeam_servicing||scene_ai_tribeam_presenting||scene_ai_tribeam_updating||
        scene_ai_spit_servicing||scene_ai_spit_presenting||scene_ai_spit_updating||
        scene_ai_cane_servicing||scene_ai_cane_presenting||scene_ai_cane_updating;
}
static int32_t scene_ai_delayed_checkpoint_family(int32_t weapon)
{
    if(campaign_enemy_tribeam_weapon(weapon))return 0;
    if(campaign_enemy_spit_weapon(weapon))return weapon==scene_ai_spit_definitions[0].flight.weapon?1:2;
    if(campaign_enemy_cane_weapon(weapon))return 3;
    return -1;
}
static uint32_t scene_ai_delayed_checkpoint_pending(void)
{
    if(scene_ai_tribeam_pending()||scene_ai_spit_pending()||scene_ai_cane_pending())return 1;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.view&&!(o->object_flags&2u)&&!(o->view.flags_810&1u)&&
           o->damage.effects.health>0&&o->combat_alert&&scene_ai_delayed_checkpoint_family(o->view.weapons[0])>=0)return 1;
    }
    return 0;
}
static uint32_t scene_ai_delayed_checkpoint_capacity(uint32_t family)
{return family==0?SCENE_AI_TRIBEAM_CAPACITY:family<3?SCENE_AI_SPIT_CAPACITY:SCENE_AI_CANE_CAPACITY;}
static uint32_t scene_ai_delayed_checkpoint_index(uint32_t family,uint32_t slot)
{return slot+(family==0?0:family<3?SCENE_AI_TRIBEAM_CAPACITY:SCENE_AI_TRIBEAM_CAPACITY+SCENE_AI_SPIT_CAPACITY);}
static uint32_t scene_ai_delayed_checkpoint_bank(uint32_t family)
{return family==0?0:family<3?1:2;}
static const scene_ai_laser_profile *scene_ai_delayed_checkpoint_profile(uint32_t family)
{
    const scene_ai_laser_profile *p;const rf_weapon_acquire_definition *a;const char *name;
    if(family>3)return NULL;
    p=family==0?&scene_ai_tribeam_definition.flight:family<3?&scene_ai_spit_definitions[family-1].flight:&scene_ai_cane_definition.flight;
    name=family==0?"TriBeam Laser":family<3?scene_ai_spit_contracts[family-1].name:"Capek Cane";
    if(p->ready!=1||p->weapon<0||p->weapon>=64||campaign_weapon_supply.names.count>64||
       (uint32_t)p->weapon>=campaign_weapon_supply.names.count||
       rf_weapon_name_find(&campaign_weapon_supply.names,name)!=p->weapon)return NULL;
    a=campaign_weapon_supply.definitions+p->weapon;
    if(a->magazine||a->capacity!=200||a->ammo_type<0||a->ammo_type>=32||p->primary.magazine||
       p->primary.burst_count!=1||p->primary.projectiles!=1||p->liquid_weapon_flags)return NULL;
    /* These independently strict loaders already seal cadence, delay, kind,
     * speed/lifetime and Big Spit's radial payload against original rows. */
    return p;
}
static int32_t scene_ai_delayed_checkpoint_delay(uint32_t family)
{return family==0?scene_ai_tribeam_definition.delay_ms:family<3?scene_ai_spit_definitions[family-1].delay_ms:scene_ai_cane_definition.delay_ms;}
static int scene_ai_delayed_checkpoint_class(uint32_t family,uint32_t slot)
{
    uint32_t cls;
    if(family!=1&&family!=2)return RF_OK;
    if(!campaign_seeds.items||!campaign_seeds.records.items||slot>=campaign_seeds.records.count||!campaign_seeds.classes)return RF_FORMAT;
    cls=campaign_seeds.items[slot].class_index;
    if(cls>=campaign_seeds.class_count||!(campaign_seeds.classes[cls].physics.flags2&0x20u)||
       !scene_npc_loadout_equal(campaign_seeds.records.items[slot].record.class_name,scene_ai_spit_definitions[family-1].owner_class))return RF_FORMAT;
    return RF_OK;
}
static int32_t scene_ai_delayed_checkpoint_now(void)
{return (int32_t)(((uint64_t)(combat_frame==UINT32_MAX?0:combat_frame)*1000/60)%RF_TIMER_PERIOD);}
static int32_t scene_ai_delayed_checkpoint_rebase(int32_t remaining)
{
    int32_t value=scene_ai_delayed_checkpoint_now()+remaining;
    if(value>RF_TIMER_PERIOD)value-=RF_TIMER_PERIOD;
    else if(value<0)value+=RF_TIMER_PERIOD;
    return value;
}
/* Copy fields explicitly rather than aliasing the three independent C types. */
static int scene_ai_delayed_checkpoint_windup(uint32_t bank,uint32_t slot,
    scene_ai_delayed_checkpoint_owner *out,uint32_t *phase,uint64_t *ticket)
{
#define DELAYED_WINDUP_COPY(p) do{*phase=(p)->phase;*ticket=(p)->ticket;out->source=(p)->source;out->source_slot=(p)->source_slot;out->remaining_ms=(p)->due_ms;out->family=(uint32_t)scene_ai_delayed_checkpoint_family((p)->weapon);}while(0)
    if(bank>2||slot>=(bank==0?SCENE_AI_TRIBEAM_CAPACITY:bank==1?SCENE_AI_SPIT_CAPACITY:SCENE_AI_CANE_CAPACITY))return RF_RANGE;
    memset(out,0,sizeof(*out));
    if(bank==0)DELAYED_WINDUP_COPY(scene_ai_tribeam_windups+slot);
    else if(bank==1)DELAYED_WINDUP_COPY(scene_ai_spit_windups+slot);
    else DELAYED_WINDUP_COPY(scene_ai_cane_windups+slot);
#undef DELAYED_WINDUP_COPY
    out->pending_slot=slot;return RF_OK;
}
static const scene_ai_laser_projectile *scene_ai_delayed_checkpoint_flight(uint32_t bank,uint32_t slot)
{return (bank==0?scene_ai_tribeams:bank==1?scene_ai_spits:scene_ai_canes)+slot;}
static int scene_ai_delayed_checkpoint_capture_owner(uint32_t source_slot,scene_ai_delayed_checkpoint_owner *out)
{
    const campaign_npc_body *o=campaign_npc_bodies+source_slot;const scene_ai_laser_profile *p;
    scene_ai_delayed_checkpoint_owner next={0};uint32_t pending=0,frame=combat_frame==UINT32_MAX?0:combat_frame;
    int32_t family=scene_ai_delayed_checkpoint_family(o->view.weapons[0]);int status;
    if(family<0)return RF_NOT_FOUND;
    next.pending_slot=UINT32_MAX;next.family=(uint32_t)family;
    for(uint32_t bank=0;bank<3;bank++)for(uint32_t slot=0;slot<(bank==0?SCENE_AI_TRIBEAM_CAPACITY:bank==1?SCENE_AI_SPIT_CAPACITY:SCENE_AI_CANE_CAPACITY);slot++){
        scene_ai_delayed_checkpoint_owner windup;uint32_t phase;uint64_t ticket;
        scene_ai_delayed_checkpoint_windup(bank,slot,&windup,&phase,&ticket);
        if(!phase||windup.source!=o->registration.handle)continue;
        if(pending++||phase!=2||!ticket||windup.source_slot!=source_slot||windup.family!=(uint32_t)family||
           scene_ai_delayed_checkpoint_bank(windup.family)!=bank||scene_ai_delayed_checkpoint_flight(bank,slot)->flight.active)return RF_FORMAT;
        if(ticket>(bank==0?scene_ai_tribeam_ticket:bank==1?scene_ai_spit_ticket:scene_ai_cane_ticket))return RF_FORMAT;
        if(windup.remaining_ms<0||windup.remaining_ms>RF_TIMER_PERIOD)return RF_FORMAT;
        status=rf_timer_remaining(windup.remaining_ms,scene_ai_delayed_checkpoint_now(),&next.remaining_ms);if(status)return status;
        if(next.remaining_ms>scene_ai_delayed_checkpoint_delay(next.family))return RF_FORMAT;
        next.pending_slot=slot;
    }
    if(!pending&&(!o->registration.view||(o->object_flags&2u)||(o->view.flags_810&1u)||o->damage.effects.health<=0||!o->combat_alert))return RF_NOT_FOUND;
    p=scene_ai_delayed_checkpoint_profile(next.family);
    if(!p||!isfinite(o->damage.effects.health)||o->damage.effects.health<=0||(o->object_flags&2u)||(o->view.flags_810&1u)||
       o->inventory.owned[p->weapon]!=1||o->inventory.loaded[p->weapon]||o->view.linked_handle!=-1||
       scene_npc_seat_attached(o->registration.handle)||o->combat_reload_pending||o->combat_reload_due||
       o->combat_burst_remaining||o->combat_alert>1||o->combat_scripted>2||
       scene_ai_delayed_checkpoint_class(next.family,source_slot))return RF_FORMAT;
    status=scene_ai_fusion_checkpoint_source(o->registration.handle,&next.uid);if(status)return status==RF_NOT_FOUND?RF_FORMAT:status;
    if(pending&&next.family==3){uint32_t ready;
        status=scene_ai_cane_muzzle_ready(o,&ready);if(status)return status;if(!ready)return RF_FORMAT;
    }
    next.source=o->registration.handle;next.source_slot=source_slot;
    next.reserve=o->inventory.reserve[campaign_weapon_supply.definitions[p->weapon].ammo_type];
    if(next.reserve<0||next.reserve>200)return RF_FORMAT;
    next.fire_remaining=o->combat_due>frame?o->combat_due-frame:0;
    next.navigation_remaining=o->combat_navigation_due>frame?o->combat_navigation_due-frame:0;
    next.burst=o->combat_burst_remaining;next.random=campaign_enemy_spread_random.value;
    next.alert=o->combat_alert;next.combat_mode=o->combat_scripted;
    if(next.fire_remaining>300||next.navigation_remaining>15||(!next.alert&&(next.fire_remaining||next.navigation_remaining||next.combat_mode)))return RF_FORMAT;
    *out=next;return RF_OK;
}
static int scene_ai_delayed_checkpoint_shape(const scene_stream *stream,uint32_t family,const scene_ai_laser_projectile *shot)
{
    const scene_ai_laser_profile *p=scene_ai_delayed_checkpoint_profile(family);uint32_t i,j,k;
    if(!stream||!shot||!p||(family==0&&!stream->tribeam_visual))return RF_FORMAT;
    if(shot->flight.active!=1||shot->weapon!=p->weapon||shot->damage_kind!=p->primary.damage_kind||
       shot->damage!=combat_enemy_primary_damage(&p->primary)||shot->liquid_weapon_flags!=p->liquid_weapon_flags||
       shot->blast_radius!=(family==2?scene_ai_spit_definitions[1].blast_radius:0)||
       shot->blast_kind!=(family==2?scene_ai_spit_definitions[1].blast_kind:0)||shot->flight.radius!=p->radius||
       !isfinite(shot->flight.remaining)||shot->flight.remaining<=0||shot->flight.remaining>p->lifetime||
       (shot->liquid.query_flags!=0x1004u&&(family==3||shot->liquid.query_flags!=4u)))return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(shot->flight.position[i])||fabsf(shot->flight.position[i])>100000||!isfinite(shot->flight.velocity[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(shot->basis[i*3+j]))return RF_FORMAT;
    }
    /* Cane renormalizes every guidance update; retained binary32 bits remain
     * authoritative. This tolerance covers normalization, never a new aim. */
    for(i=0;i<3;i++){
        if(fabsf(shot->flight.velocity[i]/p->speed-shot->basis[6+i])>.001f)return RF_FORMAT;
        for(j=i;j<3;j++){
            double dot=0;for(k=0;k<3;k++)dot+=(double)shot->basis[i*3+k]*shot->basis[j*3+k];
            if(fabs(dot-(i==j?1.0:0.0))>.001)return RF_FORMAT;
        }
        j=(i+1)%3;k=(i+2)%3;
        if(fabsf(shot->basis[j]*shot->basis[3+k]-shot->basis[k]*shot->basis[3+j]-shot->basis[6+i])>.001f)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_ai_delayed_checkpoint_lock(uint32_t slot,uint32_t source,uint32_t *role,uint32_t *uid,int32_t *remaining)
{
    const scene_ai_cane_homing *lock=scene_ai_cane_locks+slot;int status;
    *role=*uid=0;*remaining=0;
    if(lock->target==UINT32_MAX){
        if(lock->target_slot!=UINT32_MAX||lock->scan_due<0||lock->scan_due>RF_TIMER_PERIOD)return RF_FORMAT;
        status=rf_timer_remaining(lock->scan_due,scene_ai_delayed_checkpoint_now(),remaining);if(status)return status;
        if(*remaining>200)return RF_FORMAT;
    }else if(lock->target_slot==UINT32_MAX){
        if(lock->target==source||!scene_ai_melee_player_identity(lock->target))return RF_NOT_FOUND;
        *role=1;
    }else{
        if(lock->target==source||!scene_ai_melee_npc_identity(lock->target_slot,lock->target))return RF_NOT_FOUND;
        status=scene_ai_fusion_checkpoint_source(lock->target,uid);if(status)return status;
        *role=2;
    }
    return RF_OK;
}
static int scene_ai_delayed_checkpoint_capture(const scene_stream *stream,unsigned char *rows,uint32_t *count)
{
    uint32_t n=0,owners=0;int status;
    if(!stream||!count)return RF_RANGE;
    if(scene_ai_delayed_checkpoint_busy())return RF_NOT_FOUND;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        scene_ai_delayed_checkpoint_owner owner;unsigned char *r;
        status=scene_ai_delayed_checkpoint_capture_owner(i,&owner);if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(owners>=SCENE_AI_DELAYED_OWNERS)return RF_RANGE;
        if(!rows){++n;++owners;continue;}r=rows+112*n++;
        checkpoint_put(r,15);checkpoint_put(r+4,owners++);checkpoint_put(r+8,owner.uid);checkpoint_put(r+12,owner.family);
        checkpoint_put(r+16,owner.pending_slot);checkpoint_put(r+20,(uint32_t)owner.remaining_ms);
        checkpoint_put(r+24,owner.fire_remaining);checkpoint_put(r+28,owner.navigation_remaining);checkpoint_put(r+32,owner.burst);
        checkpoint_put(r+36,owner.random);checkpoint_put(r+40,(uint32_t)owner.reserve);checkpoint_put(r+44,owner.alert);checkpoint_put(r+48,owner.combat_mode);
    }
    for(uint32_t bank=0;bank<3;bank++)for(uint32_t slot=0;slot<(bank==0?SCENE_AI_TRIBEAM_CAPACITY:bank==1?SCENE_AI_SPIT_CAPACITY:SCENE_AI_CANE_CAPACITY);slot++){
        const scene_ai_laser_projectile *shot=scene_ai_delayed_checkpoint_flight(bank,slot);
        scene_ai_delayed_checkpoint_owner windup;uint32_t phase,uid;uint64_t ticket;int32_t family;
        unsigned char *r;
        scene_ai_delayed_checkpoint_windup(bank,slot,&windup,&phase,&ticket);
        if(phase){scene_ai_delayed_checkpoint_owner owner;
            if(phase!=2||windup.source_slot>=campaign_npc_body_count||
               scene_ai_delayed_checkpoint_capture_owner(windup.source_slot,&owner)||owner.pending_slot!=slot||
               scene_ai_delayed_checkpoint_bank(owner.family)!=bank||owner.source!=windup.source)return RF_FORMAT;
        }
        if(!shot->flight.active)continue;
        family=scene_ai_delayed_checkpoint_family(shot->weapon);
        if(family<0||scene_ai_delayed_checkpoint_bank((uint32_t)family)!=bank||phase)return RF_FORMAT;
        status=scene_ai_delayed_checkpoint_shape(stream,(uint32_t)family,shot);if(status)return status;
        status=scene_ai_fusion_checkpoint_source(shot->source,&uid);if(status)return status;
        if(rows){r=rows+112*n;
            checkpoint_put(r,16+(uint32_t)family);checkpoint_put(r+4,slot);checkpoint_put(r+8,uid);
            for(uint32_t k=0;k<3;k++){checkpoint_put_float(r+16+k*4,shot->flight.position[k]);checkpoint_put_float(r+28+k*4,shot->flight.velocity[k]);}
            checkpoint_put_float(r+40,shot->flight.radius);checkpoint_put(r+44,shot->liquid.query_flags);
            scene_ai_projectile_put_double(r+48,shot->flight.remaining);checkpoint_put_float(r+56,shot->damage);
            checkpoint_put(r+60,(uint32_t)shot->damage_kind);checkpoint_put(r+64,shot->liquid_weapon_flags);checkpoint_put_float(r+68,shot->blast_radius);
            for(uint32_t k=0;k<9;k++)checkpoint_put_float(r+72+k*4,shot->basis[k]);checkpoint_put(r+108,(uint32_t)shot->blast_kind);
        }++n;
        if(family==3){uint32_t role,target;int32_t remaining;
            if(!scene_ai_cane_flight_tickets[slot]||scene_ai_cane_flight_tickets[slot]>scene_ai_cane_ticket)return RF_FORMAT;
            status=scene_ai_delayed_checkpoint_lock(slot,shot->source,&role,&target,&remaining);if(status)return status;
            if(rows){r=rows+112*n;checkpoint_put(r,20);checkpoint_put(r+4,slot);checkpoint_put(r+8,uid);
                checkpoint_put(r+16,role);checkpoint_put(r+20,target);checkpoint_put(r+24,(uint32_t)remaining);}
            ++n;
        }
    }
    *count=n;return RF_OK;
}
static int scene_ai_delayed_checkpoint_read(const scene_stream *stream,scene_ai_delayed_checkpoint_stage *stage,const unsigned char *r)
{
    uint32_t kind=checkpoint_u32(r),slot=checkpoint_u32(r+4),uid=checkpoint_u32(r+8),source,resolved;
    if(!stage||kind<15||kind>20||!uid||uid>(uint32_t)INT32_MAX||
       scene_ai_projectile_source_handle(uid,&source)||scene_ai_fusion_checkpoint_source(source,&resolved)||resolved!=uid)return RF_FORMAT;
    if(kind==15){
        scene_ai_delayed_checkpoint_owner *o;uint32_t family=checkpoint_u32(r+12);const scene_ai_laser_profile *p;
        p=scene_ai_delayed_checkpoint_profile(family);
        if(!p||(family==0&&!stream->tribeam_visual)||slot>=SCENE_AI_DELAYED_OWNERS||stage->owners[slot].uid)return RF_FORMAT;
        for(uint32_t i=0;i<SCENE_AI_DELAYED_OWNERS;i++)if(stage->owners[i].uid==uid)return RF_FORMAT;
        for(uint32_t i=52;i<112;i++)if(r[i])return RF_FORMAT;
        o=stage->owners+slot;o->uid=uid;o->source=source;o->family=family;o->source_slot=UINT32_MAX;
        for(uint32_t i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==source)o->source_slot=i;
        if(o->source_slot==UINT32_MAX||scene_ai_delayed_checkpoint_class(family,o->source_slot))return RF_FORMAT;
        o->pending_slot=checkpoint_u32(r+16);o->remaining_ms=(int32_t)checkpoint_u32(r+20);
        o->fire_remaining=checkpoint_u32(r+24);o->navigation_remaining=checkpoint_u32(r+28);o->burst=checkpoint_u32(r+32);
        o->random=checkpoint_u32(r+36);o->reserve=(int32_t)checkpoint_u32(r+40);o->alert=checkpoint_u32(r+44);o->combat_mode=checkpoint_u32(r+48);
        if((o->pending_slot==UINT32_MAX?o->remaining_ms!=0:o->pending_slot>=scene_ai_delayed_checkpoint_capacity(family))||
           o->remaining_ms< -RF_TIMER_PERIOD/2||o->remaining_ms>scene_ai_delayed_checkpoint_delay(family)||
           o->fire_remaining>300||o->navigation_remaining>15||o->burst||o->reserve<0||o->reserve>200||o->alert>1||o->combat_mode>2||
           (!o->alert&&(o->fire_remaining||o->navigation_remaining||o->combat_mode))||
           (!o->alert&&o->pending_slot==UINT32_MAX)||
           (stage->random_present&&stage->random!=o->random))return RF_FORMAT;
        stage->random=o->random;stage->random_present=1;++stage->owner_count;return RF_OK;
    }
    if(checkpoint_u32(r+12))return RF_FORMAT;
    if(kind==20){
        uint32_t role=checkpoint_u32(r+16),target=checkpoint_u32(r+20);int32_t remaining=(int32_t)checkpoint_u32(r+24);
        scene_ai_cane_homing *lock;
        if(slot>=SCENE_AI_CANE_CAPACITY||stage->lock_present[slot]||role>2||
           (role!=2&&target)||(role==2&&(!target||target>(uint32_t)INT32_MAX))||
           (role?remaining!=0:(remaining< -RF_TIMER_PERIOD/2||remaining>200)))return RF_FORMAT;
        for(uint32_t i=28;i<112;i++)if(r[i])return RF_FORMAT;
        lock=stage->locks+slot;lock->target=lock->target_slot=UINT32_MAX;lock->scan_due=remaining;
        if(role==1){
            lock->target=campaign_player_object.handle;
            if(!scene_ai_melee_player_identity(lock->target)||lock->target==source)return RF_FORMAT;
        }else if(role==2){
            if(scene_ai_projectile_source_handle(target,&lock->target)||lock->target==source||
               scene_ai_fusion_checkpoint_source(lock->target,&resolved)||resolved!=target)return RF_FORMAT;
            for(uint32_t i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==lock->target)lock->target_slot=i;
            if(lock->target_slot==UINT32_MAX)return RF_FORMAT;
        }
        stage->lock_present[slot]=1;stage->lock_source_uid[slot]=uid;stage->target_role[slot]=role;stage->target_uid[slot]=target;return RF_OK;
    }
    {uint32_t family=kind-16,index;scene_ai_laser_projectile *shot;const scene_ai_laser_profile *p=scene_ai_delayed_checkpoint_profile(family);
        if(!p||slot>=scene_ai_delayed_checkpoint_capacity(family))return RF_FORMAT;
        index=scene_ai_delayed_checkpoint_index(family,slot);shot=stage->flights+index;
        if(shot->flight.active)return RF_FORMAT;
        shot->source=source;shot->weapon=p->weapon;shot->flight.active=1;
        shot->flight.radius=checkpoint_float(r+40);shot->liquid.query_flags=checkpoint_u32(r+44);
        shot->flight.remaining=scene_ai_projectile_double(r+48);shot->damage=checkpoint_float(r+56);
        shot->damage_kind=(int32_t)checkpoint_u32(r+60);shot->liquid_weapon_flags=checkpoint_u32(r+64);
        shot->blast_radius=checkpoint_float(r+68);shot->blast_kind=(int32_t)checkpoint_u32(r+108);
        for(uint32_t k=0;k<3;k++){shot->flight.position[k]=checkpoint_float(r+16+k*4);shot->flight.velocity[k]=checkpoint_float(r+28+k*4);}
        for(uint32_t k=0;k<9;k++)shot->basis[k]=checkpoint_float(r+72+k*4);
        if(scene_ai_delayed_checkpoint_shape(stream,family,shot))return RF_FORMAT;
        stage->source_uid[index]=uid;++stage->live[scene_ai_delayed_checkpoint_bank(family)];
    }
    return RF_OK;
}
static int scene_ai_delayed_checkpoint_complete(scene_ai_delayed_checkpoint_stage *stage)
{
    uint8_t reserved[SCENE_AI_DELAYED_SLOTS]={0};uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;
    if(!stage)return RF_RANGE;
    memset(stage->windups,0,sizeof(stage->windups));
    for(uint32_t i=0;i<SCENE_AI_DELAYED_OWNERS;i++){
        const scene_ai_delayed_checkpoint_owner *o=stage->owners+i;uint32_t index;
        if((i<stage->owner_count)!=(o->uid!=0))return RF_FORMAT;
        if(!o->uid)continue;
        if(frame>UINT32_MAX-o->fire_remaining||frame>UINT32_MAX-o->navigation_remaining)return RF_RANGE;
        if(o->pending_slot==UINT32_MAX)continue;
        index=scene_ai_delayed_checkpoint_index(o->family,o->pending_slot);
        if(reserved[index]||stage->flights[index].flight.active)return RF_FORMAT;
        reserved[index]=1;++stage->windups[scene_ai_delayed_checkpoint_bank(o->family)];
    }
    for(uint32_t i=0;i<SCENE_AI_CANE_CAPACITY;i++){
        uint32_t index=scene_ai_delayed_checkpoint_index(3,i);
        if(stage->lock_present[i]!=(stage->flights[index].flight.active!=0)||
           (stage->lock_present[i]&&stage->lock_source_uid[i]!=stage->source_uid[index]))return RF_FORMAT;
    }
    /* New process-owned tickets prevent a suspended old callback from naming
     * a restored reservation or Cane loop. Never serialize/reuse old tickets. */
    if(UINT64_MAX-scene_ai_tribeam_ticket<stage->windups[0]||
       UINT64_MAX-scene_ai_spit_ticket<stage->windups[1]||
       UINT64_MAX-scene_ai_cane_ticket<(uint64_t)stage->windups[2]+stage->live[2])return RF_RANGE;
    return RF_OK;
}
static const scene_npc_checkpoint_restore_entry *scene_ai_delayed_checkpoint_candidate(
    const scene_npc_checkpoint_restore_stage *npcs,uint32_t uid,uint32_t handle)
{
    const scene_npc_checkpoint_restore_entry *found=NULL;
    if(!npcs||!uid||!handle||handle==UINT32_MAX)return NULL;
    for(uint32_t i=0;i<npcs->count;i++)if(npcs->entries[i].saved.uid==uid){if(found)return NULL;found=npcs->entries+i;}
    if(!found||!found->registered||found->saved.retired||found->handle!=handle||
       found->slot>=campaign_npc_body_count||(uint32_t)campaign_seeds.records.items[found->slot].record.uid!=uid)return NULL;
    return found;
}
static int scene_ai_projectile_checkpoint_delayed_admit(const scene_ai_projectile_checkpoint_stage *projectiles,
    const scene_npc_checkpoint_restore_stage *npcs)
{
    const scene_ai_delayed_checkpoint_stage *stage;
    if(!projectiles)return RF_OK;stage=&projectiles->delayed;
    for(uint32_t i=0;i<SCENE_AI_DELAYED_SLOTS;i++)if(stage->flights[i].flight.active){
        if(!scene_ai_delayed_checkpoint_candidate(npcs,stage->source_uid[i],stage->flights[i].source))return RF_FORMAT;
    }
    for(uint32_t i=0;i<stage->owner_count;i++){
        const scene_ai_delayed_checkpoint_owner *o=stage->owners+i;
        const scene_ai_laser_profile *p=scene_ai_delayed_checkpoint_profile(o->family);
        const scene_npc_checkpoint_restore_entry *e=scene_ai_delayed_checkpoint_candidate(npcs,o->uid,o->source);
        const rf_npc_checkpoint_record *r=e?&e->saved:NULL;uint32_t mode;
        if(!e||!p||e->slot!=o->source_slot||r->dead_pose||!isfinite(r->health)||r->health<=0||(e->flags&2u)||
           r->primary!=p->weapon||r->inventory.owned[p->weapon]!=1||r->inventory.loaded[p->weapon]||
           r->inventory.reserve[campaign_weapon_supply.definitions[p->weapon].ammo_type]!=o->reserve||
           r->combat_alert!=o->alert||r->reload_present||r->combat.reload_remaining||r->guard_retreat.present||r->ai_mode==13||
           scene_ai_delayed_checkpoint_class(o->family,e->slot))return RF_FORMAT;
        mode=r->combat.active==0?0:r->combat.active==2||r->combat.active==3?1:r->combat.active==4?2:UINT32_MAX;
        if(mode!=o->combat_mode||(r->combat.active&&(r->combat.burst!=o->burst||r->combat.due_remaining!=o->fire_remaining||r->combat.spread_rng!=o->random)))return RF_FORMAT;
        /* Authored model tags are admitted before storage publication. The
         * actual animated muzzle/aim is intentionally evaluated only at due. */
        if(o->pending_slot!=UINT32_MAX&&(o->family==1||o->family==2)){
            int32_t tag;if(rf_scene_corpse_file_tag(e->slot+1,"primary_1",0,&tag)||tag<0)return RF_FORMAT;
        }
        if(o->pending_slot!=UINT32_MAX&&o->family==3){
            int32_t tag;
            const campaign_npc_body *live=campaign_npc_bodies+e->slot;
            if(!live->weapon_hand_count||live->weapon_hand_count>2||live->weapon_hands[0]<0||
               !rf_weapon_world_model_token(campaign_weapon_models.weapons,p->weapon)||
               rf_weapon_world_tag(campaign_weapon_models.weapons,p->weapon,1,campaign_weapon_tag,NULL,&tag)||tag<0)return RF_FORMAT;
        }
    }
    for(uint32_t i=0;i<SCENE_AI_CANE_CAPACITY;i++)if(stage->lock_present[i]&&stage->target_role[i]==2){
        const scene_npc_checkpoint_restore_entry *e=scene_ai_delayed_checkpoint_candidate(npcs,stage->target_uid[i],stage->locks[i].target);
        if(!e||e->slot!=stage->locks[i].target_slot)return RF_FORMAT;
    }
    if(npcs)for(uint32_t i=0;i<npcs->count;i++){
        const scene_npc_checkpoint_restore_entry *e=npcs->entries+i;const rf_npc_checkpoint_record *r=&e->saved;
        if(stage->random_present&&((r->shot_count&&r->shot_rng!=stage->random)||
           (r->combat.active&&r->combat.spread_rng!=stage->random)||
           (r->guard_retreat.present&&r->guard_retreat.spread_rng!=stage->random)))return RF_FORMAT;
        /* RFAP8 producers include ordinary selected delayed actors even after
         * their flight retires. Missing controller data cannot silently fall
         * back to RFNC's old30-frame ordinary awareness constructor. */
        if(stage->present&&e->registered&&!r->retired&&!r->dead_pose&&r->health>0&&!(e->flags&2u)&&
           r->combat_alert&&scene_ai_delayed_checkpoint_family(r->primary)>=0){
            uint32_t found=0;for(uint32_t j=0;j<stage->owner_count;j++)found+=stage->owners[j].uid==r->uid;
            if(found!=1)return RF_FORMAT;
        }
    }
    return RF_OK;
}
/* Assignment-only final-success publication follows outgoing timeline resets.
 * No debit, cadence restart, target acquisition, launch audio or hit is replayed.
 * Preparation/close failure never publishes these new owners or pools. */
static void scene_ai_projectile_checkpoint_delayed_finish(const scene_ai_projectile_checkpoint_stage *projectiles)
{
    const scene_ai_delayed_checkpoint_stage *stage;uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;
    if(!projectiles)return;stage=&projectiles->delayed;
    memcpy(scene_ai_tribeams,stage->flights,sizeof(scene_ai_tribeams));
    memcpy(scene_ai_spits,stage->flights+SCENE_AI_TRIBEAM_CAPACITY,sizeof(scene_ai_spits));
    memcpy(scene_ai_canes,stage->flights+SCENE_AI_TRIBEAM_CAPACITY+SCENE_AI_SPIT_CAPACITY,sizeof(scene_ai_canes));
    memcpy(scene_ai_cane_locks,stage->locks,sizeof(scene_ai_cane_locks));
    rf_scene_ai_tribeam[3]=stage->live[0];rf_scene_ai_spit[3]=stage->live[1];rf_scene_ai_cane[3]=stage->live[2];
    for(uint32_t i=0;i<stage->owner_count;i++){
        const scene_ai_delayed_checkpoint_owner *o=stage->owners+i;campaign_npc_body *npc=campaign_npc_bodies+o->source_slot;
        const scene_ai_laser_profile *p=scene_ai_delayed_checkpoint_profile(o->family);
        npc->combat_due=o->alert?frame+o->fire_remaining:0;
        npc->combat_navigation_due=o->alert?frame+o->navigation_remaining:0;npc->combat_burst_remaining=o->burst;
        if(o->pending_slot==UINT32_MAX)continue;
#define DELAYED_WINDUP_ASSIGN(pending,sequence) do{(pending)->ticket=++(sequence);(pending)->phase=2;(pending)->source=o->source;(pending)->source_slot=o->source_slot;(pending)->start_frame=frame;(pending)->weapon=p->weapon;(pending)->due_ms=scene_ai_delayed_checkpoint_rebase(o->remaining_ms);}while(0)
        if(o->family==0)DELAYED_WINDUP_ASSIGN(scene_ai_tribeam_windups+o->pending_slot,scene_ai_tribeam_ticket);
        else if(o->family<3)DELAYED_WINDUP_ASSIGN(scene_ai_spit_windups+o->pending_slot,scene_ai_spit_ticket);
        else DELAYED_WINDUP_ASSIGN(scene_ai_cane_windups+o->pending_slot,scene_ai_cane_ticket);
#undef DELAYED_WINDUP_ASSIGN
    }
    if(stage->random_present)campaign_enemy_spread_random.value=stage->random;
    for(uint32_t i=0;i<SCENE_AI_SPIT_CAPACITY;i++)if(scene_ai_spits[i].flight.active)scene_spit_visual_bind(i,scene_ai_spits+i,frame);
    for(uint32_t i=0;i<SCENE_AI_CANE_CAPACITY;i++)if(scene_ai_canes[i].flight.active){
        scene_ai_cane_flight_tickets[i]=++scene_ai_cane_ticket;
        scene_ai_cane_locks[i].scan_due=scene_ai_delayed_checkpoint_rebase(stage->locks[i].scan_due);
        scene_cane_visual_bind(i,scene_ai_canes+i,frame);
    }
}
#endif
