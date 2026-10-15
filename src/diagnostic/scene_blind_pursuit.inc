#ifndef RF_SCENE_BLIND_PURSUIT_INCLUDED
#define RF_SCENE_BLIND_PURSUIT_INCLUDED
/*406dba refresh precedes406dbf strict timeout.409050 same-target calls do
 * not refresh298. Practical existing30-frame phase and two geometry rays;
 * neither acquisition cone/range nor shot/reload readiness is visibility. */
static int scene_blind_weapon(int32_t weapon)
{
    campaign_enemy_weapon_selection selected={0};int status;
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,campaign_shotgun_id,
        campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    if(weapon<0||weapon>=64)return RF_NOT_FOUND;
    status=campaign_enemy_weapon_select(weapon,ids,campaign_primary,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,&selected);
    if(status==RF_NOT_FOUND)status=campaign_enemy_extra_weapon_select(weapon,&campaign_weapon_supply,&selected);
    /* Target-age qualification precedes candidate resource demand. Projectile
     * and continuous resources may be unresident; their validated catalog IDs
     * are sufficient here. Firing remains owned by its existing full selector. */
    if(status==RF_NOT_FOUND&&(weapon==campaign_rocket_id||weapon==campaign_grenade_id||
       weapon==campaign_flame_id||weapon==campaign_fusion_id)){
        const rf_weapon_acquire_definition *ammo;
        if((uint32_t)weapon>=campaign_weapon_supply.names.count)return RF_FORMAT;
        ammo=campaign_weapon_supply.definitions+weapon;
        status=ammo->ammo_type<0||ammo->ammo_type>=32||ammo->magazine<=0?RF_FORMAT:RF_OK;
    }
    return status;
}
static int scene_blind_source(uint32_t slot,int32_t weapon,uint32_t retained,float *seconds)
{
    const rf_entity_seed_class *c;rf_level_entity_ai ai;char held[2][RF_LEVEL_NAME_CAPACITY];uint32_t cls;int status;
    if(!seconds||retained>1||!campaign_seeds.items||!campaign_seeds.classes||!campaign_seeds.records.items||
       slot>=campaign_seeds.records.count||!scene_blind_classes.valid||
       scene_blind_classes.count!=campaign_seeds.class_count)return RF_FORMAT;
    cls=campaign_seeds.items[slot].class_index;if(cls>=scene_blind_classes.count)return RF_FORMAT;
    c=campaign_seeds.classes+cls;
    if(scene_blind_classes.seconds[cls]<0||c->physics.movement_index>=16||
       (campaign_modes[c->physics.movement_index].index!=1&&campaign_modes[c->physics.movement_index].index!=3)||
       campaign_seeds.items[slot].spawn.seat_host_uid!=-1)return RF_NOT_FOUND;
    status=rf_level_entity_ai_read(campaign_seeds.records.items+slot,&ai);if(status)return status;
    if(ai.mode!=1&&ai.mode!=2)return RF_NOT_FOUND;
    status=rf_level_entity_held_clutter_read(campaign_seeds.records.items+slot,held);if(status)return status;
    for(uint32_t j=0;j<2;j++)if(held[j][0]&&!scene_npc_loadout_equal(held[j],"none"))return RF_NOT_FOUND;
    if(weapon< -1||weapon>=64)return RF_FORMAT;
    if(!retained){status=scene_blind_weapon(weapon);if(status)return status;}
    *seconds=scene_blind_classes.seconds[cls];
    return isfinite(*seconds)&&*seconds>=0&&*seconds<=86400.f?RF_OK:RF_FORMAT;
}
static uint32_t scene_blind_target_handle(const campaign_npc_body *o)
{
    if(!o||!o->combat_alert||o->combat_scripted>2||o->script_shoot.active)return 0;
    return o->combat_scripted?o->combat_target:campaign_player_object.handle;
}
/* Registry identity gives the member address. Convert only after a bounded
 * integer span/stride check; no all-target scan on each active owner's tick. */
