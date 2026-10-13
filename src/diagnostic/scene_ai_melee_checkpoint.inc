#ifndef RF_SCENE_AI_MELEE_CHECKPOINT_INCLUDED
#define RF_SCENE_AI_MELEE_CHECKPOINT_INCLUDED
/* RFAP12 kind26, extended by RFAP14 families5-7, is this port's receipt, not original save
 * bytes. RF.exe42606f..4260ce schedules each authored impact independently;
 * 4fa360/4fa3f0 supply the existing modulo-time semantics. Keep exact remaining
 * bits/deadlines: never restart an animation, repeat a hit or acquire a target.
 * Include after RFAP stage, delayed candidate joins and the runtime owner. */
static int32_t scene_ai_melee_checkpoint_family(int32_t weapon)
{
    if(campaign_enemy_drone_smash_weapon(weapon))return 0;
    if(scene_ai_reeper_claws.ready)for(uint32_t i=0;i<SCENE_AI_REEPER_CLAWS;i++)
        if(scene_ai_reeper_claws.ids[i]==weapon)return (int32_t)i+1;
    return -1;
}
static int scene_ai_melee_checkpoint_profile(uint32_t family,int32_t *weapon,
    const rf_weapon_primary_definition **primary,uint32_t *count,const int32_t **delays)
{
    const char *name;
    if(family>SCENE_AI_REEPER_CLAWS||!weapon||!primary||!count||!delays)return RF_FORMAT;
    if(!family){
        if(!scene_ai_drone_smash_definition.ready)return RF_FORMAT;
        *weapon=scene_ai_drone_smash_definition.weapon;*primary=&scene_ai_drone_smash_definition.primary;
        *count=1;*delays=&scene_ai_drone_smash_definition.delay_ms;name="Drone Smash";
    }else{
        if(!scene_ai_reeper_claws.ready)return RF_FORMAT;
        *weapon=scene_ai_reeper_claws.ids[family-1];*primary=scene_ai_reeper_claws.primary+family-1;
        *count=scene_ai_reeper_claws.impacts[family-1].primary_count;
        *delays=scene_ai_reeper_claws.impacts[family-1].primary_ms;name=scene_ai_reeper_claw_names[family-1];
    }
    if(*weapon<0||*weapon>=64||campaign_weapon_supply.names.count>64||
       campaign_weapon_supply.names.primary_count>campaign_weapon_supply.names.count||
       (uint32_t)*weapon>=campaign_weapon_supply.names.primary_count||
       rf_weapon_name_find(&campaign_weapon_supply.names,name)!=*weapon||
       (*primary)->magazine||(*primary)->reload_seconds||(*primary)->burst_count!=1||
       (*primary)->burst_seconds||(*primary)->projectiles!=1||!isfinite((*primary)->fire_seconds)||
       (*primary)->fire_seconds<=0||(*primary)->fire_seconds>2.5f||!*count||*count>2||
       (*delays)[0]<=0||(*delays)[0]>800||(*count==2&&((*delays)[1]<=(*delays)[0]||(*delays)[1]>800)))return RF_FORMAT;
    return RF_OK;
}
static uint32_t scene_ai_melee_checkpoint_pending(void)
{
    if(scene_ai_drone_smash_save_pending()||(campaign_npc_body_count&&!campaign_npc_bodies))return 1;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.view&&!(o->object_flags&2u)&&!(o->view.flags_810&1u)&&
           o->damage.effects.health>0&&o->combat_alert&&scene_ai_melee_checkpoint_family(o->view.weapons[0])>=0)return 1;
    }
    return 0;
}
/* Resolve an authored identity only. Saved life, selection, orders and seats
 * belong to the candidate, never to the current constructor's defaults. */
