#ifndef RF_SCENE_GUARD_RETREAT_INCLUDED
#define RF_SCENE_GUARD_RETREAT_INCLUDED
/* Selected guard1/owned12mm/Direct3, actual surviving player damage.
 *407fb0/4085f0 onset ordering; the approved finite-prefix and ordinary port
 * return deliberately replace original408ac0/406cc0, perception and peers. */
static uint32_t scene_guard_retreat_active(const campaign_npc_body *o)
{return o&&o->flight.qualified==2;}
static int scene_guard_retreat_player_onfoot(scene_stream *s)
{
    scene_npc_reload_player_receipt receipt;
    return scene_npc_reload_player_admit(s,&receipt);
}
static uint32_t scene_guard_retreat_canonical(const campaign_npc_body *o)
{
    return o->ai_override.action_1364==-1&&o->ai_override.motion_1368==-1&&o->ai_override.word_834==-1&&
        !((o->ai_mode.flags_530|o->view.flags_7d0|o->firing.flags_7d0)&8u)&&
        !((o->view.flags_810|o->damage.effects.flags_810)&0x02000000u);
}
static int scene_guard_retreat_authored(uint32_t index)
{
    const rf_level_owned_entity *record;rf_level_entity_spawn spawn;rf_level_entity_ai ai;
    const char *name;const uint8_t *raw;uint32_t at=4,size;int status,style;
    static const int32_t overrides[4]={-1,1,3,2};
    if(!campaign_seeds.items||!campaign_seeds.records.items||index>=campaign_seeds.records.count)return RF_RANGE;
    record=campaign_seeds.records.items+index;name=record->record.class_name;
    if(rf_emitter_name_lookup(&name,1,"guard1")!=0)return RF_NOT_FOUND;
    if(!scene_guard_retreat_class.valid||campaign_seeds.items[index].class_index!=scene_guard_retreat_class.cls)return RF_FORMAT;
    status=rf_level_entity_spawn_read(record,&spawn);if(status)return status;
    status=rf_level_entity_ai_read(record,&ai);if(status)return status;
    if(ai.attack_style>=4||ai.mode!=1||spawn.seat_host_uid!=-1)return RF_NOT_FOUND;
    style=overrides[ai.attack_style];if(style<0)style=scene_guard_retreat_class.style;
    if(style!=3)return RF_NOT_FOUND;
    raw=record->raw;size=record->record.bytes;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<48)return RF_FORMAT;at+=48;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<13)return RF_FORMAT;at+=13;
    for(uint32_t i=0;i<2;i++){status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;}
    if(at>size||size-at<29)return RF_FORMAT;at+=29;
    for(uint32_t i=0;i<7;i++){status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;}
    if(at>size||size-at<35)return RF_FORMAT;
    /* Authored Alarm UID-1 and the same source creation exclusion as medic.
     * The full parser has already checked complete raw-record exhaustion. */
    for(uint32_t i=14;i<18;i++)if(raw[at+i]!=255)return RF_NOT_FOUND;
    if(raw[at+18+11])return RF_NOT_FOUND;
    return RF_OK;
}
static int scene_guard_retreat_binding(uint32_t index,int32_t primary,int32_t secondary,
    const rf_weapon_inventory *inventory,const rf_entity_motion_selection *selection,uint32_t resident)
{
    uint32_t span,cls;const char *name;int status;
    status=scene_guard_retreat_authored(index);if(status)return status;
    if(primary<0||primary>=64||primary!=campaign_pistol_id||secondary!=-1||!inventory||inventory->owned[primary]!=1)return RF_NOT_FOUND;
    cls=campaign_seeds.items[index].class_index;
    if(cls>=campaign_seeds.class_count)return RF_FORMAT;
    name=campaign_seeds.classes[cls].model;
    if(!memchr(name,0,sizeof(campaign_seeds.classes[cls].model)))return RF_FORMAT;
    if(campaign_seeds.classes[cls].model_kind!=2||rf_emitter_name_lookup(&name,1,"ult2_guard.vcm")!=0||
       (campaign_seeds.classes[cls].physics.flags2&0x100u)||
       (campaign_seeds.classes[cls].physics.flags&0x20010u)!=0x20010u||
       (campaign_seeds.classes[cls].physics.flags&(0x200u|0x400000u)))return RF_NOT_FOUND;
    return scene_npc_reload_binding(index,cls,primary,inventory,selection,resident,&span);
}
/* Whole resource demand is checked before the first onset/reset/RNG effect.
 * Exact mappings and active/controller contributions, not guessed filenames.
 * The existing cache budget stays unchanged and no borrower is fabricated. */