static uint32_t scene_blind_target_slot(uint32_t handle,uint32_t *slot)
{
    uintptr_t first,address,delta;const void *registered;
    if(!slot||!handle||handle==UINT32_MAX||!campaign_npc_bodies||!campaign_npc_body_count)return 0;
    registered=rf_object_registry_lookup(&campaign_registry,handle);if(!registered)return 0;
    first=(uintptr_t)&campaign_npc_bodies[0].registration;address=(uintptr_t)registered;
    if(address<first)return 0;delta=address-first;
    if(delta%sizeof(*campaign_npc_bodies)||delta/sizeof(*campaign_npc_bodies)>=campaign_npc_body_count)return 0;
    *slot=(uint32_t)(delta/sizeof(*campaign_npc_bodies));
    return !scene_npc_reload_identity(*slot,campaign_npc_bodies+*slot)&&campaign_npc_bodies[*slot].registration.handle==handle;
}
static uint32_t scene_blind_target_live(uint32_t handle)
{
    uint32_t slot;const campaign_npc_body *v;
    if(!handle||handle==UINT32_MAX)return 0;
    if(handle==campaign_player_object.handle)
        return campaign_player_object.view==&campaign_player_view&&campaign_player_view.linked_handle==-1&&
            rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object&&
            rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view&&
            isfinite(campaign_player_damage.state.effects.health)&&campaign_player_damage.state.effects.health>0;
    if(!scene_blind_target_slot(handle,&slot))return 0;v=campaign_npc_bodies+slot;
    return v->body.allocated_bytes&&isfinite(v->damage.effects.health)&&v->damage.effects.health>0&&
        !(v->view.flags_810&1u)&&scene_npc_reload_standalone(v);
}
/* Qualification of immutable/current owner, not temporary update eligibility.
 * Hidden, frozen, reloading and Stand Ground actors retain their sight age. */
static int scene_blind_owner(uint32_t slot,int32_t weapon,uint32_t retained,float *seconds)
{
    const campaign_npc_body *o;const campaign_model_owner *m;const rf_entity_pose *p;int status;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;o=campaign_npc_bodies+slot;
    status=scene_blind_source(slot,weapon,retained,seconds);if(status)return status;
    if(scene_npc_reload_identity(slot,o)||!scene_npc_reload_standalone(o)||
       o->registration.handle==campaign_player_object.handle||!o->persistence_registered||
       !o->body.allocated_bytes||!isfinite(o->body.state.mass)||o->body.state.mass<=0||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||(o->view.flags_810&1u)||
       scene_npc_seat_attached(o->registration.handle)||
       !campaign_model_owners||slot>=campaign_model_owner_count||!campaign_poses.items||slot>=campaign_poses.count)return RF_NOT_FOUND;
    m=campaign_model_owners+slot;p=campaign_poses.items+slot;
    if(!m->registration.loaded||m->owned||m->pose!=p||m->registration.active!=&p->playback.completion.active||
       !p->matrices||!p->generations||!p->bone_count||p->skeleton!=o->selection.mapping.skeleton||
       !m->registration.next||!m->registration.previous||m->registration.next->previous!=&m->registration||
       m->registration.previous->next!=&m->registration)return RF_NOT_FOUND;
    return RF_OK;
}
/* Called at actual accepted target/alert mutations, never from a capture or
 * candidate path. Same-target reissues retain the original grace. */
