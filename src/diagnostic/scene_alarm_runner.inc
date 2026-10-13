#ifndef RF_SCENE_ALARM_RUNNER_INCLUDED
#define RF_SCENE_ALARM_RUNNER_INCLUDED
/* Original4087a0 +4049b0/404a30. Bounded on-foot player damage/sight
 * consumers reuse the existing practical route/movement and sight policy.
 * No new perception, arbitrary destinations or actor/animation pools. */
enum { SCENE_ALARM_TRIGGER_DAMAGE, SCENE_ALARM_TRIGGER_SIGHT };
static uint64_t scene_alarm_runner_epoch;
static uint32_t scene_alarm_runner_servicing,scene_alarm_runner_fault;
static void scene_alarm_runner_reset(void)
{++scene_alarm_runner_epoch;scene_alarm_runner_fault=0;}
/* requests,started,route refusals,duplicate refusals,arrivals,clips,activated,
 * cancelled,last UID,last alarm,status,optional audio requests */
static uint32_t scene_alarm_runner_stats[12];
static rf_runtime_event *scene_alarm_runner_event(uint32_t uid)
{
    rf_runtime_event *found=NULL;
    if(!uid||uid==UINT32_MAX||!campaign_events.items)return NULL;
    for(uint32_t i=0;i<campaign_events.count;i++){
        rf_runtime_event *e=campaign_events.items+i;
        if(!e->authored||e->authored->record.uid!=uid)continue;
        if(found||e->retired||e->object_kind!=6||e->state.type!=46||strcmp(e->authored->record.type,"Alarm")||
           rf_object_registry_lookup(&campaign_registry,e->handle)!=e||!e->authored->record.has_orientation)return NULL;
        found=e;
    }
    return found;
}
static int scene_alarm_runner_authored(uint32_t slot,uint32_t *uid,const rf_level_event **event)
{
    const rf_level_owned_entity *record;rf_level_entity_spawn spawn;rf_level_entity_ai ai;
    const uint8_t *raw;uint32_t at=4,bytes,cls;rf_runtime_event *e;int status;
    if(!uid||!event||!campaign_seeds.items||!campaign_seeds.classes||!campaign_seeds.records.items||
       slot>=campaign_seeds.records.count)return RF_RANGE;
    record=campaign_seeds.records.items+slot;cls=campaign_seeds.items[slot].class_index;
    if(cls>=campaign_seeds.class_count)return RF_FORMAT;
    if((campaign_seeds.classes[cls].physics.flags&0x20011u)!=0x20011u||
       (campaign_seeds.classes[cls].physics.flags&(0x200u|0x400000u))||
       campaign_seeds.classes[cls].physics.use_kind==1||campaign_seeds.classes[cls].model_kind!=2)return RF_NOT_FOUND;
    status=rf_level_entity_spawn_read(record,&spawn);if(status)return status;
    status=rf_level_entity_ai_read(record,&ai);if(status)return status;
    /* Initial implementation admits authored Waiting, with no independent
     * patrol/seat owner to steal. Explicit later commands remain authoritative. */
    if(ai.mode!=1||spawn.seat_host_uid!=-1)return RF_NOT_FOUND;
    raw=record->raw;bytes=record->record.bytes;
    status=scene_medic_flight_raw_string(raw,bytes,&at);if(status)return status;
    if(at>bytes||bytes-at<48)return RF_FORMAT;at+=48;
    status=scene_medic_flight_raw_string(raw,bytes,&at);if(status)return status;
    if(at>bytes||bytes-at<13)return RF_FORMAT;at+=13;
    for(uint32_t i=0;i<2;i++){status=scene_medic_flight_raw_string(raw,bytes,&at);if(status)return status;}
    if(at>bytes||bytes-at<29)return RF_FORMAT;at+=29;
    for(uint32_t i=0;i<7;i++){status=scene_medic_flight_raw_string(raw,bytes,&at);if(status)return status;}
    if(at>bytes||bytes-at<35)return RF_FORMAT;
    *uid=(uint32_t)raw[at+14]|(uint32_t)raw[at+15]<<8|(uint32_t)raw[at+16]<<16|(uint32_t)raw[at+17]<<24;
    if(raw[at+18+11])return RF_NOT_FOUND; /* Damage-flight no-flee pre-gate. */
    e=scene_alarm_runner_event(*uid);if(!e)return RF_NOT_FOUND;
    for(uint32_t i=0;i<3;i++)if(!isfinite(e->authored->record.position[i]))return RF_FORMAT;
    for(uint32_t i=0;i<9;i++)if(!isfinite(e->authored->record.orientation_disk[i]))return RF_FORMAT;
    *event=&e->authored->record;return RF_OK;
}
static int scene_alarm_runner_resources(uint32_t slot,const rf_entity_motion_selection *selection,uint32_t demand)
{
    const rf_entity_playback_model *m;const rf_entity_pose *pose;uint32_t skeleton,count=4,unique=0,cache[23];
    int32_t ids[23];uint64_t missing=0;int status;
    if(!selection||demand>1||!campaign_poses.items||slot>=campaign_npc_body_count||slot>=campaign_poses.count)return RF_RANGE;
    skeleton=selection->mapping.skeleton;
    if(!campaign_playback_resources.models||!campaign_motion_catalog.models||
       skeleton>=campaign_playback_resources.model_count||skeleton>=campaign_motion_catalog.model_count)return RF_NOT_FOUND;
    m=campaign_playback_resources.models+skeleton;pose=campaign_poses.items+slot;
    ids[0]=selection->mapping.states[0];ids[1]=selection->mapping.states[2];
    ids[2]=selection->mapping.states[4];ids[3]=selection->mapping.actions[44];
    if(!m->resources||!m->cache_ids||!campaign_motion_catalog.models[skeleton].items)return RF_FORMAT;
    if(demand){
        if(pose->skeleton!=skeleton||pose->playback.completion.active.count>16||pose->playback.completion.frozen||pose->playback.completion.active.freeze_slot!=-1)return RF_NOT_FOUND;
        for(uint32_t j=0;j<3;j++){
            int32_t state=j==0?pose->controller.current:j==1?pose->controller.next:
                pose->controller.override_enabled?pose->controller.override_state:-1;
            if(state< -1||state>=23)return RF_FORMAT;
            if(state>=0)ids[count++]=selection->mapping.states[state];
        }
        for(uint32_t j=0;j<pose->playback.completion.active.count;j++)ids[count++]=pose->playback.completion.active.slots[j].motion;
    }
    if(demand){
        uint32_t slots=pose->playback.completion.active.count;
        for(uint32_t j=0;j<slots;j++)if(pose->playback.completion.active.slots[j].motion==ids[3]){slots=0;break;}
        if(slots>=16)return RF_NOT_FOUND;
    }
    for(uint32_t j=0;j<count;j++){
        int32_t id=ids[j];uint32_t c,k;const rf_motion_file *file;
        if(id<0||(uint32_t)id>=m->count||(uint32_t)id>=campaign_motion_catalog.models[skeleton].count)return RF_NOT_FOUND;
        if(j==3&&(m->resources[id].looping||m->resources[id].comparison.end_tick<=m->resources[id].comparison.start_tick))return RF_NOT_FOUND;
        c=m->cache_ids[id];file=&campaign_motion_catalog.models[skeleton].items[id].file;
        if(c>=campaign_npc_motion_count||!campaign_npc_motion_data||!campaign_npc_motion_sizes||!file->archive||!file->entry.size||m->resources[id].references<0||
           m->resources[id].looping!=campaign_motion_catalog.models[skeleton].items[id].looping||
           memcmp(&m->resources[id].comparison,&campaign_motion_catalog.models[skeleton].items[id].comparison,sizeof(m->resources[id].comparison))||
           (campaign_npc_motion_data[c]&&campaign_npc_motion_sizes[c]!=file->entry.size))return RF_FORMAT;
        for(k=0;k<unique;k++)if(cache[k]==c)break;
        if(k==unique){cache[unique++]=c;if(!campaign_npc_motion_data[c])missing+=file->entry.size;}
    }
    if(!demand)return RF_OK;
    if((uint64_t)campaign_npc_motion_bytes+missing>SCENE_NPC_MOTION_BUDGET)return RF_NOT_FOUND;
    for(uint32_t j=0;j<count;j++){status=campaign_npc_motion_require(skeleton,(uint32_t)ids[j]);if(status)return status;}
    return RF_OK;
}
static int scene_alarm_runner_cancel(campaign_npc_body *o,int32_t now,uint32_t preserve)
{
    rf_entity_ai_transition_state next;int status;
    if(!o||preserve>1||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(scene_medic_flight_restoring||!o->alarm.present)return RF_OK;
    if(o->alarm_fault)return RF_FORMAT;
    preserve=preserve&&o->ai_mode.action_280!=16;
    ++scene_alarm_runner_epoch;
    if(!preserve){
        /* Incoming Play_Animation clears its old script marker before starting
         * a new clip. Never stop that incoming clip, including identical IDs. */
        if(o->alarm.phase==3&&o->script_animation.active&&
           o->script_animation.motion==o->selection.mapping.actions[44]){
            status=campaign_animation_cancel(o);if(status){o->alarm_fault=1;return status;}
            memset(&o->script_animation,0,sizeof(o->script_animation));
        }
        if(o->ai_mode.action_280==16){
            next=o->ai_mode;
            status=rf_entity_ai_set_action(&next,2,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
            status=rf_entity_ai_set_state(&next,0,(float)now);if(status)return status;
            o->ai_mode=next;o->view.action_520=2;
        }
        memset(&o->script_move,0,sizeof(o->script_move));o->script_move.stop=1;
        memset(&o->navigation,0,sizeof(o->navigation));
        memset(o->command_714,0,12);memset(o->look.command_708,0,12);
    }
    memset(&o->alarm,0,sizeof(o->alarm));++scene_alarm_runner_stats[7];return RF_OK;
}
static int scene_alarm_runner_request(campaign_npc_body *o,uint32_t source,int32_t now,
    uint32_t reason,uint32_t *handled)
{
    const rf_level_event *event;uint32_t slot,uid,found=0;int status;
    rf_entity_ai_transition_state next;campaign_npc_route before_route;
    float before_target[3];uint32_t before_index;
    if(!o||!handled||reason>SCENE_ALARM_TRIGGER_SIGHT)return RF_RANGE;*handled=0;
    if(o->alarm_fault)return RF_FORMAT;
    if(o->alarm.present){*handled=1;return RF_OK;}
    if(scene_alarm_runner_fault)return RF_FORMAT;
    if(scene_alarm_runner_servicing||scene_medic_flight_restoring||!scene_actor_collision_owner||
       !scene_actor_collision_owner->collision||
       source!=campaign_player_object.handle||source==o->registration.handle||!scene_medic_live(o)||
       campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,source)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)source)!=&campaign_player_view||
       campaign_player_damage.state.effects.health<=0||campaign_cutscene_runtime.active||
       /* Damage keeps its full-form exclusion. Sight follows ordinary fresh
        * acquisition: only an uncompromised active disguise blocks it. */
       (campaign_player_form.active&&(reason==SCENE_ALARM_TRIGGER_DAMAGE||!campaign_player_form.compromised))||
       (reason==SCENE_ALARM_TRIGGER_SIGHT&&o->damage.effects.affiliation!=0)||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||
       !isfinite(o->movement.speed)||o->movement.speed<=0||(o->object_flags&(2u|0x4000u))||
       (o->view.flags_810&(1u|0x400u))||((o->view.flags_814|o->damage.effects.flags_814)&0x10u)||
       campaign_npc_ai_suppressed(o)||(o->ai_mode.action_280!=0&&o->ai_mode.action_280!=2)||
       o->ai_mode.state_2b4||o->view.linked_handle!=-1||o->attachment_75c!=-1||o->controller_handle||
       o->patrol.enabled||o->script_move.active||o->script_move.path.count||o->navigation.retained.count||
       o->script_look.active||o->script_shoot.active||o->script_once.count||o->script_animation.active||
       o->combat_alert||o->combat_scripted||o->combat_reload_pending||o->combat_reload_due||
       o->medic.pending||o->medic.clip_owned||o->flight.entered||o->flight.qualified==2||o->damage.effects.burn||
       scene_npc_seat_attached(o->registration.handle)||campaign_npc_physics_suspended(o)||
       o->support.handle||o->piece_support.tag||o->scripted_physics_seen||o->scripted_physics||
       o->movement_slot>=16||campaign_modes[o->movement_slot].index!=1||rf_scene_alarm[4])return RF_OK;
    slot=(uint32_t)(o-campaign_npc_bodies);
    status=scene_alarm_runner_authored(slot,&uid,&event);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    ++scene_alarm_runner_stats[0];
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *peer=campaign_npc_bodies+i;
        if(peer!=o&&peer->alarm.present&&peer->alarm.event==uid&&scene_medic_live(peer)&&
           peer->damage.effects.health>0&&!(peer->object_flags&2u)){
            ++scene_alarm_runner_stats[3];return RF_OK;
        }
    }
    status=scene_guard_retreat_player_onfoot(scene_actor_collision_owner);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_alarm_runner_resources(slot,&o->selection,1);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    next=o->ai_mode;status=rf_entity_ai_set_action(&next,16,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    status=rf_entity_ai_set_state(&next,1,(float)now);if(status)return status;next.flags_530&=~0x200000u;
    /* The ordinary route helper publishes only this owner's route. Restore
     * its previous route/target on refusal, before any action is accepted. */
    before_route=o->navigation;memcpy(before_target,o->script_move.target,12);before_index=o->script_move.route_index;
    memcpy(o->script_move.target,event->position,12);
    ++scene_alarm_runner_servicing;status=campaign_script_route(scene_actor_collision_owner,o,&found);--scene_alarm_runner_servicing;
    if(status||!found){o->navigation=before_route;memcpy(o->script_move.target,before_target,12);o->script_move.route_index=before_index;
        ++scene_alarm_runner_stats[2];return status;}
    memset(&o->script_move,0,sizeof(o->script_move));memcpy(o->script_move.target,event->position,12);
    o->alarm=(rf_npc_checkpoint_alarm){1,uid,1,0};o->ai_mode=next;o->view.action_520=16;
    o->script_move.active=1;o->script_move.event=uid;o->script_move.follow=0;o->script_move.stop=0;
    o->script_move.retry=0;o->script_move.origin=SCENE_MOVE_ORIGIN_NONE;
    o->combat_target=o->combat_alert=o->combat_scripted=o->combat_due=o->combat_navigation_due=o->combat_burst_remaining=0;scene_blind_clear(o);
    ++scene_alarm_runner_epoch;++scene_alarm_runner_stats[1];
    scene_alarm_runner_stats[8]=(uint32_t)campaign_seeds.records.items[slot].record.uid;scene_alarm_runner_stats[9]=uid;
    *handled=1;return RF_OK;
}
static int scene_alarm_runner_damage(campaign_npc_body *o,uint32_t source,int32_t now,uint32_t *handled)
{return scene_alarm_runner_request(o,source,now,SCENE_ALARM_TRIGGER_DAMAGE,handled);}
/*403a80 assigns an observed target then408ac0 tries4087a0 before weapons.
 * Reuse the port's existing sight cadence/cone/world-mover cover; this is
 * only a new consumer for authored Waiting runners, not general perception.
 * No sight-origin state survives onset: RFNC31's existing runner is complete. */