static int scene_guard_retreat_resources(uint32_t index,const rf_entity_motion_selection *selection,
    uint32_t demand)
{
    int32_t ids[28];uint32_t count=0,unique=0,cache[28],skeleton=selection->mapping.skeleton;
    const rf_entity_pose *pose;const rf_entity_playback_model *model;uint64_t missing=0;int status;
    if(index>=campaign_poses.count||!campaign_poses.items||skeleton>=campaign_playback_resources.model_count||
       skeleton>=campaign_motion_catalog.model_count)return RF_FORMAT;
    pose=campaign_poses.items+index;model=campaign_playback_resources.models+skeleton;
    if(pose->skeleton!=skeleton||!model->resources||!model->cache_ids||
       !campaign_motion_catalog.models[skeleton].items||pose->playback.completion.active.count>16||
       pose->playback.completion.frozen||pose->playback.completion.active.freeze_slot!=-1)return RF_NOT_FOUND;
    ids[count++]=selection->mapping.states[0];ids[count++]=selection->mapping.states[2];
    ids[count++]=selection->mapping.states[4];ids[count++]=selection->mapping.states[6];
    ids[count++]=selection->mapping.states[13];ids[count++]=selection->mapping.actions[2];ids[count++]=selection->mapping.actions[39];
    for(uint32_t j=0;j<3;j++){
        int32_t state=j==0?pose->controller.current:j==1?pose->controller.next:
            pose->controller.override_enabled?pose->controller.override_state:-1;
        if(state< -1||state>=23)return RF_FORMAT;if(state>=0)ids[count++]=selection->mapping.states[state];
    }
    for(uint32_t j=0;j<pose->playback.completion.active.count;j++)ids[count++]=pose->playback.completion.active.slots[j].motion;
    /* A movement controller may retain both source clips while crossfading. */
    {uint32_t slots=pose->playback.completion.active.count;
     for(uint32_t k=3;k<=4;k++){
        uint32_t j;for(j=0;j<pose->playback.completion.active.count;j++)
            if(pose->playback.completion.active.slots[j].motion==ids[k])break;
        if(j==pose->playback.completion.active.count&&(k==3||ids[k]!=ids[3]))++slots;
     }
     if(slots>16)return RF_NOT_FOUND;}
    for(uint32_t j=0;j<count;j++){
        int32_t id=ids[j];uint32_t c,k;const rf_entity_model_motion *motion;
        if(id<0||(uint32_t)id>=model->count||(uint32_t)id>=campaign_motion_catalog.models[skeleton].count)return RF_NOT_FOUND;
        c=model->cache_ids[id];motion=campaign_motion_catalog.models[skeleton].items+id;
        if(c>=campaign_npc_motion_count||!campaign_npc_motion_data||!campaign_npc_motion_sizes||
           !motion->file.archive||!motion->file.entry.size||model->resources[id].references<0||
           model->resources[id].looping!=motion->looping||
           memcmp(&model->resources[id].comparison,&motion->comparison,sizeof(motion->comparison)))return RF_FORMAT;
        if(model->resources[id].references==INT32_MAX){
            for(k=0;k<pose->playback.completion.active.count;k++)
                if(pose->playback.completion.active.slots[k].motion==id)break;
            if(k==pose->playback.completion.active.count)return RF_NOT_FOUND;
        }
        for(k=0;k<unique;k++)if(cache[k]==c)break;
        if(k==unique){cache[unique++]=c;if(!campaign_npc_motion_data[c])missing+=motion->file.entry.size;}
        if(campaign_npc_motion_data[c]&&campaign_npc_motion_sizes[c]!=motion->file.entry.size)return RF_FORMAT;
        if(!demand&&(!campaign_npc_motion_data[c]||motion->file.resident!=campaign_npc_motion_data[c]))return RF_NOT_FOUND;
    }
    if((uint64_t)campaign_npc_motion_bytes+missing>SCENE_NPC_MOTION_BUDGET)return RF_NOT_FOUND;
    if(demand)for(uint32_t j=0;j<count;j++){
        status=campaign_npc_motion_require(skeleton,(uint32_t)ids[j]);if(status)return status;
    }
    return RF_OK;
}
static int scene_guard_retreat_geometry(scene_stream *s,const campaign_npc_body *o)
{
    const rf_level_navigation_workspace *w=&campaign_navigation_workspace;
    if(!s||!s->collision||!campaign_navigation.count||campaign_navigation.count>65534||
       w->count!=campaign_navigation.count||!w->storage||!w->references||!w->scratch||
       !campaign_navigation.nodes||!campaign_navigation.references||scene_flight_search_busy)return RF_NOT_FOUND;
    if((s->collision->room_count&&!s->collision->views)||(s->collision->primary_count&&!s->collision->primary)||
       (s->collision->child_count&&!s->collision->children)||
       !scene_flight_finite3(o->published)||!scene_flight_finite3(o->eye_position)||
       !isfinite(o->navigation_radius)||o->navigation_radius<0||
       !isfinite(o->death_item_extent_7c4)||o->death_item_extent_7c4<0)return RF_FORMAT;
    for(uint32_t j=0;j<w->count;j++){
        const rf_entity_navigation_candidate *n=&campaign_navigation.nodes[j].candidate;
        if(w->references[j].candidate!=n||campaign_navigation.references[j].candidate!=n||
           w->references[j].order_key!=j+1||!scene_flight_finite3(n->position)||
           !isfinite(n->radius)||n->radius<0||!isfinite(n->height)||n->height<0)return RF_FORMAT;
    }
    return RF_OK;
}
static uint32_t scene_guard_retreat_overlap(const campaign_npc_body *o,uint32_t entering)
{
    uint32_t pursuit=entering&&o->script_move.follow==2&&o->script_move.origin==SCENE_MOVE_ORIGIN_NONE;
    return o->patrol.enabled||(!pursuit&&(o->script_move.active||o->navigation.retained.count))||
        o->script_move.path.count||o->script_move.path.indices||o->script_move.origin||
        o->script_look.active||o->script_shoot.active||o->script_once.count||o->script_animation.active||
        o->combat_scripted||o->controller_handle||!scene_npc_reload_standalone(o)||
        o->ai_mode.state_2b4!=0||o->medic.pending||o->scripted_physics||o->scripted_physics_seen||
        o->ai_override.action_1364!=-1||o->ai_override.motion_1368!=-1||o->ai_override.word_834!=-1||
        (o->flight.qualified&&o->flight.qualified!=2)||
        (!o->combat_reload_pending&&o->combat_reload_due)||
        (o->combat_alert&&(o->combat_alert!=1||o->combat_target!=campaign_player_object.handle));
}
/* Entry must not spend its lottery on a playback/physical owner that the
 * existing composed checkpoint cannot represent. This does not demand a
 * globally quiet world or relax post-return ordinary snapshot guards. */