static void scene_blind_sync(campaign_npc_body *o,uint32_t frame)
{
    uint32_t target,slot;float seconds;
    if(!o||!campaign_npc_bodies||o<campaign_npc_bodies||o>=campaign_npc_bodies+campaign_npc_body_count)return;
    target=scene_blind_target_handle(o);
    if(o->pursuit_return.phase&&(o->pursuit_return.target!=target||
       o->pursuit_return.combat_mode!=o->combat_scripted)){
        scene_pursuit_token_invalidate_owner(o); /* actual return relation replacement */
        scene_pursuit_return_receipt_clear(&o->pursuit_return);
    }
    /* Close target/order replacement immediately, before capture can observe
     * a new combat relation paired with the old tactical discriminator. */
    if((o->tactical.flags&SCENE_TACTICAL_PRESENT)&&
       (o->tactical.target!=target||
        (o->tactical.flags>>SCENE_TACTICAL_MODE_SHIFT)!=o->combat_scripted)){
        scene_pursuit_token_invalidate_owner(o); /* actual tactical relation replacement */
        if(o->tactical.phase==SCENE_TACTICAL_RELOCATING)campaign_pursuit_stop(o);
        scene_tactical_receipt_release(&o->tactical);
    }
    if(!target||target==UINT32_MAX||target==o->registration.handle){scene_blind_clear(o);return;}
    if(o->pursuit_delay.flags&SCENE_PURSUIT_DELAY_PRESENT){
        if(o->pursuit_delay.target!=target)memset(&o->pursuit_delay,0,sizeof(o->pursuit_delay));
        else o->pursuit_delay.flags=SCENE_PURSUIT_DELAY_PRESENT|(o->combat_scripted<<SCENE_PURSUIT_DELAY_MODE_SHIFT);
    }
    if(o->blind.present==1&&o->blind.target==target)return;
    slot=(uint32_t)(o-campaign_npc_bodies);
    if(scene_blind_owner(slot,o->view.weapons[0],0,&seconds)||!scene_blind_target_live(target)){scene_blind_clear(o);return;}
    o->blind.present=1;o->blind.target=target;o->blind.last_seen=frame;
}
static uint32_t scene_blind_age_limit(float seconds)
{return (uint32_t)((double)seconds*60.0)+2u;}
/* Unconditional receipt bookkeeping before higher-priority dispatch. Never
 * invokes rays, tactical cancellation or acquisition. Rebase a saturated
 * anchor each frame, so suppression cannot turn a uint32 wrap into grace. */