static int scene_alarm_runner_sight(scene_stream *stream,uint32_t slot,uint32_t frame,
    const float player_eye[3],uint32_t *handled)
{
    campaign_npc_body *o;scene_npc_reload_player_receipt player;
    const rf_level_event *event;uint32_t uid,blocked,handle;float delta[3],distance;
    uint64_t epoch;int status;
    if(!handled||!stream||!player_eye||slot>=campaign_npc_body_count)return RF_RANGE;
    *handled=0;o=campaign_npc_bodies+slot;
    if(stream!=scene_actor_collision_owner||!stream->collision||scene_medic_flight_restoring||
       campaign_cutscene_runtime.active||campaign_endgame.phase||
       (frame%30+campaign_enemy_sight_phase_offset)%30!=slot%30||
       o->alarm.present||o->combat_alert||o->combat_scripted||o->combat_target||
       o->script_once.count||o->script_shoot.active||o->script_look.active||
       o->script_animation.active||o->script_animation.freeze||
       o->script_move.active||o->script_move.path.count||o->patrol.enabled||
       o->navigation.retained.count||!campaign_enemy_opposed_live(o)||
       !o->body.spheres.items||!o->body.spheres.count||
       o->damage.effects.affiliation!=0||campaign_npc_ai_suppressed(o)||
       (o->ai_mode.action_280!=0&&o->ai_mode.action_280!=2)||
       !campaign_enemy_mode_admits(o,1,0)||
       !isfinite(campaign_player_damage.state.effects.health)||campaign_player_damage.state.effects.health<=0||
       (campaign_player_form.active&&!campaign_player_form.compromised))return RF_OK;
    status=scene_alarm_runner_authored(slot,&uid,&event);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_npc_reload_player_admit(stream,&player);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(!campaign_enemy_opposed_cone(o,player_eye,delta,&distance))return RF_OK;
    handle=o->registration.handle;epoch=scene_alarm_runner_epoch;
    status=combat_obstructed(stream,o->eye_position,delta,&blocked);if(status)return status;
    if(blocked)return RF_OK;
    /* The query is synchronous and gameplay-inert. Authenticate its actual
     * player/body/stream and runner identity before resource/route admission. */
    status=scene_npc_reload_player_seal(stream,&player);if(status)return status;
    if(epoch!=scene_alarm_runner_epoch||slot>=campaign_npc_body_count||o!=campaign_npc_bodies+slot||
       o->registration.handle!=handle||!campaign_enemy_opposed_live(o))return RF_FORMAT;
    return scene_alarm_runner_request(o,player.player,scene_medic_now(frame),SCENE_ALARM_TRIGGER_SIGHT,handled);
}
static int scene_alarm_runner_live_present(uint32_t *out)
{
    if(!out)return RF_RANGE;
    if(scene_alarm_runner_fault||scene_alarm_runner_servicing)return RF_FORMAT;*out=0;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        if(campaign_npc_bodies[i].alarm_fault)return RF_FORMAT;
        if(campaign_npc_bodies[i].alarm.present)*out=1;
    }
    return RF_OK;
}
/* Loaded-level identity is mandatory even for otherwise valid RFNC bytes. */
static int scene_alarm_runner_candidate(uint32_t slot,const rf_npc_checkpoint_record *r,
    const rf_entity_motion_selection *selection)
{
    const rf_level_event *event;const rf_entity_playback_model *m;uint32_t uid,found=0;int32_t id;int status;
    if(!r||!selection)return RF_RANGE;if(!r->alarm.present)return r->ai_mode==16?RF_FORMAT:RF_OK;
    status=scene_alarm_runner_authored(slot,&uid,&event);if(status)return status;
    if(uid!=r->alarm.event||r->ai_mode!=16||r->alarm.phase<1||r->alarm.phase>3||
       r->alarm.started!=(r->alarm.phase==3)||!r->movement_present||r->movement_slot>=16||
       campaign_modes[r->movement_slot].index!=1||r->speed_mode>2||r->flags&0x4000u)return RF_FORMAT;
    status=scene_alarm_runner_resources(slot,selection,0);if(status)return status;
    if(r->alarm.phase==1){
        if(!r->move.active||r->move.event!=uid||r->move.follow||r->move.path_count||
           r->move.path_mode||r->move.path_index||r->move.path_reverse||memcmp(r->move.target,event->position,12)||
           memcmp(r->move.route_goal,event->position,12))return RF_FORMAT;
    }else{
        float x=r->position[0]-event->position[0],z=r->position[2]-event->position[2];
        if(r->move.active||x*x+z*z>.25f)return RF_FORMAT;
    }
    if(r->alarm.phase!=3)return r->script_animation.active?RF_FORMAT:RF_OK;
    id=selection->mapping.actions[44];m=campaign_playback_resources.models+selection->mapping.skeleton;
    if(!r->animation_present||!r->script_animation.active||r->script_animation.motion!=id||
       r->script_animation.loop||r->script_animation.freeze||r->playback.completion.frozen||
       r->playback.completion.active.count>16)return RF_FORMAT;
    for(uint32_t j=0;j<r->playback.completion.active.count;j++){
        const rf_motion_active_slot *a=r->playback.completion.active.slots+j;
        if(a->motion!=id)continue;
        if(++found!=1||a->weight<=0||a->tick<m->resources[id].comparison.start_tick||
           a->tick>=m->resources[id].comparison.end_tick)return RF_FORMAT;
    }
    return found==1?RF_OK:RF_FORMAT;
}
static int scene_alarm_runner_capture(uint32_t slot,rf_npc_checkpoint_alarm *out)
{
    const campaign_npc_body *o;const rf_level_event *event;uint32_t uid;int status;
    if(!out||slot>=campaign_npc_body_count)return RF_RANGE;o=campaign_npc_bodies+slot;memset(out,0,sizeof(*out));
    if(scene_alarm_runner_fault||scene_alarm_runner_servicing||o->alarm_fault)return RF_FORMAT;
    if(!o->alarm.present)return o->ai_mode.action_280==16?RF_FORMAT:RF_OK;
    status=scene_alarm_runner_authored(slot,&uid,&event);if(status)return status;
    if(!scene_medic_live(o)||o->alarm.event!=uid||o->ai_mode.action_280!=16||o->view.action_520!=16||
       o->ai_mode.state_2b4!=(o->alarm.phase==1?1:10)||
       o->alarm.started!=(o->alarm.phase==3)||o->patrol.enabled||o->script_move.origin||
       o->script_move.path.count||o->script_move.path.indices||o->piece_support.tag||
       o->damage.effects.burn||o->medic.pending||o->medic.clip_owned||
       o->flight.entered||o->flight.qualified==2||o->combat_target||
       campaign_cutscene_runtime.active)return RF_FORMAT;
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if(i!=slot&&campaign_npc_bodies[i].alarm.present&&
       campaign_npc_bodies[i].alarm.event==uid)return RF_FORMAT;
    *out=o->alarm;return RF_OK;
}
#endif
