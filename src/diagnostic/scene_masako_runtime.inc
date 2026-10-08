#ifndef RF_SCENE_MASAKO_RUNTIME_INCLUDED
#define RF_SCENE_MASAKO_RUNTIME_INCLUDED
/* The named boss path is a linked actor handoff, not a second fighter AI.
 * 464700 creates masako_endgame; 4204a1 transfers the dying fighter's pose,
 * unhides the child, targets the player and kills all fighter-class actors.
 * 41f012 enables trigger4490 and starts event18371 after the child's death
 * animation. Shared NPC movement, weapons, armor/shield and death remain
 * authoritative. Include after the vehicle and NPC ownership services. */
static campaign_npc_body *scene_masako_actor(void)
{
    if(!scene_masako.enabled||!campaign_npc_bodies||scene_masako.actor_slot>=campaign_npc_body_count||
       scene_masako.actor_slot>=campaign_seeds.records.count||
       campaign_seeds.records.items[scene_masako.actor_slot].record.uid!=SCENE_MASAKO_GENERATED_UID||
       strcmp(campaign_seeds.records.items[scene_masako.actor_slot].record.script_name,"masako_endgame")||
       campaign_seeds.items[scene_masako.actor_slot].class_index!=scene_masako.class_index)return NULL;
    return campaign_npc_bodies+scene_masako.actor_slot;
}
static scene_passive_vehicle *scene_masako_parent(void)
{
    if(!scene_masako.enabled)return NULL;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        scene_passive_vehicle *p=campaign_passive_vehicles+i;
        if(p->uid==scene_masako.parent_uid&&p->resource_kind==2&&p->object_kind==11&&
           rf_object_registry_lookup(&campaign_registry,p->handle)==p)return p;
    }
    return NULL;
}
static int scene_masako_live_valid(void)
{
    campaign_npc_body *a=scene_masako_actor();scene_passive_vehicle *p=scene_masako_parent();
    if(!scene_masako.enabled)return RF_OK;
    if(!scene_masako.ready||!a||!p||scene_masako.phase>SCENE_MASAKO_FINISHED||scene_masako.end_effects>3)return RF_FORMAT;
    if(scene_masako.phase==SCENE_MASAKO_DORMANT){
        if(p->damage.destroyed||p->damage.state.effects.health<=0||a->damage.effects.health<=0||!(a->object_flags&0x4000u))return RF_FORMAT;
    }else{
        if(!p->damage.destroyed||p->damage.state.effects.health>0)return RF_FORMAT;
        if(scene_masako.phase==SCENE_MASAKO_ACTIVE&&(a->damage.effects.health<=0||(a->object_flags&(2u|0x4000u))))return RF_FORMAT;
        if(scene_masako.phase>=SCENE_MASAKO_DYING&&a->damage.effects.health>0)return RF_FORMAT;
    }
    if((scene_masako.phase<SCENE_MASAKO_DYING&&scene_masako.end_effects)||
       (scene_masako.phase==SCENE_MASAKO_FINISHED&&scene_masako.end_effects!=3))return RF_FORMAT;
    return RF_OK;
}
static int scene_masako_runtime_initialize(void)
{
    campaign_npc_body *a;if(!scene_masako.enabled)return RF_OK;
    a=scene_masako_actor();if(!a||!scene_masako_parent())return RF_NOT_FOUND;
    if(a->registration.view){
        if(a->registration.view!=&a->view||rf_object_registry_lookup(&campaign_registry,a->registration.handle)!=&a->registration)return RF_FORMAT;
        /* Original secondary814 OR10. Do not clear the independently saved
         * nano-shield-disable bit20 or overwrite armor and class defaults. */
        a->view.flags_814|=0x10u;a->damage.effects.flags_814|=0x10u;
        scene_masako.phase=a->damage.effects.health<=0?SCENE_MASAKO_DYING:
            (a->object_flags&0x4000u)?SCENE_MASAKO_DORMANT:SCENE_MASAKO_ACTIVE;
    }else if(a->persistence_registered&&rf_scene_defeated_actors.items[a->persistence_slot].retired){
        scene_masako.phase=SCENE_MASAKO_FINISHED;scene_masako.end_effects=3;
    }else return RF_FORMAT;
    scene_masako.ready=1;rf_scene_masako_phase[3]=a->registration.handle;
    rf_scene_masako_phase[4]=scene_masako.phase;
    memcpy(rf_scene_masako_phase+10,&a->damage.effects.class_health,4);
    memcpy(rf_scene_masako_phase+11,&a->damage.effects.class_armor,4);
    return RF_OK;
}
static int scene_masako_vehicle_destroy(uint32_t handle)
{
    campaign_npc_body *a;scene_passive_vehicle *p;rf_physics_body_state body;
    uint32_t flags,slot,handled;float published[3],angles[3]={0};int status;
    if(!scene_masako.enabled||(p=scene_masako_parent())==NULL||p->handle!=handle)return RF_OK;
    if(scene_masako.phase!=SCENE_MASAKO_DORMANT)return RF_OK;
    a=scene_masako_actor();slot=scene_masako.actor_slot;
    if(!scene_masako.ready||!a||!a->registration.view||a->registration.view!=&a->view||
       a->damage.effects.health<=0||!a->body.allocated_bytes||slot>=campaign_model_owner_count||
       rf_object_registry_lookup(&campaign_registry,a->registration.handle)!=&a->registration)return RF_NOT_FOUND;
    /* Prepare all fallible pose math before publishing the original handoff.
     * This copies the real dying host pose; no replacement spawn/height. */
    body=a->body.state;flags=a->object_flags&~0x4000u;
    memcpy(body.position,p->pose.position,12);memcpy(body.next_position,p->pose.position,12);
    memcpy(body.orientation,p->pose.input_matrix,36);memcpy(body.next_orientation,p->pose.input_matrix,36);
    status=rf_physics_tensor_world(body.local_tensor,body.orientation,body.world_tensor);if(status)return status;
    status=rf_physics_publish_position(&body,published,&flags);if(status)return status;
    {float horizontal=sqrtf(body.orientation[6]*body.orientation[6]+body.orientation[8]*body.orientation[8]);
     float span=horizontal+fabsf(body.orientation[7]);if(!isfinite(span)||span<=0)return RF_FORMAT;
     angles[0]=asinf(fmaxf(-1,fminf(1,body.orientation[7]/span)));angles[1]=atan2f(body.orientation[6],body.orientation[8]);}
    a->body.state=body;a->object_flags=flags;a->view.flags_7c=a->room.flags=flags;
    memcpy(a->previous,published,12);memcpy(a->published,published,12);
    memcpy(a->look.body_angles,angles,12);memset(a->look.angles.angles_87c,0,12);
    memcpy(a->look.orientation,body.orientation,36);memcpy(campaign_model_owners[slot].basis,body.orientation,36);
    memcpy(campaign_model_owners[slot].position,published,12);
    a->support.handle=0;a->support.material=-1;a->body.state.flags&=~0x400000u;
    memset(a->support_velocity,0,12);memset(&a->piece_support,0,sizeof(a->piece_support));a->piece_reacquire=0;
    status=rf_scene_npc_publish_position(a->registration.handle);if(status)return status;
    status=rf_scene_npc_refresh_room(a->registration.handle);if(status)return status;
    status=campaign_npc_eye_update(slot);if(status)return status;
    /*409050 +408ac0: ordinary player-directed combat, no fabricated Attack
     * event and no new weapon grants. Constructor already supplied SAR. */
    if(campaign_player_object.view==&campaign_player_view){
        a->combat_alert=1;a->combat_target=campaign_player_object.handle;a->combat_scripted=0;
        a->combat_due=combat_frame==UINT32_MAX?0:combat_frame;a->combat_navigation_due=0;
    }
    /* Commit before visiting fighters so a recursive death callback cannot
     * reveal this actor twice. 40a210 is exactly class724 bit800. */
    scene_masako.phase=SCENE_MASAKO_ACTIVE;++rf_scene_masako_phase[5];
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        scene_passive_vehicle *f=campaign_passive_vehicles+i;
        if(!(f->damage.class_flags&0x800u))continue;
        f->damage.state.effects.health=0;f->damage.destroyed=1;
        scene_vehicle_attack_cancel(f->handle);scene_vehicle_ai_cancel(f->handle);
        /* These static fighter classes have no replacement death model or
         * skeletal death action. 418f80 removes the old solid owner; retain
         * only its registered tombstone for ordinary When_Dead/save identity. */
        if(f->attached){status=campaign_passive_vehicle_detach(NULL,f->handle);if(status)return status;}
        status=scene_secondary_vehicle_remove(f->handle,&handled);if(status)return status;
        f->damage.object_flags|=2u|0x4000u;f->pose.flags|=2u|0x4000u;
        memset(f->velocity,0,12);memset(f->pose.velocity,0,12);f->motion_valid=0;
    }
    /*420597..4205b5 also kills the player's current vehicle, even a non-
     * fighter. The port retains its existing clearance-safe ejection owner. */
    if(scene_driller_damage_runtime_valid(scene_driller_damage_owner)){
        scene_driller_damage_runtime *v=scene_driller_damage_owner;
        if((v->damage.class_flags&0x800u)||v->entry->session.active){
            if(v->entry->session.active)v->damage.state.effects.flags_814|=0x800u;
            v->damage.state.effects.health=0;v->damage.destroyed=1;v->damage.destruction_pending=1;
            v->entry->host.alive=0;v->eject_pending=v->entry->session.active;scene_vehicle_ai_cancel(v->entry->host.handle);
        }
    }
    rf_scene_masako_phase[4]=scene_masako.phase;return RF_OK;
}
static int scene_masako_runtime_tick(scene_stream *stream)
{
    campaign_npc_body *a;scene_passive_vehicle *p;rf_entity_pose *pose;int status;
    if(!scene_masako.enabled||!scene_masako.ready)return RF_OK;
    a=scene_masako_actor();p=scene_masako_parent();if(!a||!p)return RF_NOT_FOUND;
    if(scene_masako.phase==SCENE_MASAKO_DORMANT){
        if(p->damage.destroyed||p->damage.state.effects.health<=0)return scene_masako_vehicle_destroy(p->handle);
        return RF_OK;
    }
    if(scene_masako.phase==SCENE_MASAKO_FINISHED)return RF_OK;
    if(a->damage.effects.health>0)return RF_OK;
    if(scene_masako.phase==SCENE_MASAKO_ACTIVE){scene_masako.phase=SCENE_MASAKO_DYING;++rf_scene_masako_phase[6];}
    if(!(a->view.flags_810&1u)){
        uint32_t entered=0;
        status=rf_scene_npc_death_entry(a->registration.handle,&entered);if(status)return status;
        if(entered){a->script_move.active=0;status=combat_death_start(scene_masako.actor_slot);if(status)return status;}
    }
    /* Completed or absent action permits original41ee40 finalization. A
     * transferred corpse owns the live playback; do not read its old slot. */
    status=campaign_model_pose(scene_masako.actor_slot,&pose);if(status)return status;
    if(pose&&a->death.action_824>=0){
        rf_entity_playback_model *model;int active=0;
        if(pose->skeleton>=campaign_playback_resources.model_count)return RF_FORMAT;
        model=campaign_playback_resources.models+pose->skeleton;
        status=rf_motion_action_active(&pose->playback,model->resources,model->count,
            a->selection.mapping.actions,a->death.action_824,&active);if(status)return status;
        if(active)return RF_OK;
    }
    /*4c0200 clears DISABLED, it does not activate this spatial trigger. */
    if(!(scene_masako.end_effects&1u)){
        for(uint32_t i=0;i<campaign_triggers.count;i++)if(campaign_triggers.items[i].authored->record.uid==4490){
            campaign_triggers.items[i].state.flags&=~16u;++rf_scene_masako_phase[7];break;
        }
        scene_masako.end_effects|=1u;
    }
    if(!(scene_masako.end_effects&2u)){
        for(uint32_t i=0;i<campaign_events.count;i++)if(campaign_events.items[i].authored->record.uid==18371){
            rf_startup_events_report report;
            if(campaign_events.items[i].state.type!=41)return RF_FORMAT;
            /* Mark before dispatch: side effects can commit before an error,
             * and must never replay across a retry or an ordinary save. */
            scene_masako.end_effects|=2u;
            status=rf_runtime_event_fire(&campaign_triggers,campaign_events.items[i].handle,UINT32_MAX,UINT32_MAX,
                campaign_blackout_now,&scene_gravity,&stream->particles,&campaign_forces,&report);
            if(status){rf_scene_masako_phase[9]=(uint32_t)status;return status;}
            ++rf_scene_masako_phase[8];break;
        }
        scene_masako.end_effects|=2u;
    }
    scene_masako.phase=SCENE_MASAKO_FINISHED;rf_scene_masako_phase[4]=scene_masako.phase;return RF_OK;
}
#endif