static int scene_blind_age(campaign_npc_body *o,uint32_t frame)
{
    uint32_t slot,cls,age,limit;float seconds;
    if(!o->blind.present)return (o->blind.target||o->blind.last_seen)?RF_FORMAT:RF_OK;
    if(o->blind.present!=1||!o->blind.target||o->blind.target==UINT32_MAX)return RF_FORMAT;
    slot=(uint32_t)(o-campaign_npc_bodies);
    if(slot>=campaign_seeds.records.count||!scene_blind_classes.valid)return RF_FORMAT;
    cls=campaign_seeds.items[slot].class_index;if(cls>=scene_blind_classes.count)return RF_FORMAT;
    seconds=scene_blind_classes.seconds[cls];if(!isfinite(seconds)||seconds<0||seconds>86400.f)return RF_FORMAT;
    age=frame-o->blind.last_seen;limit=scene_blind_age_limit(seconds);
    if(age>limit)o->blind.last_seen=frame-limit;
    return RF_OK;
}
static uint32_t scene_blind_paid(uint32_t handle)
{
    if(scene_ai_tribeam_source_pending(handle)||scene_ai_cane_source_pending(handle)||scene_ai_spit_source_pending(handle))return 1;
    for(uint32_t i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++)
        if(scene_ai_drone_smash_pending[i].phase&&scene_ai_drone_smash_pending[i].source==handle)return 1;
    return 0;
}
static uint32_t scene_blind_release_admits(const campaign_npc_body *o)
{
    /*409080..4090b4: the source target setter can refuse even expiry(-1). */
    if((o->ai_mode.flags_530|o->view.flags_7d0|o->firing.flags_7d0)&0x80u)return 0;
    if(o->ai_mode.action_280==15)return 1;
    return !(o->object_flags&8u)&&scene_npc_reload_standalone(o)&&
        ((o->view.flags_810&0x10000u)||
         (campaign_seeds.classes[campaign_seeds.items[o-campaign_npc_bodies].class_index].physics.flags&0x10u));
}
#include "scene_pursuit_delay_sample_guard.inc"
static int scene_blind_tick(scene_stream *s,uint32_t slot,uint32_t frame)
{
    campaign_npc_body *o=campaign_npc_bodies+slot;float seconds,player_eye[3];
    const float *body=NULL,*eye=NULL;uint32_t target,blocked,expired,hidden_target=0;int status;
    /* First entry to this supported domain may follow dismount or a valid
     * weapon change without another target setter. Only absent state seeds;
     * existing same-target age is never replenished here or in capture. */
    if(!o->blind.present)scene_blind_sync(o,frame);
    if(!o->blind.present)return RF_OK;
    target=scene_blind_target_handle(o);
    if(target!=o->blind.target||!scene_blind_target_live(target)){scene_blind_clear(o);return RF_OK;}
    status=scene_blind_owner(slot,o->view.weapons[0],1,&seconds);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_pursuit_delay_live_sync(slot);if(status)return status;
    if(o->movement_slot>=16||(campaign_modes[o->movement_slot].index!=1&&campaign_modes[o->movement_slot].index!=3))return RF_OK;
    if((o->object_flags&(2u|0x4000u))||campaign_npc_ai_suppressed(o)||
       o->scripted_physics||o->script_animation.freeze||o->ai_mode.action_280==1||
       o->ai_mode.action_280==7||o->ai_mode.action_280==13||o->ai_mode.action_280==14||
       o->flight.entered||scene_guard_retreat_active(o)||o->guard_retreat_fault||o->alarm.present||o->alarm_fault||o->medic.pending||
       scene_noise_active(o)||campaign_cutscene_runtime.active||campaign_endgame.phase)return RF_OK;
    if((frame%30+campaign_enemy_sight_phase_offset)%30==slot%30){
        scene_pursuit_delay_sample_guard sample_guard;float body_delta[3],eye_delta[3];
        if(target==campaign_player_object.handle){
            status=scene_medic_player_eye(frame,player_eye);
            if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
            body=scene_actor_body.state.position;eye=player_eye;
        }else {
            uint32_t target_slot;if(!scene_blind_target_slot(target,&target_slot))return RF_FORMAT;
            body=campaign_npc_bodies[target_slot].body.state.position;eye=campaign_npc_bodies[target_slot].eye_position;
            hidden_target=!!(campaign_npc_bodies[target_slot].object_flags&(2u|0x4000u));
        }
        if(!body||!eye)return RF_FORMAT;
        scene_pursuit_delay_sample_capture(slot,&sample_guard);
        for(uint32_t j=0;j<3;j++){
            body_delta[j]=body[j]-o->eye_position[j];eye_delta[j]=eye[j]-o->eye_position[j];
            if(!isfinite(body_delta[j])||!isfinite(eye_delta[j]))return RF_FORMAT;
        }
        blocked=1;
        if(!hidden_target){
            status=combat_obstructed(s,sample_guard.eye,body_delta,&blocked);if(status)return status;
            status=scene_pursuit_delay_sample_valid(&sample_guard);if(status)return status;
            if(blocked){
                status=combat_obstructed(s,sample_guard.eye,eye_delta,&blocked);if(status)return status;
                status=scene_pursuit_delay_sample_valid(&sample_guard);if(status)return status;
            }
        }
        if(o->pursuit_delay.flags&SCENE_PURSUIT_DELAY_PRESENT){
            scene_pursuit_sight_clock next=o->pursuit_delay.sight;
            status=scene_pursuit_sight_sample(&next,scene_patrol_now(frame),!blocked);if(status)return status;
            o->pursuit_delay.sight=next;
        }
        if(!blocked)o->blind.last_seen=frame;
    }
    status=rf_entity_ai_blind_pursuit_expired((float)((double)(frame-o->blind.last_seen)/60.0),0,seconds,0,0,&expired);
    if(status||!expired)return status;
    if(!scene_blind_release_admits(o)||scene_blind_paid(o->registration.handle))return RF_OK;
    scene_pursuit_token_invalidate_owner(o); /* admitted Blind target expiry */
    if(o->script_move.active&&o->script_move.follow==2&&o->script_move.origin==SCENE_MOVE_ORIGIN_NONE)
        campaign_pursuit_stop(o);
    o->combat_target=o->combat_alert=o->combat_scripted=0;
    o->combat_burst_remaining=o->combat_due=o->combat_navigation_due=0;scene_blind_clear(o);
    return scene_patrol_return(o,scene_patrol_now(frame));
}
#endif