static int scene_guard_retreat_save_owners(uint32_t index,const campaign_npc_body *o,int32_t now,uint32_t frame)
{
    const rf_entity_pose *pose=campaign_poses.items+index;
    const rf_entity_playback_model *model=campaign_playback_resources.models+pose->skeleton;
    rf_npc_checkpoint_pain pain={0};int status,pending;
    if(o->damage.effects.burn||o->death_item_fault||o->script_animation.freeze||
       o->look.body_angles[0]!=0||o->look.body_angles[2]!=0||
       o->movement.mode<0||o->movement.mode>2||o->movement_orientation!=campaign_identity[0]||
       (o->view.flags_810&0x400u)||o->body.spheres.count>8||
       (o->body.spheres.count&&!o->body.spheres.items)||
       o->combat_burst_remaining>256||
       (o->combat_due>frame&&o->combat_due-frame>3600)||
       (o->combat_navigation_due>frame&&o->combat_navigation_due-frame>3600))return RF_NOT_FOUND;
    for(uint32_t j=0;j<3;j++)if(!isfinite(o->body.state.velocity[j])||fabsf(o->body.state.velocity[j])>.001f)
        return RF_NOT_FOUND; /* No new airborne profile is granted by entry. */
    for(uint32_t j=0;j<64;j++)if(o->firing.active[j])return RF_NOT_FOUND;
    status=scene_npc_reload_live(index,frame,NULL);if(status)return status;
    status=scene_npc_pain_checkpoint_capture(index,o,pose,now,&pain);if(status)return RF_NOT_FOUND;
    status=rf_timer_pending(o->unholster.deadline_518,now,&pending);if(status)return status;if(pending)return RF_NOT_FOUND;
    status=rf_timer_pending(o->death.deadline_4b8,now,&pending);if(status)return status;if(pending)return RF_NOT_FOUND;
    if(!pose->playback.completion.active.count&&(pose->controller.override_enabled||pose->controller.duration!=0||
       pose->controller.current>0||pose->controller.next>0))return RF_NOT_FOUND;
    for(uint32_t j=0;j<pose->playback.completion.active.count;j++){
        int32_t id=pose->playback.completion.active.slots[j].motion;
        if(model->resources[id].looping||scene_npc_pain_checkpoint_clip(&pain,&o->selection,id))continue;
        if(scene_npc_checkpoint_residual_weapon_clip(index,campaign_seeds.items[index].class_index,
            o->view.weapons[0],&o->inventory,&o->selection,&pose->playback,id,1,!!(o->crouch.flags&SCENE_CROUCH_PRESENT)))continue;
        if(pain.present&&scene_npc_pain_checkpoint_residual(index,campaign_seeds.items[index].class_index,
            o->view.weapons[0],&o->selection,&pose->playback,id,1))continue;
        return RF_NOT_FOUND;
    }
    return RF_OK;
}
static int scene_guard_retreat_cancel(campaign_npc_body *o,int32_t now,uint32_t disposition)
{
    rf_entity_ai_transition_state next;int status;
    if(!o||disposition>SCENE_GUARD_CANCEL_PRESERVE_INCOMING||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(scene_medic_flight_restoring)return RF_OK;
    if(o->guard_retreat_fault)return RF_FORMAT;
    scene_noise_cancel(o);
    scene_patrol_dwell_clear(o); /* Accepted command supersedes the finite node hold. */
    if(!scene_guard_retreat_active(o))return scene_civilian_flight_cancel(o,now,disposition);
    scene_pursuit_token_invalidate_owner(o); /* accepted active cancellation */
    o->guard_retreat_fault=1;
    if(!o->flight.entered||(o->guard_resume_action!=0&&o->guard_resume_action!=2)||
       o->ai_mode.action_280!=o->view.action_520)return RF_FORMAT;
    if(disposition==SCENE_GUARD_CANCEL_RESTORE_ORDINARY){
        if(o->ai_mode.action_280!=7)return RF_FORMAT;
        next=o->ai_mode;
        status=rf_entity_ai_set_action(&next,o->guard_resume_action,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
        /* This helper runs before an incoming route/command commits. No
         * authored path is stolen and no independent reload/pain is reset. */
        memset(&o->navigation,0,sizeof(o->navigation));
        memset(o->command_714,0,12);memset(o->look.command_708,0,12);
        o->ai_mode=next;o->view.action_520=next.action_280;
    }else if(o->ai_mode.action_280==7)return RF_FORMAT;
    /* PRESERVE_INCOMING leaves every newly owned action/route/command bank.
     * Only the typed references inside this obsolete local episode retire. */
    memset(&o->flight,0,sizeof(o->flight));o->guard_resume_action=0;
    o->guard_retreat_fault=0;return RF_OK;
}
static int scene_guard_retreat_return(campaign_npc_body *o,int32_t now)
{
    uint32_t target=o->flight.target_handle;int status;
    status=scene_guard_retreat_cancel(o,now,SCENE_GUARD_CANCEL_RESTORE_ORDINARY);if(status)return status;
    /* Existing target-liveness policy releases a missing/dead player. Never
     * retarget a reused handle, start a shot, or renew the30-frame deadline. */
    if(target!=campaign_player_object.handle||!scene_flight_owner_live(o)||
       campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,target)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)target)!=&campaign_player_view||
       !campaign_enemy_target_living(o,target,campaign_player_damage.state.effects.health)){
        campaign_pursuit_stop(o);o->combat_alert=o->combat_target=o->combat_scripted=0;scene_blind_clear(o);
        o->combat_due=o->combat_navigation_due=o->combat_burst_remaining=0;
    }
    return RF_OK;
}
static int scene_guard_retreat_cancel_player(int32_t now)
{
    int status;
    if(scene_medic_flight_restoring)return RF_OK;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->guard_retreat_fault)return RF_FORMAT;
        scene_noise_cancel(o);
        if(!scene_guard_retreat_active(o))continue;
        status=scene_guard_retreat_cancel(o,now,SCENE_GUARD_CANCEL_RESTORE_ORDINARY);if(status)return status;
    }
    return RF_OK;
}
static int scene_guard_retreat_live(uint32_t index,uint32_t frame)
{
    const campaign_npc_body *o=campaign_npc_bodies+index;scene_medic_flight_nav nav;scene_medic_flight_start start;int status;
    if(o->guard_retreat_fault)return RF_FORMAT;
    if(!scene_guard_retreat_active(o))return RF_OK;
    if(!o->flight.entered||o->ai_mode.action_280!=7||o->view.action_520!=7||
       (o->guard_resume_action!=0&&o->guard_resume_action!=2)||!scene_guard_retreat_canonical(o)||
       o->flight.target_kind!=SCENE_FLIGHT_TARGET_PLAYER||o->flight.target_handle!=campaign_player_object.handle||
       o->flight.sight_deadline_294!=-1||o->flight.no_route_deadline_4d4!=-1||o->flight.stall_deadline_4d0!=-1||
       o->flight.visible_published_29c||o->flight.visible_eye_29d||o->flight.deadline_250<0||o->flight.reset_deadline_1370<0||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||
       !isfinite(campaign_player_damage.state.effects.health)||campaign_player_damage.state.effects.health<=0||
       (o->view.flags_810&1u)||(o->object_flags&(2u|0x4000u))||scene_guard_retreat_overlap(o,0)||
       o->combat_alert!=1||o->combat_target!=o->flight.target_handle||o->combat_burst_remaining>256||
       (o->combat_due>frame&&o->combat_due-frame>3600)||
       (o->combat_navigation_due>frame&&o->combat_navigation_due-frame>3600))return RF_FORMAT;
    status=scene_guard_retreat_binding(index,o->view.weapons[0],o->view.weapons[1],&o->inventory,&o->selection,0);if(status)return status;
    status=scene_npc_reload_live(index,frame,NULL);if(status)return status;
    status=scene_medic_flight_nav_capture(o,&nav,&start);if(status)return status;
    if(nav.count<2||nav.count>4||nav.previous<0||nav.current!=nav.previous+1||
       (uint32_t)nav.current>=nav.count||!nav.start_valid||!nav.eye_valid||
       memcmp(nav.pair,nav.prefix+nav.previous,sizeof(nav.pair)))return RF_FORMAT;
    return scene_guard_retreat_player_onfoot(scene_actor_collision_owner);
}
static int scene_guard_retreat_live_present(uint32_t frame,uint32_t *present)
{
    int status;if(!present||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    *present=0;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        status=scene_guard_retreat_live(i,frame);if(status)return status;
        *present|=scene_guard_retreat_active(campaign_npc_bodies+i);
    }
    return RF_OK;
}
static uint32_t scene_guard_retreat_section_wait(void)
{
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if(scene_guard_retreat_active(campaign_npc_bodies+i)||campaign_npc_bodies[i].alarm.present||campaign_npc_bodies[i].alarm_fault)return 1;
    return 0;
}
static uint32_t scene_guard_retreat_same_owner(const campaign_npc_body *o,uint32_t index,uint32_t handle,
    uint64_t epoch,uint64_t lifetime)
{
    return epoch==scene_continuous_charge_runtime.epoch&&lifetime==scene_medic_flight_lifetime&&
        campaign_npc_bodies&&index<campaign_npc_body_count&&o==campaign_npc_bodies+index&&
        o->registration.handle==handle&&scene_flight_owner_live(o);
}
static int scene_guard_retreat_damage(campaign_npc_body *o,uint32_t source,int32_t now,uint32_t *handled)
{
    scene_medic_flight_threat_view threat={0};uint32_t index,handle,found=0,frame;
    uint64_t epoch,lifetime;int status;int32_t duration,remaining;float seconds;
    if(!o||!handled)return RF_RANGE;*handled=0;
    if(scene_medic_flight_restoring)return RF_OK;
    if(o->guard_retreat_fault)return RF_FORMAT;
    if(source!=campaign_player_object.handle){
        if(scene_guard_retreat_active(o))return scene_guard_retreat_cancel(o,now,SCENE_GUARD_CANCEL_RESTORE_ORDINARY);
        return RF_OK;
    }
    index=(uint32_t)(o-campaign_npc_bodies);frame=combat_frame==UINT32_MAX?0:combat_frame;
    if(campaign_player_object.view!=&campaign_player_view||source==o->registration.handle||
       rf_object_registry_lookup(&campaign_registry,source)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)source)!=&campaign_player_view||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||
       !isfinite(campaign_player_damage.state.effects.health)||campaign_player_damage.state.effects.health<=0||
       (o->object_flags&(2u|0x4000u))||(o->view.flags_810&1u))return RF_OK;
    if((!scene_guard_retreat_active(o)&&(o->ai_mode.action_280!=0&&o->ai_mode.action_280!=2))||
       o->view.action_520!=o->ai_mode.action_280||scene_guard_retreat_overlap(o,1)||
       o->movement_slot>=16||(campaign_modes[o->movement_slot].index!=1&&campaign_modes[o->movement_slot].index!=3)||
       frame>UINT32_MAX-30)return RF_OK;
    status=scene_guard_retreat_binding(index,o->view.weapons[0],o->view.weapons[1],&o->inventory,&o->selection,0);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_guard_retreat_player_onfoot(scene_actor_collision_owner);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_guard_retreat_geometry(scene_actor_collision_owner,o);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    threat.handle=source;threat.view=&campaign_player_view;memcpy(threat.position,rf_scene_actor_pose.public_position,12);
    status=scene_medic_player_eye(frame,threat.eye);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    threat.model_radius=campaign_player_geometry.model_radius;
    status=scene_guard_retreat_resources(index,&o->selection,1);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_npc_reload_playback(index);if(status)return status;
    status=scene_guard_retreat_save_owners(index,o,now,frame);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    handle=o->registration.handle;epoch=scene_continuous_charge_runtime.epoch;lifetime=scene_medic_flight_lifetime;
    scene_pursuit_token_invalidate_owner(o); /* accepted reset/lottery episode */
    *handled=1;o->guard_retreat_fault=1;
    /*40805c primary,408068 secondary (which itself ends in primary), then
     * speech stop. Every actual synchronous callback retains lifetime checks. */
    status=rf_scene_npc_reset_ai_animation(handle,0);if(status)return status;
    if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND;
    status=rf_scene_npc_reset_ai_animation(handle,1);if(status)return status;
    if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND;
    for(uint32_t i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++){
        scene_message_voice *v=campaign_message_voices+i;
        if(v->route==SCENE_MESSAGE_ACTOR&&v->key==(uint32_t)campaign_seeds.records.items[index].record.uid&&
           campaign_message_active(v))campaign_message_release(v);
        if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND;
    }
    if(scene_medic_flight_random()%4u){
        /* Lottery-only still publishes the independent shared CRT receipt. */
        o->guard_retreat_fault=0;
        if(scene_guard_retreat_active(o)){status=scene_guard_retreat_return(o,now);if(status)return status;}
        campaign_enemy_reaction(o,source);return RF_OK;
    }
    seconds=(float)(2.0+(double)scene_medic_flight_random()/32768.0);
    o->ai_mode.flags_530&=~8u;o->view.flags_7d0&=~8u;o->firing.flags_7d0&=~8u;
    if((o->damage.effects.flags_814&0x10u)||campaign_npc_ai_suppressed(o)||o->ai_mode.action_280==17){
        o->guard_retreat_fault=0;return RF_OK; /* Source handled late refusal. */
    }
    status=rf_scene_npc_reset_ai_animation(handle,1);if(status)return status;
    if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND;
    status=rf_timer_set(&o->flight.reset_deadline_1370,now,30000+(int32_t)(scene_medic_flight_random()%30001u));if(status)return status;
    status=campaign_pain_reset_retained(NULL,handle,o->view.weapons[0]);if(status)return status;
    if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND;
    o->view.flags_810&=~0x02000000u;o->damage.effects.flags_810=o->view.flags_810;o->ai_override.word_834=-1;
    if(!scene_guard_retreat_canonical(o))return RF_FORMAT;
    duration=(int32_t)((double)seconds*1000.0);
    if(scene_guard_retreat_active(o)){
        if(o->ai_mode.action_280!=7||o->view.action_520!=7||o->flight.target_handle!=source)return RF_FORMAT;
        status=rf_timer_remaining(o->flight.deadline_250,now,&remaining);if(status)return status;
        if(duration>remaining){status=rf_timer_set(&o->flight.deadline_250,now,duration);if(status)return status;}
        o->guard_retreat_fault=0;return RF_OK;
    }
    {int32_t reset=o->flight.reset_deadline_1370;static const float biases[3]={-.5f,0,.5f};
     uint32_t ordinal=0;
     /* Match the constructor's registered skeletal-owner ordinal, which can
      * differ from the raw row index when a model-less row precedes us. */
     for(uint32_t j=0;j<index;j++)ordinal+=campaign_poses.items[j].skeleton!=UINT32_MAX;
     memset(&o->flight,0,sizeof(o->flight));o->flight.reset_deadline_1370=reset;
     o->flight.sight_deadline_294=o->flight.no_route_deadline_4d4=o->flight.stall_deadline_4d0=-1;
     o->flight.nav.previous=o->flight.nav.current=-1;o->flight.nav.wait_134=now;
     o->flight.nav.approach_15c=1;o->flight.nav.bias_158=biases[ordinal%3];}
    campaign_enemy_reaction(o,source); /* Existing first30 once; same-player cadence survives. */
    campaign_pursuit_stop(o);memset(&o->script_move,0,sizeof(o->script_move));
    o->navigation.retained.count=0;
    o->navigation.start.radius=o->navigation_radius;o->navigation.start.height=o->death_item_extent_7c4;o->navigation.start.word_040=0;
    o->guard_resume_action=o->ai_mode.action_280;
    status=rf_timer_set(&o->flight.deadline_250,now,duration);if(status)return status;
    status=rf_entity_ai_set_action(&o->ai_mode,7,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    o->view.action_520=7;o->flight.qualified=2;o->flight.entered=1;
    o->flight.target_kind=SCENE_FLIGHT_TARGET_PLAYER;o->flight.target_handle=source;
    status=scene_medic_flight_nav_request(scene_actor_collision_owner,o,&threat,now,&found);if(status)return status;
    if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND;
    o->guard_retreat_fault=0;
    if(!found)return scene_guard_retreat_return(o,now);
    return RF_OK;
}
#endif