static int scene_ai_melee_checkpoint_identity(uint32_t value,uint32_t decode,
    uint32_t *uid,uint32_t *handle,uint32_t *slot)
{
    uint32_t found=0;
    if(!value||value==UINT32_MAX||decode>1||!uid||!handle||!slot||!campaign_npc_bodies||
       !campaign_seeds.records.items||campaign_seeds.records.count<campaign_npc_body_count)return RF_FORMAT;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;uint32_t key;
        if(value!=(decode?(uint32_t)campaign_seeds.records.items[i].record.uid:o->registration.handle))continue;
        if(found++||!scene_ai_drone_smash_npc_identity(i,o->registration.handle)||
           scene_ai_fusion_checkpoint_source(o->registration.handle,&key))return RF_FORMAT;
        *uid=key;*handle=o->registration.handle;*slot=i;
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static int scene_ai_melee_checkpoint_target(uint32_t role,uint32_t uid,uint32_t *handle,uint32_t *slot)
{
    uint32_t resolved;
    if(!handle||!slot)return RF_RANGE;*handle=0;*slot=UINT32_MAX;
    if(role==0)return uid?RF_FORMAT:RF_OK;
    if(role==1){
        if(uid||!scene_ai_melee_player_identity(campaign_player_object.handle))return RF_FORMAT;
        *handle=campaign_player_object.handle;return RF_OK;
    }
    if(role!=2||scene_ai_melee_checkpoint_identity(uid,1,&resolved,handle,slot)||resolved!=uid)return RF_FORMAT;
    return RF_OK;
}
static int scene_ai_melee_checkpoint_shape(const scene_ai_melee_checkpoint_owner *o)
{
    const rf_weapon_primary_definition *primary;const int32_t *delays;uint32_t count;
    int32_t weapon;
    if(!o||scene_ai_melee_checkpoint_profile(o->family,&weapon,&primary,&count,&delays)||
       o->order>3||o->fire_remaining>1200||o->navigation_remaining>15||o->burst>256||(!o->order&&o->burst))return RF_FORMAT;
    if(o->pending_slot==UINT32_MAX)return o->remaining||o->remaining_ms[0]||o->remaining_ms[1]||
        o->target_role||o->target_uid||o->order_target_role||o->order_target_uid?RF_FORMAT:RF_OK;
    if(o->pending_slot>=SCENE_AI_DRONE_SMASH_PENDING||o->order>2||o->burst||
       !o->target_role||o->target_role>2||(!o->order&&o->target_role!=1)||
       (o->order==2&&o->target_role!=2)||o->order_target_role>2||
       (!o->order&&o->order_target_role>1)||
       (o->order&&(o->order_target_role!=o->target_role||o->order_target_uid!=o->target_uid))||
       (count==1?o->remaining!=1:(o->remaining!=3&&o->remaining!=2)))return RF_FORMAT;
    for(uint32_t i=0;i<2;i++){
        if(!(o->remaining&(1u<<i))){if(o->remaining_ms[i])return RF_FORMAT;continue;}
        if(o->remaining_ms[i]<-RF_TIMER_PERIOD/2||o->remaining_ms[i]>delays[i])return RF_FORMAT;
    }
    if(o->remaining==3&&o->remaining_ms[1]-o->remaining_ms[0]!=delays[1]-delays[0])return RF_FORMAT;
    return RF_OK;
}
static int scene_ai_melee_checkpoint_capture_owner(uint32_t source_slot,scene_ai_melee_checkpoint_owner *out)
{
    const campaign_npc_body *npc=campaign_npc_bodies+source_slot;
    scene_ai_melee_checkpoint_owner o={0};const scene_ai_drone_smash_strike *strike=NULL;
    const rf_weapon_primary_definition *primary;const int32_t *delays;
    uint32_t count,resolved_slot;int32_t weapon,family=scene_ai_melee_checkpoint_family(npc->view.weapons[0]);int status;
    o.pending_slot=UINT32_MAX;o.target_slot=UINT32_MAX;
    for(uint32_t i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++){
        const scene_ai_drone_smash_strike *p=scene_ai_drone_smash_pending+i;
        if(!p->phase||p->source!=npc->registration.handle)continue;
        if(strike||p->phase!=2||!p->ticket||p->ticket>scene_ai_drone_smash_ticket||
           p->source_slot!=source_slot||p->weapon!=npc->view.weapons[0])return RF_FORMAT;
        strike=p;o.pending_slot=i;
    }
    if(!strike&&(family<0||!npc->registration.view||(npc->object_flags&2u)||(npc->view.flags_810&1u)||
       npc->damage.effects.health<=0||!npc->combat_alert))return RF_NOT_FOUND;
    if(family<0||!isfinite(npc->damage.effects.health)||npc->damage.effects.health<=0||
       (npc->object_flags&2u)||(npc->view.flags_810&1u)||npc->combat_alert!=1)return RF_FORMAT;
    o.family=(uint32_t)family;
    if(o.family>SCENE_AI_LEGACY_CLAWS){int32_t ranged,melee;
        if(scene_ai_class_melee_binding(source_slot,npc->view.weapons[0],&npc->inventory,&ranged,&melee)||
           melee!=npc->view.weapons[0])return RF_FORMAT;
    }
    if(scene_ai_melee_checkpoint_profile(o.family,&weapon,&primary,&count,&delays)||
       npc->inventory.owned[weapon]!=1||npc->inventory.loaded[weapon]||
       npc->combat_reload_pending||npc->combat_reload_due)return RF_FORMAT;
    status=scene_ai_melee_checkpoint_identity(npc->registration.handle,0,&o.uid,&o.source,&resolved_slot);if(status)return status;
    if(resolved_slot!=source_slot)return RF_FORMAT;o.source_slot=source_slot;
    o.order=npc->combat_scripted;o.ai_action=npc->ai_mode.action_280;
    o.fire_remaining=npc->combat_due>combat_frame?npc->combat_due-combat_frame:0;
    o.navigation_remaining=npc->combat_navigation_due>combat_frame?npc->combat_navigation_due-combat_frame:0;
    o.burst=npc->combat_burst_remaining;o.random=campaign_enemy_spread_random.value;
    if(strike){
        campaign_npc_body *source,*victim;const float *eye;const rf_physics_body *body;
        /* The live helper reads eligibility/identities, not geometry. Actual
         * eye/ray/range and shield contact are evaluated only by the due hit. */
        if(!scene_ai_drone_smash_live(strike,scene_actor_body.state.position,&source,&victim,&eye,&body,&primary))return RF_NOT_FOUND;
        o.remaining=strike->remaining;o.target=strike->target;o.target_slot=strike->target_slot;
        o.target_role=strike->target_slot==UINT32_MAX?1:2;
        if(o.target_role==2){uint32_t target;
            status=scene_ai_melee_checkpoint_identity(o.target,0,&o.target_uid,&target,&resolved_slot);if(status)return status;
            if(target!=o.target||resolved_slot!=o.target_slot)return RF_FORMAT;
        }
        o.order_target=strike->order_target;
        o.order_target_role=o.order?o.target_role:o.order_target?1:0;
        o.order_target_uid=o.order?o.target_uid:0;
        if(!o.order&&o.order_target&&o.order_target!=campaign_player_object.handle)return RF_FORMAT;
        for(uint32_t i=0;i<2;i++)if(o.remaining&(1u<<i)){
            if(strike->due_ms[i]<0||strike->due_ms[i]>RF_TIMER_PERIOD)return RF_FORMAT;
            status=rf_timer_remaining(strike->due_ms[i],scene_ai_delayed_checkpoint_now(),o.remaining_ms+i);if(status)return status;
        }
    }
    status=scene_ai_melee_checkpoint_shape(&o);if(status)return status;*out=o;return RF_OK;
}
static int scene_ai_melee_checkpoint_capture_version(unsigned char *rows,uint32_t *count,uint32_t *version)
{
    uint32_t n=0;int status;
    if(!count||!version)return RF_RANGE;
    *version=12;
    if(scene_ai_melee_checkpoint_busy())return RF_NOT_FOUND;
    if(combat_frame==UINT32_MAX||(campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    /* Every pending slot must be owned, including a stale slot whose source
     * is absent from the current actor iteration. Never silently drop it. */
    for(uint32_t i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++){
        const scene_ai_drone_smash_strike *p=scene_ai_drone_smash_pending+i;
        if(!p->phase)continue;
        if(!scene_ai_drone_smash_npc_identity(p->source_slot,p->source)||p->phase!=2)return RF_FORMAT;
        for(uint32_t j=0;j<i;j++)if(scene_ai_drone_smash_pending[j].phase&&
            scene_ai_drone_smash_pending[j].ticket==p->ticket)return RF_FORMAT;
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        scene_ai_melee_checkpoint_owner o;unsigned char *r;
        status=scene_ai_melee_checkpoint_capture_owner(i,&o);if(status==RF_NOT_FOUND){
            /* Invalid armed ownership is a save refusal, never an omission. */
            for(uint32_t j=0;j<SCENE_AI_DRONE_SMASH_PENDING;j++)if(scene_ai_drone_smash_pending[j].phase&&
                scene_ai_drone_smash_pending[j].source_slot==i)return RF_NOT_FOUND;
            continue;
        }
        if(status)return status;if(n>=SCENE_AI_MELEE_CHECKPOINT_CAPACITY)return RF_RANGE;
        if(o.family>SCENE_AI_LEGACY_CLAWS)*version=14;
        if(!rows){++n;continue;}r=rows+112*n;
        checkpoint_put(r,26);checkpoint_put(r+4,n++);checkpoint_put(r+8,o.uid);checkpoint_put(r+12,o.family);
        checkpoint_put(r+16,o.pending_slot);checkpoint_put(r+20,o.remaining);
        checkpoint_put(r+24,(uint32_t)o.remaining_ms[0]);checkpoint_put(r+28,(uint32_t)o.remaining_ms[1]);
        checkpoint_put(r+32,o.target_role);checkpoint_put(r+36,o.target_uid);checkpoint_put(r+40,o.order);
        checkpoint_put(r+44,o.order_target_role);checkpoint_put(r+48,o.order_target_uid);checkpoint_put(r+52,(uint32_t)o.ai_action);
        checkpoint_put(r+56,o.fire_remaining);checkpoint_put(r+60,o.navigation_remaining);
        checkpoint_put(r+64,o.burst);checkpoint_put(r+68,o.random);
    }
    *count=n;return RF_OK;
}
/* Preserve the existing count-only snapshot/export contract. */
static int scene_ai_melee_checkpoint_capture(unsigned char *rows,uint32_t *count)
{uint32_t version;return scene_ai_melee_checkpoint_capture_version(rows,count,&version);}
static int scene_ai_melee_checkpoint_read(scene_ai_melee_checkpoint_stage *stage,const unsigned char *r,uint32_t version)
{
    scene_ai_melee_checkpoint_owner o={0};uint32_t slot=checkpoint_u32(r+4),uid=checkpoint_u32(r+8),ignored;
    if(!stage||checkpoint_u32(r)!=26||slot>=SCENE_AI_MELEE_CHECKPOINT_CAPACITY||stage->owners[slot].uid)return RF_FORMAT;
    for(uint32_t i=72;i<112;i++)if(r[i])return RF_FORMAT;
    o.family=checkpoint_u32(r+12);o.pending_slot=checkpoint_u32(r+16);o.remaining=checkpoint_u32(r+20);
    o.remaining_ms[0]=(int32_t)checkpoint_u32(r+24);o.remaining_ms[1]=(int32_t)checkpoint_u32(r+28);
    o.target_role=checkpoint_u32(r+32);o.target_uid=checkpoint_u32(r+36);o.order=checkpoint_u32(r+40);
    o.order_target_role=checkpoint_u32(r+44);o.order_target_uid=checkpoint_u32(r+48);o.ai_action=(int32_t)checkpoint_u32(r+52);
    o.fire_remaining=checkpoint_u32(r+56);o.navigation_remaining=checkpoint_u32(r+60);
    o.burst=checkpoint_u32(r+64);o.random=checkpoint_u32(r+68);
    if((o.family>SCENE_AI_LEGACY_CLAWS&&version<14)||scene_ai_melee_checkpoint_shape(&o)||
       scene_ai_melee_checkpoint_identity(uid,1,&o.uid,&o.source,&o.source_slot)||o.uid!=uid||
       scene_ai_melee_checkpoint_target(o.target_role,o.target_uid,&o.target,&o.target_slot)||
       scene_ai_melee_checkpoint_target(o.order_target_role,o.order_target_uid,&o.order_target,&ignored)||
       (o.target&&o.source==o.target)||(stage->count&&stage->random!=o.random))return RF_FORMAT;
    for(uint32_t i=0;i<SCENE_AI_MELEE_CHECKPOINT_CAPACITY;i++){
        const scene_ai_melee_checkpoint_owner *p=stage->owners+i;
        if(!p->uid)continue;
        if(p->uid==uid||(o.pending_slot!=UINT32_MAX&&p->pending_slot==o.pending_slot))return RF_FORMAT;
    }
    stage->owners[slot]=o;stage->random=o.random;++stage->count;
    stage->armed+=o.pending_slot!=UINT32_MAX;return RF_OK;
}
static int scene_ai_melee_checkpoint_complete(const scene_ai_melee_checkpoint_stage *stage)
{
    uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;
    if(!stage)return RF_RANGE;
    for(uint32_t i=0;i<SCENE_AI_MELEE_CHECKPOINT_CAPACITY;i++){
        const scene_ai_melee_checkpoint_owner *o=stage->owners+i;
        if((i<stage->count)!=(o->uid!=0))return RF_FORMAT;
        if(o->uid&&(frame>UINT32_MAX-o->fire_remaining||frame>UINT32_MAX-o->navigation_remaining))return RF_RANGE;
    }
    return UINT64_MAX-scene_ai_drone_smash_ticket<stage->armed?RF_RANGE:RF_OK;
}
/* RFNS admission is already read-only and knows the complete candidate seat
 * association. Reuse it rather than looking at the outgoing linked_handle.
 * Living RFNC retains the current810 death bit; refuse it, never revive it. */
static int scene_ai_melee_checkpoint_candidate_live(const scene_npc_checkpoint_restore_entry *e,
    const scene_npc_seat_checkpoint_stage *seats)
{
    float basis[9];uint32_t seated;
    if(!e||!e->registered||e->saved.retired||e->saved.dead_pose||!isfinite(e->saved.health)||
       e->saved.health<=0||(e->flags&(2u|0x4000u))||
       e->slot>=campaign_npc_body_count||(campaign_npc_bodies[e->slot].view.flags_810&1u)||
       scene_npc_seat_checkpoint_basis(seats,&e->saved,basis,&seated)||seated)return RF_FORMAT;
    return RF_OK;
}
static int scene_ai_melee_checkpoint_admit(const scene_ai_projectile_checkpoint_stage *projectiles,
    const scene_npc_checkpoint_restore_stage *npcs,const scene_npc_seat_checkpoint_stage *seats,
    const scene_turret_generated_checkpoint_stage *generated,const scene_world_player_stage *player,uint32_t player_linked,
    uint32_t spread_rng,const scene_turret_player_checkpoint_stage *turret_player)
{
    const scene_ai_melee_checkpoint_stage *stage;
    if(!projectiles)return RF_OK;stage=&projectiles->melee;
    if(stage->count&&(stage->random!=spread_rng||
       (turret_player&&stage->random!=turret_player->rng)||
       (projectiles->delayed.random_present&&stage->random!=projectiles->delayed.random)))return RF_FORMAT;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_ai_melee_checkpoint_owner *o=stage->owners+i;
        const scene_npc_checkpoint_restore_entry *e=scene_ai_delayed_checkpoint_candidate(npcs,o->uid,o->source);
        const rf_npc_checkpoint_record *r=e?&e->saved:NULL;
        const rf_weapon_primary_definition *primary;const int32_t *delays;uint32_t count,mode;int32_t weapon;
        if(!e||e->slot!=o->source_slot||scene_ai_melee_checkpoint_profile(o->family,&weapon,&primary,&count,&delays)||
           r->dead_pose||!isfinite(r->health)||r->health<=0||(e->flags&2u)||r->combat_alert!=1||
           r->primary!=weapon||r->inventory.owned[weapon]!=1||r->inventory.loaded[weapon]||
           r->reload_present||r->combat.reload_remaining||r->guard_retreat.present||
           r->ai_mode!=o->ai_action||e->effective_ai_mode!=o->ai_action)return RF_FORMAT;
        if(o->family>SCENE_AI_LEGACY_CLAWS){int32_t ranged,melee;
            if(scene_ai_class_melee_binding(e->slot,r->primary,&r->inventory,&ranged,&melee)||melee!=r->primary)return RF_FORMAT;
        }
        mode=r->combat.active==0?0:r->combat.active==1?3:r->combat.active==2||r->combat.active==3?1:r->combat.active==4?2:UINT32_MAX;
        if(mode!=o->order||(r->combat.active&&(r->combat.burst!=o->burst||
           r->combat.due_remaining!=o->fire_remaining||r->combat.spread_rng!=o->random)))return RF_FORMAT;
        if(o->pending_slot==UINT32_MAX)continue;
        if(scene_ai_melee_checkpoint_candidate_live(e,seats)||(r->holster&1u)||r->shot_count||
           r->script_animation.active||o->ai_action==1||o->ai_action==14||
           (o->ai_action==4&&r->move.active&&!o->order&&r->route.origin!=SCENE_MOVE_ORIGIN_ACTOR_DEFAULT))return RF_FORMAT;
        if(generated)for(uint32_t j=0;j<generated->count;j++)if(generated->rows[j].key.base_uid==o->uid)return RF_FORMAT;
        /* An omitted/legacy companion cannot turn a still-bound generated
         * base into a handheld source. This identity guard has no counters. */
        for(uint32_t j=0;j<scene_turret_generated_count;j++)if(scene_turret_generated[j].base_handle==o->source&&
            scene_turret_generated_base(scene_turret_generated+j))return RF_FORMAT;
        if(o->order&&(r->combat.active==3||e->attack_target_handle!=o->target||
           r->combat.event!=o->target_uid||o->order_target!=o->target))return RF_FORMAT;
        if(o->target_role==1){
            if(!player||player_linked||!isfinite(player->player.player.health)||player->player.player.health<=0||
               !scene_ai_melee_player_identity(o->target)||campaign_player_view.linked_handle!=-1||
               (campaign_player_view.flags_7c&(2u|0x4000u))||(campaign_player_view.flags_810&1u))return RF_FORMAT;
        }else{
            const scene_npc_checkpoint_restore_entry *target=scene_ai_delayed_checkpoint_candidate(npcs,o->target_uid,o->target);
            if(!target||target->slot!=o->target_slot||scene_ai_melee_checkpoint_candidate_live(target,seats))return RF_FORMAT;
        }
    }
    if(npcs)for(uint32_t i=0;i<npcs->count;i++){
        const scene_npc_checkpoint_restore_entry *e=npcs->entries+i;const rf_npc_checkpoint_record *r=&e->saved;
        if(stage->count&&((r->shot_count&&r->shot_rng!=stage->random)||
           (r->combat.active&&r->combat.spread_rng!=stage->random)||
           (r->guard_retreat.present&&r->guard_retreat.spread_rng!=stage->random)))return RF_FORMAT;
        if(stage->present&&e->registered&&!r->retired&&!r->dead_pose&&r->health>0&&!(e->flags&2u)&&
           r->combat_alert&&scene_ai_melee_checkpoint_family(r->primary)>=0){
            uint32_t found=0;for(uint32_t j=0;j<stage->count;j++)found+=stage->owners[j].uid==r->uid;
            if(found!=1)return RF_FORMAT;
        }
    }
    return RF_OK;
}
/* Called after successful storage close and outgoing reset. Same bank slots
 * retain service/allocation order; fresh process tickets invalidate suspended
 * old stacks. Cadence/target assignment performs no attack or presentation. */
static void scene_ai_melee_checkpoint_finish(const scene_ai_projectile_checkpoint_stage *projectiles)
{
    uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;const scene_ai_melee_checkpoint_stage *stage;
    if(!projectiles)return;stage=&projectiles->melee;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_ai_melee_checkpoint_owner *o=stage->owners+i;campaign_npc_body *npc=campaign_npc_bodies+o->source_slot;
        npc->combat_due=frame+o->fire_remaining;npc->combat_navigation_due=frame+o->navigation_remaining;
        npc->combat_burst_remaining=o->burst;
        if(o->pending_slot!=UINT32_MAX){
            scene_ai_drone_smash_strike *p=scene_ai_drone_smash_pending+o->pending_slot;
            p->ticket=++scene_ai_drone_smash_ticket;p->phase=2;p->source=o->source;p->source_slot=o->source_slot;
            p->target=o->target;p->target_slot=o->target_slot;p->order=o->order;p->order_target=o->order_target;
            p->remaining=o->remaining;p->weapon=npc->view.weapons[0];p->ai_action=o->ai_action;
            for(uint32_t j=0;j<2;j++)p->due_ms[j]=(o->remaining&(1u<<j))?scene_ai_delayed_checkpoint_rebase(o->remaining_ms[j]):-1;
            /* Ordinary RFNC constructs the player target. Preserve the exact
             * accepted nullable order_target rather than cancelling on load. */
            npc->combat_target=o->order_target;
        }
    }
    if(stage->count)campaign_enemy_spread_random.value=stage->random;
}
#endif
